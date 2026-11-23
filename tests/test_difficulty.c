#include "difficulty.h"
#include "fileman.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define assert(condition) do { if (!(condition)) abort(); } while (0)

static void archive_error(char *text, int code, ...) { (void)text; (void)code; }
static unsigned word(const unsigned char *p) { return p[0] | (unsigned)p[1] << 8; }
static int signed_word(const unsigned char *p) { return (int16_t)word(p); }
int main(int argc, char **argv)
{
   assert(difficulty_speed(DIFFICULTY_EASY, 4) == 5);
   assert(difficulty_speed(DIFFICULTY_CLASSIC, 4) == 4);
   assert(difficulty_speed(DIFFICULTY_HARD, 4) == 3);
   assert(difficulty_speed(DIFFICULTY_HARD, 1) == 1);
   assert(difficulty_shot_power(DIFFICULTY_EASY, 1) == 5);
   assert(difficulty_shot_power(DIFFICULTY_CLASSIC, 1) == 1);
   assert(difficulty_shot_power(DIFFICULTY_HARD, 1) == 3);
   assert(difficulty_enemy_shield(DIFFICULTY_EASY, 1) == 4);
   assert(difficulty_enemy_shield(DIFFICULTY_CLASSIC, 1) == 1);
   assert(difficulty_enemy_shield(DIFFICULTY_HARD, 1) == 4);
   assert(difficulty_enemy_shield(DIFFICULTY_HARD, 9999) == 39996);
   assert(argc == 2 && initfilemanager(4, 0, 8192, archive_error) == 1);
   void *db = opendatabase(argv[1]); assert(db != NULL);
   for (int level = 0; level < 5; ++level) {
      char name[32]; size_t size = 0;
      snprintf(name, sizeof(name), "LEVEL%d.SLI", level);
      unsigned char *sprites = loadfile(db, name); assert(sprites != NULL);
      int nsprites = word(sprites); unloadfile(sprites);
      snprintf(name, sizeof(name), "LEVEL%d.EXP", level);
      unsigned char *explosions = loadfile(db, name); assert(explosions != NULL);
      int nexplosions = word(explosions); unloadfile(explosions);
      snprintf(name, sizeof(name), "LEVEL%d.FOE", level);
      assert(database_file_size(db, name, &size));
      unsigned char *foes = loadfile(db, name); assert(foes != NULL && size >= 2);
      unsigned count = word(foes), eligible = 0;
      assert(2 + count * 4 <= size);
      for (unsigned i = 0; i < count; ++i) {
         const unsigned char *index = foes + 2 + i * 4;
         size_t offset = 2 + word(index) + 16 * word(index + 2);
         assert(offset + 12 <= size);
         const unsigned char *f = foes + offset;
         eligible += difficulty_regular_enemy(signed_word(f), signed_word(f+2), signed_word(f+4),
                                             signed_word(f+8), nsprites, signed_word(f+6), nexplosions);
      }
      assert(eligible > 0);
      unloadfile(foes);
   }
   closedatabase(db); shutfilemanager();
   return 0;
}
