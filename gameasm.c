/* Allegro port Copyright (C) 2014 Gavin Smith. */
#include "baller.h"
#include "xmode.h"
#include "sound.h"
#ifdef LASTBEER_SDL
#include "modern/difficulty.h"
#endif
#ifdef LASTBEER_AUTOMATION
static int test_crossing_slot = -1;
#endif

/*  Structure to save a complete position during the game.
    This is used to get back to a 'try again' point if the
    bottle broke. */

struct lastposstrc {
  int score;                /* Player's score. */
  short nattacks;           /* Attacks to go. */
  struct attackstrc *attack;              /* ptr to attacks. */
  short nbigboss;           /* N of Big Bosses to go. */
  short count;              /* Frame counter. */
};

struct lastposstrc lastposition;

short invincible; /* Is Eichli invincible? */
short frameinc;   /* Frame count increment step. */
short nbigboss;  /* Number of endlevel monsters that
                    must still be destroyed. */

/* Variables used to draw the score. */
extern short scoredigit;        /* # of digits already drawn. */
extern short scorepos;                 /* Current x position. */
extern int playscore;                /* Remaining score. */

short const10d;               /* Faster Division. */

/* Function to access field in starstrc array. */
int starstrc_member (unsigned char *data, int i, int member)
{
  data += i * STARSTRC_SIZE;
  return data[member] + (data[member + 1] << 8);
}

void set_starstrc_member (unsigned char *data, int i, int member,
                         unsigned short int value)
{
  data += i * STARSTRC_SIZE;
  data[member] = value & 0x00FF;
  data[member + 1] = (value & 0xFF00) >> 8;
}

/*
 defarm: Define a friendly object.
          Arguments are: pointer to a 'armstrc'.
                        x and y as a start location. */
int defarm (int x, int y, struct armstrc *arm)
{
  struct aniarm *a = _arm;
  int narms = MAXARMS;

  /* Get free arm slot. */
  while (narms--) {
    if (a->object == -1) {
      a->flags = arm->flags;
      a->shot = arm->shot;
      a->period = arm->period;
      a->periodcnt = 0;
      a->object = defobject (arm->sprite, x, y, OBJ_HIGH);

      return a->object;
    }
    a++;
  }
  return -1;
}

/* defshot: Define a shot.
   Arguments:
     shot:      Which shot should we define?
     x, y:      Define coordinates.
     addr:      addr of shot, that defines this shot. */
int defshot (int shot, int x, int y, struct anishot *addr)
{
  int ctr = MAXSHOTS;
  struct anishot *ptr = _shot;
  int ss_off;
  struct shotstrc *ss;

  do {
    if (ptr->object == -1) break;
    ptr++;
  } while (--ctr);

  /* If empty slot not found */
  if (ptr - (struct anishot *) &_shot == MAXSHOTS) return -1;

  /* Really don't know why we do this. Only done when shots are releasing
     other shots. */
  ptr->go = (ptr <= addr) ? 1 : 0;

  ss_off = weapon.shot[shot];
  ss = (struct shotstrc *) ((char *) weapon.shotfile + 2 + ss_off);
  //ss = ptrindex[shot * 2];

  /* Note: ss->data is an array, so we are pointing to the first element. */
  ptr->data = ss->data;
  ptr->power = ss->power;
#ifdef LASTBEER_SDL
  ptr->power = difficulty_shot_power(game_difficulty, ptr->power);
#ifdef LASTBEER_AUTOMATION
  compat_test_shot_power(game_difficulty, ss->power, ptr->power);
#endif
#endif
  ptr->speed = ss->speed;

  return ptr->object = defobject(intindex[ss->sprite],
                           x + ss->shotx, y + ss->shoty, OBJ_LOW);

}

/* This one defines any kind of foe. (Path-followers, Shots, ...). */
/* Return zero when the spawn should be retried because a pool is full. */
int deffoe (short foe, short x, short y, short x0, short y0)
{
  int ctr = MAXFOES;
  struct anifoe *ptr = &_foe[0];
  int fs_off;
  struct foestrc *fs;

  /* Scan for a free enemy slot. */
  do {
    if (ptr->object == -1) break;
    ptr++;
  } while (--ctr);
  
  if (ctr == 0)
    return 0;

  /* lfoeofs is "level foe offset" */
  fs = ptrindex[lfoeofs + foe];

  /* Aimed shots have no target during the death animation. */
  if ((fs->flags & FOE_LINE) && !(fs->flags & FOE_PATH) &&
      (_arm[0].object < 0 || _arm[0].object >= MAXOBJS))
    return 1;

  /* Copy flags and check type of foe. */

  ptr->flags = fs->flags;

  if (!(ptr->flags & (FOE_TRANSPARENT | FOE_INVINCIBLE))) {
    /* Foe can explode. */
    ptr->shield = fs->shield;
#ifdef LASTBEER_SDL
    ptr->shield = difficulty_enemy_shield(game_difficulty, ptr->shield);
#endif
    ptr->score = fs->score;
    ptr->expl = fs->expl;
  }

  if (ptr->flags & FOE_PATH) {
    /* Foe follows a predefined path. */
    ptr->cpath = fs->path;
  }

  if (ptr->flags & FOE_LINE) {
    int our_x0;
    int our_y0;

    int our_z0;
    int our_z1;

    /* Foe uses a runtime defined path (line mode). */
    ptr->speed = fs->speed;

    /* Calculate line draw variables z0, z1, dx, dy, dz. */

    our_x0 = x0; /* Destination */
    our_y0 = y0;

    if (!(ptr->flags & FOE_PATH)) {
      our_x0 = _obj[_arm[0].object].xa; /* Location of player's ship. */
      our_y0 = _obj[_arm[0].object].ya;
    }

    our_z0 = 1;
    our_x0 -= x;

    if (our_x0 < 0) {
      our_x0 *= -1; /* Must be positive. */
      our_z0 *= -1;
    }

    ptr->dx = our_x0; /* Horizontal distance to destination. */
    ptr->z0 = our_z0;

    our_z1 = 1;
    our_y0 -= y;
    if (our_y0 < 0) {
      our_y0 *= -1;
      our_z1 *= -1;
    }

    ptr->dy = our_y0;
    ptr->z1 = our_z1;

    ptr->pixelcnt = max(our_x0, our_y0);
    ptr->dz = our_x0 / 2;
  }

  /* lsprofs == "level sprite offset" */
  ptr->object = defobject(intindex[lsprofs + fs->sprite],
                                 x, y, OBJ_LOW);
  if (ptr->object < 0)
    return 0;
  if (ptr->flags & FOE_STOPCOUNT)
    frameinc = 0;
  return 1;
}

/* Required enemies released by scripts must be retried if a pool is full. */
static int spawn_scripted_foe(short foe, short x, short y, short x0, short y0)
{
  struct foestrc *fs = ptrindex[lfoeofs + foe];
  if (deffoe(foe, x, y, x0, y0)) return 1;
  return !(fs->flags & (FOE_ENDLEVEL | FOE_STOPCOUNT));
}

/* Explosion scripts can release a boss; retain them while slots are full. */
static struct { int object, explosion; } pending_explosions[MAXOBJS];
static int pending_explosion_count;

void clear_pending_explosions(void)
{
  pending_explosion_count = 0;
}

void defexpl (int obj, int explosion)
{
  struct aniexpl *ptr = _expl;
  int ctr = MAXEXPLS;
  
  do {
    if (ptr->object == -1)
      break;
  } while (ptr++, --ctr);

  if (ctr == 0) {
    if (pending_explosion_count < MAXOBJS) {
      pending_explosions[pending_explosion_count].object = obj;
      pending_explosions[pending_explosion_count++].explosion = explosion;
    } else {
      abandonobject(obj);
    }
    return;
  }

  ptr->object = obj;
  ptr->data = ptrindex [lexplofs + explosion];
  ptr->x = _obj[obj].xa + _obj[obj].xs / 2;
  ptr->y = _obj[obj].ya + _obj[obj].ys / 2;
}

void a_arm (int x, int y)
{
  int ctr;
  int obj_idx;
  struct objstrc *obj;

  obj_idx = _arm[0].object;
  obj = &_obj[obj_idx];

  if (invincible & 0x0002)
    flash(obj_idx);

  /* Check whether main bottle is off the play field. */
  if (obj->xa + x < windowx0) {
    x = windowx0 - obj->xa;
    /* Now obj->xa + x == windowx0 */
  }
  if (obj->xa + obj->xs + x > windowx1) {
    x = windowx1 - obj->xa - obj->xs;
  }
  if (obj->ya + y < windowy0) {
    y = windowy0 - obj->ya;
  }
  if (obj->ya + obj->ys + y > windowy1) {
    y = windowy1 - obj->ya - obj->ys;
  }

  ctr = MAXARMS - 1;
  do {
    if (_arm[ctr].object != -1) 
      moveobjectdelta(_arm[ctr].object, x, y);
  } while (ctr--);
}

void a_shot (void)
{
  int x, y;

  unsigned short cmd;
  int ctr;
  struct anishot *ptr;

  ctr = MAXSHOTS;
  ptr = _shot;
  
  for (; ctr > 0 ; ptr++, ctr-- ) {
    if (ptr->object == -1) continue;
    
    if (ptr->go != 1)
      {
        ptr->go = 1;
        continue;
      }
  nextcoord:
      cmd = *(ptr->data);

      if ((cmd & 0xfff0) != 0x8000) {
        short our_dx, our_dy;
        ptr->dx = (signed short) ptr->data[0];
        ptr->dy = (signed short) ptr->data[1];

        our_dx = smooth_move (ptr->dx, subtick);
        our_dy = smooth_move (ptr->dy, subtick);
        moveobjectdelta (ptr->object, our_dx, our_dy);

        if (subtick != 0)
          continue;
        /* Reflecting shots retain per-subtick deltas, while homing shots
           need full-frame velocity for their steering calculations. */
        if (ptr->data[2] != SHOTHOMING) {
          ptr->dx = our_dx;
          ptr->dy = our_dy;
        }
        ptr->data += 2;

      } else switch (cmd) {
      case SHOTEND:
        abandonobject (ptr->object);
        ptr->object = -1;
        continue;
      case SHOTRELEASE:
        if (subtick != 0)
          continue;
        defshot (ptr->data[1], _obj[ptr->object].xa, _obj[ptr->object].ya, ptr);
        ptr->data += 2;
        goto nextcoord;
      case SHOTHOMING:
        {
        struct anifoe *f = _foe;
        int fctr = MAXFOES;
        int x, y, dx, dy;

        do {
          if (f->object == -1) continue;
          if (f->flags & (FOE_INVINCIBLE | FOE_TRANSPARENT)) continue;
          break;
        } while (f++, --fctr);

        if (fctr == 0) {
          /* No eligible foe found */
          moveobjectdelta (ptr->object, smooth_move (ptr->dx, subtick),
                           smooth_move (ptr->dy, subtick));
          break;
        }

        x = _obj[f->object].xa;
        y = _obj[f->object].ya;
        
        /* Get mid point of object. */
        x += _obj[f->object].xs / 2;
        y += _obj[f->object].ys / 2;
        
        x -= _obj[ptr->object].xa;
        y -= _obj[ptr->object].ya;
       
        if (abs(x) > abs(y)) {
          y /= abs(x) / ptr->speed + 1;

          if (x < 0)
            x = -ptr->speed;
          else
            x = ptr->speed;
        } else {
          x /= abs(y) / ptr->speed + 1;

          if (y < 0)
            y = -ptr->speed;
          else
            y = ptr->speed;
        }
        
        dx = ptr->dx;
        dy = ptr->dy;

        if (x <= dx)
          dx--;
        else
          dx++;

        if (y <= dy)
          dy--;
        else
          dy++;

        if (subtick == 0)
          {
            ptr->dx = dx;
            ptr->dy = dy;
          }
        moveobjectdelta (ptr->object, smooth_move (dx, subtick),
                         smooth_move (dy, subtick));
        }
        break;
      case SHOTREFLECT:
        {
        int x, y, xs, ys;
        x = _obj[ptr->object].xa;
        y = _obj[ptr->object].ya;
        xs = _obj[ptr->object].xs;
        ys = _obj[ptr->object].ys;

        x += ptr->dx;

        /* Shot reflects at left border. */
        if (x < windowx0) {
          ptr->dx *= -1; /* Change direction */
          x += ptr->dx;
          x += ptr->dx;
        /* Shot reflects at right border. */
        } else if (x + xs > windowx1) {
          ptr->dx *= -1; /* Change direction */
          x += ptr->dx;
          x += ptr->dx;
        }

        y += ptr->dy;

        /* Shot reflects at top border. */
        if (y < windowy0) {
          ptr->dy *= -1; /* Change direction */
          y += ptr->dy;
          y += ptr->dy;
        /* Shot reflects at bottom border. */
        } else if (y + ys > windowy1) {
          ptr->dy *= -1; /* Change direction */
          y += ptr->dy;
          y += ptr->dy;
        }

        moveobject (ptr->object, x, y);

        if (subtick != 0)
          continue;
        ptr->data++;
        break;
        }
      } /* switch */

    if (outofwindow (ptr->object)) {
      abandonobject (ptr->object);
      ptr->object = -1;
    }
  }
}

/* This proc interprets the next command of
   enemy in his path and performs proper
   action. */
void a_foe (void)
{
  unsigned short cmd;
  int ctr;
  struct anifoe *ptr;

  ptr = &_foe[0]; /* was si */
  ctr = MAXFOES; /* was bp */
        
  do {
    if (ptr->object == -1)
      continue;

    if (ptr->flags & FOE_LINE) {
      void a_foeline (struct anifoe *ptr);
      a_foeline(ptr);
      continue;
    }

    do {
      /* Sometimes, we execute the next command right away instead of
         going to the next foe. */
    nextcoord: 

      /* Interpret the command. */
      if (((unsigned short) *ptr->cpath & 0xfff0) != A_COMMAND) {
        moveobjectdelta (ptr->object,
                         smooth_move (ptr->cpath[0], subtick),
                         smooth_move (ptr->cpath[1], subtick));
        if (subtick == 0)
          ptr->cpath += 2;
      } else switch ((unsigned short) *ptr->cpath) { 
      case FOEENDPATH:
        /* Object has reached end of path and must be destroyed. */
        abandonobject (ptr->object);
        ptr->object = -1;
        break;
      case FOECHANGESPRITE:
        /* Object wants to change it's look. */
        changesprite (ptr->object, intindex[lsprofs + ptr->cpath[1]]);
        ptr->cpath += 2;
        goto nextcoord;
      case FOERELEASEFOE:
        /* Object releases another object. */
        if (!spawn_scripted_foe (ptr->cpath[1],
                _obj[ptr->object].xa + ptr->cpath[2],
                _obj[ptr->object].ya + ptr->cpath[3],
                0, 0)) break;
        ptr->cpath += 4;
        goto nextcoord;
      case FOECYCLEPATH:
        ptr->cpath = ptr->path;
        moveobject (ptr->object, ptr->savex, ptr->savey);
        goto nextcoord;
      case FOEMARK:
        ptr->path = ++ptr->cpath; 
        ptr->savex = _obj[ptr->object].xa;
        ptr->savey = _obj[ptr->object].ya;
        goto nextcoord;
      case FOESOUND:
        playsample (ptrindex[lsndofs + ptr->cpath[1]]);
        ptr->cpath += 2;
        goto nextcoord;
      }
    } while (0);
  } while(ptr++, --ctr);
}

void a_foeline (struct anifoe *ptr)
{
  int our_dz = ptr->dz;
  int our_x = 0;
  int our_y = 0;

  int ctr = ptr->speed;
  do {
    if (our_dz < ptr->dx) {
      our_dz += ptr->dy;
      our_x += ptr->z0;
    }

    if (our_dz >= ptr->dx) {
      our_dz -= ptr->dx;
      our_y += ptr->z1;
    }

    if (ptr->flags & FOE_PATH) {
      if (subtick == 0
          && 0 == --ptr->pixelcnt) {
        /* Target coordinates have been reached. */
        ptr->flags &= ~FOE_LINE;
        our_x += _obj[ptr->object].xa;
        our_y += _obj[ptr->object].ya;
        moveobject (ptr->object, our_x, our_y);
        return;
      }
    }
  } while (--ctr);

  if (subtick == 0)
    ptr->dz = our_dz;

  our_x = smooth_move (our_x, subtick);
  our_y = smooth_move (our_y, subtick);

  our_x += _obj[ptr->object].xa;
  our_y += _obj[ptr->object].ya;

  moveobject (ptr->object, our_x, our_y);

  if (outofwindow(ptr->object)) {
    abandonobject (ptr->object);
    ptr->object = -1;
  }
}

void a_expl (void)
{
  int ctr = MAXEXPLS;
  struct aniexpl *ptr = _expl;

  while (pending_explosion_count > 0) {
    int available = 0;
    for (int i = 0; i < MAXEXPLS; ++i)
      if (_expl[i].object == -1) { available = 1; break; }
    if (!available) break;
    int obj = pending_explosions[0].object;
    int explosion = pending_explosions[0].explosion;
    --pending_explosion_count;
    memmove(pending_explosions, pending_explosions + 1,
            pending_explosion_count * sizeof(pending_explosions[0]));
    defexpl(obj, explosion);
  }

  do {
    unsigned short int cmd;

    if (ptr->object == -1)
      continue;

  nextcommand:
    cmd = *ptr->data;

    switch (cmd) {
    case EXPLEND:
      ptr->object = -1;
      break;
    case EXPLNEW:
      defobject (intindex[lsprofs + ptr->data[1]],
                 ptr->x + (signed short) ptr->data[2],
                 ptr->y + (signed short) ptr->data[3],
                 OBJ_HIGH | OBJ_ONECYCLE);
      ptr->data += 4;
      goto nextcommand;
    case EXPLWAIT:
      ptr->data++;
      break;
    case EXPLSOUND:
      playsample(ptrindex[lsndofs + ptr->data[1]]);
      ptr->data++;
      goto nextcommand;
    case EXPLREMOVEOBJ:
      abandonobject (ptr->object);
      ptr->data++;
      goto nextcommand;
    case EXPLRELEASEFOE:
      if (!spawn_scripted_foe (ptr->data[1],
              ptr->x + (signed short) ptr->data[2],
              ptr->y + (signed short) ptr->data[3],
              0, 0)) break;
      ptr->data += 4;
      goto nextcommand;
    case EXPLNEWPATH:
      if (!spawn_scripted_foe (ptr->data[1],
              ptr->x + (signed short) ptr->data[2],
              ptr->y + (signed short) ptr->data[3],
              ptr->data[4],
              ptr->data[5])) break;
      ptr->data += 6;
      goto nextcommand;
    default:
      ptr->data++;
      break;
    }

  } while (ptr++, --ctr);
}

void foehit (void)
{
  struct anifoe *ptr = _foe;
  int ctr = MAXFOES;

  struct anishot *s;
  int sctr;

  do {
    if (ptr->object == -1 || (ptr->flags & FOE_TRANSPARENT))
      continue;

    s = _shot;
    sctr = MAXSHOTS;
    do {
      if (s->object == -1)
        continue;
      if (crashtest (ptr->object, s->object)) {
        if (ptr->flags & FOE_INVINCIBLE)
          s->power = 0; /* Kill shot */
        else {
          int tmp_power;

          flash (ptr->object);

          tmp_power = s->power;
          s->power -= ptr->shield;
          ptr->shield -= tmp_power;

          if (ptr->shield <= 0) {
            score += ptr->score;
            defexpl (ptr->object, ptr->expl);

            /* Forget about object - it's controlled by a_expl now */
            ptr->object = -1; 

            /* Was it a big boss? */
            if (ptr->flags & FOE_ENDLEVEL) {
              nbigboss--;
            }

            /* Check for stopped counter */
            if (ptr->flags & FOE_STOPCOUNT) {
              frameinc = 1;
            }
          }
        }

        if (s->power <= 0) {
          abandonobject (s->object);
          s->object = -1;
        }

        /* The foe no longer has an object after a fatal hit. */
        if (ptr->object == -1)
          break;
      }
    } while (s++, --sctr);
  } while (ptr++, --ctr);
}


/* checks whether the heroic bottle of cool fresh Eichhof
   Lager has been hit by a nasty enemy beer. 
   RETURNS: 1 if hit. */
int armhit (void)
{
  int ctr, ctr2;
  struct anifoe *ptr;
  struct aniarm *a;

  if (invincible)
    return 0;

  /* First we check whether Eichli has been hit by a hostile shot. */

  ptr = _foe;
  ctr = MAXFOES;

  do {
    if (ptr->object == -1)
      continue;

    if (!crashtest (ptr->object, _arm[0].object))
      continue;

    /* Eichli has been hit */

    /* Do a beautiful explosion. */
    defexpl (_arm[0].object, 0);

    /* Destroy all weapons. */
    a = _arm;
    ctr2 = MAXARMS;

    do {
      if (a->object != -1) {
        abandonobject (a->object);
        a->object = -1;
      }
    } while (a++, --ctr2);

    /* Indiacte destroyed Eichli. */
    return 1;

  } while (ptr++, --ctr);

  return 0;
}

/* Voll power aus allen Rohren.
   what == TRUE  : es soll geschossen werden.
   what == FALSE : Friede. Kein schuss soll fallen. */

void fire (int what)
{
  int ctr = MAXARMS;
  struct aniarm *ptr = _arm;

  if (what) goto fire_label;

  do {
    if (ptr->object != -1) {
      if (ptr->periodcnt != 0) { /* Gun not already reloaded */
        ptr->periodcnt--; /* Decrement reload delay. */ 
      }
    }
    ptr++;
  } while (--ctr);

  return;

fire_label:
  do {
    if (ptr->object != -1) {
      if (!ptr->periodcnt--) {
        ptr->periodcnt = ptr->period;
        defshot (ptr->shot, 
                 _obj[ptr->object].xa,
                 _obj[ptr->object].ya,
                 (struct anishot *) -1);
      }
    }

    ptr++;
  } while (--ctr);
}

void keyboard (void)
{
  if (subtick == 0)
    fire (key[key_fire]
#ifdef LASTBEER_SDL
          || compat_mouse_fire()
#endif
    );

  int xchange = 0, ychange = 0;
  int movement_speed = shipspeed;
#ifdef LASTBEER_SDL
  movement_speed = difficulty_speed(game_difficulty, shipspeed);
#endif

#ifndef LASTBEER_SDL
  {
    static int fullscreen_key_down;
    int combination = key[KEY_ALT] && key[KEY_ENTER];
    if (combination && !fullscreen_key_down) toggle_fullscreen();
    fullscreen_key_down = combination;
  }
#endif

  {
    int step = movement_speed / RESOLUTION;
    if (subtick % RESOLUTION < movement_speed % RESOLUTION) ++step;
    xchange = ((key[key_right] != 0) - (key[key_left] != 0)) * step;
    ychange = ((key[key_down] != 0) - (key[key_up] != 0)) * step;
#ifdef LASTBEER_SDL
    {
      int target_x, target_y;
      if (compat_mouse_target(key[key_left] || key[key_right] || key[key_up] || key[key_down],
                              &target_x, &target_y)) {
        const struct objstrc *ship = &_obj[_arm[0].object];
        int dx = target_x - (ship->xa + ship->xs / 2);
        int dy = target_y - (ship->ya + ship->ys / 2);
        xchange = dx < -step ? -step : dx > step ? step : dx;
        ychange = dy < -step ? -step : dy > step ? step : dy;
      }
    }
#endif
  }

  if (key[key_pause]) {
    /* Wait for pause key to be released. */
    while (key[key_pause]) rest(1);

    /* Wait for key to be pressed. */
    clear_keybuf();
    while (!keypressed());

    /* If the player pressed the pause key to unpause, wait until the key
       is lifted to avoid pausing again. */
    while (key[key_pause]) rest(1);
#ifdef LASTBEER_SDL
    /* Time spent paused must not become a burst of catch-up updates. */
    updates_due = 0;
    tick = 0;
#endif
  }
  a_arm(xchange, ychange);
}

/* Return 0 on subtick, 1 on main tick. */
int waitforsubtick (void)
{
#ifdef LASTBEER_SDL
  while (updates_due <= 0) {
    rest(1);
    if (key[KEY_TAB] && updates_due <= 0) updates_due = 1;
  }
  --updates_due;
  tick = 0;
  return subtick_ == 0;
#else
  while (!tick) {
      rest (10);
#if 1
    if (key[KEY_TAB])
      tick = 1;
#endif
  }
  tick = 0;
  if (subtick_ == 0)
    return 1;
  return 0;
#endif
}

void waitfortick (void)
{
  while (!(tick && subtick_ == 0))
    rest (10);
  tick = 0;
}

void settick (void)
{
#ifdef LASTBEER_SDL
  /* Bound catch-up after a long stall or a suspended window. */
  if (updates_due < 8) updates_due++;
#else
  updates_due++;
#endif
  subtick_++;
  if (subtick_ >= RESOLUTION)
    subtick_ = 0;
  tick = 1;
}
END_OF_FUNCTION (settick)

/* set a starting position of a level. */
void setplayposition (short nattacks, struct attackstrc *attack, short nbigb)
{
  lastposition.nattacks = nattacks;
  lastposition.attack = attack;
  lastposition.nbigboss = nbigb;
  
  lastposition.score = score;
  lastposition.count = 0;
}

#ifdef LASTBEER_SDL
/* Reuse a modest, destructible enemy from the current level, without its
   scripted path or boss/stop-count behavior. */
static void spawn_hard_crossing(void)
{
  int chosen = -1, eligible = 0, slot = -1;
  for (int i = 0; i < MAXFOES; ++i) if (_foe[i].object == -1) { slot = i; break; }
  if (slot < 0) return;
  for (int i = 0; i < level.nfoes; ++i) {
    struct foestrc *foe = level.foe[i];
    if (!difficulty_regular_enemy(foe->flags, foe->shield, foe->score, foe->sprite,
                                  level.nsprites, foe->expl, level.nexpls)) continue;
    if (rand() % ++eligible == 0) chosen = i;
  }
  if (chosen < 0) return;
  int direction = (rand() & 1) ? 1 : -1;
  int y = 8 + rand() % 28;
  deffoe(chosen, direction > 0 ? windowx0 : windowx1, y, 0, 0);
  if (_foe[slot].object < 0) return;
  _foe[slot].flags = FOE_LINE;
  _foe[slot].speed = 3;
  _foe[slot].dx = windowx1 - windowx0;
  _foe[slot].dy = 0;
  _foe[slot].dz = 0;
  _foe[slot].z0 = direction;
  _foe[slot].z1 = 0;
#ifdef LASTBEER_AUTOMATION
  test_crossing_slot = slot;
  compat_test_hard_spawn(chosen, slot, _obj[_foe[slot].object].x, direction);
#endif
}
#endif

int play()
{
  short terminate, nattacks;

  short count;
  struct attackstrc *attack;
#ifdef LASTBEER_SDL
  int hard_spawn_timer = game_difficulty == DIFFICULTY_HARD ? GAMESPEED * 6 + rand() % (GAMESPEED * 5) : 0;
#ifdef LASTBEER_AUTOMATION
  if (getenv("LASTBEER_TEST_HARD_SPAWN") || getenv("LASTBEER_TEST_DIFFICULTY_MENU")) hard_spawn_timer = 2;
#endif
#endif

  score = lastposition.score;
  nbigboss = lastposition.nbigboss;
  nattacks = lastposition.nattacks;

  count =  lastposition.count;
  attack = lastposition.attack;
  
  frameinc = 1;
  scoredigit = 0;

  invincible = SHIELD_CTIME; /* First Eichli is invincible. */
  terminate = 0; /* No reason to terminate. */
  updates_due = 0;
  subtick = 0;
#ifdef LASTBEER_SDL
  compat_mouse_gameplay_begin(_obj[_arm[0].object].xa + _obj[_arm[0].object].xs / 2,
                              _obj[_arm[0].object].ya + _obj[_arm[0].object].ys / 2);
#endif
#ifdef LASTBEER_AUTOMATION
  TEST_STAGE("playing");
#endif
  do {
    int attack_blocked = 0;
    next_attack:
    if (nattacks != 0 && count == attack->count) {
      if (((unsigned short) attack->foe & 0xfff0) != A_COMMAND) {
        if (!deffoe(attack->foe, attack->x, attack->y, 0, 0)) {
          attack_blocked = 1;
          goto attack_wait;
        }
        nattacks--;
        attack++;
        goto next_attack;
      } else switch ((unsigned short) attack->foe) {
      case A_GOFIELD:
        gostarfield();
        attack++;
        goto next_attack;
      case A_STOPFIELD:
        stopstarfield();
        attack++;
        goto next_attack;
      case A_SOUND:
        playsample (ptrindex[lsndofs + attack->x]);
        attack++;
        goto next_attack;
      case A_MARK:
        lastposition.attack = attack;
        lastposition.count = count;

        lastposition.nattacks = nattacks;
        lastposition.nbigboss = nbigboss;
        lastposition.score = score;
        attack++;
        goto next_attack;
      }
    }

    attack_wait:
    /* ESC key test. */
    if (key[KEY_ESC]) {
      lifes = -1; /* Request game abort. */
      break;
    }

    do
      {
        waitforsubtick ();

#ifdef LASTBEER_SDL
        /* Keep simulation time when a display refresh takes longer than one
           update; skip presentation while processing the pending updates. */
        compat_skip_present(updates_due > 0);
#endif
        updatescreen();
#ifdef LASTBEER_SDL
        compat_skip_present(0);
#endif
#ifdef LASTBEER_AUTOMATION
        {
          int test_shots = 0;
          if (test_crossing_slot >= 0 && _foe[test_crossing_slot].object >= 0 &&
              _foe[test_crossing_slot].flags == FOE_LINE)
            compat_test_hard_progress(_obj[_foe[test_crossing_slot].object].x);
          for (int test_i = 0; test_i < MAXSHOTS; ++test_i)
            if (_shot[test_i].object != -1) ++test_shots;
          TEST_FRAME(_arm[0].object >= 0 ? _obj[_arm[0].object].x : -1, test_shots);
        }
#endif
        if (terminate != 2)
          keyboard();
    
        foehit();

        if (nbigboss == 0) {
            nbigboss = 1;
            invincible = WIN_CTIME;
            terminate = 1; /* All Endlevel monsters destroyed. */
        }

        if (!(cheatlevel & CHEATCRASH) && !key[KEY_TAB] && armhit()) {
            invincible = LOSE_CTIME;
            terminate = 2; /* Eichli destroyed. */
        }

        dispscore();
        a_shot();
        a_foe();
        subtick++;
        if (subtick == RESOLUTION)
          subtick = 0;

      }
    while (subtick != 0);


    a_expl();

#ifdef LASTBEER_SDL
    if (game_difficulty == DIFFICULTY_HARD && !terminate && --hard_spawn_timer <= 0) {
      spawn_hard_crossing();
      hard_spawn_timer = GAMESPEED * 7 + rand() % (GAMESPEED * 7);
    }
#endif

    if (!attack_blocked) count += frameinc;
    if (invincible) invincible--;
  } while ((invincible != 0) || terminate == 0);

#ifdef LASTBEER_SDL
  compat_gameplay_end();
#endif
  haltsound();

  if (terminate == 1) {
    return 1; /* next level */
  } else {
    return 0;
  }

}
