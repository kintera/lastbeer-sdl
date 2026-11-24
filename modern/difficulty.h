#ifndef LASTBEER_DIFFICULTY_H
#define LASTBEER_DIFFICULTY_H
enum { DIFFICULTY_EASY, DIFFICULTY_CLASSIC, DIFFICULTY_HARD };
int difficulty_speed(int difficulty, int base_speed);
int difficulty_shot_power(int difficulty, int power);
int difficulty_enemy_shield(int difficulty, int shield);
long difficulty_shop_reward(int difficulty, long score_gained);
long difficulty_level_money(int difficulty, long level_money);
int difficulty_regular_enemy(int flags, int shield, int score, int sprite, int sprites, int explosion, int explosions);
#endif
