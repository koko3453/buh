#ifndef BUH_RENDER_RENDER_H
#define BUH_RENDER_RENDER_H

#include <SDL.h>
#include <SDL_ttf.h>

#include "core/game.h"

void draw_circle(SDL_Renderer *r, int cx, int cy, int radius, SDL_Color color);
void draw_filled_circle(SDL_Renderer *r, int cx, int cy, int radius, SDL_Color color);
void draw_glow(SDL_Renderer *r, int cx, int cy, int radius, SDL_Color color);
void draw_diamond(SDL_Renderer *r, int cx, int cy, int size, SDL_Color color);
void draw_text(SDL_Renderer *r, TTF_Font *font, int x, int y, SDL_Color color, const char *text);
void draw_text_centered(SDL_Renderer *r, TTF_Font *font, int cx, int y, SDL_Color color, const char *text);
void draw_text_centered_outline(SDL_Renderer *r, TTF_Font *font, int cx, int y, SDL_Color text, SDL_Color outline, int thickness, const char *msg);
void draw_sword_orbit(Game *g, int offset_x, int offset_y, float cam_x, float cam_y);

void render_game(Game *g);

#endif
