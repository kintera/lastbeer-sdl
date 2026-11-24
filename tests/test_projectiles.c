#include <stdio.h>

#include "../baller.h"
#include "../xmode.h"
#include "../sound.h"

void a_shot(void);
void a_expl(void);
void foehit(void);
int deffoe(short foe, short x, short y, short x0, short y0);
void clear_pending_explosions(void);
extern short frameinc;

static int check(int condition, const char *message)
{
  if (!condition) fprintf(stderr, "Projectile test: %s\n", message);
  return condition;
}

static void reset_projectiles(void)
{
  int i;
  killallobjects();
  clear_pending_explosions();
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

  /* All object slots must be usable, including the first. */
  for (i = 0; i < MAXOBJS; ++i) _obj[i].active = 1;
  _obj[0].active = 0;
  if (!check(defobject(sprite, 40, 40, OBJ_LOW) == 0,
             "the first object slot could not be allocated")) return 0;
  for (i = 0; i < MAXOBJS; ++i) _obj[i].active = 0;

  /* Delayed rendering must not collapse multiple simulation ticks. */
  updates_due = 0;
  for (i = 0; i < 4; ++i) settick();
  for (i = 3; i >= 0; --i) {
    waitforsubtick();
    if (!check(updates_due == i, "queued simulation ticks were lost")) return 0;
  }

  /* Raw audio borrows its source; compressed audio owns its decoded bytes. */
  {
    unsigned char bytes[] = {128, 0x59, 0x01};
    struct sndstrc source = {5, 15, 0, 3, bytes};
    SAMPLE *sample = create_SAMPLE(&source);
    if (!check(sample && sample->data == bytes && sample->len == 3,
               "raw sample did not preserve its borrowed data")) return 0;
    free_SAMPLE(sample);
    source.flags = SND_PACKED4;
    sample = create_SAMPLE(&source);
    if (!check(sample && sample->data != bytes && sample->len == 5 &&
               ((unsigned char *)sample->data)[0] == 128,
               "packed sample did not own decoded data")) return 0;
    free_SAMPLE(sample);
  }

  /* A full explosion pool must delay the explosion rather than lose it. */
  unsigned short explosion_path[] = { EXPLREMOVEOBJ, EXPLEND };
  reset_projectiles();
  lexplofs = 0;
  ptrindex[0] = explosion_path;
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
      !check(_obj[foe].destroy == 0, "queued explosion lost its object") ||
      !check(_shot[1].power == 5, "a second shot hit an already defeated enemy")) return 0;
  for (i = 0; i < MAXEXPLS; ++i) _expl[i].object = -1;
  a_expl();
  if (!check(_obj[foe].destroy == 2, "queued explosion did not run when a slot opened")) return 0;
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

  /* The final level releases a required boss from an explosion. */
  for (int path = 0; path < 2; ++path) {
    struct foestrc boss = { .flags = FOE_ENDLEVEL | FOE_STOPCOUNT, .shield = 10, .sprite = 0 };
    unsigned short script[] = { path ? EXPLNEWPATH : EXPLRELEASEFOE, 0, 0, 0, 0, 0, EXPLWAIT, EXPLEND };
    int wait_offset = path ? 6 : 4;
    script[wait_offset] = EXPLWAIT;
    reset_projectiles();
    lfoeofs = lsprofs = 0;
    ptrindex[0] = &boss;
    intindex[0] = sprite;
    frameinc = 1;
    for (i = 0; i < MAXOBJS; ++i) defobject(sprite, 0, 0, OBJ_LOW);
    _expl[0].object = 0;
    _expl[0].data = script;
    _expl[0].x = _expl[0].y = 40;
    a_expl();
    if (!check(_expl[0].data == script && frameinc == 1,
               "explosion discarded a boss when the object pool was full")) return 0;
    killallobjects();
    a_expl();
    if (!check(_foe[0].object >= 0 && frameinc == 0 && _expl[0].data == script + wait_offset + 1,
               "explosion did not retry the boss when space became available")) return 0;
  }

  /* Homing shots keep full-frame velocity even without a target. */
  {
    unsigned short path[] = {0, (unsigned short)-12, SHOTHOMING};
    reset_projectiles();
    first_shot = defobject(sprite, 100, 100, OBJ_LOW);
    _shot[0].object = first_shot;
    _shot[0].go = 1;
    _shot[0].data = path;
    _shot[0].speed = 12;
    subtick = 0;
    a_shot();
    if (!check(_shot[0].dy == -12 && _obj[first_shot].y == 97,
               "homing shot did not retain its full velocity")) return 0;
    subtick = 1;
    a_shot();
    if (!check(_obj[first_shot].y == 94, "homing shot accelerated without a target")) return 0;
  }

  /* No aimed shot should attempt to target a player who has died. */
  {
    struct { short flags, shield, score, expl, sprite, speed; short path[1]; } foe_data = {0};
    reset_projectiles();
    lfoeofs = lsprofs = 0;
    ptrindex[0] = &foe_data;
    intindex[0] = sprite;
    _arm[0].object = -1;
    frameinc = 1;
    foe_data.flags = FOE_LINE;
    if (!check(deffoe(0, 50, 50, 0, 0) && _foe[0].object == -1,
               "aimed shot spawned after player death")) return 0;

    /* Failed stop-count spawns must not stop the level timer, and must
       succeed once an object slot becomes available. */
    foe_data.flags = FOE_PATH | FOE_STOPCOUNT;
    for (i = 0; i < MAXOBJS; ++i) _obj[i].active = 1;
    if (!check(!deffoe(0, 50, 50, 0, 0) && frameinc == 1,
               "failed enemy spawn stopped the level timer")) return 0;
    _obj[MAXOBJS / 2].active = 0;
    if (!check(deffoe(0, 50, 50, 0, 0) && frameinc == 0 && _foe[0].object >= 0,
               "enemy spawn did not recover after a slot became free")) return 0;
    for (i = 0; i < MAXOBJS; ++i) _obj[i].active = 0;
  }
  return 1;
}
