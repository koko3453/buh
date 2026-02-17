#ifndef BUH_SYSTEMS_ENEMIES_H
#define BUH_SYSTEMS_ENEMIES_H

#include "core/game.h"

const char *enemy_label(Game *g, Enemy *e);
void spawn_enemy(Game *g, int def_index);
void update_enemies(Game *g, float dt);

#endif
