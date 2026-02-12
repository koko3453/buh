#include "game.h"

void spawn_bullet(Game *g, float x, float y, float vx, float vy, float damage, int pierce, int homing, int from_player,
                  int weapon_index, float bleed_chance, float burn_chance, float slow_chance, float stun_chance,
                  float armor_shred_chance) {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!g->bullets[i].active) {
      Bullet *b = &g->bullets[i];
      memset(b, 0, sizeof(*b));
      b->active = 1;
      b->x = x;
      b->y = y;
      b->vx = vx;
      b->vy = vy;
      b->damage = damage;
      b->pierce = pierce;
      b->radius = 6.0f;
      b->lifetime = 2.2f;
      b->homing = homing;
      b->from_player = from_player;
      b->weapon_index = weapon_index;
      b->bleed_chance = bleed_chance;
      b->burn_chance = burn_chance;
      b->slow_chance = slow_chance;
      b->stun_chance = stun_chance;
      b->armor_shred_chance = armor_shred_chance;
      return;
    }
  }
}

void spawn_puddle(Game *g, float x, float y, float radius, float dps, float ttl) {
  for (int i = 0; i < MAX_PUDDLES; i++) {
    if (!g->puddles[i].active) {
      Puddle *p = &g->puddles[i];
      memset(p, 0, sizeof(*p));
      p->active = 1;
      p->x = x;
      p->y = y;
      p->radius = radius;
      p->dps = dps;
      p->ttl = ttl;
      p->log_timer = 0.0f;
      return;
    }
  }
}

void spawn_weapon_fx(Game *g, int type, float x, float y, float angle, float duration, int target_enemy) {
  for (int i = 0; i < MAX_WEAPON_FX; i++) {
    if (!g->weapon_fx[i].active) {
      WeaponFX *fx = &g->weapon_fx[i];
      memset(fx, 0, sizeof(*fx));
      fx->active = 1;
      fx->type = type;
      fx->x = x;
      fx->y = y;
      fx->angle = angle;
      fx->timer = 0.0f;
      fx->duration = duration;
      fx->target_enemy = target_enemy;
      return;
    }
  }
}

void spawn_scythe_fx(Game *g, float cx, float cy, float angle, float radial_speed, float angle_speed, float damage) {
  for (int i = 0; i < MAX_WEAPON_FX; i++) {
    if (!g->weapon_fx[i].active) {
      WeaponFX *fx = &g->weapon_fx[i];
      memset(fx, 0, sizeof(*fx));
      fx->active = 1;
      fx->type = 0;
      fx->x = cx;
      fx->y = cy;
      fx->angle = angle;
      fx->timer = 0.0f;
      fx->duration = 8.0f;
      fx->radius = 0.0f;
      fx->radial_speed = radial_speed;
      fx->angle_speed = angle_speed;
      fx->damage = damage;
      fx->scythe_id = ++g->scythe_id_counter;
      fx->scythe_hit_boss = 0;
      fx->start_angle = angle;
      return;
    }
  }
}

void update_weapon_fx(Game *g, float dt) {
  Stats stats = player_total_stats(&g->player, &g->db);
  for (int i = 0; i < MAX_WEAPON_FX; i++) {
    if (!g->weapon_fx[i].active) continue;
    WeaponFX *fx = &g->weapon_fx[i];
    fx->timer += dt;
    if (fx->type == 0) {
      fx->radius = fx->radial_speed * fx->timer;
      fx->angle = fx->start_angle + fx->angle_speed * fx->timer;
      float px = fx->x + cosf(fx->angle) * fx->radius;
      float py = fx->y + sinf(fx->angle) * fx->radius;
      if (px < -20.0f || px > ARENA_W + 20.0f || py < -20.0f || py > ARENA_H + 20.0f) {
        fx->active = 0;
        continue;
      }
      float hit_r = 34.0f;
      float hit_r2 = hit_r * hit_r;
      for (int e = 0; e < MAX_ENEMIES; e++) {
        Enemy *en = &g->enemies[e];
        if (!en->active) continue;
        if (en->spawn_invuln > 0.0f) continue;
        if (en->scythe_hit_id == fx->scythe_id) continue;
        float dx = en->x - px;
        float dy = en->y - py;
        if (dx * dx + dy * dy <= hit_r2) {
          mark_enemy_hit(en);
          float final_dmg = player_apply_hit_mods(g, en, fx->damage);
          en->hp -= final_dmg;
          en->scythe_hit_id = fx->scythe_id;
          player_try_item_proc(g, e, &stats);
          if (en->hp <= 0.0f) {
            g->player.hp = clampf(g->player.hp + 6.0f, 0.0f, stats.max_hp);
          }
        }
      }
      if (g->mode == MODE_BOSS_EVENT && g->boss.active && !fx->scythe_hit_boss) {
        float dx = g->boss.x - px;
        float dy = g->boss.y - py;
        float r = g_boss_defs[g->boss.def_index].radius + hit_r;
        if (dx * dx + dy * dy <= r * r) {
          g->boss.hp -= fx->damage;
          fx->scythe_hit_boss = 1;
        }
      }
    }
    if (fx->timer >= fx->duration) {
      fx->active = 0;
    }
  }
}

void update_puddles(Game *g, float dt) {
  for (int i = 0; i < MAX_PUDDLES; i++) {
    Puddle *p = &g->puddles[i];
    if (!p->active) continue;
    p->ttl -= dt;
    p->log_timer -= dt;
    if (p->ttl <= 0.0f) {
      p->active = 0;
      continue;
    }

    float radius2 = p->radius * p->radius;
    for (int e = 0; e < MAX_ENEMIES; e++) {
      if (!g->enemies[e].active) continue;
      Enemy *en = &g->enemies[e];
      if (en->spawn_invuln > 0.0f) continue;
      float dx = en->x - p->x;
      float dy = en->y - p->y;
      float d2 = dx * dx + dy * dy;
      if (d2 <= radius2) {
        en->hp -= p->dps * dt;
        if (p->log_timer <= 0.0f) {
          log_combatf(g, "puddle tick %s for %.1f", enemy_label(g, en), p->dps * dt);
        }
      }
    }
    if (p->log_timer <= 0.0f) p->log_timer = 0.25f;
  }
}

void update_bullets(Game *g, float dt) {
  Player *p = &g->player;
  Stats stats = player_total_stats(p, &g->db);
  float item_burn = player_burn_on_hit(p, &g->db);
  for (int i = 0; i < MAX_BULLETS; i++) {
    Bullet *b = &g->bullets[i];
    if (!b->active) continue;
    b->lifetime -= dt;
    if (b->lifetime <= 0.0f) { b->active = 0; continue; }

    if (b->homing && b->from_player) {
      float best = 999999.0f;
      int best_i = -1;
      for (int e = 0; e < MAX_ENEMIES; e++) {
        if (!g->enemies[e].active) continue;
        float dx = g->enemies[e].x - b->x;
        float dy = g->enemies[e].y - b->y;
        float d2 = dx*dx + dy*dy;
        if (d2 < best) { best = d2; best_i = e; }
      }
      if (best_i >= 0) {
        float tx = g->enemies[best_i].x - b->x;
        float ty = g->enemies[best_i].y - b->y;
        vec_norm(&tx, &ty);
        b->vx = 0.85f * b->vx + 0.15f * tx * 350.0f;
        b->vy = 0.85f * b->vy + 0.15f * ty * 350.0f;
      }
    }

    b->x += b->vx * dt;
    b->y += b->vy * dt;

    if (b->x < 0 || b->x > ARENA_W || b->y < 0 || b->y > ARENA_H) {
      b->active = 0;
      continue;
    }

    if (b->from_player) {
      if (g->mode == MODE_BOSS_EVENT && g->boss.active) {
        const BossDef *bdef = &g_boss_defs[g->boss.def_index];
        float dx = g->boss.x - b->x;
        float dy = g->boss.y - b->y;
        float r = bdef->radius;
        if (dx * dx + dy * dy < r * r) {
          g->boss.hp -= b->damage;
          b->pierce -= 1;
          if (b->pierce < 0) { b->active = 0; }
          continue;
        }
      }
      for (int e = 0; e < MAX_ENEMIES; e++) {
        Enemy *en = &g->enemies[e];
        if (!en->active) continue;
        float dx = en->x - b->x;
        float dy = en->y - b->y;
        float radius = 16.0f;
        if (dx * dx + dy * dy < (radius + b->radius) * (radius + b->radius)) {
          if (en->spawn_invuln > 0.0f) {
            b->active = 0;
            break;
          }
          mark_enemy_hit(en);
          float dmg = player_apply_hit_mods(g, en, b->damage);
          en->hp -= dmg;
          if (b->weapon_index >= 0 && b->weapon_index < g->db.weapon_count) {
            WeaponDef *w = &g->db.weapons[b->weapon_index];
            log_combatf(g, "hit %s with %s for %.1f", enemy_label(g, en), w->name, dmg);
          } else {
            log_combatf(g, "hit %s for %.1f", enemy_label(g, en), dmg);
          }
          if (b->bleed_chance > 0.0f && frandf() < b->bleed_chance) {
            en->bleed_stacks = (en->bleed_stacks < 5) ? en->bleed_stacks + 1 : 5;
            en->bleed_timer = 4.0f;
            log_combatf(g, "bleed applied to %s", enemy_label(g, en));
          }
          if (b->burn_chance > 0.0f && frandf() < b->burn_chance) {
            en->burn_timer = 4.0f;
            log_combatf(g, "burn applied to %s", enemy_label(g, en));
          }
          if (item_burn > 0.0f && en->burn_timer > 0.0f) {
            log_combatf(g, "burn_on_hit applied to %s", enemy_label(g, en));
          }
          if (b->slow_chance > 0.0f && frandf() < b->slow_chance) {
            en->slow_timer = 2.5f;
            log_combatf(g, "slow applied to %s", enemy_label(g, en));
          }
          if (b->stun_chance > 0.0f && frandf() < b->stun_chance) {
            en->stun_timer = 0.6f;
            log_combatf(g, "stun applied to %s", enemy_label(g, en));
          }
          if (b->armor_shred_chance > 0.0f && frandf() < b->armor_shred_chance) {
            en->armor_shred_timer = 3.0f;
            log_combatf(g, "armor_shred applied to %s", enemy_label(g, en));
          }
          player_try_item_proc(g, e, &stats);
          b->pierce -= 1;
          if (b->pierce < 0) { b->active = 0; }
          break;
        }
      }
    } else {
      float dx = p->x - b->x;
      float dy = p->y - b->y;
      if (dx * dx + dy * dy < 400.0f) {
        float dmg = damage_after_armor(b->damage, stats.armor);
        p->hp -= dmg;
        b->active = 0;
      }
    }
  }
}

void update_sword_orbit(Game *g, float dt) {
  if (!g) return;
  WeaponSlot *slot = NULL;
  for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
    if (!g->player.weapons[i].active) continue;
    WeaponDef *w = &g->db.weapons[g->player.weapons[i].def_index];
    if (weapon_is(w, "sword")) {
      slot = &g->player.weapons[i];
      break;
    }
  }
  if (!slot) return;
  g->player.sword_orbit_angle += dt * SWORD_ORBIT_SPEED;
  if (g->player.sword_orbit_angle > 6.28318f) g->player.sword_orbit_angle -= 6.28318f;
}

void fire_weapons(Game *g, float dt) {
  Player *p = &g->player;
  Stats stats = player_total_stats(p, &g->db);
  float attack_speed = 1.0f + stats.attack_speed;
  float cooldown_scale = clampf(1.0f - stats.cooldown_reduction, 0.4f, 1.0f);
  float item_burn = player_burn_on_hit(p, &g->db);

  for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
    WeaponSlot *slot = &p->weapons[i];
    if (!slot->active) continue;
    WeaponDef *w = &g->db.weapons[slot->def_index];

    if (weapon_is(w, "sword")) {
      float level_mul = 1.0f + 0.2f * (slot->level - 1);
      float damage = w->damage * level_mul * (1.0f + stats.damage);
      float bleed_chance, burn_chance, slow_chance, stun_chance, shred_chance;
      weapon_status_chances(w, &bleed_chance, &burn_chance, &slow_chance, &stun_chance, &shred_chance);
      slow_chance += player_slow_on_hit(p, &g->db);
      slow_chance = clampf(slow_chance, 0.0f, 1.0f);
      burn_chance += item_burn;
      burn_chance = clampf(burn_chance, 0.0f, 1.0f);

      int sword_count = slot->level;
      if (sword_count < 1) sword_count = 1;
      float orbit_radius = w->range * SWORD_ORBIT_RANGE_SCALE;
      float half_w = 0.5f * (float)SWORD_ORBIT_WIDTH;
      float half_l = 0.5f * orbit_radius;

      for (int s = 0; s < sword_count; s++) {
        float angle = g->player.sword_orbit_angle + (6.28318f * (float)s / (float)sword_count);
        float angle_hit = angle + 1.570796f;
        float tip_x = p->x + cosf(angle) * orbit_radius;
        float tip_y = p->y + sinf(angle) * orbit_radius;
        float mid_x = (p->x + tip_x) * 0.5f;
        float mid_y = (p->y + tip_y) * 0.5f;
        float cos_a = cosf(angle_hit);
        float sin_a = sinf(angle_hit);

        if (g->mode == MODE_BOSS_EVENT && g->boss.active) {
          if (g->boss.sword_hit_cd > 0.0f) {
          } else {
            float dx = g->boss.x - mid_x;
            float dy = g->boss.y - mid_y;
            float local_x = -dx * sin_a + dy * cos_a;
            float local_y = dx * cos_a + dy * sin_a;
            float clamp_x = clampf(local_x, -half_w, half_w);
            float clamp_y = clampf(local_y, -half_l, half_l);
            float ddx = local_x - clamp_x;
            float ddy = local_y - clamp_y;
            float boss_r = g_boss_defs[g->boss.def_index].radius;
            if (ddx * ddx + ddy * ddy <= boss_r * boss_r) {
              float final_dmg = player_roll_crit_damage(&stats, w, damage);
              g->boss.hp -= final_dmg;
              g->boss.sword_hit_cd = SWORD_ORBIT_HIT_COOLDOWN / attack_speed;
            }
          }
        }

        for (int e = 0; e < MAX_ENEMIES; e++) {
          Enemy *en = &g->enemies[e];
          if (!en->active) continue;
          if (en->spawn_invuln > 0.0f) continue;
          if (en->sword_hit_cd > 0.0f) continue;
          float dx = en->x - mid_x;
          float dy = en->y - mid_y;
          float local_x = -dx * sin_a + dy * cos_a;
          float local_y = dx * cos_a + dy * sin_a;
          if (fabsf(local_x) > half_w || fabsf(local_y) > half_l) continue;
          mark_enemy_hit(en);
          float final_dmg = player_roll_crit_damage(&stats, w, damage);
          final_dmg = player_apply_hit_mods(g, en, final_dmg);
          en->hp -= final_dmg;
          en->sword_hit_cd = SWORD_ORBIT_HIT_COOLDOWN / attack_speed;
          log_combatf(g, "hit %s with %s for %.1f", enemy_label(g, en), w->name, final_dmg);
          if (bleed_chance > 0.0f && frandf() < bleed_chance) {
            en->bleed_stacks = (en->bleed_stacks < 5) ? en->bleed_stacks + 1 : 5;
            en->bleed_timer = 4.0f;
            log_combatf(g, "bleed applied to %s", enemy_label(g, en));
          }
          if (burn_chance > 0.0f && frandf() < burn_chance) {
            en->burn_timer = 4.0f;
            log_combatf(g, "burn applied to %s", enemy_label(g, en));
          }
          if (item_burn > 0.0f && en->burn_timer > 0.0f) {
            log_combatf(g, "burn_on_hit applied to %s", enemy_label(g, en));
          }
          if (slow_chance > 0.0f && frandf() < slow_chance) {
            en->slow_timer = 2.5f;
            log_combatf(g, "slow applied to %s", enemy_label(g, en));
          }
          if (stun_chance > 0.0f && frandf() < stun_chance) {
            en->stun_timer = 0.6f;
            log_combatf(g, "stun applied to %s", enemy_label(g, en));
          }
          if (shred_chance > 0.0f && frandf() < shred_chance) {
            en->armor_shred_timer = 3.0f;
            log_combatf(g, "armor_shred applied to %s", enemy_label(g, en));
          }
          player_try_item_proc(g, e, &stats);
        }
      }

      continue;
    }

    slot->cd_timer -= dt * attack_speed;
    if (slot->cd_timer > 0.0f) continue;

    float best = 999999.0f;
    int target = -1;
    int target_is_boss = 0;
    float target_x = 0.0f;
    float target_y = 0.0f;
    if (g->mode == MODE_BOSS_EVENT && g->boss.active) {
      target_is_boss = 1;
      target_x = g->boss.x;
      target_y = g->boss.y;
      float dx = target_x - p->x;
      float dy = target_y - p->y;
      best = dx * dx + dy * dy;
    } else if (weapon_is(w, "alchemist_puddle")) {
      int onscreen_count = 0;
      float cam_min_x = g->camera_x;
      float cam_max_x = g->camera_x + g->view_w;
      float cam_min_y = g->camera_y;
      float cam_max_y = g->camera_y + g->view_h;
      for (int e = 0; e < MAX_ENEMIES; e++) {
        if (!g->enemies[e].active) continue;
        if (g->enemies[e].spawn_invuln > 0.0f) continue;
        float ex = g->enemies[e].x;
        float ey = g->enemies[e].y;
        if (ex < cam_min_x || ex > cam_max_x || ey < cam_min_y || ey > cam_max_y) continue;
        onscreen_count++;
      }
      if (onscreen_count <= 0) continue;
      int pick = rand() % onscreen_count;
      for (int e = 0; e < MAX_ENEMIES; e++) {
        if (!g->enemies[e].active) continue;
        if (g->enemies[e].spawn_invuln > 0.0f) continue;
        float ex = g->enemies[e].x;
        float ey = g->enemies[e].y;
        if (ex < cam_min_x || ex > cam_max_x || ey < cam_min_y || ey > cam_max_y) continue;
        if (pick-- == 0) { target = e; break; }
      }
      if (target < 0) continue;
    } else {
      for (int e = 0; e < MAX_ENEMIES; e++) {
        if (!g->enemies[e].active) continue;
        if (g->enemies[e].spawn_invuln > 0.0f) continue;
        float dx = g->enemies[e].x - p->x;
        float dy = g->enemies[e].y - p->y;
        float d2 = dx * dx + dy * dy;
        if (d2 < best) { best = d2; target = e; }
      }
      if (target < 0) continue;
    }

    float range = w->range;
    if (!weapon_is(w, "alchemist_puddle")) {
      if (range > 0.0f && best > range * range) continue;
    }

    float tx = (target_is_boss ? target_x : g->enemies[target].x) - p->x;
    float ty = (target_is_boss ? target_y : g->enemies[target].y) - p->y;
    vec_norm(&tx, &ty);

    float level_mul = 1.0f + 0.2f * (slot->level - 1);
    float damage = w->damage * level_mul * (1.0f + stats.damage);

    float bleed_chance, burn_chance, slow_chance, stun_chance, shred_chance;
    weapon_status_chances(w, &bleed_chance, &burn_chance, &slow_chance, &stun_chance, &shred_chance);

    slow_chance += player_slow_on_hit(p, &g->db);
    slow_chance = clampf(slow_chance, 0.0f, 1.0f);

    burn_chance += item_burn;
    burn_chance = clampf(burn_chance, 0.0f, 1.0f);

    if (weapon_is(w, "lightning_zone")) {
      float range = w->range * (1.0f + 0.1f * (slot->level - 1));
      float range2 = range * range;
      if (g->mode == MODE_BOSS_EVENT && g->boss.active) {
        float ex = g->boss.x - p->x;
        float ey = g->boss.y - p->y;
        float d2 = ex * ex + ey * ey;
        if (d2 <= range2) {
          float final_dmg = player_roll_crit_damage(&stats, w, damage);
          g->boss.hp -= final_dmg;
        }
      } else {
        for (int e = 0; e < MAX_ENEMIES; e++) {
          if (!g->enemies[e].active) continue;
          Enemy *en = &g->enemies[e];
          if (en->spawn_invuln > 0.0f) continue;
          float ex = en->x - p->x;
          float ey = en->y - p->y;
          float d2 = ex * ex + ey * ey;
          if (d2 <= range2) {
            mark_enemy_hit(en);
            float final_dmg = player_roll_crit_damage(&stats, w, damage);
            final_dmg = player_apply_hit_mods(g, en, final_dmg);
            en->hp -= final_dmg;
            log_combatf(g, "hit %s with %s for %.1f", enemy_label(g, en), w->name, final_dmg);
            player_try_item_proc(g, e, &stats);
            if (frandf() < 0.15f) {
              en->stun_timer = 0.3f;
              log_combatf(g, "stun applied to %s", enemy_label(g, en));
            }
          }
        }
      }
      float level_cd = clampf(1.0f - 0.05f * (slot->level - 1), 0.7f, 1.0f);
      slot->cd_timer = w->cooldown * cooldown_scale * level_cd;
      continue;
    }

    if (weapon_is(w, "alchemist_puddle")) {
      float range = (w->range > 0.0f ? w->range : 90.0f);
      float dps = damage;
      float px = target_is_boss ? target_x : g->enemies[target].x;
      float py = target_is_boss ? target_y : g->enemies[target].y;
      spawn_puddle(g, px, py, range, dps, 5.0f);
      log_combatf(g, "puddle spawned (r=%.0f dps=%.1f)", range, dps);
      float level_cd = clampf(1.0f - 0.05f * (slot->level - 1), 0.7f, 1.0f);
      slot->cd_timer = w->cooldown * cooldown_scale * level_cd;
      continue;
    }

    if (weapon_is(w, "laser") || weapon_is(w, "whip") || weapon_is(w, "chain_blades")) {
      float range = w->range;
      float half_width = weapon_is(w, "whip") ? 8.0f : 10.0f;
      if (g->mode == MODE_BOSS_EVENT && g->boss.active) {
        float ex = g->boss.x - p->x;
        float ey = g->boss.y - p->y;
        float proj = ex * tx + ey * ty;
        if (proj > 0.0f && proj <= range) {
          float perp = fabsf(ex * (-ty) + ey * tx);
          if (perp <= half_width + g_boss_defs[g->boss.def_index].radius) {
            float final_dmg = player_roll_crit_damage(&stats, w, damage);
            g->boss.hp -= final_dmg;
          }
        }
      }
      for (int e = 0; e < MAX_ENEMIES; e++) {
        if (!g->enemies[e].active) continue;
        float ex = g->enemies[e].x - p->x;
        float ey = g->enemies[e].y - p->y;
        float proj = ex * tx + ey * ty;
        if (proj < 0.0f || proj > range) continue;
        float perp = fabsf(ex * (-ty) + ey * tx);
        if (perp <= half_width) {
          Enemy *en = &g->enemies[e];
          if (en->spawn_invuln > 0.0f) continue;
          mark_enemy_hit(en);
          float final_dmg = player_roll_crit_damage(&stats, w, damage);
          final_dmg = player_apply_hit_mods(g, en, final_dmg);
          en->hp -= final_dmg;
          log_combatf(g, "hit %s with %s for %.1f", enemy_label(g, en), w->name, final_dmg);
          if (bleed_chance > 0.0f && frandf() < bleed_chance) {
            en->bleed_stacks = (en->bleed_stacks < 5) ? en->bleed_stacks + 1 : 5;
            en->bleed_timer = 4.0f;
            log_combatf(g, "bleed applied to %s", enemy_label(g, en));
          }
          if (burn_chance > 0.0f && frandf() < burn_chance) {
            en->burn_timer = 4.0f;
            log_combatf(g, "burn applied to %s", enemy_label(g, en));
          }
          if (item_burn > 0.0f && en->burn_timer > 0.0f) {
            log_combatf(g, "burn_on_hit applied to %s", enemy_label(g, en));
          }
          if (slow_chance > 0.0f && frandf() < slow_chance) {
            en->slow_timer = 2.5f;
            log_combatf(g, "slow applied to %s", enemy_label(g, en));
          }
          if (stun_chance > 0.0f && frandf() < stun_chance) {
            en->stun_timer = 0.6f;
            log_combatf(g, "stun applied to %s", enemy_label(g, en));
          }
          if (shred_chance > 0.0f && frandf() < shred_chance) {
            en->armor_shred_timer = 3.0f;
            log_combatf(g, "armor_shred applied to %s", enemy_label(g, en));
          }
          player_try_item_proc(g, e, &stats);
          if (weapon_is(w, "chain_blades")) {
            en->x -= tx * 20.0f;
            en->y -= ty * 20.0f;
          }
        }
      }
      float level_cd = clampf(1.0f - 0.05f * (slot->level - 1), 0.7f, 1.0f);
      slot->cd_timer = w->cooldown * cooldown_scale * level_cd;
      continue;
    }

    if (weapon_is(w, "scythe")) {
      float base_angle = atan2f(ty, tx) + p->scythe_throw_angle;
      p->scythe_throw_angle -= (3.14159f / 4.0f);
      float travel_speed = 140.0f + 12.0f * (slot->level - 1);
      float angle_speed = 2.5f;
      float final_dmg = player_roll_crit_damage(&stats, w, damage);
      spawn_scythe_fx(g, p->x, p->y, base_angle, travel_speed, angle_speed, final_dmg);
      float level_cd = clampf(1.0f - 0.05f * (slot->level - 1), 0.7f, 1.0f);
      slot->cd_timer = w->cooldown * cooldown_scale * level_cd;
      continue;
    }

    if (weapon_is(w, "vampire_bite")) {
      float range = w->range;
      float range2 = range * range;
      int hits = 0;
      for (int e = 0; e < MAX_ENEMIES; e++) {
        if (!g->enemies[e].active) continue;
        Enemy *en = &g->enemies[e];
        if (en->spawn_invuln > 0.0f) continue;
        float ex = en->x - p->x;
        float ey = en->y - p->y;
        float d2 = ex * ex + ey * ey;
        if (d2 <= range2) {
          mark_enemy_hit(en);
          float final_dmg = player_roll_crit_damage(&stats, w, damage);
          final_dmg = player_apply_hit_mods(g, en, final_dmg);
          en->hp -= final_dmg;
          log_combatf(g, "hit %s with %s for %.1f", enemy_label(g, en), w->name, final_dmg);
          spawn_weapon_fx(g, 1, en->x, en->y, 0.0f, 0.6f, e);
          p->hp = clampf(p->hp + final_dmg * 0.15f, 0.0f, stats.max_hp);
          if (burn_chance > 0.0f && frandf() < burn_chance) {
            en->burn_timer = 4.0f;
            log_combatf(g, "burn applied to %s", enemy_label(g, en));
          }
          if (slow_chance > 0.0f && frandf() < slow_chance) {
            en->slow_timer = 2.5f;
            log_combatf(g, "slow applied to %s", enemy_label(g, en));
          }
          if (stun_chance > 0.0f && frandf() < stun_chance) {
            en->stun_timer = 0.6f;
            log_combatf(g, "stun applied to %s", enemy_label(g, en));
          }
          if (shred_chance > 0.0f && frandf() < shred_chance) {
            en->armor_shred_timer = 3.0f;
            log_combatf(g, "armor_shred applied to %s", enemy_label(g, en));
          }
          player_try_item_proc(g, e, &stats);
          hits++;
        }
      }
      float level_cd = clampf(1.0f - 0.05f * (slot->level - 1), 0.7f, 1.0f);
      slot->cd_timer = w->cooldown * cooldown_scale * level_cd;
      continue;
    }

    if (weapon_is(w, "daggers")) {
      float range = w->range;
      float range2 = range * range;
      int max_targets = 3 + (slot->level - 1);
      if (max_targets > 6) max_targets = 6;

      int targets[6] = {-1, -1, -1, -1, -1, -1};
      float dists[6] = {999999.0f, 999999.0f, 999999.0f, 999999.0f, 999999.0f, 999999.0f};

      for (int e = 0; e < MAX_ENEMIES; e++) {
        if (!g->enemies[e].active) continue;
        if (g->enemies[e].spawn_invuln > 0.0f) continue;
        float ex = g->enemies[e].x - p->x;
        float ey = g->enemies[e].y - p->y;
        float d2 = ex * ex + ey * ey;
        if (d2 > range2) continue;

        for (int t = 0; t < max_targets; t++) {
          if (d2 < dists[t]) {
            for (int s = max_targets - 1; s > t; s--) {
              targets[s] = targets[s-1];
              dists[s] = dists[s-1];
            }
            targets[t] = e;
            dists[t] = d2;
            break;
          }
        }
      }

      float speed = w->projectile_speed > 0 ? w->projectile_speed : 400.0f;
      for (int t = 0; t < max_targets; t++) {
        if (targets[t] < 0) continue;
        Enemy *en = &g->enemies[targets[t]];
        float dx = en->x - p->x;
        float dy = en->y - p->y;
        vec_norm(&dx, &dy);
        float angle = atan2f(dy, dx);

        spawn_weapon_fx(g, 2, p->x, p->y, angle, 0.25f, targets[t]);

        mark_enemy_hit(en);
        float final_dmg = player_roll_crit_damage(&stats, w, damage);
        final_dmg = player_apply_hit_mods(g, en, final_dmg);
        en->hp -= final_dmg;
        log_combatf(g, "hit %s with %s for %.1f", enemy_label(g, en), w->name, final_dmg);
        if (bleed_chance > 0.0f && frandf() < bleed_chance) {
          en->bleed_stacks = (en->bleed_stacks < 5) ? en->bleed_stacks + 1 : 5;
          en->bleed_timer = 4.0f;
          log_combatf(g, "bleed applied to %s", enemy_label(g, en));
        }
        if (burn_chance > 0.0f && frandf() < burn_chance) {
          en->burn_timer = 4.0f;
          log_combatf(g, "burn applied to %s", enemy_label(g, en));
        }
        if (slow_chance > 0.0f && frandf() < slow_chance) {
          en->slow_timer = 2.5f;
          log_combatf(g, "slow applied to %s", enemy_label(g, en));
        }
        if (stun_chance > 0.0f && frandf() < stun_chance) {
          en->stun_timer = 0.6f;
          log_combatf(g, "stun applied to %s", enemy_label(g, en));
        }
        if (shred_chance > 0.0f && frandf() < shred_chance) {
          en->armor_shred_timer = 3.0f;
          log_combatf(g, "armor_shred applied to %s", enemy_label(g, en));
        }
        player_try_item_proc(g, targets[t], &stats);
      }

      float level_cd = clampf(1.0f - 0.05f * (slot->level - 1), 0.7f, 1.0f);
      slot->cd_timer = w->cooldown * cooldown_scale * level_cd;
      continue;
    }

    if (weapon_is(w, "short_sword") || weapon_is(w, "longsword") || weapon_is(w, "axe") ||
        weapon_is(w, "greatsword") || weapon_is(w, "hammer") ||
        weapon_is(w, "fists")) {
      float range = w->range;
      float arc_deg = weapon_is(w, "axe") || weapon_is(w, "greatsword") || weapon_is(w, "hammer") ? 110.0f : 80.0f;
      float arc_cos = cosf(arc_deg * (3.14159f / 180.0f));
      for (int e = 0; e < MAX_ENEMIES; e++) {
        if (!g->enemies[e].active) continue;
        float ex = g->enemies[e].x - p->x;
        float ey = g->enemies[e].y - p->y;
        float d2 = ex * ex + ey * ey;
        if (d2 > range * range) continue;
        float len = sqrtf(d2);
        if (len < 0.001f) len = 0.001f;
        float nx = ex / len;
        float ny = ey / len;
        float dot = nx * tx + ny * ty;
        if (dot >= arc_cos) {
          Enemy *en = &g->enemies[e];
          if (en->spawn_invuln > 0.0f) continue;
          mark_enemy_hit(en);
          float final_dmg = player_roll_crit_damage(&stats, w, damage);
          final_dmg = player_apply_hit_mods(g, en, final_dmg);
          en->hp -= final_dmg;
          log_combatf(g, "hit %s with %s for %.1f", enemy_label(g, en), w->name, final_dmg);
          if (bleed_chance > 0.0f && frandf() < bleed_chance) {
            en->bleed_stacks = (en->bleed_stacks < 5) ? en->bleed_stacks + 1 : 5;
            en->bleed_timer = 4.0f;
            log_combatf(g, "bleed applied to %s", enemy_label(g, en));
          }
          if (burn_chance > 0.0f && frandf() < burn_chance) {
            en->burn_timer = 4.0f;
            log_combatf(g, "burn applied to %s", enemy_label(g, en));
          }
          if (slow_chance > 0.0f && frandf() < slow_chance) {
            en->slow_timer = 2.5f;
            log_combatf(g, "slow applied to %s", enemy_label(g, en));
          }
          if (stun_chance > 0.0f && frandf() < stun_chance) {
            en->stun_timer = 0.6f;
            log_combatf(g, "stun applied to %s", enemy_label(g, en));
          }
          if (shred_chance > 0.0f && frandf() < shred_chance) {
            en->armor_shred_timer = 3.0f;
            log_combatf(g, "armor_shred applied to %s", enemy_label(g, en));
          }
          player_try_item_proc(g, e, &stats);
        }
      }
      float level_cd = clampf(1.0f - 0.05f * (slot->level - 1), 0.7f, 1.0f);
      slot->cd_timer = w->cooldown * cooldown_scale * level_cd;
      continue;
    }

    if (weapon_is(w, "pistol") || weapon_is(w, "machine_gun") || weapon_is(w, "sniper") ||
        weapon_is(w, "crossbow") || weapon_is(w, "rocket_launcher") || weapon_is(w, "boomerang") ||
        weapon_is(w, "shotgun") || weapon_is(w, "wand") || weapon_is(w, "frost_wand") ||
        weapon_is(w, "fire_staff") || weapon_is(w, "orb_of_chaos")) {
      int pellets = (w->pellets > 0) ? w->pellets : 1;
      float spread = (w->spread > 0.0f) ? w->spread : 0.0f;
      float base_angle = atan2f(ty, tx);
      for (int s = 0; s < pellets; s++) {
        float angle = base_angle;
        if (pellets > 1) {
          float step = spread * (3.14159f / 180.0f) / (float)(pellets - 1);
          angle += step * (float)s - (step * (float)(pellets - 1) * 0.5f);
        }
        float vx = cosf(angle) * w->projectile_speed;
        float vy = sinf(angle) * w->projectile_speed;

        float final_dmg = player_roll_crit_damage(&stats, w, damage);
        spawn_bullet(g, p->x, p->y, vx, vy, final_dmg, w->pierce, w->homing, 1, slot->def_index,
                     bleed_chance, burn_chance, slow_chance, stun_chance, shred_chance);
      }
      float level_cd = clampf(1.0f - 0.05f * (slot->level - 1), 0.7f, 1.0f);
      slot->cd_timer = w->cooldown * cooldown_scale * level_cd;
      continue;
    }
  }
}
