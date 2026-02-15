#include "game.h"

const char *enemy_label(Game *g, Enemy *e) {
  if (!g || !e) return "enemy";
  int idx = e->def_index;
  if (idx >= 0 && idx < g->db.enemy_count) return g->db.enemies[idx].name;
  return "enemy";
}

void spawn_enemy(Game *g, int def_index) {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!g->enemies[i].active) {
      Enemy *e = &g->enemies[i];
      memset(e, 0, sizeof(*e));
      e->active = 1;
      e->def_index = def_index;
      EnemyDef *def = &g->db.enemies[def_index];
      e->hp = def->hp;
      e->max_hp = def->hp;
      e->spawn_invuln = 1.0f;
      e->hit_timer = -1.0f;
      float x = 0.0f;
      float y = 0.0f;
      float margin = 20.0f;
      float cam_min_x = g->camera_x;
      float cam_max_x = g->camera_x + g->view_w;
      float cam_min_y = g->camera_y;
      float cam_max_y = g->camera_y + g->view_h;
      int side = rand() % 4;
      if (side == 0) {
        x = cam_min_x - margin;
        y = cam_min_y + frandf() * g->view_h;
      } else if (side == 1) {
        x = cam_max_x + margin;
        y = cam_min_y + frandf() * g->view_h;
      } else if (side == 2) {
        x = cam_min_x + frandf() * g->view_w;
        y = cam_min_y - margin;
      } else {
        x = cam_min_x + frandf() * g->view_w;
        y = cam_max_y + margin;
      }
      x = clampf(x, 40.0f, ARENA_W - 40.0f);
      y = clampf(y, 40.0f, ARENA_H - 40.0f);
      e->x = x;
      e->y = y;
      return;
    }
  }
}

void update_enemies(Game *g, float dt) {
  Player *p = &g->player;
  Stats stats = player_total_stats(p, &g->db);
  for (int i = 0; i < MAX_ENEMIES; i++) {
    Enemy *e = &g->enemies[i];
    if (!e->active) continue;
    EnemyDef *def = &g->db.enemies[e->def_index];
    float dx = p->x - e->x;
    float dy = p->y - e->y;
    float dist2 = dx * dx + dy * dy;
    float dist = sqrtf(dist2);

    if (e->spawn_invuln > 0.0f) e->spawn_invuln -= dt;
    if (e->debuffs.burn_timer > 0.0f) {
      e->debuffs.burn_timer -= dt;
      e->hp -= 4.0f * dt;
    }
    if (e->debuffs.bleed_timer > 0.0f) {
      e->debuffs.bleed_timer -= dt;
      e->hp -= e->debuffs.bleed_stacks * 1.5f * dt;
      if (e->debuffs.bleed_timer <= 0.0f) e->debuffs.bleed_stacks = 0;
    }
    if (e->debuffs.slow_timer > 0.0f) e->debuffs.slow_timer -= dt;
    if (e->debuffs.stun_timer > 0.0f) e->debuffs.stun_timer -= dt;
    if (e->debuffs.armor_shred_timer > 0.0f) e->debuffs.armor_shred_timer -= dt;
    if (e->sword_hit_cd > 0.0f) e->sword_hit_cd -= dt;

    float aura_range = player_slow_aura(p, &g->db);
    if (aura_range > 0.0f && dist < aura_range) {
      e->debuffs.slow_timer = 0.5f;
    }

    float burn_range = player_burn_aura(p, &g->db);
    if (burn_range > 0.0f && dist < burn_range) {
      if (e->debuffs.burn_timer <= 0.0f) {
        log_combatf(g, "burn_aura applied to %s", enemy_label(g, e));
      }
      e->debuffs.burn_timer = 0.5f;
    }

    if (e->debuffs.stun_timer <= 0.0f &&
        (strcmp(def->role, "ranged") == 0 || strcmp(def->role, "boss") == 0 || strcmp(def->role, "turret") == 0)) {
      e->cooldown -= dt;
      if (e->cooldown <= 0.0f) {
        float vx = dx;
        float vy = dy;
        vec_norm(&vx, &vy);
        spawn_bullet(g, e->x, e->y, vx * def->projectile_speed, vy * def->projectile_speed, def->damage, 0, 0, 0,
                     -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
        e->cooldown = def->cooldown;
      }
    }

    if (e->debuffs.stun_timer <= 0.0f && strcmp(def->role, "charger") == 0) {
      e->charge_timer -= dt;
      if (e->charge_timer <= 0.0f) {
        float vx = dx;
        float vy = dy;
        vec_norm(&vx, &vy);
        e->vx = vx * def->charge_speed;
        e->vy = vy * def->charge_speed;
        e->charge_time = 0.35f;
        e->charge_timer = def->charge_cooldown;
      }
    }

    if (e->debuffs.stun_timer <= 0.0f && strcmp(def->role, "turret") != 0) {
      if (e->charge_time > 0.0f) {
        e->x += e->vx * dt;
        e->y += e->vy * dt;
        e->charge_time -= dt;
      } else {
        float vx = dx;
        float vy = dy;
        vec_norm(&vx, &vy);
        float slow_mul = (e->debuffs.slow_timer > 0.0f) ? 0.5f : 1.0f;
        e->x += vx * def->speed * slow_mul * dt;
        e->y += vy * def->speed * slow_mul * dt;
      }
    }

    if (dist < 20.0f) {
      float dmg = damage_after_armor(def->damage, stats.armor);
      float applied = dmg * dt;
      p->hp -= applied;
      float thorns = player_thorns_percent(p, &g->db);
      if (thorns > 0.0f) {
        e->hp -= applied * thorns;
        log_combatf(g, "thorns reflect %.1f to %s", applied * thorns, enemy_label(g, e));
      }
    }

    if (strcmp(def->role, "exploder") == 0 && dist < 28.0f) {
      float dmg = damage_after_armor(def->damage, stats.armor);
      float applied = dmg * 2.0f;
      p->hp -= applied;
      float thorns = player_thorns_percent(p, &g->db);
      if (thorns > 0.0f) {
        e->hp -= applied * thorns;
        log_combatf(g, "thorns reflect %.1f to %s", applied * thorns, enemy_label(g, e));
      }
      e->hp = 0;
    }

    if (e->hp <= 0.0f) {
      e->active = 0;
      g->kills += 1;
      if (e->spawn_invuln <= 0.0f) {
        float lifesteal = player_lifesteal_on_kill(p, &g->db);
        if (lifesteal > 0.0f) {
          p->hp = clampf(p->hp + lifesteal, 0.0f, stats.max_hp);
          log_combatf(g, "lifesteal_on_kill +%.1f HP", lifesteal);
        }
        spawn_drop(g, e->x, e->y, 0, 1 + rand() % 2);
        if (frandf() < 0.05f) spawn_drop(g, e->x, e->y, 1, 10 + rand() % 10);
      }
    }
  }
}
