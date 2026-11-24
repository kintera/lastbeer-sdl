/* Allegro port Copyright (C) 2014 Gavin Smith. */

#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#define MENUX           142     // X-offset for first menupoint
#define MENUY           58      // Y-offset for first menupoint
#define MSTEP           3       // How far to move at once
#define MENUDELTA       9/MSTEP // 1/MSTEP distance between menupoints
#define MENXTXT		160	// X-offset for menutext sprite
#define MENYTXT		46	// Y-offset for menutext sprite
#define MENYTEXT	60	// Y-offset for menutext
#define MENXTEXT	159	// X-offset for menutext
#define MENYERR         99

#define ENDLOGOX		20
#define ENDLOGOY		110

#define CREDITDELAY		100
#define CREDITSPEED		2
#define STORYSPEED		2
#define BACKDELAY		502
#define TITLEDELAY		460

#include "xmode.h"
#include "sound.h"
#include "fileman.h"
#include "baller.h"

#ifndef O_BINARY
# define O_BINARY 0
#endif

#ifdef LASTBEER_SDL
#ifdef LASTBEER_AUTOMATION
static int test_save_failure;
void compat_test_set_save_failure(int enabled) { test_save_failure = enabled; }
#endif
/* The SDL port uses its own file because its scancodes differ from Allegro 4. */
void loadconfig(void)
{
   int fd;
   int safe_to_save = 0;
   short values[7];
   const char *path = compat_config_path();
   key_up = KEY_UP; key_down = KEY_DOWN; key_left = KEY_LEFT;
   key_right = KEY_RIGHT; key_fire = KEY_SPACE; key_pause = KEY_P;
   if (!path) { init_default_highscores(); return; }
   fd = open(path, O_RDONLY | O_BINARY);
   if (fd < 0) {
      int missing = errno == ENOENT;
      if (init_default_highscores() && missing) saveconfig();
      return;
   }
   if (read(fd, values, sizeof(values)) == sizeof(values)) {
      int valid = 1;
      for (int i = 0; i < 6; ++i) {
         if (values[i] <= 0 || values[i] >= 128 || values[i] == KEY_ESC || values[i] == KEY_ENTER || values[i] == KEY_F11) valid = 0;
         for (int j = 0; j < i; ++j) if (values[i] == values[j]) valid = 0;
      }
      if (values[6] != 0 && values[6] != 1) valid = 0;
      if (valid) {
         key_up = values[0]; key_down = values[1]; key_left = values[2];
         key_right = values[3]; key_fire = values[4]; key_pause = values[5];
         speaker(values[6]);
      }
      /* Preserve scores even if an older or damaged key mapping is rejected. */
      safe_to_save = loadhighscore(&fd) && valid;
   }
   close(fd);
   if (init_default_highscores() && safe_to_save) saveconfig();
}
int saveconfig_checked(void)
{
   const char *path = compat_config_path();
   short values[7] = {key_up, key_down, key_left, key_right,
                      key_fire, key_pause, sound_config_value()};
   int fd = -1, ok = 0;
   char *temporary;
   if (!path || !(temporary = malloc(strlen(path) + 48))) {
      fprintf(stderr, "Could not save settings and scores.\n");
      return 0;
   }
   for (int attempt = 0; attempt < 32 && fd < 0; ++attempt) {
      sprintf(temporary, "%s.tmp-%llu-%d", path, (unsigned long long)compat_ticks(), attempt);
      fd = open(temporary, O_CREAT | O_EXCL | O_WRONLY | O_BINARY, 0600);
   }
   if (fd >= 0) {
      ok = write(fd, values, sizeof(values)) == sizeof(values) && savehighscore(&fd);
#ifdef LASTBEER_AUTOMATION
      if (test_save_failure) ok = 0;
#endif
      if (close(fd) != 0) ok = 0;
      if (ok) ok = compat_replace_file(temporary, path);
      if (!ok) remove(temporary);
   }
   if (!ok) fprintf(stderr, "Could not save settings and scores.\n");
   free(temporary);
   return ok;
}
void saveconfig(void) { (void)saveconfig_checked(); }
#else
void loadconfig(void)
{
   int filvar;
   int sound;

   char *fullpath = alloca (strlen (LOCALSTATEDIR) + 1
                            + strlen ("CONFIG.HIG") + 1);
   sprintf (fullpath, "%s/%s", LOCALSTATEDIR,  "CONFIG.HIG");

   /* Look for installed high score file; failing that, look in current
      directory. */
   if ((filvar = open(fullpath, O_RDONLY | O_BINARY, S_IREAD)) == -1
       && (filvar = open("CONFIG.HIG", O_RDONLY | O_BINARY, S_IREAD)) == -1) {
      key_up = KEY_UP;
      key_down = KEY_DOWN;
      key_left = KEY_LEFT;
      key_right = KEY_RIGHT;
      key_fire = KEY_SPACE;
      key_pause = KEY_P;
      return;
   }

   /* FIXME: Don't assume that a short int is 2 bytes and little-endian. */
   read(filvar, &key_up, sizeof(short));
   read(filvar, &key_down, sizeof(short));
   read(filvar, &key_left, sizeof(short));
   read(filvar, &key_right, sizeof(short));
   read(filvar, &key_fire, sizeof(short));
   read(filvar, &key_pause, sizeof(short));
   read(filvar, &sound, sizeof(short));
   speaker(sound);
   loadhighscore(&filvar);
   close(filvar);
}

void saveconfig(void)
{
   int filvar;
   int sound;

   if ((filvar = open(LOCALSTATEDIR "/CONFIG.HIG",
                      O_CREAT | O_WRONLY | O_TRUNC | O_BINARY, S_IWRITE)) == -1
       && (filvar = open("CONFIG.HIG",
                      O_CREAT | O_WRONLY | O_TRUNC | O_BINARY, S_IWRITE)) == -1)
     return;

   write(filvar, &key_up, sizeof(short));
   write(filvar, &key_down, sizeof(short));
   write(filvar, &key_left, sizeof(short));
   write(filvar, &key_right, sizeof(short));
   write(filvar, &key_fire, sizeof(short));
   write(filvar, &key_pause, sizeof(short));
   sound = speaker(-1);
   write(filvar, &sound, sizeof(short));
   savehighscore(&filvar);
   close(filvar);
}

#endif

static void closingtime(void)
{
   char *logo_file;
   struct sprstrc *logo;
   char *snd_data;
   struct sndstrc snd;
   SAMPLE *snd_handle;

   while (keypressed()) readkey();
   clearscreen();
   logo_file = loadfile(datapool, "CLOSE.SPR");
   create_sprstrc (&logo, logo_file);
   snd_data  = loadfile(datapool, "CLOSE.SND");
   create_sndstrc (&snd, snd_data);
   snd_handle = create_SAMPLE (&snd);
   playsample(snd_handle);
#ifdef LASTBEER_SDL
   clear_keybuf();
#endif
   putspritedirect(logo, ENDLOGOX, ENDLOGOY, 0);
   glowin(0);

   //while(!pressedkeys && soundbusy());
#ifndef LASTBEER_SDL
   clear_keybuf();
#endif
#ifdef LASTBEER_AUTOMATION
   TEST_STAGE("closing");
#endif
#ifdef LASTBEER_SDL
   {
      uint64_t start = compat_ticks();
      int reason = 0;
      while (compat_ticks() - start < 5000) {
         int x, y;
         int event = compat_menu_poll(0, &x, &y);
         if (event == -2) { reason = 1; break; }
         if (event > 0) { reason = 2; break; }
         rest(5);
      }
#ifdef LASTBEER_AUTOMATION
      compat_test_closing_result(reason, compat_ticks() - start);
#endif
   }
#else
   while (!keypressed());
#endif

   glowout();
   haltsound();
   free_SAMPLE(snd_handle);
   unloadfile((void *) snd_data);
   unloadfile((void *) logo_file); free(logo);
   clearscreen();
   setstandardpalette();
}

static void credits(void)
{
   void  *ptr;
   int   d, line, i;

   ptr = loadfile(datapool, "CREDIT.PCX");
   glowout();
   setpage(0);
   clearscreen();
   showpcx256(ptr, 0);
   //unloadfile(ptr);
   showpage(0);
   glowin(1);

   d = CREDITSPEED;			// Direction and speed of movement.
   line = 0;
   i = 0;
   if(!waitdelayedkey(CREDITDELAY)){
      do {
         waitfortick();
         showpcx256(ptr,-line);
         showpage(page);
	 line += d;
	 if (line < 0) {
	    i++;
	    d = -d; line += 2*d;
	    if (waitdelayedkey(CREDITDELAY)) break;
	 }
	 // 204 ist die linie, bei der das scrolling umgekehrt wird.
	 // Es ist ein proebli wert, der sehr stark vom kunstlerischen
	 // koennen des grafikers abhaengt.
	 if (line > 204) {
	    d = -d; line += 2*d;
	    if (waitdelayedkey(CREDITDELAY)) break;
	 }
      } while (i!=2 && !keypressed());
   }
}


void showtitle(void)
{
   void  *ptr;
   int filvar;
   void *buffer;

   struct sndstrc snd;
   SAMPLE *s;

   setpage(0);
   setvanillapalette(0);
   clearscreen();
   ptr = loadfile(datapool, "BACK.PCX");
   showpcx256(ptr, 0);
   unloadfile(ptr);

   ptr = loadfile(datapool, "TITLE.PCX");
   setpage(1);
   showpcx256(ptr, 10);
   unloadfile(ptr);

   ptr = loadfile(datapool, "TITLE.SND");
   create_sndstrc (&snd, ptr);
   s = create_SAMPLE (&snd);
   playsample(s);

   setpage(0);
   glowin(0);
#ifdef LASTBEER_AUTOMATION
   TEST_STAGE("background");
#endif
   waitdelayedkey(BACKDELAY);
   glowout();

   setpage(1);
   glowin(0);

#ifdef LASTBEER_AUTOMATION
   TEST_STAGE("title");
#endif
   if (!waitdelayedkey(TITLEDELAY)) {
      credits();
   }
   glowout();

   stop_sample (s);
   free_SAMPLE(s);
   unloadfile(ptr);
   //closefile(filvar);
   //free(buffer);
}

static int initmenu(SAMPLE **s, int *pobj, int *pspr, struct sprstrc **mspr)
{
   char  *ptr;
   struct sprstrc *sps;
   struct sndstrc ss;

// Load Menu, show and unload it

   clearscreen();
   setpage(0);
   ptr = loadfile(datapool, "MENU.PCX");
   showpcx256(ptr, 10);
   copypage(0,1);
   unloadfile(ptr);

// Load Pointer and place it

   ptr = loadfile(datapool, "MENCUR.SPR");
   create_sprstrc (&sps, ptr);
   *pspr = defsprite(sps, 2);
   unloadfile(ptr);
   *pobj = defobject(*pspr, MENUX, MENUY, OBJ_LOW);

// Load Menutext sprite and put it

   ptr = loadfile(datapool, "MENTXT.SPR");
   create_sprstrc (mspr, ptr);
   putspritedirect(*mspr, MENXTXT, MENYTXT, 0);

// Load Sound, play it and display all

   ptr = loadfile(datapool, "MENU.SND");
   create_sndstrc (&ss, ptr);
   *s = create_SAMPLE (&ss);
   playsample (*s);

   glowin(0);
   waitfortick();
   updatescreen();

   return 1;  // first menupoint is selected
}

static void endmenu(SAMPLE *snd, int pobj, int pspr, struct sprstrc *mspr)
{
   glowout();
   killobject(pobj);
   killsprite(pspr);
   unloadfile(mspr);
   stop_sample(snd);
   free_SAMPLE(snd);
   haltsound();
}

static int newkey(int y, const char *s, struct sprstrc *f1)
{
  unsigned short int k, ks, ka;

  /* Empty key press buffer, but slowly. This is not ideal. */
  while(keypressed()) readkey();

  writetext(MENXTXT,y,s,f1);
  showpage(page);

  k = readkey();
  ks = (0xFF00 & k) >> 8; /* Key scan code. */
  ka = 0x00FF & k; /* Key ASCII value */

  if (ka >= 0x21) {
    if (ka >= 0x60)
      ka &= 0x5F; /* Convert to upper case */
    putspritedirect(f1, MENXTXT+40, y, ka - 0x20);
  } else {
    char *n; /* Key name */

    switch (ks) {
    case KEY_ESC: 
      n = "ESC"; break;
    case KEY_SPACE: 
      n = "SPACE"; break;
    case KEY_CAPSLOCK: 
      n = "CPSLCK"; break;
    case KEY_BACKSPACE: 
      n = "BKSP"; break;
    case KEY_TAB: 
      n = "TAB"; break;
    case KEY_NUMLOCK: 
      n = "NUMLCK"; break;
    case KEY_SCRLOCK: 
      n = "SCRLCK"; break;
    case KEY_ENTER:
      n = "ENTER"; break;
    case KEY_LCONTROL:
    case KEY_RCONTROL:
      n = "CTRL"; break;
    case KEY_UP:
      n = "UP"; break;
    case KEY_PGUP:
      n = "PGUP"; break;
    case KEY_LSHIFT:
      n = "LSHFT"; break;
    case KEY_RSHIFT:
      n = "RSHFT"; break;
    case KEY_ASTERISK:
      n = "[*]"; break;
    case KEY_ALT:
      n = "ALT"; break;
    case KEY_LEFT:
      n = "LEFT"; break;
    case KEY_5_PAD:
      n = "[5]"; break;
    case KEY_RIGHT:
      n = "RIGHT"; break;
    case KEY_END:
      n = "END"; break;
    case KEY_DOWN:
      n = "DOWN"; break;
    case KEY_PGDN:
      n = "PGDN"; break;
    case KEY_INSERT:
      n = "INSERT"; break;
    case KEY_DEL:
      n = "DELETE"; break;
    case KEY_MINUS_PAD:
      n = "[-]"; break;
    case KEY_PLUS_PAD:
      n = "[+]"; break;
    case KEY_F1:
      n = "F1"; break;
    case KEY_F2:
      n = "F2"; break;
    case KEY_F3:
      n = "F3"; break;
    case KEY_F4:
      n = "F4"; break;
    case KEY_F5:
      n = "F5"; break;
    case KEY_F6:
      n = "F6"; break;
    case KEY_F7:
      n = "F7"; break;
    case KEY_F8:
      n = "F1"; break;
    case KEY_F9:
      n = "F9"; break;
    case KEY_F10:
      n = "F10"; break;
    case KEY_F11:
      n = "F11"; break;
    case KEY_F12:
      n = "F12"; break;
    default:
      n = "";
    }
    writetext(MENXTXT+40,y,n,f1);
  }
  return ks;
}

static void defkeys(struct sprstrc *font1)
{

// Left

   key_left = newkey(MENYTEXT-7, "LEFT", font1);

// Right

   key_right = newkey(MENYTEXT+3, "RIGHT", font1);

// Up

   key_up = newkey(MENYTEXT+13, "UP", font1);

// Down

   key_down = newkey(MENYTEXT+23, "DOWN", font1);

// Fire

   key_fire = newkey(MENYTEXT+33, "FIRE", font1);

// Pause

   key_pause = newkey(MENYTEXT+43, "PAUSE", font1);

   showpage(page);

   waitforkey();
   saveconfig();
}

static int showoptions(struct sprstrc *ofont, struct sprstrc *mspr)
{
   putspritedirect(mspr, MENXTXT, MENYTXT, 1);    // Clear mentxt

   writetext(MENXTEXT, MENYTEXT-8, "OPTIONS", ofont);
   if(speaker(-1)) writetext(MENXTEXT, MENYTEXT+3, "1 SOUND ON", ofont);
   else writetext(MENXTEXT, MENYTEXT+3, "1 SOUND OFF", ofont);
   writetext(MENXTEXT, MENYTEXT+MSTEP*MENUDELTA+3, "2 CHOOSE KEYS", ofont);
   writetext(MENXTEXT, MENYTEXT+MSTEP*MENUDELTA*2+3, "3 MAINMENU", ofont);
   return 1;
}

static int options(SAMPLE *snd, int *pobj, int pspr, struct sprstrc *mspr)
{
   char *ofont_data;
   struct sprstrc *ofont;
   int    quit, selected;
   int i;

   //haltsound();

   ofont_data = loadfile(datapool, "OFONT.SPR");
   create_sprstrc (&ofont, ofont_data);

// Clear old menu and write new one

   selected = showoptions(ofont, mspr);
   moveobject(*pobj, MENUX, MENUY+7);

   quit = FALSE;

// The Selection starts

   do{
      while (keypressed()) {
         clear_keybuf();
	 waitfortick();
	 updatescreen();
      }
      while (!keypressed()) {
	 waitfortick();
	 updatescreen();
      }
      if(key[KEY_1] || (selected==1 && key[key_fire])){   // Sound on/off
	 if(speaker(-1)){ 	// Turn Sound off
	    writetext(MENXTEXT, MENYTEXT+3, "1 SOUND OFF", ofont);
	    speaker(0);
	    saveconfig();
	 }else{			// Turn Sound on
	    if(speaker(1)){
	       writetext(MENXTEXT, MENYTEXT+3, "1 SOUND ON ", ofont);
	       saveconfig();
	    }else{
	       writetext(MENXTEXT, MENYERR, "SOUNDBLASTER", ofont);
	       writetext(MENXTEXT, MENYERR+ofont->ys+1, " NOT FOUND", ofont);
	    }
	 }
      }
      if(key[KEY_2] || (selected==2 && key[key_fire])){   // Redefine Keys
	 killobject(*pobj);
	 putspritedirect(mspr, MENXTXT, MENYTXT, 1); 	   // Clear mentxt
	 defkeys(ofont);
	 selected = showoptions(ofont, mspr);
         showpage(page);
	 *pobj = defobject(pspr, MENUX, MENUY+7, OBJ_LOW);
      }
      if(key[KEY_3] || (selected==3 && key[key_fire])){ // Quit Options
	 quit = TRUE;
      }
      if(key[key_up] && selected>1){                       // Up
	 for (i = 0; i < MENUDELTA; i++) {
	    moveobjectdelta(*pobj, 0, -MSTEP);
	    waitfortick();
	    updatescreen();
	 }
	 selected--;
      }
      if(key[key_down] && selected<3){                     // Down
	 if(selected == 1){
	    writetext(MENXTEXT, MENYERR, "            ", ofont);
	    writetext(MENXTEXT, MENYERR+ofont->ys+1, "          ", ofont);
	 }
	 for (i = 0; i < MENUDELTA; i++) {
	    moveobjectdelta(*pobj, 0, MSTEP);
	    waitfortick();
	    updatescreen();
	 }
	 selected++;
      }
   }while(!quit);

// Restore mainmenu

   play_sample(snd, 255, 128, 1000, 0);
   putspritedirect(mspr, MENXTXT, MENYTXT, 0);
   moveobject(*pobj, MENUX, MENUY);
   waitfortick();
   updatescreen();

   unloadfile(ofont);

   return 1;  // first menupoint is selected
}

static void story(void)
{
   void *ptr;
   void *sound;
   int d, line;
   SAMPLE *s;
   struct sndstrc ss;

   clearscreen();
   setpage(0);
   ptr = loadfile(datapool, "LONGTIME.PCX");
   showpcx256(ptr,0);

   sound = loadfile(datapool, "LONGTIME.SND");
   create_sndstrc (&ss, sound);
   s = create_SAMPLE (&ss);
   playloop(s);

   glowin(0);

   d = CREDITSPEED;			// Direction and speed of movement.
   line = 0; 				// Starting at line 0.
   waitforkey();
   
   do {
      //blit (full_pages[0], screen, 0, line, 0, 0, screen->w, screen->h);
      showpcx256(ptr,-line);
      showpage(0);
      /* This slows it down but makes sure it proceeds at a steady pace. */
      waitfortick();
      line += d;
   } while (line < 198);

   waitforkey();

   // Stop sound and free memory
   glowout();
   stop_sample (s);
   free_SAMPLE(s);
   unloadfile(sound);
   unloadfile(ptr);
}

 //----------------//
//      Menu	    //
 //----------------//

#ifndef LASTBEER_SDL
void menu(void)
{
   int    quit, selected, i;
   int    pointobj, pointspr;
   struct sprstrc *menuspr;
   SAMPLE *snd;
   int    c;

   showtitle();

   quit = FALSE;
   selected = initmenu(&snd, &pointobj, &pointspr, &menuspr);
   c = 0;
   do{
      clear_keybuf();
#ifdef LASTBEER_AUTOMATION
      TEST_STAGE("menu");
#endif
      while (!keypressed()) {
	 c = cyclepalette(232, 254, c);
	 waitfortick();
	 updatescreen();
      }
      if(key[KEY_1] || (selected==1 && key[key_fire])){   // Play
	 endmenu(snd, pointobj, pointspr, menuspr);
	 clearscreen();
	 setstandardpalette();
	 playthegame();
	 selected = initmenu(&snd, &pointobj, &pointspr, &menuspr);
      }
      if(key[KEY_2] || (selected==2 && key[key_fire])){   // Show Highscore
	 endmenu(snd, pointobj, pointspr, menuspr);
	 highscore(FALSE);
	 selected = initmenu(&snd, &pointobj, &pointspr, &menuspr);
      }
      if(key[KEY_3] || (selected==3 && key[key_fire])){ // Show Title
	 endmenu(snd, pointobj, pointspr, menuspr);
	 showtitle();
	 selected = initmenu(&snd, &pointobj, &pointspr, &menuspr);
      }
      if(key[KEY_4] || (selected==4 && key[key_fire])){  // Story
	 endmenu(snd, pointobj, pointspr, menuspr);
	 story();
	 selected = initmenu(&snd, &pointobj, &pointspr, &menuspr);
      }
      if(key[KEY_5] || (selected==5 && key[key_fire])){  // Options
	 selected = options(snd, &pointobj, pointspr, menuspr);
      }
      if(key[KEY_6] || (selected==6 && key[key_fire])){   // Quit
	 endmenu(snd, pointobj, pointspr, menuspr);
	 closingtime();
	 quit = TRUE;
      }
      if(key[key_up] && selected>1){                       // Up
	 for (i = 0; i < MENUDELTA; i++) {
	    moveobjectdelta(pointobj, 0, -MSTEP);
	    c = cyclepalette(232, 254, c);
	    waitfortick();
	    updatescreen();
	 }
	 selected--;
      }
      if(key[key_down] && selected<6){                     // Down
	 for (i = 0; i < MENUDELTA; i++) {
	    moveobjectdelta(pointobj, 0, MSTEP);
	    c = cyclepalette(232, 254, c);
	    waitfortick();
	    updatescreen();
	 }
	 selected++;
      }
   }while(!quit);
}




#endif /* !LASTBEER_SDL */

#ifdef LASTBEER_SDL
/* SDL menu: compact, legible panels rendered into the same game framebuffer. */
enum { UI_BG = 240, UI_HEADER, UI_PANEL, UI_SELECTED, UI_ACCENT, UI_RULE };
static BITMAP *ui_art;
static PALETTE ui_art_palette;
static struct sprstrc *ui_cursor;
static struct sprstrc *ui_title_font;
static unsigned char ui_panel_blend[2][256];
static unsigned char ui_option_blend[2][256], ui_rainbow[7][3];
static RGB ui_blend_colors[256];
static unsigned char ui_blend_used[256];

/* Reserve unused palette entries for the two panel colors blended with the
   menu artwork. This gives the indexed framebuffer real translucent colors. */
static void ui_prepare_panel_blends(struct sprstrc *title_font, struct sprstrc *font)
{
   unsigned char used[256] = {0}, needed[256] = {0};
   struct sprstrc *sprites[] = {title_font, font, ui_cursor};
   for (int y = 0; y < ui_art->h; ++y)
      for (int x = 0; x < ui_art->w; ++x) used[getpixel(ui_art, x, y)] = 1;
   for (int i = 0; i < 3; ++i)
      for (int j = 0; j < sprites[i]->xs * sprites[i]->ys * sprites[i]->maxn; ++j)
         used[(unsigned char)sprites[i]->data[j]] = 1;
   for (int i = UI_BG; i <= UI_RULE; ++i) used[i] = 1;
   for (int row = 0; row < 6; ++row)
      for (int y = 45 + row * 20; y < 45 + row * 20 + 16; ++y)
         for (int x = 182; x < 312; ++x) needed[getpixel(ui_art, x, y)] = 1;
   memset(ui_blend_used, 0, sizeof(ui_blend_used));
   {
      const RGB rainbow[] = {{63, 16, 12}, {63, 34, 5}, {63, 58, 8}, {20, 58, 12},
                             {8, 58, 58}, {24, 37, 63}, {57, 24, 63}};
      for (int i = 0; i < 7; ++i) for (int shade = 0; shade < 3; ++shade) {
         int slot, intensity = shade == 0 ? 100 : shade == 1 ? 60 : 30;
         for (slot = 1; slot < 256 && used[slot]; ++slot) {}
         if (slot == 256) { ui_rainbow[i][shade] = 15; continue; }
         used[slot] = ui_blend_used[slot] = 1;
         ui_blend_colors[slot] = (RGB){rainbow[i].r * intensity / 100,
                                       rainbow[i].g * intensity / 100,
                                       rainbow[i].b * intensity / 100};
         ui_rainbow[i][shade] = slot;
         compat_video_mark_ui_color(slot);
      }
   }
   for (int selected = 0; selected < 2; ++selected) {
      RGB panel = selected ? (RGB){29, 17, 5} : (RGB){13, 9, 4};
      for (int source = 0; source < 256; ++source) if (needed[source]) {
         int slot;
         for (slot = 1; slot < 256 && used[slot]; ++slot) {}
         if (slot == 256) { ui_panel_blend[selected][source] = selected ? UI_SELECTED : UI_PANEL; continue; }
         used[slot] = ui_blend_used[slot] = 1;
         RGB art = ui_art_palette[source];
         ui_blend_colors[slot] = (RGB){(3 * panel.r + 2 * art.r) / 5,
                                       (3 * panel.g + 2 * art.g) / 5,
                                       (3 * panel.b + 2 * art.b) / 5};
         ui_panel_blend[selected][source] = slot;
         compat_video_mark_ui_color(slot);
      }
   }
   /* Options use a subtler 15% view of the artwork than the main menu. */
   memset(needed, 0, sizeof(needed));
   for (int y = 0; y < ui_art->h; ++y)
      for (int x = 0; x < ui_art->w; ++x) needed[getpixel(ui_art, x, y)] = 1;
   for (int selected = 0; selected < 2; ++selected) {
      RGB panel = selected ? (RGB){29, 17, 5} : (RGB){13, 9, 4};
      for (int source = 0; source < 256; ++source) if (needed[source]) {
         int slot;
         for (slot = 1; slot < 256 && used[slot]; ++slot) {}
         if (slot == 256) { ui_option_blend[selected][source] = selected ? UI_SELECTED : UI_PANEL; continue; }
         used[slot] = ui_blend_used[slot] = 1;
         RGB art = ui_art_palette[source];
         ui_blend_colors[slot] = (RGB){(17 * panel.r + 3 * art.r) / 20,
                                       (17 * panel.g + 3 * art.g) / 20,
                                       (17 * panel.b + 3 * art.b) / 20};
         ui_option_blend[selected][source] = slot;
         compat_video_mark_ui_color(slot);
      }
   }
}

static void ui_rect(int x, int y, int w, int h, int color)
{
   for (int p = 0; p < 2; ++p)
      rectfill(full_pages[p], x, y, x + w - 1, y + h - 1, color);
}

static void ui_text(struct sprstrc *font, int x, int y, const char *text)
{
   int xx = x;
   int font_style = compat_menu_style(0), rainbow = compat_menu_style(1), letter = 0;
   struct sprstrc *source = font_style == 2 && font != ui_title_font ? ui_title_font : font;
   int draw_width = source->xs;
   for (const unsigned char *c = (const unsigned char *)text; *c; ++c) {
      int ch = toupper(*c);
      int glyph = ch - ' ';
      if (glyph < 0 || glyph >= source->maxn) glyph = '?' - ' ';
      if (xx + draw_width > 316) break;
      if (ch == '%') {
         /* The original small menu font's percent glyph is blank. */
         static const unsigned char percent[] = {25, 26, 4, 11, 19};
         for (int p = 0; p < 2; ++p)
            for (int row = 0; row < 5 && row < font->ys; ++row)
               for (int col = 0; col < 5 && col < draw_width; ++col)
                  if (percent[row] & (1 << (4 - col))) putpixel(full_pages[p], xx + col, y + row + 1, rainbow ? ui_rainbow[letter % 7][0] : 15);
         xx += draw_width;
         ++letter;
         continue;
      }
      for (int p = 0; p < 2; ++p) {
         for (int row = 0; row < font->ys; ++row)
            for (int col = 0; col < draw_width; ++col) {
               int sr = row * source->ys / font->ys, sc = col;
               unsigned char pixel = (unsigned char)source->data[glyph * source->xs * source->ys + sr * source->xs + sc];
               if (pixel) {
                  int shade = pixel >= 26 && pixel <= 30 ? 2 : pixel >= 22 && pixel <= 25 ? 1 : 0;
                  int color = rainbow ? ui_rainbow[letter % 7][shade] : pixel;
                  putpixel(full_pages[p], xx + col, y + row, color);
                  if (font_style == 1 && col + 1 < draw_width) putpixel(full_pages[p], xx + col + 1, y + row, color);
               }
            }
      }
      xx += draw_width;
      ++letter;
   }
}

static void ui_palette(void)
{
   memcpy(palette, ui_art_palette, sizeof(PALETTE));
   palette[15] = (RGB){63, 63, 63};
   palette[24] = (RGB){49, 49, 39};
   palette[25] = (RGB){63, 61, 48};
   palette[UI_BG] = (RGB){5, 4, 2};
   palette[UI_HEADER] = (RGB){9, 7, 3};
   palette[UI_PANEL] = (RGB){13, 9, 4};
   palette[UI_SELECTED] = (RGB){29, 17, 5};
   palette[UI_ACCENT] = (RGB){63, 42, 3};
   palette[UI_RULE] = (RGB){36, 25, 4};
   for (int i = 0; i < 256; ++i) if (ui_blend_used[i]) palette[i] = ui_blend_colors[i];
   setpalette(palette);
}

static void ui_base(struct sprstrc *title_font, struct sprstrc *small_font,
                    const char *section)
{
   for (int p = 0; p < 2; ++p) blit(ui_art, full_pages[p], 0, 0, 0, 0, 320, 240);
   ui_rect(6, 3, 190, section ? 29 : 17, UI_HEADER);
   ui_rect(6, section ? 31 : 19, 190, 1, UI_ACCENT);
   ui_text(title_font, 12, 7, "THE LAST EICHHOF");
   if (section) ui_text(small_font, 12, 19, section);
}

static void ui_main_row(struct sprstrc *font, int y, const char *label, int selected, int frame)
{
   for (int p = 0; p < 2; ++p)
      for (int yy = y; yy < y + 16; ++yy)
         for (int x = 182; x < 312; ++x)
            putpixel(full_pages[p], x, yy, ui_panel_blend[selected][getpixel(ui_art, x, yy)]);
   ui_rect(182, y + 16, 130, 1, UI_RULE);
   if (selected) {
      /* The original animated beer pointer retains its artwork and palette. */
      for (int p = 0; p < 2; ++p)
         for (int row = 0; row < ui_cursor->ys; ++row)
            for (int col = 0; col < ui_cursor->xs; ++col) {
               unsigned char pixel = (unsigned char)ui_cursor->data[frame * ui_cursor->xs * ui_cursor->ys + row * ui_cursor->xs + col];
               if (pixel) putpixel(full_pages[p], 163 + col, y + 6 + row, pixel);
            }
   }
   ui_text(font, 189, y + 5, label);
}

static void ui_row(struct sprstrc *font, int y, const char *label,
                   const char *value, int selected)
{
   for (int p = 0; p < 2; ++p)
      for (int yy = y; yy < y + 17; ++yy)
         for (int x = 12; x < 308; ++x)
            putpixel(full_pages[p], x, yy, ui_option_blend[selected][getpixel(ui_art, x, yy)]);
   ui_rect(12, y + 17, 296, 1, UI_BG);
   if (selected) ui_rect(12, y, 3, 17, UI_ACCENT);
   ui_text(font, 22, y + 5, label);
   if (value) ui_text(font, 195, y + 5, value);
}

static void ui_show(void) {
   setpage(0);
#ifdef LASTBEER_AUTOMATION
   TEST_MENU_CAPTURE(1);
#endif
   showpage(0);
#ifdef LASTBEER_AUTOMATION
   TEST_MENU_CAPTURE(0);
#endif
}
static int ui_up(int scan) { return scan == key_up; }
static int ui_down(int scan) { return scan == key_down; }
static int ui_accept(int scan) { return scan == KEY_ENTER || scan == key_fire; }
/* Mouse coordinates are in the same 320x240 space as the menu rows. */
static int ui_read_rows(int *selected, int left, int right, int y, int step,
                        int height, int first, int count, int video_menu)
{
   for (;;) {
      int mx = 0, my = 0;
      int scan = compat_menu_poll(video_menu && (*selected == 6 || *selected == 7), &mx, &my);
      if (scan < 0) {
         int row = (my - y) / step;
         if (mx >= left && mx < right && my >= y && row >= 0 && row < count &&
             (my - y) % step < height) {
            int changed = *selected != first + row;
            *selected = first + row;
            if (scan == -2) {
               compat_menu_sound(MENU_SOUND_SELECT);
               if (video_menu && (*selected == 6 || *selected == 7))
                  return mx < 250 ? key_left : key_right;
               return KEY_ENTER;
            }
            if (changed) { compat_menu_sound(MENU_SOUND_MOVE); return 0; }
         }
      } else if (scan) {
         if (scan == KEY_ESC) compat_menu_sound(MENU_SOUND_BACK);
         else if (ui_accept(scan)) compat_menu_sound(MENU_SOUND_SELECT);
         else if (ui_up(scan) || ui_down(scan) || scan == key_left || scan == key_right)
            compat_menu_sound(MENU_SOUND_MOVE);
         return scan;
      }
      rest(5);
   }
}

static void ui_controls(struct sprstrc *title_font, struct sprstrc *font)
{
   const char *labels[] = {"MOVE UP", "MOVE DOWN", "MOVE LEFT", "MOVE RIGHT", "FIRE / SELECT", "PAUSE"};
   int *bindings[] = {&key_up, &key_down, &key_left, &key_right, &key_fire, &key_pause};
   const int defaults[] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SPACE, KEY_P};
   int selected = 0, capture = 0;
   clear_keybuf();
   for (;;) {
      ui_base(title_font, font, "CONTROLS / KEYBOARD");
      for (int i = 0; i < 6; ++i) {
         const char *value = capture && selected == i ? "PRESS A KEY" : compat_key_name(*bindings[i]);
         ui_row(font, 42 + i * 18, labels[i], value, selected == i);
      }
      ui_row(font, 150, "RESET DEFAULTS", NULL, selected == 6);
      ui_row(font, 168, "BACK", NULL, selected == 7);
      ui_text(font, 15, 196, capture ? "ESC / PAD EAST CANCELS" : "SELECT TO CHANGE A KEY");
      ui_text(font, 15, 207, capture ? "ENTER AND F11 ARE RESERVED" : "DUPLICATE KEYS ARE SWAPPED");
      ui_text(font, 15, 218, "PAD: D PAD / STICK  SOUTH FIRE");
      ui_show();
      if (capture) {
#ifdef LASTBEER_AUTOMATION
         TEST_STAGE("bind");
#endif
         int scan = compat_read_binding_key();
         if (scan == KEY_ENTER || scan == KEY_F11) continue;
         if (scan != KEY_ESC && scan > 0 && scan < 128) {
            for (int i = 0; i < 6; ++i)
               if (i != selected && *bindings[i] == scan) *bindings[i] = *bindings[selected];
            *bindings[selected] = scan;
            saveconfig();
         }
         capture = 0;
         continue;
      }
#ifdef LASTBEER_AUTOMATION
      TEST_STAGE("controls");
#endif
      int scan = ui_read_rows(&selected, 12, 308, 42, 18, 17, 0, 8, 0);
      if (scan == KEY_ESC) break;
      if (ui_up(scan)) { if (selected > 0) --selected; }
      else if (ui_down(scan)) { if (selected < 7) ++selected; }
      else if (ui_accept(scan)) {
         if (selected < 6) capture = 1;
         else if (selected == 6) {
            for (int i = 0; i < 6; ++i) *bindings[i] = defaults[i];
            saveconfig();
         } else break;
      }
   }
   clear_keybuf();
}

static void ui_controller_select(struct sprstrc *title_font, struct sprstrc *font)
{
   int selected = 0, top = 0;
   clear_keybuf();
   for (;;) {
      int count = compat_controller_count();
      int active = compat_controller_selected();
      int last = count + 1; /* AUTO, connected controllers, BACK */
      if (selected > last) selected = last;
      if (selected < top) top = selected;
      if (selected >= top + 7) top = selected - 6;
      ui_base(title_font, font, "OPTIONS / SELECT CONTROLLER");
      for (int row = 0; row < 7 && top + row <= last; ++row) {
         int item = top + row;
         char controller_label[26];
         const char *label = item == 0 ? "AUTO / ALL" : item == last ? "BACK" : compat_controller_name(item - 1);
         if (item > 0 && item < last) { snprintf(controller_label, sizeof(controller_label), "%.25s", label); label = controller_label; }
         const char *value = (item == 0 && active == -1) || (item > 0 && item < last && active == item - 1) ? "ACTIVE" : NULL;
         ui_row(font, 41 + row * 19, label, value, selected == item);
      }
      if (count == 0) ui_text(font, 15, 184, "NO CONTROLLER CONNECTED");
      else if (active == -2) ui_text(font, 15, 184, "SAVED CONTROLLER NOT CONNECTED");
      ui_text(font, 15, 207, "AUTO ACCEPTS ALL CONNECTED PADS");
      ui_text(font, 15, 219, "KEYBOARD ALWAYS WORKS");
      ui_show();
#ifdef LASTBEER_AUTOMATION
      TEST_STAGE("controller-select");
#endif
      int scan = ui_read_rows(&selected, 12, 308, 41, 19, 17, top,
                              last - top + 1 < 7 ? last - top + 1 : 7, 0);
      if (scan == KEY_ESC) break;
      if (ui_up(scan)) { if (selected > 0) --selected; }
      else if (ui_down(scan)) { if (selected < last) ++selected; }
      else if (ui_accept(scan)) {
         if (selected == last) break;
         compat_controller_select(selected - 1);
         clear_keybuf();
      }
   }
   clear_keybuf();
}

static void ui_pad_controls(struct sprstrc *title_font, struct sprstrc *font)
{
   const char *labels[] = {"MOVE UP", "MOVE DOWN", "MOVE LEFT", "MOVE RIGHT", "FIRE / SELECT", "BACK", "PAUSE"};
   int selected = 0, capture = 0;
   clear_keybuf();
   for (;;) {
      ui_base(title_font, font, "OPTIONS / CONTROLLER BINDINGS");
      for (int i = 0; i < 7; ++i) {
         const char *value = capture && selected == i ? "PRESS PAD" : compat_pad_binding_name(i);
         ui_row(font, 39 + i * 17, labels[i], value, selected == i);
      }
      ui_row(font, 158, "RESET DEFAULTS", NULL, selected == 7);
      ui_row(font, 175, "BACK", NULL, selected == 8);
      ui_text(font, 15, 207, capture ? "PRESS A BUTTON OR MOVE A STICK" : "SELECT AN ACTION TO REBIND");
      ui_text(font, 15, 219, capture ? "HOLD EAST / ESC CANCELS" : "DUPLICATE CONTROLS ARE SWAPPED");
      ui_show();
      if (capture) {
         int trigger, result;
         compat_pad_begin_capture();
#ifdef LASTBEER_AUTOMATION
         TEST_STAGE("pad-bind");
#endif
         do { result = compat_pad_poll_capture(&trigger); } while (!result);
         if (result > 0) compat_pad_bind(selected, trigger);
         compat_pad_end_capture();
         capture = 0;
         continue;
      }
#ifdef LASTBEER_AUTOMATION
      TEST_STAGE("pad-controls");
#endif
      int scan = ui_read_rows(&selected, 12, 308, 39, 17, 17, 0, 9, 0);
      if (scan == KEY_ESC) break;
      if (ui_up(scan)) { if (selected > 0) --selected; }
      else if (ui_down(scan)) { if (selected < 8) ++selected; }
      else if (ui_accept(scan)) {
         if (selected < 7) capture = 1;
         else if (selected == 7) compat_pad_reset();
         else break;
      }
   }
   clear_keybuf();
}

static void ui_video_settings(struct sprstrc *title_font, struct sprstrc *font)
{
   const char *filters[] = {"PIXEL", "SMOOTH", "SCALE2X"};
   const char *scaling[] = {"FIT", "INTEGER", "STRETCH"};
   const char *scanlines[] = {"OFF", "LIGHT", "MEDIUM", "STRONG"};
   int selected = 0, unavailable = 0;
   clear_keybuf();
   compat_video_repeat_reset();
   for (;;) {
      char brightness[8], contrast[8];
      int displayed_fullscreen = compat_video_setting(0);
      snprintf(brightness, sizeof(brightness), "%d%%", compat_video_setting(6));
      snprintf(contrast, sizeof(contrast), "%d%%", compat_video_setting(7));
      ui_base(title_font, font, "OPTIONS / VIDEO");
      ui_row(font, 37, "DISPLAY", displayed_fullscreen ? "FULLSCREEN" : "WINDOW", selected == 0);
      ui_row(font, 55, "WINDOW SIZE", compat_video_window_size_name(), selected == 1);
      ui_row(font, 73, "IMAGE FILTER", filters[compat_video_setting(2)], selected == 2);
      ui_row(font, 91, "SCALING", scaling[compat_video_setting(3)], selected == 3);
      ui_row(font, 109, "SCANLINES", scanlines[compat_video_setting(4)], selected == 4);
      ui_row(font, 127, "VERTICAL SYNC", compat_video_setting(5) ? "ON" : "OFF", selected == 5);
      ui_row(font, 145, "BRIGHTNESS", brightness, selected == 6);
      ui_row(font, 163, "CONTRAST", contrast, selected == 7);
      ui_row(font, 181, "BACK", NULL, selected == 8);
      ui_text(font, 15, 207, unavailable ? "SETTING NOT AVAILABLE" : "LEFT / RIGHT TO CHANGE");
      ui_text(font, 15, 218, "F11 OR ALT ENTER: FULLSCREEN");
      ui_show();
      if (displayed_fullscreen != compat_video_setting(0)) continue;
#ifdef LASTBEER_AUTOMATION
      TEST_STAGE("video-settings");
#endif
      int scan = ui_read_rows(&selected, 12, 308, 37, 18, 17, 0, 9, 1);
      if (scan == KEY_ESC) break;
      if (ui_up(scan)) { if (selected > 0) --selected; unavailable = 0; }
      else if (ui_down(scan)) { if (selected < 8) ++selected; unavailable = 0; }
      else if (scan == key_left || scan == key_right || ui_accept(scan)) {
         if (selected == 8) break;
         int old = compat_video_setting(selected);
         if (selected == 6 || selected == 7) {
            if (scan == key_left && old > 0) unavailable = !compat_video_set(selected, old - 1);
            else if (scan == key_right && old < 150) unavailable = !compat_video_set(selected, old + 1);
         } else {
            int count = selected == 1 ? compat_video_window_size_count() : selected == 4 ? 4 : (selected == 2 || selected == 3) ? 3 : 2;
            int value = (old + (scan == key_left ? count - 1 : 1)) % count;
            unavailable = !compat_video_set(selected, value);
         }
      }
   }
   clear_keybuf();
   compat_video_repeat_reset();
}

static void ui_menu_style(struct sprstrc *title_font, struct sprstrc *font)
{
   const char *fonts[] = {"CLASSIC", "BOLD", "TITLE BLOCK"};
   const char *colors[] = {"WHITE", "RAINBOW"};
   int selected = 0;
   clear_keybuf();
   for (;;) {
      ui_base(title_font, font, "OPTIONS / MENU STYLE");
      ui_row(font, 50, "FONT", fonts[compat_menu_style(0)], selected == 0);
      ui_row(font, 70, "TEXT COLOR", colors[compat_menu_style(1)], selected == 1);
      ui_row(font, 90, "BACK", NULL, selected == 2);
      ui_text(font, 15, 140, "TITLE USES THE GAME LOGO FONT");
      ui_text(font, 15, 156, "LEFT / RIGHT TO CHANGE");
      ui_show();
#ifdef LASTBEER_AUTOMATION
      TEST_STAGE("menu-style");
#endif
      int scan = ui_read_rows(&selected, 12, 308, 50, 20, 17, 0, 3, 0);
      if (scan == KEY_ESC) break;
      if (ui_up(scan)) { if (selected > 0) --selected; }
      else if (ui_down(scan)) { if (selected < 2) ++selected; }
      else if (scan == key_left || scan == key_right || ui_accept(scan)) {
         if (selected == 2) break;
         int count = selected == 0 ? 3 : 2;
         int value = (compat_menu_style(selected) + (scan == key_left ? count - 1 : 1)) % count;
         compat_menu_set_style(selected, value);
      }
   }
   clear_keybuf();
}

static int ui_play_difficulty(struct sprstrc *title_font, struct sprstrc *font)
{
   const char *labels[] = {"EASY", "CLASSIC", "HARD", "BACK"};
   const char *description[][2] = {
      {"FASTER MOVEMENT", "MORE DAMAGE / SHOP MONEY"},
      {"ORIGINAL MOVEMENT", "ORIGINAL WEAPON DAMAGE"},
      {"SLOWER / LESS DAMAGE", "EXTRA ENEMIES CROSS THE TOP"},
      {"RETURN TO MAIN MENU", ""}
   };
   int selected = compat_difficulty();
   clear_keybuf();
   for (;;) {
      ui_base(title_font, font, "SELECT DIFFICULTY");
      for (int i = 0; i < 4; ++i)
         ui_row(font, 45 + i * 21, labels[i], NULL, selected == i);
      ui_text(font, 15, 150, description[selected][0]);
      ui_text(font, 15, 164, description[selected][1]);
      ui_text(font, 15, 207, "MOVE KEYS / PAD  NAVIGATE");
      ui_text(font, 15, 219, "ENTER / FIRE  SELECT   ESC  BACK");
      ui_show();
#ifdef LASTBEER_AUTOMATION
      TEST_STAGE("play-difficulty");
#endif
      int scan = ui_read_rows(&selected, 12, 308, 45, 21, 17, 0, 4, 0);
      if (scan == KEY_ESC) break;
      if (ui_up(scan)) { if (selected > 0) --selected; }
      else if (ui_down(scan)) { if (selected < 3) ++selected; }
      else if (ui_accept(scan)) {
         if (selected == 3) break;
         compat_set_difficulty(selected);
         clear_keybuf();
         return 1;
      }
   }
   clear_keybuf();
   return 0;
}

static void ui_input_settings(struct sprstrc *title_font, struct sprstrc *font)
{
   const char *modes[] = {"KEYBOARD ONLY", "KEYS + MOUSE", "CONTROLLER"};
   int selected = 0;
   clear_keybuf();
   for (;;) {
      int mode = compat_input_mode();
      ui_base(title_font, font, "OPTIONS / CONTROLS");
      ui_row(font, 45, "INPUT MODE", modes[mode], selected == 0);
      ui_row(font, 65, "BINDINGS", mode == 2 ? "CONTROLLER" : "KEYBOARD", selected == 1);
      ui_row(font, 85, "SELECT CONTROLLER", NULL, selected == 2);
      ui_row(font, 105, "CONTROLLER BINDINGS", NULL, selected == 3);
      ui_row(font, 125, "BACK", NULL, selected == 4);
      ui_text(font, 15, 185, "MODE APPLIES DURING PLAY");
      ui_text(font, 15, 207, "KEYBOARD / PAD NAVIGATE MENUS");
      ui_text(font, 15, 219, "LEFT / RIGHT TO CHANGE MODE");
      ui_show();
#ifdef LASTBEER_AUTOMATION
      TEST_STAGE("input-settings");
#endif
      int scan = ui_read_rows(&selected, 12, 308, 45, 20, 17, 0, 5, 0);
      if (scan == KEY_ESC) break;
      if (ui_up(scan)) { if (selected > 0) --selected; }
      else if (ui_down(scan)) { if (selected < 4) ++selected; }
      else if (selected == 0 && (scan == key_left || scan == key_right || ui_accept(scan)))
         compat_set_input_mode((mode + (scan == key_left ? 2 : 1)) % 3);
      else if (ui_accept(scan)) {
         if (selected == 1) { if (mode == 2) ui_pad_controls(title_font, font); else ui_controls(title_font, font); }
         else if (selected == 2) ui_controller_select(title_font, font);
         else if (selected == 3) ui_pad_controls(title_font, font);
         else break;
         ui_palette();
      }
   }
   clear_keybuf();
}

static void ui_options(struct sprstrc *title_font, struct sprstrc *font, SAMPLE *music)
{
   const char *difficulties[] = {"EASY", "CLASSIC", "HARD"};
   int selected = 0;
   clear_keybuf();
   for (;;) {
      ui_base(title_font, font, "OPTIONS");
      ui_row(font, 45, "SOUND", speaker(-1) ? "ON" : (sound_unavailable() ? "RETRY" : "OFF"), selected == 0);
      ui_row(font, 65, "CONTROLS", NULL, selected == 1);
      ui_row(font, 85, "VIDEO SETTINGS", NULL, selected == 2);
      ui_row(font, 105, "DIFFICULTY", difficulties[compat_difficulty()], selected == 3);
      ui_row(font, 125, "MENU STYLE", NULL, selected == 4);
      ui_row(font, 145, "BACK", NULL, selected == 5);
      ui_text(font, 15, 207, "MOVE KEYS / PAD  NAVIGATE");
      ui_text(font, 15, 219, "ENTER / FIRE  SELECT   ESC  BACK");
      ui_show();
#ifdef LASTBEER_AUTOMATION
      TEST_STAGE("options");
#endif
      int scan = ui_read_rows(&selected, 12, 308, 45, 20, 17, 0, 6, 0);
      if (scan == KEY_ESC) break;
      if (ui_up(scan)) { if (selected > 0) --selected; }
      else if (ui_down(scan)) { if (selected < 5) ++selected; }
      else if (selected == 3 && (scan == key_left || scan == key_right)) {
         compat_set_difficulty((compat_difficulty() + (scan == key_left ? 2 : 1)) % 3);
      }
      else if (ui_accept(scan)) {
         if (selected == 0) {
            int was_on = speaker(-1);
            if (!was_on && speaker(1)) {
               if (music) playsample(music);
               compat_menu_sound(MENU_SOUND_SELECT);
            }
            else if (was_on) speaker(0);
            saveconfig();
         }
         else if (selected == 1) ui_input_settings(title_font, font);
         else if (selected == 2) ui_video_settings(title_font, font);
         else if (selected == 3) compat_set_difficulty((compat_difficulty() + 1) % 3);
         else if (selected == 4) ui_menu_style(title_font, font);
         else break;
         ui_palette();
      }
   }
   clear_keybuf();
}

static void ui_stop_music(SAMPLE **sound, void **data)
{
   if (*sound) { stop_sample(*sound); free_SAMPLE(*sound); *sound = NULL; }
   if (*data) { unloadfile(*data); *data = NULL; }
}
static void ui_start_music(SAMPLE **sound, void **data)
{
   struct sndstrc sample;
   *data = loadfile(datapool, "MENU.SND");
   create_sndstrc(&sample, *data);
   *sound = create_SAMPLE(&sample);
   playsample(*sound);
}

static void ui_draw_main(struct sprstrc *title_font, struct sprstrc *font,
                         const char **items, int selected, int frame)
{
#ifdef LASTBEER_AUTOMATION
   compat_test_menu_frame(frame);
#endif
   ui_base(title_font, font, NULL);
   for (int i = 0; i < 6; ++i) ui_main_row(font, 45 + i * 20, items[i], i == selected, frame);
   ui_rect(179, 174, 134, 55, UI_HEADER);
   ui_text(font, 183, 179, "MOVE: KEYS / PAD");
   ui_text(font, 183, 191, "SELECT: FIRE / A");
   ui_text(font, 183, 203, "F11: FULLSCREEN");
   {
      const char *difficulty[] = {"MODE: EASY", "MODE: CLASSIC", "MODE: HARD"};
      ui_text(font, 183, 215, difficulty[compat_difficulty()]);
   }
   ui_show();
}

void menu(void)
{
   const char *items[] = {"PLAY", "HIGH SCORES", "TITLE", "STORY", "OPTIONS", "QUIT"};
   char *title_data = loadfile(datapool, "FONT1.SPR");
   char *font_data = loadfile(datapool, "OFONT.SPR");
   char *cursor_data = loadfile(datapool, "MENCUR.SPR");
   struct sprstrc *title_font, *font;
   SAMPLE *music = NULL;
   void *music_data = NULL;
   int selected = 0, done = 0;
   create_sprstrc(&title_font, title_data);
   ui_title_font = title_font;
   create_sprstrc(&font, font_data);
   create_sprstrc(&ui_cursor, cursor_data);
   showtitle();
   compat_mouse_cursor_visible(1);
   {
      void *art_data = loadfile(datapool, "MENU.PCX");
      ui_art = create_bitmap(320, 240);
      clear_bitmap(full_pages[0]);
      setpage(0);
      showpcx256(art_data, 10);
      memcpy(ui_art_palette, palette, sizeof(PALETTE));
      blit(full_pages[0], ui_art, 0, 0, 0, 0, 320, 240);
      /* MENU.PCX includes the left half of the old menu's selection frame.
         The new menu draws its own rows, so remove that obsolete bracket. */
      rectfill(ui_art, 134, 45, 160, 119, 0);
#ifdef LASTBEER_AUTOMATION
      compat_test_menu_background(ui_art);
#endif
      unloadfile(art_data);
   }
   ui_prepare_panel_blends(title_font, font);
   compat_video_clarity_controls(1);
   ui_palette();
   ui_start_music(&music, &music_data);
   clear_keybuf();
   while (!done) {
      int frame = (int)((compat_ticks() / 120) % ui_cursor->maxn);
      ui_draw_main(title_font, font, items, selected, frame);
#ifdef LASTBEER_AUTOMATION
      TEST_STAGE("menu");
#endif
      int scan = 0;
      while (!scan) {
         int mx = 0, my = 0;
         int event = compat_menu_poll(0, &mx, &my);
         if (event < 0 && mx >= 182 && mx < 312 && my >= 45 && my < 165) {
            int row = (my - 45) / 20;
            if ((my - 45) % 20 < 16) {
               if (selected != row) {
                  selected = row;
                  if (event == -1) compat_menu_sound(MENU_SOUND_MOVE);
                  ui_draw_main(title_font, font, items, selected, frame);
               }
               if (event == -2) scan = KEY_ENTER;
            }
         } else if (event > 0) scan = event;
         int next = (int)((compat_ticks() / 120) % ui_cursor->maxn);
         if (next != frame) { frame = next; ui_draw_main(title_font, font, items, selected, frame); }
         if (!scan) rest(8);
      }
      int action = -1;
      if (ui_up(scan)) { if (selected > 0) { --selected; compat_menu_sound(MENU_SOUND_MOVE); } }
      else if (ui_down(scan)) { if (selected < 5) { ++selected; compat_menu_sound(MENU_SOUND_MOVE); } }
      else if (ui_accept(scan)) action = selected;
      else if (scan >= KEY_1 && scan <= KEY_6) action = scan - KEY_1;
      if (action < 0) continue;
      compat_menu_sound(MENU_SOUND_SELECT);
      if (action == 4) { ui_options(title_font, font, music); ui_palette(); continue; }
      if (action == 0 && !ui_play_difficulty(title_font, font)) continue;
      ui_stop_music(&music, &music_data);
      compat_video_clarity_controls(0);
      compat_mouse_cursor_visible(0);
      if (action == 0) {
         clearscreen(); setstandardpalette(); playthegame();
      } else if (action == 1) highscore(FALSE);
      else if (action == 2) showtitle();
      else if (action == 3) story();
      else if (action == 5) { closingtime(); done = 1; }
      if (!done) { compat_mouse_cursor_visible(1); compat_video_clarity_controls(1); ui_palette(); ui_start_music(&music, &music_data); clear_keybuf(); }
   }
   ui_stop_music(&music, &music_data);
   destroy_bitmap(ui_art); ui_art = NULL;
   free(ui_cursor); ui_cursor = NULL; unloadfile(cursor_data);
   free(title_font); free(font);
   ui_title_font = NULL;
   unloadfile(title_data); unloadfile(font_data);
}
#endif /* LASTBEER_SDL */
