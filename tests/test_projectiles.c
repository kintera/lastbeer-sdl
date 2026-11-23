#include <stdio.h>

#include "../baller.h"
#include "../xmode.h"

void a_shot(void);
void foehit(void);

static int check(int condition, const char *message)
{
  if (!condition) fprintf(stderr, "Projectile test: %s\n", message);
  return condition;
}

static void reset_projectiles(void)
{
  int i;
  killallobjects();
  clear_bitmap(pages[0]);
  clear_bitmap(pages[1]);
  for (i = 0; i < MAXFOES; ++i) _foe[i].object = -1;
  for (i = 0; i < MAXSHOTS; ++i) _shot[i].object = -1;
  for (i = 0; i < MAXEXPLS; ++i) _expl[i].object = -1;
  page = 0;
  subtick = 1;
}

int test_projectile_cleanup(void)
{
  char pixels[9] = { 7, 7, 7, 7, 7, 7, 7, 7, 7 };
  struct sprstrc sprite_data = { 3, 3, 1, pixels };
  unsigned short end_path[] = { SHOTEND };
  int sprite = defsprite(&sprite_data, 0);
  int foe, first_shot, second_shot, i;

  if (!check(sprite >= 0, "could not create test sprite")) return 0;

  /* A full explosion pool must not leave the defeated enemy on screen. */
  reset_projectiles();
  for (i = 0; i < MAXEXPLS; ++i) _expl[i].object = 0;
  foe = defobject(sprite, 40, 40, OBJ_LOW);
  _foe[0].object = foe;
  _foe[0].flags = 0;
  _foe[0].shield = 1;
  first_shot = defobject(sprite, 40, 40, OBJ_LOW);
  second_shot = defobject(sprite, 40, 40, OBJ_LOW);
  _shot[0].object = first_shot;
  _shot[0].power = 5;
  _shot[1].object = second_shot;
  _shot[1].power = 5;
  updatescreen();
  updatescreen();
  foehit();
  if (!check(_foe[0].object == -1, "enemy was not defeated") ||
      !check(_obj[foe].destroy == 2, "enemy was orphaned when explosion slots were full") ||
      !check(_shot[1].power == 5, "a second shot hit an already defeated enemy")) return 0;
  abandonobject(first_shot);
  abandonobject(second_shot);
  updatescreen();
  updatescreen();
  if (!check(!_obj[foe].active, "defeated enemy remains active") ||
      !check(getpixel(pages[0], 40, 40) == 0 && getpixel(pages[1], 40, 40) == 0,
             "defeated enemy remains on a display page")) return 0;

  /* A finished shot must be removed without accessing an invalid object. */
  reset_projectiles();
  first_shot = defobject(sprite, 50, 50, OBJ_LOW);
  _shot[0].object = first_shot;
  _shot[0].go = 1;
  _shot[0].data = end_path;
  updatescreen();
  updatescreen();
  a_shot();
  updatescreen();
  updatescreen();
  if (!check(_shot[0].object == -1 && !_obj[first_shot].active,
             "finished shot remains active") ||
      !check(getpixel(pages[0], 50, 50) == 0 && getpixel(pages[1], 50, 50) == 0,
             "finished shot remains on a display page")) return 0;
  return 1;
}
