#include "render/render.h"

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

