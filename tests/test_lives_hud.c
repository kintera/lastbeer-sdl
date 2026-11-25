#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "../baller.h"

static void require(int condition, const char *message)
{
   if (!condition) { fprintf(stderr, "SMOKE: lives HUD: %s\n", message); exit(1); }
}

static int sprite_matches(struct sprstrc *font, int position, int glyph)
{
   int p, x, y;
   for (p = 0; p < 2; ++p)
      for (y = 0; y < font->ys; ++y)
         for (x = 0; x < font->xs; ++x)
            if (getpixel(full_pages[p], position + x, BARSCOREY + y) !=
                (unsigned char)font->data[glyph * font->xs * font->ys + y * font->xs + x])
               return 0;
   return 1;
}

void test_lives_hud(struct sprstrc *bar, struct sprstrc *font)
{
   int saved = lifes, p, x, y, i;
   unsigned char sixpack[2][7][44], large[7][44];
   lifes = 6;
   displifes();
   for (i = 0; i < 6; ++i)
      require(sprite_matches(font, 20 + i * 4, BARLIFEDIGIT), "six-pack is missing a bottle");
   for (p = 0; p < 2; ++p)
      for (y = 0; y < 7; ++y)
         for (x = 0; x < 44; ++x)
            sixpack[p][y][x] = (unsigned char)getpixel(full_pages[p], x, BARSCOREY + y);

   lifes = 7;
   displifes();
   require(sprite_matches(font, 32, 7) && sprite_matches(font, 40, BARLIFEDIGIT),
           "seven lives should show 7 x bottle");
   for (p = 0; p < 2; ++p) {
      int color = (unsigned char)font->data[1];
      require(getpixel(full_pages[p], 36, BARSCOREY + 2) == color &&
              getpixel(full_pages[p], 37, BARSCOREY + 3) == color &&
              getpixel(full_pages[p], 38, BARSCOREY + 2) == color,
              "multiplication sign is missing");
   }
   lifes = 12;
   displifes();
   require(sprite_matches(font, 28, 1) && sprite_matches(font, 32, 2) &&
           sprite_matches(font, 40, BARLIFEDIGIT), "two-digit count is wrong");
   lifes = 9999;
   displifes();
   for (i = 0; i < 4; ++i) require(sprite_matches(font, 20 + i * 4, 9), "four-digit count is wrong");

   lifes = 12345;
   displifes();
   require(sprite_matches(font, 40, BARLIFEDIGIT), "large count lost its bottle");
   for (y = 0; y < 7; ++y)
      for (x = 0; x < 44; ++x)
         large[y][x] = (unsigned char)getpixel(full_pages[0], x, BARSCOREY + y);
   lifes = 12346;
   displifes();
   for (i = y = 0; y < 7; ++y)
      for (x = 0; x < 36; ++x)
         if (large[y][x] != getpixel(full_pages[0], x, BARSCOREY + y)) ++i;
   require(i > 0, "large counts do not change when a life changes");
   lifes = INT_MAX;
   displifes();
   require(sprite_matches(font, 40, BARLIFEDIGIT), "maximum count lost its bottle");

   lifes = 6;
   displifes();
   for (p = 0; p < 2; ++p)
      for (y = 0; y < 7; ++y)
         for (x = 0; x < 44; ++x)
            require(getpixel(full_pages[p], x, BARSCOREY + y) == sixpack[p][y][x],
                    "changing back to six left stale count pixels");
   lifes = 0;
   displifes();
   for (p = 0; p < 2; ++p)
      for (y = 0; y < 7; ++y)
         for (x = 0; x < 44; ++x)
            require(getpixel(full_pages[p], x, BARSCOREY + y) ==
                    (unsigned char)bar->data[(y + 1) * bar->xs + x], "empty bar retained a bottle");
   lifes = saved;
   displifes();
   fprintf(stderr, "SMOKE: lives HUD shows six bottles, then a count and bottle\n");
}
