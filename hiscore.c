// module hiscore.c
/* Allegro port Copyright (C) 2014 Gavin Smith. */
// [c] copyright 1993 by ALPHA-HELIX

#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifndef O_BINARY
# define O_BINARY 0
#endif
#define	MAXENTRIES	8
#define	NOCP		15	// Number of Chars playername
#define	NOCW		33	// Number of Chars winnertext
#define YOFFS		60	// Y-offset for listbegin
#define SXOFFS		255     // X-offset for centiliter
#define NXOFFS		55	// X-offset for names

#include "xmode.h"
#include "sound.h"
#include "fileman.h"
#include "baller.h"
#ifdef LASTBEER_SDL
#include "modern/difficulty.h"
#endif

struct playerdatas{
   long score;
   char name[NOCP+1];
};

struct highscoretext{
   struct playerdatas player[MAXENTRIES];
   char winnertext[NOCW+1];
};

static struct highscoretext classic_scores={{{   81000L, "ATOMMUELL"},
				    {   29600L, "POWER"},
				    {   14800L, "JUBEL"},
				    {    8110L, "ZYNAX"},
				    {    8099L, "HEAVY"},
				    {    6367L, "WASSERGLAS"},
				    {    4558L, "TWEETY"},
				    {       1L, "TRITONE"}},
				   "EIN PROSIT AUF ALPHA-HELIX"};
static struct highscoretext *current_scores = &classic_scores;
#define hstxt (*current_scores)
#ifdef LASTBEER_SDL
static struct highscoretext easy_scores, hard_scores;
static struct highscoretext *score_table(int difficulty)
{
   return difficulty == DIFFICULTY_EASY ? &easy_scores :
          difficulty == DIFFICULTY_HARD ? &hard_scores : &classic_scores;
}
static const char *difficulty_name(int difficulty)
{
   return difficulty == DIFFICULTY_EASY ? "EASY" : difficulty == DIFFICULTY_HARD ? "HARD" : "CLASSIC";
}
#endif

static void cursorblink(int x, int y, char symbol, struct sprstrc *font)
{
   int set;

   set=FALSE;
   if(symbol==0) symbol=' ';

   clear_keybuf();
   while(!keypressed()) {
      if(set==0){
	 set=8;
	 putspritedirect(font, x, y, 3);
      }
      if(set==4){
	 putspritedirect(font, x, y, symbol-' ');
      }
      set--;
      showpage(page);
      waitfortick();
   }
   if(set>3) putspritedirect(font, x, y, symbol-' ');
   showpage(page);
}

static void codescore(void)
{
   int i,j;

   for(j=0;j<MAXENTRIES;j++){
      for(i=0;i<=NOCP;i++){
	 hstxt.player[j].name[i]+=3*i;
      }
      hstxt.player[j].score+=123456789;
   }
   for(j=0;j<=NOCW;j++){
      hstxt.winnertext[j]+=j*2;
   }
}

static void decodescore(void)
{
   int i,j;

   for(j=0;j<MAXENTRIES;j++){
      for(i=0;i<=NOCP;i++){
	 hstxt.player[j].name[i]-=3*i;
      }
      hstxt.player[j].score-=123456789;
   }
   for(j=0;j<=NOCW;j++){
      hstxt.winnertext[j]-=j*2;
   }
}

void loadhighscore(int *filvar)
{
   struct highscoretext saved;
#ifdef LASTBEER_SDL
   struct highscoretext *previous = current_scores;
   current_scores = &classic_scores;
#endif
   if (read(*filvar, &saved, sizeof(saved)) != sizeof(saved)) {
#ifdef LASTBEER_SDL
      current_scores = previous;
#endif
      return;
   }
   {
      struct highscoretext original = hstxt;
      hstxt = saved;
      decodescore();
      for (int i = 0; i < MAXENTRIES; ++i)
         if (!memchr(hstxt.player[i].name, 0, sizeof(hstxt.player[i].name))) {
            hstxt = original;
#ifdef LASTBEER_SDL
            current_scores = previous;
#endif
            return;
         }
      if (!memchr(hstxt.winnertext, 0, sizeof(hstxt.winnertext))) hstxt = original;
   }
#ifdef LASTBEER_SDL
   {
      char magic[4];
      if (read(*filvar, magic, sizeof(magic)) == sizeof(magic) && memcmp(magic, "DIF1", 4) == 0) {
         struct highscoretext extra[2];
         if (read(*filvar, extra, sizeof(extra)) == sizeof(extra)) {
            struct highscoretext decoded[2];
            int valid = 1;
            for (int d = 0; d < 2; ++d) {
               struct highscoretext original;
               current_scores = d ? &hard_scores : &easy_scores;
               original = hstxt;
               hstxt = extra[d];
               decodescore();
               for (int i = 0; i < MAXENTRIES; ++i)
                  if (!memchr(hstxt.player[i].name, 0, sizeof(hstxt.player[i].name))) valid = 0;
               if (!memchr(hstxt.winnertext, 0, sizeof(hstxt.winnertext))) valid = 0;
               decoded[d] = hstxt;
               hstxt = original;
            }
            if (valid) { easy_scores = decoded[0]; hard_scores = decoded[1]; }
         }
      }
   }
   current_scores = previous;
#endif
}

void savehighscore(int *filvar)
{
#ifdef LASTBEER_SDL
   struct highscoretext *previous = current_scores;
   current_scores = &classic_scores;
#endif
   codescore();
   write(*filvar, &hstxt, sizeof(struct highscoretext));
   decodescore();
#ifdef LASTBEER_SDL
   write(*filvar, "DIF1", 4);
   current_scores = &easy_scores; codescore(); write(*filvar, &hstxt, sizeof(hstxt)); decodescore();
   current_scores = &hard_scores; codescore(); write(*filvar, &hstxt, sizeof(hstxt)); decodescore();
   current_scores = previous;
#endif
}

#ifdef LASTBEER_AUTOMATION
int compat_test_score_storage(void)
{
   struct highscoretext saved_classic = classic_scores, saved_easy = easy_scores, saved_hard = hard_scores;
   struct highscoretext *previous = current_scores;
   FILE *file = tmpfile();
   int ok = 1, fd;
   if (!file) return 0;
   fd = fileno(file);
   memset(&classic_scores, 0, sizeof(classic_scores));
   memset(&easy_scores, 0, sizeof(easy_scores));
   memset(&hard_scores, 0, sizeof(hard_scores));
   classic_scores.player[0].score = 222; strcpy(classic_scores.player[0].name, "CLASSIC");
   easy_scores.player[0].score = 111; strcpy(easy_scores.player[0].name, "EASY");
   hard_scores.player[0].score = 333; strcpy(hard_scores.player[0].name, "HARD");
   current_scores = &hard_scores;
   savehighscore(&fd);
   memset(&classic_scores, 0, sizeof(classic_scores));
   memset(&easy_scores, 0, sizeof(easy_scores));
   memset(&hard_scores, 0, sizeof(hard_scores));
   if (lseek(fd, 0, SEEK_SET) < 0) ok = 0;
   loadhighscore(&fd);
   if (classic_scores.player[0].score != 222 || easy_scores.player[0].score != 111 ||
       hard_scores.player[0].score != 333 || strcmp(easy_scores.player[0].name, "EASY") ||
       strcmp(hard_scores.player[0].name, "HARD") || current_scores != &hard_scores) ok = 0;
   /* A file from before difficulty support must still load as Classic. */
   if (ftruncate(fd, sizeof(classic_scores)) || lseek(fd, 0, SEEK_SET) < 0) ok = 0;
   memset(&classic_scores, 0, sizeof(classic_scores));
   memset(&easy_scores, 0, sizeof(easy_scores));
   memset(&hard_scores, 0, sizeof(hard_scores));
   loadhighscore(&fd);
   if (classic_scores.player[0].score != 222 || easy_scores.player[0].score || hard_scores.player[0].score) ok = 0;
   fclose(file);
   classic_scores = saved_classic; easy_scores = saved_easy; hard_scores = saved_hard; current_scores = previous;
   return ok;
}

void compat_test_prepare_score_entry(int difficulty)
{
   const char *rank = getenv("LASTBEER_TEST_SCORE_RANK");
   if (rank && strcmp(rank, "second") == 0) {
      struct highscoretext *table = score_table(difficulty);
      table->player[0].score = 120000;
      strcpy(table->player[0].name, "CHAMP");
      strcpy(table->winnertext, "CHAMPION");
   }
   if (rank && strcmp(rank, "none") == 0) saveconfig();
}

int compat_test_score_entry(int difficulty)
{
   struct highscoretext saved_classic = classic_scores, saved_easy = easy_scores, saved_hard = hard_scores;
   struct highscoretext *previous = current_scores;
   const char *path = compat_config_path();
   const char *rank = getenv("LASTBEER_TEST_SCORE_RANK");
   int second = rank && strcmp(rank, "second") == 0;
   int no_entry = rank && strcmp(rank, "none") == 0;
   int fd = path ? open(path, O_RDONLY | O_BINARY) : -1;
   int ok = 0;
   if (fd < 0) return 0;
   memset(&classic_scores, 0, sizeof(classic_scores));
   memset(&easy_scores, 0, sizeof(easy_scores));
   memset(&hard_scores, 0, sizeof(hard_scores));
   if (lseek(fd, 7 * sizeof(short), SEEK_SET) >= 0) {
      loadhighscore(&fd);
      current_scores = score_table(difficulty);
      ok = (no_entry || (hstxt.player[second].score == 100000 &&
           strcmp(hstxt.player[second].name, "A") == 0 &&
           strcmp(hstxt.winnertext, second ? "CHAMPION" : "B") == 0 &&
           (!second || (hstxt.player[0].score == 120000 && strcmp(hstxt.player[0].name, "CHAMP") == 0)))) &&
           (!no_entry || hstxt.player[0].score == (difficulty == DIFFICULTY_CLASSIC ? 81000 : 0)) &&
           (difficulty == DIFFICULTY_CLASSIC || classic_scores.player[0].score == 81000) &&
           (difficulty == DIFFICULTY_EASY || easy_scores.player[0].score == 0) &&
           (difficulty == DIFFICULTY_HARD || hard_scores.player[0].score == 0);
   }
   close(fd);
   classic_scores = saved_classic; easy_scores = saved_easy; hard_scores = saved_hard; current_scores = previous;
   return ok;
}
#endif

void highscore(char fullmode)
{
   int i,j,x,xs,currentpl,inlist;
   char keys;
   char *font2_file, *font_file;
   struct sprstrc *font2, *font;

   struct sndstrc snd;
   void *ptr;
   SAMPLE *s;
#ifdef LASTBEER_SDL
   int score_difficulty = fullmode ? game_difficulty : compat_difficulty();
   current_scores = score_table(score_difficulty);
#endif

   setvanillapalette(0);
   setpage(0); showpage(0); clearscreen();
   ptr = loadfile(datapool, "SCORE.PCX");
   showpcx256(ptr, 0);
   unloadfile(ptr);

   font_file = loadfile(datapool, "FONT1.SPR");
   create_sprstrc (&font, font_file);
   font2_file = loadfile(datapool, "FONT2.SPR");
   create_sprstrc (&font2, font2_file);

   ptr = loadfile(datapool, "HS.SND");
   create_sndstrc (&snd, ptr);
   s = create_SAMPLE (&snd);
   playloop(s);

// Checking for a place in the highscore

   inlist=FALSE;
   if(fullmode){
      currentpl=0;
      while(!inlist && currentpl<MAXENTRIES){
	 if(score<=hstxt.player[currentpl].score) currentpl++;
	 else inlist=TRUE;
      }

// Is in highscore

      if(inlist){
	 for(i=MAXENTRIES-1; i>currentpl; i--) hstxt.player[i]=hstxt.player[i-1];
	 strcpy(hstxt.player[currentpl].name,"");
	 hstxt.player[currentpl].score=score;
	 if(currentpl==0) strcpy(hstxt.winnertext,"");

      }
   }
// Show new highscore

   for(i=0;i<MAXENTRIES;i++){
      writetext(NXOFFS,YOFFS+i*14,hstxt.player[i].name,font);
      writenumber(SXOFFS,YOFFS+i*14,hstxt.player[i].score,font);
   }
#ifdef LASTBEER_SDL
   writetext(8, 43, difficulty_name(score_difficulty), font2);
   if (!fullmode) writetext(164, 43, "LEFT / RIGHT", font2);
#endif
   x=(320-strlen(hstxt.winnertext)*font2->xs)/2;
   writetext(x,YOFFS+(MAXENTRIES+3)*14-6,hstxt.winnertext,font2);
   glowin(0);

// Enter name

   if(inlist){
      xs=font->xs;
      i=0;
      x=NXOFFS;
      //setvect(0x09, int09);
      do{
#ifdef LASTBEER_AUTOMATION
         if (getenv("LASTBEER_TEST_SCORE_ENTRY")) TEST_STAGE("score-name");
#endif
	 cursorblink(x,YOFFS+currentpl*14,hstxt.player[currentpl].name[i],font);
         /* Read ASCII value */
	 keys=readkey() & 0xFF;
	 if(keys<=122 && keys>=97) keys-=32;
	 if((keys<=90 && keys>=65) || keys==63 || keys == 46 ||
		keys==39 || (keys<=57 && keys >=48) ||
		keys==32 || keys==33 || keys==34){
	    hstxt.player[currentpl].name[i]=keys;
	    hstxt.player[currentpl].name[i+1]=0;
	    putspritedirect(font, x, YOFFS+currentpl*14, keys-' ');
	    if(i<NOCP-1){
	       i++;
	       x+=xs;
	    }
	 }
	 else if(keys==0x08 && i>0){
	    if(i!=(NOCP-1) || hstxt.player[currentpl].name[NOCP-1]==0){
	       i--;
	       x-=xs;
	    }
	    hstxt.player[currentpl].name[i]=0;
	    putspritedirect(font, x, YOFFS+currentpl*14, 0);
	 }
         showpage(page); /* Update screen, as we are not drawing
                            directly to video memory. */
      }while(keys!=0x0d);
      if(hstxt.player[currentpl].name[0]==0){
	 strcpy(hstxt.player[currentpl].name,"TOO DRUNK");
	 writetext(x,YOFFS+currentpl*14,hstxt.player[currentpl].name,font);
      }

// Winnertext

      if(currentpl==0){
	 writetext(64, YOFFS+(MAXENTRIES+1)*14+8, "PLEASE ENTER WINNER TEXT", font);
	 i=0;
	 xs=font2->xs;
	 do{
#ifdef LASTBEER_AUTOMATION
            if (getenv("LASTBEER_TEST_SCORE_ENTRY")) TEST_STAGE("score-winner");
#endif
	    if(i == NOCW){
	       x = (320 + (NOCW-3) * xs) / 2;
	       cursorblink(x,YOFFS+(MAXENTRIES+3)*14-6,hstxt.winnertext[i-1],font2);
	    }else{
	       x = (320 + (i-1) * xs) / 2;
	       cursorblink(x,YOFFS+(MAXENTRIES+3)*14-6,hstxt.winnertext[i],font2);
	    }
	    keys=readkey() & 0xFF;
	    if(keys<=122 && keys>=97) keys-=32;
	    if((keys<=93 && keys>=65) || (keys<=59 && keys>=39) ||
			keys==32 || keys==33 || keys==34 ||
			keys==61 || keys==63){
	       if(i<NOCW){
		  hstxt.winnertext[i]=keys;
		  hstxt.winnertext[i+1]=0;
		  i++;
	       }else{
		  hstxt.winnertext[NOCW-1]=keys;
		  hstxt.winnertext[NOCW]=0;
	       }
	       clearregion(YOFFS+(MAXENTRIES+3)*14-5,10);
	       writetext((320-(i+1)*xs)/2,YOFFS+(MAXENTRIES+3)*14-6,hstxt.winnertext,font2);
	    }
	    else if(keys==0x08 && i>0){
	       i--;
	       hstxt.winnertext[i]=0;
	       clearregion(YOFFS+(MAXENTRIES+3)*14-5,10);
	       writetext((320-(i+1)*xs)/2,YOFFS+(MAXENTRIES+3)*14-6,hstxt.winnertext,font2);
	    }
            showpage(page); /* Update screen, as we are not drawing
                               directly to video memory. */
	 }while(keys!=0x0d);
	 if(hstxt.winnertext[0]==0){
	    strcpy(hstxt.winnertext,"TOO DRUNK TO WRITE DOWN A TEXT!!!");
	    x=(320-strlen(hstxt.winnertext)*xs)/2;
	    writetext(x,YOFFS+(MAXENTRIES+3)*14-6,hstxt.winnertext,font2);
	 }

	 writetext(64, YOFFS+(MAXENTRIES+1)*14+8, "                        ", font);
         showpage(page);
      }
      //setvect(0x09, newint09);
      saveconfig();
   }else if(fullmode){

// Not in highscore

      writetext(NXOFFS,YOFFS+(MAXENTRIES+1)*14-3,"YOUR SCORE",font);
      writenumber(SXOFFS,YOFFS+(MAXENTRIES+1)*14-3,score,font);
      showpage(page);
   }
   /* Submitting a score already confirms the screen; do not require a
      second, unexplained key press before returning to the menu. */
   if (!inlist) {
#ifdef LASTBEER_SDL
      if (fullmode) {
         const char *prompt = "PRESS A KEY / BUTTON";
         writetext((320 - strlen(prompt) * font2->xs) / 2, 43, prompt, font2);
         showpage(page);
      }
#endif
      clear_keybuf();
#ifdef LASTBEER_AUTOMATION
      TEST_MENU_CAPTURE(1);
      showpage(0);
      TEST_MENU_CAPTURE(0);
      if (!fullmode) compat_test_score_view(score_difficulty);
      TEST_STAGE("score");
#endif
      while (!keypressed());
#ifdef LASTBEER_SDL
      if (!fullmode) {
         for (;;) {
            int scan = (readkey() >> 8) & 255;
            if (scan != key_left && scan != key_right) break;
            score_difficulty = (score_difficulty + (scan == key_left ? 2 : 1)) % 3;
            current_scores = score_table(score_difficulty);
            clear_bitmap(full_pages[0]); clear_bitmap(full_pages[1]);
            setpage(0);
            char *background = loadfile(datapool, "SCORE.PCX");
            showpcx256(background, 0); unloadfile(background);
            for (i = 0; i < MAXENTRIES; ++i) {
               writetext(NXOFFS, YOFFS+i*14, hstxt.player[i].name, font);
               writenumber(SXOFFS, YOFFS+i*14, hstxt.player[i].score, font);
            }
            writetext(8, 43, difficulty_name(score_difficulty), font2);
            writetext(164, 43, "LEFT / RIGHT", font2);
            x = (320-strlen(hstxt.winnertext)*font2->xs)/2;
            writetext(x, YOFFS+(MAXENTRIES+3)*14-6, hstxt.winnertext, font2);
            showpage(0);
#ifdef LASTBEER_AUTOMATION
            TEST_MENU_CAPTURE(1);
            showpage(0);
            TEST_MENU_CAPTURE(0);
            compat_test_score_view(score_difficulty);
            TEST_STAGE("score");
#endif
            while (!keypressed());
         }
      }
#endif
   }

   glowout();
   stop_sample (s);
   unloadfile (ptr);

   unloadfile(font2_file); free (font2);
   unloadfile(font_file); free (font);
#ifdef LASTBEER_SDL
   current_scores = &classic_scores;
#endif
}
