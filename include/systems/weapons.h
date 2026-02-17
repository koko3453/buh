#ifndef BUH_SYSTEMS_WEAPONS_H
#define BUH_SYSTEMS_WEAPONS_H

#include "core/game.h"

void weapons_clear(Player *p);
void equip_weapon(Player *p, int def_index);
int weapon_is_owned(Player *p, int def_index, int *out_level);
int weapon_choice_allowed(Game *g, int def_index);
void apply_item(Player *p, Database *db, ItemDef *it, int item_index);

void spawn_puddle(Game *g, float x, float y, float radius, float dps, float ttl, int kind);
void spawn_weapon_fx(Game *g, int type, float x, float y, float angle, float duration, int target_enemy);
void spawn_scythe_fx(Game *g, float cx, float cy, float angle, float radial_speed, float angle_speed, float damage);
void spawn_bullet(Game *g, float x, float y, float vx, float vy, float damage, int pierce, int homing, int from_player,
                  int weapon_index, float bleed_chance, float burn_chance, float slow_chance, float stun_chance,
                  float armor_shred_chance);

void update_weapon_fx(Game *g, float dt);
void update_puddles(Game *g, float dt);
void update_bullets(Game *g, float dt);
void fire_weapons(Game *g, float dt);
void update_sword_orbit(Game *g, float dt);

#endif
