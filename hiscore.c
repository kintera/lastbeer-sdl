// module hiscore.c
/* Allegro port Copyright (C) 2014 Gavin Smith. */
// [c] copyright 1993 by ALPHA-HELIX

#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

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

#ifdef LASTBEER_SDL
static struct highscoretext classic_scores;
#else
static struct highscoretext classic_scores={{{   81000L, "ATOMMUELL"},
				    {   29600L, "POWER"},
				    {   14800L, "JUBEL"},
				    {    8110L, "ZYNAX"},
				    {    8099L, "HEAVY"},
				    {    6367L, "WASSERGLAS"},
				    {    4558L, "TWEETY"},
				    {       1L, "TRITONE"}},
				   "EIN PROSIT AUF ALPHA-HELIX"};
#endif
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

#ifdef LASTBEER_SDL
static const char score_characters[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.!?";
static const char *score_spoof_names[] = {
   "SPUDWISER", "HEINYKEN", "CORNONA", "STELLA ARTOAST", "PABSNT BLUE",
   "MILLER BLIGHT", "COORS BLIGHT", "GUINMESS", "DOS EGGQUIS", "SAMMY HOPS"
};
static const char *score_random_beer(void)
{
   return score_spoof_names[compat_random_index((int)(sizeof(score_spoof_names) / sizeof(score_spoof_names[0])))];
}
static int score_is_blank(const char *text)
{
   while (*text) if (*text++ != ' ') return 0;
   return 1;
}
int init_default_highscores(void)
{
   struct highscoretext *tables[] = {&classic_scores, &easy_scores, &hard_scores};
   int changed = 0;
   /* Replace the exact untouched original table, but keep all player scores. */
   static const struct { const char *name; long score; } old_defaults[] = {
      {"ATOMMUELL", 81000}, {"POWER", 29600}, {"JUBEL", 14800}, {"ZYNAX", 8110},
      {"HEAVY", 8099}, {"WASSERGLAS", 6367}, {"TWEETY", 4558}, {"TRITONE", 1}
   };
   int untouched = 1;
   for (int i = 0; i < MAXENTRIES; ++i)
      if (classic_scores.player[i].score != old_defaults[i].score ||
          strcmp(classic_scores.player[i].name, old_defaults[i].name) != 0) untouched = 0;
   if (untouched) memset(classic_scores.player, 0, sizeof(classic_scores.player));
   for (int d = 0; d < 3; ++d) {
      int filled = 0;
      int choices[10];
      for (int i = 0; i < 10; ++i) choices[i] = i;
      for (int i = 9; i > 0; --i) {
         int j = compat_random_index(i + 1), tmp = choices[i];
         choices[i] = choices[j]; choices[j] = tmp;
      }
      int next = 0;
      for (int i = 0; i < MAXENTRIES; ++i) {
         struct playerdatas *entry = &tables[d]->player[i];
         if (entry->score == 0 && entry->name[0] == 0) {
            strcpy(entry->name, score_spoof_names[choices[next++]]);
            /* Scores display with two decimal places: 100.01 through 499.99. */
            entry->score = 10001 + compat_random_index(39999);
            changed = filled = 1;
         }
      }
      for (int i = 1; filled && i < MAXENTRIES; ++i) {
         struct playerdatas entry = tables[d]->player[i];
         int j = i;
         while (j > 0 && tables[d]->player[j - 1].score < entry.score) {
            tables[d]->player[j] = tables[d]->player[j - 1]; --j;
         }
         tables[d]->player[j] = entry;
      }
   }
   return changed;
}
void reset_highscores(void)
{
   memset(&classic_scores, 0, sizeof(classic_scores));
   memset(&easy_scores, 0, sizeof(easy_scores));
   memset(&hard_scores, 0, sizeof(hard_scores));
   init_default_highscores();
}
static struct sprstrc *score_strip_font;
static int score_strip_index = -1;
static int score_mouse_click, score_mouse_x, score_mouse_y;
#endif
static void cursorblink(int x, int y, char symbol, struct sprstrc *font)
{
   int set;

   set=FALSE;
   if(symbol==0) symbol=' ';
#ifdef LASTBEER_SDL
   score_mouse_click = 0;
#endif

   while(!keypressed()) {
#ifdef LASTBEER_SDL
      int mouse_event = compat_mouse_poll(&score_mouse_x, &score_mouse_y);
      if (mouse_event == -2) {
         score_mouse_click = 1;
         break;
      }
      if (mouse_event == -1 && score_strip_font && score_mouse_x >= 0 &&
          score_mouse_y >= 228 && score_mouse_y < 228 + score_strip_font->ys) {
         int index = score_mouse_x / score_strip_font->xs;
         if (index < (int)strlen(score_characters) && index != score_strip_index) {
            int previous = score_strip_index;
            if (previous >= 0)
               putspritedirect(score_strip_font, previous * score_strip_font->xs, 228,
                               score_characters[previous] - ' ');
            score_strip_index = index;
            putspritedirect(score_strip_font, index * score_strip_font->xs, 228, 3);
            set = 8;
#ifdef LASTBEER_AUTOMATION
            if (getenv("LASTBEER_TEST_SCORE_HOVER")) {
               int width = score_strip_font->xs, height = score_strip_font->ys;
               const unsigned char *pixels = (const unsigned char *)score_strip_font->data;
               const unsigned char *cursor = pixels + 3 * width * height;
               const unsigned char *old_letter = previous >= 0 ?
                  pixels + (score_characters[previous] - ' ') * width * height : NULL;
               for (int p = 0; p < 2; ++p) for (int py = 0; py < height; ++py) for (int px = 0; px < width; ++px) {
                  int offset = py * width + px;
                  if (getpixel(full_pages[p], index * width + px, 228 + py) != cursor[offset] ||
                      (old_letter && getpixel(full_pages[p], previous * width + px, 228 + py) != old_letter[offset])) {
                     fprintf(stderr, "SMOKE: score hover did not redraw the letter cursor\n"); exit(1);
                  }
               }
               compat_test_score_hover(index);
            }
#endif
         }
      }
#endif
      if(set==0){
	 set=8;
	 putspritedirect(font, x, y, 3);
#ifdef LASTBEER_SDL
         if (score_strip_font && score_strip_index >= 0)
            putspritedirect(score_strip_font, score_strip_index * score_strip_font->xs, 228, 3);
#endif
      }
      if(set==4){
	 putspritedirect(font, x, y, symbol-' ');
#ifdef LASTBEER_SDL
         if (score_strip_font && score_strip_index >= 0)
            putspritedirect(score_strip_font, score_strip_index * score_strip_font->xs, 228,
                            score_characters[score_strip_index] - ' ');
#endif
      }
      set--;
      showpage(page);
      waitfortick();
   }
   if(set>3) putspritedirect(font, x, y, symbol-' ');
#ifdef LASTBEER_SDL
   if (score_strip_font && score_strip_index >= 0)
      putspritedirect(score_strip_font, score_strip_index * score_strip_font->xs, 228,
                      score_characters[score_strip_index] - ' ');
#endif
   showpage(page);
}

#ifdef LASTBEER_SDL
/* The controller edits a character in place; DONE is a separate position. */
static int score_pad_edit(int event, char *text, int capacity, int *position, int *done, int keyboard)
{
   int scan = (event >> 8) & 255;
   int left = keyboard ? KEY_LEFT : key_left, right = keyboard ? KEY_RIGHT : key_right;
   int up = keyboard ? KEY_UP : key_up, down = keyboard ? KEY_DOWN : key_down;
   int length = (int)strlen(text);
   if (!keyboard && scan == KEY_ESC) return 1;
   if (!keyboard && scan == key_fire && *done) return 1;
   if (scan == left) {
      if (*done) { *done = 0; *position = length < capacity ? length : capacity - 1; }
      else if (*position > 0) --*position;
   } else if (scan == right || (!keyboard && scan == key_fire)) {
      if (!*done) {
         if (*position < length && *position + 1 < capacity) ++*position;
         else *done = 1;
      }
   }
   if (scan == up || scan == down) {
      if (!*done) {
         const char *found = strchr(score_characters, *position < length ? text[*position] : ' ');
         int count = (int)strlen(score_characters);
         int choice = found ? (int)(found - score_characters) : 0;
         choice = (choice + (scan == up ? 1 : count - 1)) % count;
         text[*position] = score_characters[choice];
         if (*position == length) text[*position + 1] = 0;
      }
   }
   {
      const char *found = strchr(score_characters, *position < (int)strlen(text) ? text[*position] : ' ');
      score_strip_index = found ? (int)(found - score_characters) : 0;
   }
   return 0;
}

/* The character strip writes at the current position; DONE submits the text. */
static int score_mouse_edit(char *text, int capacity, int *position, int *done,
                            int text_x, int text_y, int char_width, int char_height)
{
   int length = (int)strlen(text);
   if (score_mouse_y >= 171 && score_mouse_y < 189 && score_mouse_x >= 230 && score_mouse_x < 300)
      return 1;
   if (score_mouse_y >= 228 && score_mouse_y < 228 + score_strip_font->ys && score_mouse_x >= 0) {
      int index = score_mouse_x / score_strip_font->xs;
      if (index < (int)strlen(score_characters)) {
         if (*position >= capacity) *position = capacity - 1;
         text[*position] = score_characters[index];
         if (*position >= length) text[*position + 1] = 0;
         score_strip_index = index;
         *done = 0;
         if (*position + 1 < capacity) ++*position;
      }
   } else if (score_mouse_y >= text_y && score_mouse_y < text_y + char_height && score_mouse_x >= text_x) {
      int clicked = (score_mouse_x - text_x) / char_width;
      if (clicked <= length && clicked < capacity) { *position = clicked; *done = 0; }
   }
   return 0;
}

static void score_draw_navigation(struct sprstrc *font)
{
   writetext(48, 180, "<", font);
   writetext(80, 180, "MODE", font);
   writetext(136, 180, ">", font);
   writetext(224, 180, "BACK", font);
}
static int score_read_navigation(int fullmode)
{
   for (;;) {
      int x, y;
      int event = compat_menu_poll(0, &x, &y);
      if (event > 0) return event;
      if (event == -2) {
         if (fullmode) return KEY_ENTER;
         if (y >= 176 && y < 196) {
            if (x >= 40 && x < 72) return key_left;
            if (x >= 128 && x < 160) return key_right;
            if (x >= 216 && x < 280) return KEY_ENTER;
         }
      }
      rest(5);
   }
}
#endif

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

int loadhighscore(int *filvar)
{
   struct highscoretext saved;
#ifdef LASTBEER_SDL
   struct highscoretext *previous = current_scores;
   int all_valid = 1;
   current_scores = &classic_scores;
#endif
   if (read(*filvar, &saved, sizeof(saved)) != sizeof(saved)) {
#ifdef LASTBEER_SDL
      current_scores = previous;
#endif
      return 0;
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
            return 0;
         }
      if (!memchr(hstxt.winnertext, 0, sizeof(hstxt.winnertext))) {
         hstxt = original;
#ifdef LASTBEER_SDL
         current_scores = previous;
#endif
         return 0;
      }
   }
#ifdef LASTBEER_SDL
   {
      char magic[4];
      int magic_size = read(*filvar, magic, sizeof(magic));
      if (magic_size == sizeof(magic) && memcmp(magic, "DIF1", 4) == 0) {
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
            else all_valid = 0;
         } else all_valid = 0;
      } else if (magic_size != 0) all_valid = 0;
   }
   current_scores = previous;
   return all_valid;
#else
   return 1;
#endif
}

static int write_score_data(int fd, const void *data, size_t size)
{
   const unsigned char *p = data;
   while (size) {
      int n = write(fd, p, size);
      if (n < 0 && errno == EINTR) continue;
      if (n <= 0) return 0;
      p += n;
      size -= n;
   }
   return 1;
}

int savehighscore(int *filvar)
{
   int ok;
#ifdef LASTBEER_SDL
   struct highscoretext *previous = current_scores;
   current_scores = &classic_scores;
#endif
   codescore();
   ok = write_score_data(*filvar, &hstxt, sizeof(struct highscoretext));
   decodescore();
#ifdef LASTBEER_SDL
   if (ok) ok = write_score_data(*filvar, "DIF1", 4);
   current_scores = &easy_scores; codescore(); if (ok) ok = write_score_data(*filvar, &hstxt, sizeof(hstxt)); decodescore();
   current_scores = &hard_scores; codescore(); if (ok) ok = write_score_data(*filvar, &hstxt, sizeof(hstxt)); decodescore();
   current_scores = previous;
#endif
   return ok;
}

#ifdef LASTBEER_AUTOMATION
static int test_default_scores(const struct highscoretext *table)
{
   for (int i = 0; i < MAXENTRIES; ++i) {
      int known = 0;
      for (unsigned j = 0; j < sizeof(score_spoof_names) / sizeof(score_spoof_names[0]); ++j)
         if (strcmp(table->player[i].name, score_spoof_names[j]) == 0) known = 1;
      if (!known || table->player[i].score <= 10000 || table->player[i].score >= 50000 ||
          (i && table->player[i - 1].score < table->player[i].score)) return 0;
   }
   return 1;
}
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
   if (!init_default_highscores() || !test_default_scores(&classic_scores) ||
       !test_default_scores(&easy_scores) || !test_default_scores(&hard_scores) ||
       init_default_highscores()) ok = 0;
   {
      static const char *old_names[] = {"ATOMMUELL", "POWER", "JUBEL", "ZYNAX", "HEAVY", "WASSERGLAS", "TWEETY", "TRITONE"};
      static const long old_scores[] = {81000, 29600, 14800, 8110, 8099, 6367, 4558, 1};
      for (int i = 0; i < MAXENTRIES; ++i) {
         strcpy(classic_scores.player[i].name, old_names[i]); classic_scores.player[i].score = old_scores[i];
      }
      if (!init_default_highscores() || !test_default_scores(&classic_scores)) ok = 0;
   }
   memset(&classic_scores, 0, sizeof(classic_scores));
   strcpy(classic_scores.player[0].name, "PLAYER"); classic_scores.player[0].score = 1234;
   {
      int found = 0;
      if (!init_default_highscores()) ok = 0;
      for (int i = 0; i < MAXENTRIES; ++i)
         if (strcmp(classic_scores.player[i].name, "PLAYER") == 0 && classic_scores.player[i].score == 1234) found = 1;
      if (!found) ok = 0;
   }
   reset_highscores();
   if (!test_default_scores(&classic_scores) || !test_default_scores(&easy_scores) ||
       !test_default_scores(&hard_scores) || init_default_highscores()) ok = 0;
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
   if (ftruncate(fd, sizeof(classic_scores) + 5) || lseek(fd, 0, SEEK_SET) < 0 || loadhighscore(&fd)) ok = 0;
   /* A file from before difficulty support must still load as Classic. */
   if (ftruncate(fd, sizeof(classic_scores)) || lseek(fd, 0, SEEK_SET) < 0) ok = 0;
   memset(&classic_scores, 0, sizeof(classic_scores));
   memset(&easy_scores, 0, sizeof(easy_scores));
   memset(&hard_scores, 0, sizeof(hard_scores));
   loadhighscore(&fd);
   if (classic_scores.player[0].score != 222 || easy_scores.player[0].score || hard_scores.player[0].score) ok = 0;
   fclose(file);
   /* A failed replacement must leave the prior settings and scores intact. */
   classic_scores.player[0].score = 222;
   saveconfig();
   classic_scores.player[0].score = 999;
   compat_test_set_save_failure(1);
   saveconfig();
   compat_test_set_save_failure(0);
   fd = open(compat_config_path(), O_RDONLY | O_BINARY);
   if (fd < 0 || lseek(fd, 7 * sizeof(short), SEEK_SET) < 0) ok = 0;
   else {
      memset(&classic_scores, 0, sizeof(classic_scores));
      loadhighscore(&fd);
      if (classic_scores.player[0].score != 222) ok = 0;
   }
   if (fd >= 0) close(fd);
   classic_scores = saved_classic; easy_scores = saved_easy; hard_scores = saved_hard; current_scores = previous;
   return ok;
}

void compat_test_prepare_score_entry(int difficulty)
{
   const char *rank = getenv("LASTBEER_TEST_SCORE_RANK");
   memset(&classic_scores, 0, sizeof(classic_scores));
   memset(&easy_scores, 0, sizeof(easy_scores));
   memset(&hard_scores, 0, sizeof(hard_scores));
   init_default_highscores();
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
      const char *expected_name = getenv("LASTBEER_TEST_SCORE_MIXED") ? "CB" :
                                  getenv("LASTBEER_TEST_SCORE_MOUSE") ? "BB" :
                                  getenv("LASTBEER_TEST_SCORE_PAD") ? "AB" :
                                  getenv("LASTBEER_TEST_SCORE_ARROWS") ? "AB" :
                                  getenv("LASTBEER_TEST_SCORE_BATCH") ? "AB" : "A";
      int name_ok = strcmp(hstxt.player[second].name, expected_name) == 0;
      int winner_ok = strcmp(hstxt.winnertext, second ? "CHAMPION" : "B") == 0;
      if (getenv("LASTBEER_TEST_SCORE_EMPTY")) {
         name_ok = winner_ok = 0;
         for (unsigned n = 0; n < sizeof(score_spoof_names) / sizeof(score_spoof_names[0]); ++n) {
            if (strcmp(hstxt.player[second].name, score_spoof_names[n]) == 0) name_ok = 1;
            if (strcmp(hstxt.winnertext, score_spoof_names[n]) == 0) winner_ok = 1;
         }
      }
      ok = (no_entry || (hstxt.player[second].score == 100000 &&
           name_ok && winner_ok &&
           (!second || (hstxt.player[0].score == 120000 && strcmp(hstxt.player[0].name, "CHAMP") == 0)))) &&
           (!no_entry || test_default_scores(&hstxt)) &&
           (difficulty == DIFFICULTY_CLASSIC || test_default_scores(&classic_scores)) &&
           (difficulty == DIFFICULTY_EASY || test_default_scores(&easy_scores)) &&
           (difficulty == DIFFICULTY_HARD || test_default_scores(&hard_scores));
   }
   close(fd);
   classic_scores = saved_classic; easy_scores = saved_easy; hard_scores = saved_hard; current_scores = previous;
   return ok;
}
#endif

void highscore(char fullmode)
{
   int i,j,x,xs,currentpl,inlist;
   int keys;
   char *font2_file, *font_file;
   struct sprstrc *font2, *font;

   struct sndstrc snd;
   void *ptr;
   SAMPLE *s;
#ifdef LASTBEER_SDL
   int score_difficulty = fullmode ? game_difficulty : compat_difficulty();
   current_scores = score_table(score_difficulty);
   compat_mouse_cursor_visible(1);
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
   if (!fullmode) score_draw_navigation(font2);
#endif
   x=(320-strlen(hstxt.winnertext)*font2->xs)/2;
   writetext(x,YOFFS+(MAXENTRIES+3)*14-6,hstxt.winnertext,font2);
   glowin(0);

// Enter name

   if(inlist){
#ifdef LASTBEER_SDL
      int pad_done = 0;
      score_strip_font = font; score_strip_index = 0;
      writetext(8, 171, "L/R POSITION", font);
      writetext(240, 171, "DONE", font);
      writetext(8, 181, "UP/DOWN LETTER  FIRE SELECT", font);
      writetext(0, 228, score_characters, font);
      showpage(page);
#endif
      clear_keybuf();
      xs=font->xs;
      i=0;
      x=NXOFFS;
      //setvect(0x09, int09);
      do{
#ifdef LASTBEER_AUTOMATION
         if (getenv("LASTBEER_TEST_SCORE_ENTRY")) TEST_STAGE("score-name");
#endif
#ifdef LASTBEER_SDL
         cursorblink(pad_done ? 230 : x, pad_done ? 171 : YOFFS+currentpl*14,
                     pad_done ? ' ' : hstxt.player[currentpl].name[i], font);
         if (score_mouse_click) {
            if (score_mouse_edit(hstxt.player[currentpl].name, NOCP, &i, &pad_done,
                                 NXOFFS, YOFFS+currentpl*14, xs, font->ys)) break;
            x = NXOFFS + i * xs;
            writetext(NXOFFS, YOFFS+currentpl*14, hstxt.player[currentpl].name, font);
            showpage(page); keys = -1; continue;
         }
#else
	 cursorblink(x,YOFFS+currentpl*14,hstxt.player[currentpl].name[i],font);
#endif
         /* Read ASCII value */
	 keys=readkey();
#ifdef LASTBEER_SDL
         if (compat_last_key_gamepad() || ((keys & 0xff) == 0 &&
             ((keys >> 8) == KEY_LEFT || (keys >> 8) == KEY_RIGHT ||
              (keys >> 8) == KEY_UP || (keys >> 8) == KEY_DOWN))) {
            if (score_pad_edit(keys, hstxt.player[currentpl].name, NOCP, &i, &pad_done,
                               !compat_last_key_gamepad())) break;
            x = NXOFFS + i * xs;
            if (i < (int)strlen(hstxt.player[currentpl].name))
               putspritedirect(font, x, YOFFS+currentpl*14, hstxt.player[currentpl].name[i]-' ');
            showpage(page);
            keys = -1;
            continue;
         }
         pad_done = 0;
         keys &= 0xff;
#else
         keys &= 0xff;
#endif
	 if(keys<=122 && keys>=97) keys-=32;
	 if((keys<=90 && keys>=65) || keys==63 || keys == 46 ||
		keys==39 || (keys<=57 && keys >=48) ||
		keys==32 || keys==33 || keys==34){
#ifdef LASTBEER_SDL
            int at_end = i >= (int)strlen(hstxt.player[currentpl].name);
#endif
	    hstxt.player[currentpl].name[i]=keys;
#ifdef LASTBEER_SDL
            if (at_end) hstxt.player[currentpl].name[i+1]=0;
#else
	    hstxt.player[currentpl].name[i+1]=0;
#endif
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
      if(
#ifdef LASTBEER_SDL
         score_is_blank(hstxt.player[currentpl].name)
#else
         hstxt.player[currentpl].name[0]==0
#endif
      ){
#ifdef LASTBEER_SDL
         strcpy(hstxt.player[currentpl].name, score_random_beer());
         writetext(NXOFFS,YOFFS+currentpl*14,hstxt.player[currentpl].name,font);
#else
	 strcpy(hstxt.player[currentpl].name,"TOO DRUNK");
	 writetext(x,YOFFS+currentpl*14,hstxt.player[currentpl].name,font);
#endif
      }

// Winnertext

      if(currentpl==0){
#ifdef LASTBEER_SDL
         pad_done = 0; score_strip_index = 0;
#endif
	 writetext(64, YOFFS+(MAXENTRIES+1)*14+8, "PLEASE ENTER WINNER TEXT", font);
	 i=0;
	 xs=font2->xs;
	 do{
#ifdef LASTBEER_AUTOMATION
            if (getenv("LASTBEER_TEST_SCORE_ENTRY")) TEST_STAGE("score-winner");
#endif
#ifdef LASTBEER_SDL
            x = (320 - (int)strlen(hstxt.winnertext) * xs) / 2 + i * xs;
            cursorblink(pad_done ? 230 : x, pad_done ? 171 : YOFFS+(MAXENTRIES+3)*14-6,
                        pad_done || i >= NOCW ? ' ' : hstxt.winnertext[i], pad_done ? font : font2);
            if (score_mouse_click) {
               int text_x = (320 - (int)strlen(hstxt.winnertext) * xs) / 2;
               if (score_mouse_edit(hstxt.winnertext, NOCW, &i, &pad_done, text_x,
                                    YOFFS+(MAXENTRIES+3)*14-6, xs, font2->ys)) break;
               clearregion(YOFFS+(MAXENTRIES+3)*14-5,10);
               writetext((320 - (int)strlen(hstxt.winnertext)*xs)/2,
                         YOFFS+(MAXENTRIES+3)*14-6, hstxt.winnertext, font2);
               showpage(page); keys = -1; continue;
            }
#else
	    if(i == NOCW){
	       x = (320 + (NOCW-3) * xs) / 2;
	       cursorblink(x,YOFFS+(MAXENTRIES+3)*14-6,hstxt.winnertext[i-1],font2);
	    }else{
	       x = (320 + (i-1) * xs) / 2;
	       cursorblink(x,YOFFS+(MAXENTRIES+3)*14-6,hstxt.winnertext[i],font2);
	    }
#endif
	    keys=readkey();
#ifdef LASTBEER_SDL
            if (compat_last_key_gamepad() || ((keys & 0xff) == 0 &&
                ((keys >> 8) == KEY_LEFT || (keys >> 8) == KEY_RIGHT ||
                 (keys >> 8) == KEY_UP || (keys >> 8) == KEY_DOWN))) {
               if (score_pad_edit(keys, hstxt.winnertext, NOCW, &i, &pad_done,
                                  !compat_last_key_gamepad())) break;
               clearregion(YOFFS+(MAXENTRIES+3)*14-5,10);
               writetext((320 - (int)strlen(hstxt.winnertext)*xs)/2,
                         YOFFS+(MAXENTRIES+3)*14-6, hstxt.winnertext, font2);
               showpage(page);
               keys = -1;
               continue;
            }
            pad_done = 0;
            keys &= 0xff;
#else
            keys &= 0xff;
#endif
	    if(keys<=122 && keys>=97) keys-=32;
	    if((keys<=93 && keys>=65) || (keys<=59 && keys>=39) ||
			keys==32 || keys==33 || keys==34 ||
			keys==61 || keys==63){
	       if(i<NOCW){
#ifdef LASTBEER_SDL
                  int at_end = i >= (int)strlen(hstxt.winnertext);
#endif
		  hstxt.winnertext[i]=keys;
#ifdef LASTBEER_SDL
                  if (at_end) hstxt.winnertext[i+1]=0;
#else
		  hstxt.winnertext[i+1]=0;
#endif
		  i++;
	       }else{
		  hstxt.winnertext[NOCW-1]=keys;
		  hstxt.winnertext[NOCW]=0;
	       }
	       clearregion(YOFFS+(MAXENTRIES+3)*14-5,10);
#ifdef LASTBEER_SDL
               writetext((320-(int)strlen(hstxt.winnertext)*xs)/2,YOFFS+(MAXENTRIES+3)*14-6,hstxt.winnertext,font2);
#else
	       writetext((320-(i+1)*xs)/2,YOFFS+(MAXENTRIES+3)*14-6,hstxt.winnertext,font2);
#endif
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
	 if(
#ifdef LASTBEER_SDL
            score_is_blank(hstxt.winnertext)
#else
            hstxt.winnertext[0]==0
#endif
         ){
#ifdef LASTBEER_SDL
            strcpy(hstxt.winnertext, score_random_beer());
#else
	    strcpy(hstxt.winnertext,"TOO DRUNK TO WRITE DOWN A TEXT!!!");
#endif
	    x=(320-strlen(hstxt.winnertext)*xs)/2;
	    writetext(x,YOFFS+(MAXENTRIES+3)*14-6,hstxt.winnertext,font2);
	 }

	 writetext(64, YOFFS+(MAXENTRIES+1)*14+8, "                        ", font);
         showpage(page);
      }
      //setvect(0x09, newint09);
      saveconfig();
#ifdef LASTBEER_SDL
      score_strip_font = NULL; score_strip_index = -1;
#endif
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
         const char *prompt = "PRESS A KEY / CLICK";
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
#ifdef LASTBEER_SDL
      if (fullmode) score_read_navigation(1);
      else {
         for (;;) {
            int scan = score_read_navigation(0);
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
            score_draw_navigation(font2);
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
         }
      }
#else
      while (!keypressed());
#endif
   }

   glowout();
   stop_sample (s);
   free_SAMPLE(s);
   unloadfile (ptr);

   unloadfile(font2_file); free (font2);
   unloadfile(font_file); free (font);
#ifdef LASTBEER_SDL
   current_scores = &classic_scores;
   compat_mouse_cursor_visible(0);
#endif
}
