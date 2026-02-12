#include "game.h"

void draw_circle(SDL_Renderer *r, int cx, int cy, int radius, SDL_Color color) {
  const int segments = 48;
  SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
  for (int i = 0; i < segments; i++) {
    float a0 = (float)i / (float)segments * 6.2831853f;
    float a1 = (float)(i + 1) / (float)segments * 6.2831853f;
    int x0 = (int)(cx + cosf(a0) * radius);
    int y0 = (int)(cy + sinf(a0) * radius);
    int x1 = (int)(cx + cosf(a1) * radius);
    int y1 = (int)(cy + sinf(a1) * radius);
    SDL_RenderDrawLine(r, x0, y0, x1, y1);
  }
}

void draw_filled_circle(SDL_Renderer *r, int cx, int cy, int radius, SDL_Color color) {
  SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
  for (int dy = -radius; dy <= radius; dy++) {
    int dx = (int)sqrtf((float)(radius * radius - dy * dy));
    SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
  }
}

void draw_glow(SDL_Renderer *r, int cx, int cy, int radius, SDL_Color color) {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (int i = radius; i > 0; i -= 2) {
    int alpha = (int)(color.a * (float)i / (float)radius * 0.3f);
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, alpha);
    draw_circle(r, cx, cy, i, (SDL_Color){color.r, color.g, color.b, (Uint8)alpha});
  }
}

void draw_diamond(SDL_Renderer *r, int cx, int cy, int size, SDL_Color color) {
  SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
  for (int i = 0; i < size; i++) {
    SDL_RenderDrawLine(r, cx - i, cy - size + i, cx + i, cy - size + i);
    SDL_RenderDrawLine(r, cx - i, cy + size - i, cx + i, cy + size - i);
  }
}

void draw_text(SDL_Renderer *r, TTF_Font *font, int x, int y, SDL_Color color, const char *text) {
  if (!font) return;
  SDL_Surface *surf = TTF_RenderText_Blended(font, text, color);
  if (!surf) return;
  SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
  SDL_Rect dst = { x, y, surf->w, surf->h };
  SDL_FreeSurface(surf);
  SDL_RenderCopy(r, tex, NULL, &dst);
  SDL_DestroyTexture(tex);
}

void draw_text_centered(SDL_Renderer *r, TTF_Font *font, int cx, int y, SDL_Color color, const char *text) {
  if (!font) return;
  SDL_Surface *surf = TTF_RenderText_Blended(font, text, color);
  if (!surf) return;
  SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
  SDL_Rect dst = { cx - surf->w / 2, y, surf->w, surf->h };
  SDL_FreeSurface(surf);
  SDL_RenderCopy(r, tex, NULL, &dst);
  SDL_DestroyTexture(tex);
}

void draw_text_centered_outline(SDL_Renderer *r, TTF_Font *font, int cx, int y, SDL_Color text, SDL_Color outline, int thickness, const char *msg) {
  if (!font || !msg) return;
  for (int dy = -thickness; dy <= thickness; dy++) {
    for (int dx = -thickness; dx <= thickness; dx++) {
      if (dx == 0 && dy == 0) continue;
      draw_text_centered(r, font, cx + dx, y + dy, outline, msg);
    }
  }
  draw_text_centered(r, font, cx, y, text, msg);
}

void draw_sword_orbit(Game *g, int offset_x, int offset_y, float cam_x, float cam_y) {
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
  WeaponDef *w = &g->db.weapons[slot->def_index];
  int count = slot->level;
  if (count < 1) count = 1;
  float orbit_radius = w->range * SWORD_ORBIT_RANGE_SCALE;
  float base_angle = g->player.sword_orbit_angle;

  for (int s = 0; s < count; s++) {
    float angle = base_angle + (6.28318f * (float)s / (float)count);
    float angle_hit = angle + 1.570796f;
    float mid_x = g->player.x + cosf(angle) * (orbit_radius * 0.5f);
    float mid_y = g->player.y + sinf(angle) * (orbit_radius * 0.5f);
    int draw_x = (int)(offset_x + mid_x - cam_x);
    int draw_y = (int)(offset_y + mid_y - cam_y);
    float angle_deg = angle * (180.0f / 3.14159f) + 90.0f;

    if (g->tex_dagger) {
      int length = (int)orbit_radius;
      int width = SWORD_ORBIT_WIDTH;
      SDL_Rect dst = { draw_x - width / 2, draw_y - length / 2, width, length };
      SDL_RenderCopyEx(g->renderer, g->tex_dagger, NULL, &dst, angle_deg, NULL, SDL_FLIP_NONE);
    } else {
      draw_glow(g->renderer, draw_x, draw_y, 12, (SDL_Color){255, 220, 150, 120});
      draw_diamond(g->renderer, draw_x, draw_y, 10, (SDL_Color){255, 230, 180, 255});
    }
    {
      float half_w = 0.5f * (float)SWORD_ORBIT_WIDTH;
      float half_l = 0.5f * orbit_radius;
      float cos_a = cosf(angle_hit);
      float sin_a = sinf(angle_hit);
      float cx0 = (-half_w) * cos_a - (-half_l) * sin_a;
      float cy0 = (-half_w) * sin_a + (-half_l) * cos_a;
      float cx1 = (half_w) * cos_a - (-half_l) * sin_a;
      float cy1 = (half_w) * sin_a + (-half_l) * cos_a;
      float cx2 = (half_w) * cos_a - (half_l) * sin_a;
      float cy2 = (half_w) * sin_a + (half_l) * cos_a;
      float cx3 = (-half_w) * cos_a - (half_l) * sin_a;
      float cy3 = (-half_w) * sin_a + (half_l) * cos_a;
      int x0 = draw_x + (int)cx0;
      int y0 = draw_y + (int)cy0;
      int x1 = draw_x + (int)cx1;
      int y1 = draw_y + (int)cy1;
      int x2 = draw_x + (int)cx2;
      int y2 = draw_y + (int)cy2;
      int x3 = draw_x + (int)cx3;
      int y3 = draw_y + (int)cy3;
      SDL_SetRenderDrawColor(g->renderer, 255, 220, 120, 160);
      SDL_RenderDrawLine(g->renderer, x0, y0, x1, y1);
      SDL_RenderDrawLine(g->renderer, x1, y1, x2, y2);
      SDL_RenderDrawLine(g->renderer, x2, y2, x3, y3);
      SDL_RenderDrawLine(g->renderer, x3, y3, x0, y0);
    }
  }
}

#if 0
void render_game(Game *g) {
  if (!g || !g->renderer) return;
  SDL_SetRenderDrawColor(g->renderer, 20, 20, 20, 255);
  SDL_RenderClear(g->renderer);

  int offset_x = 0;
  int offset_y = 0;
  float cam_x = g->camera_x;
  float cam_y = g->camera_y;

  if (g->mode == MODE_BOSS_EVENT) {
    cam_x = g->boss_room_x - g->view_w * 0.5f;
    cam_y = g->boss_room_y - g->view_h * 0.5f;
    float max_cam_x = ARENA_W - g->view_w;
    float max_cam_y = ARENA_H - g->view_h;
    if (max_cam_x < 0.0f) max_cam_x = 0.0f;
    if (max_cam_y < 0.0f) max_cam_y = 0.0f;
    cam_x = clampf(cam_x, 0.0f, max_cam_x);
    cam_y = clampf(cam_y, 0.0f, max_cam_y);
  }

  if (g->tex_ground) {
    int tile = 64;
    int start_x = (int)(cam_x / tile) * tile - tile;
    int start_y = (int)(cam_y / tile) * tile - tile;
    for (int y = start_y; y < cam_y + g->view_h + tile; y += tile) {
      for (int x = start_x; x < cam_x + g->view_w + tile; x += tile) {
        SDL_Rect dst = { (int)(offset_x + x - cam_x), (int)(offset_y + y - cam_y), tile, tile };
        SDL_RenderCopy(g->renderer, g->tex_ground, NULL, &dst);
      }
    }
  } else {
    SDL_SetRenderDrawColor(g->renderer, 28, 28, 28, 255);
    SDL_RenderClear(g->renderer);
  }

  if (g->tex_wall) {
    SDL_Rect wall = { (int)(offset_x - cam_x), (int)(offset_y - cam_y), ARENA_W, ARENA_H };
    SDL_RenderCopy(g->renderer, g->tex_wall, NULL, &wall);
  }

  for (int i = 0; i < MAX_DROPS; i++) {
    if (!g->drops[i].active) continue;
    Drop *d = &g->drops[i];
    int px = (int)(offset_x + d->x - cam_x);
    int py = (int)(offset_y + d->y - cam_y);
    if (d->type == 0) {
      if (g->tex_exp_orb) {
        SDL_Rect dst = { px - 10, py - 10, 20, 20 };
        SDL_RenderCopy(g->renderer, g->tex_exp_orb, NULL, &dst);
      } else {
        draw_glow(g->renderer, px, py, 8, (SDL_Color){100, 200, 255, 120});
        draw_filled_circle(g->renderer, px, py, 4, (SDL_Color){120, 220, 255, 255});
      }
    } else if (d->type == 1) {
      if (g->tex_health_flask) {
        SDL_Rect dst = { px - 12, py - 12, 24, 24 };
        SDL_RenderCopy(g->renderer, g->tex_health_flask, NULL, &dst);
      } else {
        draw_glow(g->renderer, px, py, 12, (SDL_Color){255, 100, 100, 80});
        draw_filled_circle(g->renderer, px, py, 6, (SDL_Color){255, 120, 120, 255});
      }
    } else if (d->type == 2) {
      draw_glow(g->renderer, px, py, 16, (SDL_Color){255, 200, 90, 120});
      draw_diamond(g->renderer, px, py, 8, (SDL_Color){255, 200, 120, 255});
    }
  }

  for (int i = 0; i < MAX_PUDDLES; i++) {
    if (!g->puddles[i].active) continue;
    int px = (int)(offset_x + g->puddles[i].x - cam_x);
    int py = (int)(offset_y + g->puddles[i].y - cam_y);
    int radius = (int)g->puddles[i].radius;
    SDL_Color core = { 60, 200, 120, 120 };
    SDL_Color glow = { 40, 160, 90, 80 };
    if (g->tex_alchemist_puddle) {
      SDL_Rect dst = { px - radius, py - radius, radius * 2, radius * 2 };
      SDL_SetTextureAlphaMod(g->tex_alchemist_puddle, 200);
      SDL_RenderCopy(g->renderer, g->tex_alchemist_puddle, NULL, &dst);
      SDL_SetTextureAlphaMod(g->tex_alchemist_puddle, 255);
    } else {
      draw_filled_circle(g->renderer, px, py, radius, core);
    }
    draw_glow(g->renderer, px, py, radius + 6, glow);
  }

  int px = (int)(offset_x + g->player.x - cam_x);
  int py = (int)(offset_y + g->player.y - cam_y);
  int player_size = 90;

  for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
    if (!g->player.weapons[i].active) continue;
    WeaponDef *w = &g->db.weapons[g->player.weapons[i].def_index];
    if (weapon_is(w, "lightning_zone")) {
      float range = w->range * (1.0f + 0.1f * (g->player.weapons[i].level - 1));
      float cd_ratio = g->player.weapons[i].cd_timer / w->cooldown;
      int alpha = (int)(40 + (1.0f - cd_ratio) * 60);
      SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
      SDL_Color zone_color = { 100, 150, 255, (Uint8)alpha };
      draw_filled_circle(g->renderer, px, py, (int)range, zone_color);
      SDL_Color border_color = { 150, 200, 255, (Uint8)(alpha + 40) };
      draw_circle(g->renderer, px, py, (int)range, border_color);
      if (cd_ratio > 0.95f) {
        SDL_Color flash = { 200, 220, 255, 120 };
        draw_filled_circle(g->renderer, px, py, (int)range, flash);
        if (g->tex_lightning_zone) {
          int sprite_size = (int)(range * 2.0f);
          SDL_Rect dst = { px - sprite_size/2, py - sprite_size/2, sprite_size, sprite_size };
          SDL_SetTextureAlphaMod(g->tex_lightning_zone, 220);
          SDL_RenderCopy(g->renderer, g->tex_lightning_zone, NULL, &dst);
          SDL_SetTextureAlphaMod(g->tex_lightning_zone, 255);
        }
      }
    }
  }

  SDL_Texture *player_tex = g->tex_player_front;
  SDL_RendererFlip flip = SDL_FLIP_NONE;
  float mdx = g->player.move_dir_x;
  float mdy = g->player.move_dir_y;
  if (g->player.is_moving) {
    if (fabsf(mdy) >= fabsf(mdx)) {
      if (mdy < 0.0f && g->tex_player_back) player_tex = g->tex_player_back;
      else if (g->tex_player_front) player_tex = g->tex_player_front;
    } else {
      if (mdx < 0.0f) player_tex = g->tex_player_left ? g->tex_player_left : g->tex_player_right;
      else player_tex = g->tex_player_right ? g->tex_player_right : g->tex_player_left;
    }
  } else {
    if (g->tex_player_front) player_tex = g->tex_player_front;
  }

  if (player_tex) {
    SDL_Rect dst = { px - player_size/2, py - player_size/2, player_size, player_size };
    SDL_RenderCopyEx(g->renderer, player_tex, NULL, &dst, 0.0, NULL, flip);
  } else {
    draw_glow(g->renderer, px, py, 25, (SDL_Color){255, 200, 80, 100});
    draw_filled_circle(g->renderer, px, py, 12, (SDL_Color){255, 200, 80, 255});
    draw_circle(g->renderer, px, py, 12, (SDL_Color){255, 230, 150, 255});
  }

  draw_sword_orbit(g, offset_x, offset_y, cam_x, cam_y);

  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!g->enemies[i].active) continue;
    Enemy *en = &g->enemies[i];
    EnemyDef *def = &g->db.enemies[en->def_index];
    int ex = (int)(offset_x + en->x - cam_x);
    int ey = (int)(offset_y + en->y - cam_y);
    int size = 36;
    int alpha = 200;
    int hit_flash = 0;
    if (en->hit_timer > 0.0f) {
      float now = (float)SDL_GetTicks() / 1000.0f;
      if (now - en->hit_timer < 0.1f) hit_flash = 1;
    }

    if (g->tex_enemy) {
      float move_dx = 0.0f;
      float move_dy = 0.0f;
      SDL_RendererFlip enemy_flip = SDL_FLIP_NONE;
      if (strcmp(def->role, "turret") != 0) {
        if (en->charge_time > 0.0f) {
          move_dx = en->vx;
          move_dy = en->vy;
        } else if (en->stun_timer <= 0.0f) {
          move_dx = g->player.x - en->x;
          move_dy = g->player.y - en->y;
        }
        if (fabsf(move_dx) > 0.01f || fabsf(move_dy) > 0.01f) {
          if (move_dx < 0.0f) enemy_flip = SDL_FLIP_HORIZONTAL;
        }
      }
      if (hit_flash) {
        SDL_SetTextureColorMod(g->tex_enemy, 255, 120, 120);
      } else if (en->slow_timer > 0.0f) {
        SDL_SetTextureColorMod(g->tex_enemy, 150, 180, 255);
      } else {
        SDL_SetTextureColorMod(g->tex_enemy, 255, 255, 255);
      }
      SDL_Rect dst = { ex - size/2, ey - size/2, size, size };
      SDL_RenderCopyEx(g->renderer, g->tex_enemy, NULL, &dst, 0.0, NULL, enemy_flip);
    } else {
      if (hit_flash) {
        draw_filled_circle(g->renderer, ex, ey, size/2, (SDL_Color){220, 100, 100, 255});
      } else if (en->slow_timer > 0.0f) {
        draw_filled_circle(g->renderer, ex, ey, size/2, (SDL_Color){100, 150, 200, 255});
      } else {
        draw_filled_circle(g->renderer, ex, ey, size/2, (SDL_Color){100, 200, 100, 255});
      }
    }
    draw_glow(g->renderer, ex, ey, size/2 + 6, (SDL_Color){255, 80, 80, alpha});
  }

  if (g->mode == MODE_BOSS_EVENT && g->boss.active) {
    const BossDef *def = &g_boss_defs[g->boss.def_index];
    int bx = (int)(offset_x + g->boss.x - cam_x);
    int by = (int)(offset_y + g->boss.y - cam_y);
    int br = (int)def->radius;
    draw_glow(g->renderer, bx, by, br + 14, (SDL_Color){255, 120, 60, 140});
    draw_filled_circle(g->renderer, bx, by, br + 6, (SDL_Color){160, 60, 40, 255});
    draw_circle(g->renderer, bx, by, br + 6, (SDL_Color){240, 200, 120, 255});
    if (g->boss.hazard_timer > 0.0f) {
      draw_glow(g->renderer, bx, by, (int)def->hazard_safe_radius + 12, (SDL_Color){80, 220, 140, 120});
      draw_circle(g->renderer, bx, by, (int)def->hazard_safe_radius, (SDL_Color){120, 255, 170, 220});
    }
  }

  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!g->bullets[i].active) continue;
    Bullet *b = &g->bullets[i];
    int dx = (int)(offset_x + b->x - cam_x);
    int dy = (int)(offset_y + b->y - cam_y);
    if (b->from_player) {
      draw_glow(g->renderer, dx, dy, 6, (SDL_Color){80, 180, 255, 80});
      draw_filled_circle(g->renderer, dx, dy, 3, (SDL_Color){100, 200, 255, 255});
    } else {
      draw_glow(g->renderer, dx, dy, 12, (SDL_Color){255, 80, 120, 100});
      draw_diamond(g->renderer, dx, dy, 6, (SDL_Color){255, 100, 130, 255});
    }
  }

  for (int i = 0; i < MAX_WEAPON_FX; i++) {
    if (!g->weapon_fx[i].active) continue;
    WeaponFX *fx = &g->weapon_fx[i];
    if (fx->type == 0) {
      float sx = fx->x + cosf(fx->angle) * fx->radius;
      float sy = fx->y + sinf(fx->angle) * fx->radius;
      int draw_x = (int)(offset_x + sx - cam_x);
      int draw_y = (int)(offset_y + sy - cam_y);
      int scythe_size = 36;
      float alpha = 200.0f;
      if (g->tex_scythe) {
        SDL_Rect dst = { draw_x - scythe_size/2, draw_y - scythe_size/2, scythe_size, scythe_size };
        SDL_SetTextureAlphaMod(g->tex_scythe, (Uint8)alpha);
        SDL_RenderCopyEx(g->renderer, g->tex_scythe, NULL, &dst, fx->angle * (180.0f / 3.14159f) + 90, NULL, SDL_FLIP_NONE);
        SDL_SetTextureAlphaMod(g->tex_scythe, 255);
      } else {
        SDL_RenderDrawLine(g->renderer, draw_x - 10, draw_y - 10, draw_x + 10, draw_y + 10);
        draw_filled_circle(g->renderer, draw_x, draw_y, 6, (SDL_Color){220, 100, 240, (Uint8)alpha});
      }
    } else if (fx->type == 1) {
      int ex = (int)(offset_x + fx->x - cam_x);
      int ey = (int)(offset_y + fx->y - cam_y);
      int size = 32;
      float alpha = 180.0f;
      if (g->tex_bite) {
        SDL_Rect dst = { ex - size/2, ey - size/2, size, size };
        SDL_SetTextureAlphaMod(g->tex_bite, (Uint8)alpha);
        SDL_RenderCopy(g->renderer, g->tex_bite, NULL, &dst);
        SDL_SetTextureAlphaMod(g->tex_bite, 255);
      } else {
        draw_glow(g->renderer, ex, ey, size/2, (SDL_Color){180, 0, 40, (Uint8)(alpha * 0.6f)});
        draw_filled_circle(g->renderer, ex - 12, ey - 8, 6, (SDL_Color){220, 20, 60, (Uint8)alpha});
        draw_filled_circle(g->renderer, ex + 12, ey - 8, 6, (SDL_Color){220, 20, 60, (Uint8)alpha});
      }
    } else if (fx->type == 2) {
      float dx = fx->x + cosf(fx->angle) * 40.0f * fx->timer;
      float dy = fx->y + sinf(fx->angle) * 40.0f * fx->timer;
      int draw_x = (int)(offset_x + dx - cam_x);
      int draw_y = (int)(offset_y + dy - cam_y);
      int size = 26;
      float alpha = 200.0f;
      if (g->tex_dagger) {
        SDL_Rect dst = { draw_x - size/2, draw_y - size/2, size, size };
        SDL_SetTextureAlphaMod(g->tex_dagger, (Uint8)alpha);
        SDL_RenderCopyEx(g->renderer, g->tex_dagger, NULL, &dst, fx->angle * (180.0f / 3.14159f) + 90, NULL, SDL_FLIP_NONE);
        SDL_SetTextureAlphaMod(g->tex_dagger, 255);
      } else {
        float tip_x = draw_x + cosf(fx->angle) * 10.0f;
        float tip_y = draw_y + sinf(fx->angle) * 10.0f;
        draw_filled_circle(g->renderer, (int)tip_x, (int)tip_y, 3, (SDL_Color){200, 200, 220, (Uint8)alpha});
      }
    }
  }

  SDL_Color text = { 210, 220, 230, 255 };
  char buf[256];
  snprintf(buf, sizeof(buf), "Kills: %d", g->kills);
  draw_text(g->renderer, g->font, 20, 20, text, buf);
  snprintf(buf, sizeof(buf), "Level: %d", g->level);
  draw_text(g->renderer, g->font, 20, 44, text, buf);
  snprintf(buf, sizeof(buf), "XP: %d / %d", g->xp, g->xp_to_next);
  draw_text(g->renderer, g->font, 20, 64, text, buf);
  snprintf(buf, sizeof(buf), "HP: %.0f", g->player.hp);
  draw_text(g->renderer, g->font, 20, 88, text, buf);
  draw_text(g->renderer, g->font, 20, 108, text, "TAB/P pause  F1 spawn  F2/F3 speed  F4 range");

  if (g->mode == MODE_BOSS_EVENT) {
    float time_pct = 0.0f;
    if (g->boss_timer_max > 0.0f) {
      time_pct = clampf(g->boss_timer / g->boss_timer_max, 0.0f, 1.0f);
    }
    int time_bar_w = 200;
    int time_bar_h = 8;
    int time_bar_x = g->window_w - time_bar_w - 20;
    int time_bar_y = 10;
    SDL_Rect time_bg = { time_bar_x, time_bar_y, time_bar_w, time_bar_h };
    SDL_Rect time_fg = { time_bar_x, time_bar_y, (int)(time_bar_w * time_pct), time_bar_h };
    SDL_SetRenderDrawColor(g->renderer, 20, 20, 20, 220);
    SDL_RenderFillRect(g->renderer, &time_bg);
    SDL_SetRenderDrawColor(g->renderer, 80, 160, 230, 255);
    SDL_RenderFillRect(g->renderer, &time_fg);
    SDL_SetRenderDrawColor(g->renderer, 80, 100, 130, 255);
    SDL_RenderDrawRect(g->renderer, &time_bg);
    int mins = (int)(g->boss_timer / 60.0f);
    int secs = (int)g->boss_timer % 60;
    snprintf(buf, sizeof(buf), "Boss Time: %d:%02d", mins, secs);
    draw_text(g->renderer, g->font, g->window_w - 220, 22, text, buf);

    if (g->boss.active) {
      const BossDef *def = &g_boss_defs[g->boss.def_index];
      float hp_pct = clampf(g->boss.hp / g->boss.max_hp, 0.0f, 1.0f);
      int bar_w = 200;
      SDL_Rect bar_bg = { g->window_w - bar_w - 20, 36, bar_w, 8 };
      SDL_Rect bar_fg = { g->window_w - bar_w - 20, 36, (int)(bar_w * hp_pct), 8 };
      SDL_SetRenderDrawColor(g->renderer, 20, 20, 20, 220);
      SDL_RenderFillRect(g->renderer, &bar_bg);
      SDL_SetRenderDrawColor(g->renderer, 240, 120, 80, 255);
      SDL_RenderFillRect(g->renderer, &bar_fg);
      draw_text(g->renderer, g->font, g->window_w - bar_w - 20, 48, text, def->name);
    }

    if (g->boss_countdown_timer > 0.0f) {
      snprintf(buf, sizeof(buf), "Boss in %.1f", g->boss_countdown_timer);
      draw_text_centered(g->renderer, g->font_title_big ? g->font_title_big : g->font_title,
                         g->window_w / 2, 60, (SDL_Color){255, 180, 120, 255}, buf);
    }
  }

  if (g->mode == MODE_START) {
    SDL_Color title = { 255, 230, 170, 255 };
    SDL_Color label = { 220, 220, 230, 255 };
    draw_text_centered(g->renderer, g->font_title_big ? g->font_title_big : g->font_title,
                       g->window_w / 2, 20, title, "BUH");
    draw_text(g->renderer, g->font, 20, 140, label, "Choose your champion:");

    int split_x = (g->window_w * 2) / 3;
    int margin = 25;
    int cols = 4;
    int card_w = (split_x - margin * 2 - (cols - 1) * 20) / cols;
    int card_h = card_w + 60;
    int start_y = 170;

    for (int i = 0; i < g->choice_count; i++) {
      LevelUpChoice *choice = &g->choices[i];
      int col = i % cols;
      int row = i / cols;
      int x = margin + col * (card_w + 20);
      int y = (int)(start_y + row * (card_h + 12) - g->start_scroll);
      SDL_Rect r = { x, y, card_w, card_h };
      choice->rect = r;

      SDL_Color bg = { 35, 40, 50, 220 };
      SDL_SetRenderDrawColor(g->renderer, bg.r, bg.g, bg.b, bg.a);
      SDL_RenderFillRect(g->renderer, &r);
      SDL_SetRenderDrawColor(g->renderer, 80, 90, 110, 255);
      SDL_RenderDrawRect(g->renderer, &r);

      CharacterDef *c = &g->db.characters[choice->index];
      SDL_Color name = { 230, 230, 240, 255 };
      SDL_Color stat = { 180, 190, 200, 255 };
      draw_text_centered(g->renderer, g->font_title, x + card_w / 2, y + 8, name, c->name);

      if (g->tex_portraits[choice->index]) {
        SDL_Rect pdst = { x + 10, y + 30, card_w - 20, card_w - 20 };
        SDL_RenderCopy(g->renderer, g->tex_portraits[choice->index], NULL, &pdst);
      }

      draw_text(g->renderer, g->font, x + 10, y + card_h - 28, stat, c->rule);
    }
  }

  if (g->mode == MODE_LEVELUP || (g->mode == MODE_PAUSE && g->pause_return_mode == MODE_LEVELUP)) {
    SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g->renderer, 0, 0, 0, 170);
    SDL_Rect cover = { 0, 0, g->window_w, g->window_h };
    SDL_RenderFillRect(g->renderer, &cover);

    SDL_Color white = { 240, 240, 240, 255 };
    SDL_Color gold = { 255, 215, 100, 255 };
    SDL_Color outline = { 20, 20, 20, 255 };
    int w = g->window_w;
    int h = g->window_h;
    int panel_w = 620;
    int panel_h = 380;
    int panel_x = (w - panel_w) / 2;
    int panel_y = (h - panel_h) / 2;
    SDL_Rect panel = { panel_x, panel_y, panel_w, panel_h };
    SDL_SetRenderDrawColor(g->renderer, 35, 40, 50, 230);
    SDL_RenderFillRect(g->renderer, &panel);
    SDL_SetRenderDrawColor(g->renderer, 90, 100, 120, 255);
    SDL_RenderDrawRect(g->renderer, &panel);

    char lvl_str[64];
    snprintf(lvl_str, sizeof(lvl_str), "LEVEL %d", g->level);
    draw_text(g->renderer, g->font, panel_x + 20, panel_y + 20, gold, lvl_str);
    draw_text(g->renderer, g->font, panel_x + 20, panel_y + 45, white, "Choose one:");

    int choice_w = 170;
    int choice_h = 90;
    int spacing = 10;
    int start_x = panel_x + 20;
    int start_y = panel_y + 80;

    for (int i = 0; i < g->choice_count; i++) {
      LevelUpChoice *choice = &g->choices[i];
      int cx = start_x + (i % 3) * (choice_w + spacing);
      int cy = start_y + (i / 3) * (choice_h + spacing);
      SDL_Rect r = { cx, cy, choice_w, choice_h };
      choice->rect = r;

      SDL_Color bg = { 45, 50, 65, 230 };
      SDL_SetRenderDrawColor(g->renderer, bg.r, bg.g, bg.b, bg.a);
      SDL_RenderFillRect(g->renderer, &r);
      SDL_SetRenderDrawColor(g->renderer, 100, 110, 130, 255);
      SDL_RenderDrawRect(g->renderer, &r);

      if (choice->type == 0) {
        ItemDef *it = &g->db.items[choice->index];
        SDL_Color tag_color = { 120, 220, 160, 255 };
        SDL_Color statline = { 200, 210, 220, 255 };
        draw_text_centered_outline(g->renderer, g->font, cx + choice_w / 2, cy + 2, tag_color, outline, 1, "ITEM");
        draw_text_centered_outline(g->renderer, g->font_title, cx + choice_w / 2, cy + 28, white, outline, 2, it->name);
        draw_text_centered_outline(g->renderer, g->font, cx + choice_w / 2, cy + 58, statline, outline, 1, it->desc);
      } else if (choice->type == 1) {
        WeaponDef *wdef = &g->db.weapons[choice->index];
        SDL_Color tag_color = { 120, 170, 255, 255 };
        SDL_Color statline = { 200, 210, 220, 255 };
        draw_text_centered_outline(g->renderer, g->font, cx + choice_w / 2, cy + 2, tag_color, outline, 1, "WEAPON");
        draw_text_centered_outline(g->renderer, g->font_title, cx + choice_w / 2, cy + 28, white, outline, 2, wdef->name);
        char statbuf[64];
        snprintf(statbuf, sizeof(statbuf), "DMG %.0f  CD %.2f", wdef->damage, wdef->cooldown);
        draw_text_centered_outline(g->renderer, g->font, cx + choice_w / 2, cy + 58, statline, outline, 1, statbuf);
      }
    }
  }

  if (g->mode == MODE_PAUSE && g->pause_return_mode != MODE_LEVELUP) {
    SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g->renderer, 0, 0, 0, 180);
    SDL_Rect cover = { 0, 0, g->window_w, g->window_h };
    SDL_RenderFillRect(g->renderer, &cover);
    draw_text_centered(g->renderer, g->font_title_big ? g->font_title_big : g->font_title,
                       g->window_w / 2, 180, (SDL_Color){255, 215, 100, 255}, "PAUSED");
  }

  if (g->mode == MODE_GAMEOVER) {
    SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g->renderer, 0, 0, 0, 190);
    SDL_Rect cover = { 0, 0, g->window_w, g->window_h };
    SDL_RenderFillRect(g->renderer, &cover);
    draw_text_centered(g->renderer, g->font_title_big ? g->font_title_big : g->font_title,
                       g->window_w / 2, 160, (SDL_Color){255, 120, 120, 255}, "GAME OVER");
    draw_text_centered(g->renderer, g->font_title, g->window_w / 2, 210, (SDL_Color){230, 230, 230, 255},
                       "Press R to restart");
  }

  SDL_RenderPresent(g->renderer);
}
#endif
