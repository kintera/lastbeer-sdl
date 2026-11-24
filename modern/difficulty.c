#include "difficulty.h"

int difficulty_speed(int difficulty, int base_speed)
{
   if (difficulty == DIFFICULTY_EASY) return base_speed + 1;
   if (difficulty == DIFFICULTY_HARD && base_speed > 1) return base_speed - 1;
   return base_speed;
}

/* Keep fractional damage as integer quarter-units, including 1-power shots. */
int difficulty_shot_power(int difficulty, int power)
{
   if (difficulty == DIFFICULTY_EASY) return power * 5;
   if (difficulty == DIFFICULTY_HARD) return power * 3;
   return power;
}
int difficulty_enemy_shield(int difficulty, int shield)
{
   return difficulty == DIFFICULTY_CLASSIC ? shield : shield * 4;
}
long difficulty_shop_reward(int difficulty, long score_gained)
{
   long reward = ((score_gained + 1500) / 2500) * 5;
   return difficulty == DIFFICULTY_EASY ? reward * 2 : reward;
}
long difficulty_level_money(int difficulty, long level_money)
{
   return difficulty == DIFFICULTY_EASY ? level_money * 2 : level_money;
}
int difficulty_regular_enemy(int flags, int shield, int score, int sprite, int sprites, int explosion, int explosions)
{
   return flags == 0x10 && shield > 0 && shield <= 20 && score > 0 && score <= 200 &&
          sprite >= 0 && sprite < sprites && explosion >= 0 && explosion < explosions;
}
