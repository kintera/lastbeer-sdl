/* Allegro port Copyright (C) 2014 Gavin Smith. */
 /*-----------------------------------------------------*/
/*                                                       */
/*            T H E   L A S T   E I C H H O F            */
/*                                                       */
/*           [c] copyrigth 1993 by ALPHA-HELIX           */
/*          This module written by Dany Schoch		 */
/*                                                       */
 /*-----------------------------------------------------*/

#define MAIN_MODULE

#include <allegro.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>

#include "xmode.h"
#include "fileman.h"
#include "sound.h"
#include "baller.h"


#define MEMORYREQUIRED		500000L	// Memory used to run.
#define CMDLEN			40	// Max length of command line.

static char   cmd[CMDLEN];		// Command line given at startup.
extern unsigned _stklen;                // Programs stacklength.


// Code.

// Error handling routines.

void error(char *text, int code, ...)
{
   void powerdown(void);	// Prototype.
   va_list   ap;		// Variable for variable argument list.

   va_start(ap, code);
   powerdown();			// Leave graphics, close files, ...
   switch (code) {
      case ENOMEM : printf("%s : NO MORE MEMORY !\n\n", text);
		    break;
      case ENOENT: printf("%s : File '%s' not found !\n\n",
		    text, va_arg(ap, char *));
		    break;
      case E2BIG  : printf("%s : Out of index table space.\n\n", text);
		    break;
      case EIO   : printf("%s : Could not read game data (file may be damaged).\n\n", text);
                    break;
      case EINVAL: printf("%s : Wrong level.\n\n", text);
		    break;
      case -1     :
      case EFAULT : printf("James Bond quitting style !!\n\n");
		    break;
      default     : perror(text);
   }
   va_end(ap);
   exit(1);
}


/*------------------------------------------------------
Function: powerup & powerdown

powerup   : Initializes the system.
powerdown : Undoes the effect of powerup.
	    Please note that powerdown should not be
	    called within powerup or the sytem will
	    hang. So errors encountered in powerup
	    can't be handled by the function 'error',
	    because this would call powerdown indirectly.
------------------------------------------------------*/
static void create_pages(void);

void powerup(void)
{
   allegro_init();

   install_keyboard();

#ifndef LASTBEER_SDL
   if (install_sound(DIGI_AUTODETECT, MIDI_NONE, 0)) {
     printf ("%s\n", allegro_error);
     exit(2);
   }
#endif

// Clear NUM_LOCK, SCROLL_LOCK and CAPS_LOCK.
   set_leds (0);

   install_timer();
   setspeed(GAMESPEED);

   initxmode();				// Enter graphic mode.
   windowy1 = BARY;			// Game window y-Size.
   create_pages ();
   page = 0;

}

static void create_pages (void)
{
  full_pages[0] = create_system_bitmap (320, 240);
  full_pages[1] = create_system_bitmap (320, 240);
  
  pages[0] = create_sub_bitmap(full_pages[0], 0, 0, windowx1, windowy1);
  pages[1] = create_sub_bitmap(full_pages[1], 0, 0, windowx1, windowy1);
}

void powerdown(void)
{
   shutxmode();

   shutsound();
   shutfilemanager();
#ifdef LASTBEER_SDL
   destroy_bitmap(pages[0]);
   destroy_bitmap(pages[1]);
   destroy_bitmap(full_pages[0]);
   destroy_bitmap(full_pages[1]);
   compat_shutdown();
#endif
}



/*------------------------------------------------------
Function: cmdline

Description: Do first steps on the command line.
------------------------------------------------------*/
void cmdline(int argc, char *argv[])
{
   int   i;
   char  *e1;

// Concatenate all command strings together in 'cmd'.
   cmd[0] = '\0';			// Clear string.
   for (i = 1; i < argc; i++) {
      if (strlen(cmd)+strlen(argv[i]) < CMDLEN)
	 strcat(cmd, argv[i]);
   }
   strupr(cmd);

// Help?
   if (strstr(cmd, "/?") || strstr(cmd, "-?")) {	// Help?
#ifdef LASTBEER_SDL
      printf("Syntax: beer-sdl [--no-sound]\n");
      printf("  --no-sound   Start without initializing audio.\n");
#else
      printf("Syntax:   BALLER [options]\n");
      printf("  /vga    Override VGA detection.\n");
      printf("  /ns     Play without sound.\n");
      printf("\n");
      printf("To force SoundBlaster on, use the BLASTER environment variable.\n");
      printf("  e.g. set BLASTER = A220 I7 D1\n");
      printf("\n\n");
#endif
      exit(0);                          // Exit nicely.
   }

// Get cheat level.

   if ((e1 = strstr(cmd, "007.")) == NULL) {
      cheatlevel = 0;			// No cheating this time.
   } else {
      cheatlevel = e1[4] - '0';
      printf("Cheat level:\n");
      if(cheatlevel & CHEATLIFES) printf("    - Unlimited lives\n");
      if(cheatlevel & CHEATMONEY) printf("    - Unlimited money\n");
      if(cheatlevel & CHEATCRASH) printf("    - Eichhof can't be destroyed\n");
      printf("\n");
   }

}

int main(int argc, char *argv[])
{
#if 0
   printf("\n\n\nTHE LAST EICHHOF [c] copyright 1993 by ALPHA-HELIX.\n");
   printf("Release 1.1\n\n");
   printf("This game is FREEWARE. Please copy like crazy.\n");

   /* Don't know if this is still valid. */
   printf("If you like it, just send a postcard to ALPHA-HELIX\n");
   printf("                                        Rehhalde 18\n");
   printf("                                        6332 Hagendorn\n");
   printf("                                        Switzerland\n");
   printf("\nInternet contact address: tritone@ezinfo.vmsmail.ethz.ch\n\n\n");
#endif

#ifdef LASTBEER_SDL
   compat_set_executable(argv[0]);
   if (argc == 2 && strcmp(argv[1], "--lastbeer-audio-probe") == 0)
      return compat_audio_probe_process();
#endif
// Process command line.
   cmdline(argc, argv);

// Do initialization.
   powerup();

#ifdef LASTBEER_AUTOMATION
   if (getenv("LASTBEER_TEST_SCORE_ENTRY")) {
      const char *test_path = compat_config_path();
      if (test_path) remove(test_path);
   }
   if (getenv("LASTBEER_TEST_PROJECTILES")) {
      extern int test_projectile_cleanup(void);
      int ok = test_projectile_cleanup();
      powerdown();
      if (!ok) return 1;
      puts("SMOKE: projectile cleanup");
      return 0;
   }
   if (getenv("LASTBEER_TEST_VIDEO_REPEAT")) {
      if (!compat_test_video_repeat()) return 1;
      powerdown();
      puts("SMOKE: held keyboard and controller video adjustment");
      return 0;
   }
   if (getenv("LASTBEER_TEST_SCORE_STORAGE")) {
      if (!compat_test_score_storage()) return 1;
      powerdown();
      puts("SMOKE: difficulty score tables and classic migration");
      return 0;
   }
   if (getenv("LASTBEER_TEST_CONTROLS")) {
      const char *test_path = compat_config_path();
      if (test_path) remove(test_path);
   }
   if (getenv("LASTBEER_TEST_CONFIG")) {
      const char *test_path = compat_config_path();
      FILE *test_file;
      if (!test_path) return 1;
      loadconfig();
      key_up = 17; /* W, distinct from the default bindings. */
      saveconfig();
      key_up = KEY_LEFT;
      loadconfig();
      if (key_up != 17) return 2;
      test_file = fopen(test_path, "wb");
      if (!test_file) return 3;
      fputc(1, test_file);
      fclose(test_file);
      key_up = KEY_RIGHT;
      loadconfig();
      if (key_up != KEY_UP) return 4;
      powerdown();
      puts("SMOKE: settings roundtrip");
      return 0;
   }
#endif
// Load options of last time.
   loadconfig();
// Check command line for 'nosound' option.
   if (strstr(cmd, "/NS") || strstr(cmd, "-NS") || strstr(cmd, "--NO-SOUND")) {
#ifdef LASTBEER_SDL
      sound_suspend();
#else
      speaker(0);
#endif
   }
#ifdef LASTBEER_SDL
   /* Show a window before testing the audio backend. If sound fails or hangs,
      continue silently instead of trapping startup before a window appears. */
   compat_show_startup();
   if (speaker(-1) && install_sound(DIGI_AUTODETECT, MIDI_NONE, 0)) sound_startup_failed();
#ifdef LASTBEER_AUTOMATION
   if (getenv("LASTBEER_EXPECT_AUDIO_FALLBACK") || getenv("LASTBEER_EXPECT_NO_AUDIO")) {
      short saved[7];
      const char *test_path = compat_config_path();
      FILE *test_file;
      saveconfig();
      test_file = test_path ? fopen(test_path, "rb") : NULL;
      if (!test_file || fread(saved, sizeof(short), 7, test_file) != 7 || saved[6] != 1) {
         fprintf(stderr, "SMOKE: temporary audio failure changed saved sound preference\n");
         if (test_file) fclose(test_file);
         return 1;
      }
      fclose(test_file);
   }
#endif
#endif

// Open date bases.
   initfilemanager(40, 512, 8192, error);
   datapool = opendatabase("BEER.DAT");

   intro();				// Show Blick intro.


   menu();

   closedatabase(datapool);

// Going down and return to OS.
   powerdown();
#ifdef LASTBEER_AUTOMATION
   TEST_STAGE("completed");
#endif
   return 0;
}
