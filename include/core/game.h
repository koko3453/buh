#ifndef BUH_CORE_GAME_H
#define BUH_CORE_GAME_H

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#include "core/config.h"
#include "core/types.h"

typedef struct {
  int type; /* 0 item, 1 weapon */
  int index;
  SDL_Rect rect;
} LevelUpChoice;

typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Cursor *cursor;
  TTF_Font *font;
  TTF_Font *font_title;
  TTF_Font *font_title_big;
  int window_w;
  int window_h;
  int view_w;
  int view_h;
  SDL_Texture *tex_ground;
  SDL_Texture *tex_wall;
  SDL_Texture *tex_health_flask;
  SDL_Texture *tex_enemy;
  SDL_Texture *tex_enemy_eye;
  SDL_Texture *tex_enemy_ghost;
  SDL_Texture *tex_boss;
  SDL_Texture *tex_player;
  SDL_Texture *tex_player_front;
  SDL_Texture *tex_player_back;
  SDL_Texture *tex_player_right;
  SDL_Texture *tex_player_left;
  SDL_Texture *tex_enemy_bolt;
  SDL_Texture *tex_lightning_zone;
  SDL_Texture *tex_laser_beam;
  SDL_Texture *tex_portraits[MAX_CHARACTERS];
  SDL_Texture *tex_scythe;
  SDL_Texture *tex_bite;
  SDL_Texture *tex_dagger;
  SDL_Texture *tex_alchemist_puddle;
  SDL_Texture *tex_exp_orb;
  SDL_Texture *tex_orb_common;
  SDL_Texture *tex_orb_uncommon;
  SDL_Texture *tex_orb_rare;
  SDL_Texture *tex_orb_epic;
  SDL_Texture *tex_orb_legendary;
  int running;
  GameMode mode;
  GameMode pause_return_mode;
  float time_scale;
  int debug_show_range;
  int debug_show_items;
  float ultimate_cd;
  int start_page;
  float start_scroll;
  int selected_character;
  int rerolls;
  int high_roll_used;
  float levelup_fade;
  int levelup_chosen;
  int levelup_selected[MAX_LEVELUP_CHOICES];
  int levelup_selected_count;

  Database db;
  Player player;
  Enemy enemies[MAX_ENEMIES];
  Bullet bullets[MAX_BULLETS];
  Drop drops[MAX_DROPS];
  Puddle puddles[MAX_PUDDLES];
  WeaponFX weapon_fx[MAX_WEAPON_FX];
  Boss boss;
  int boss_def_index;
  float boss_event_cd;
  float boss_countdown_timer;
  float boss_timer;
  float boss_timer_max;
  float boss_room_x;
  float boss_room_y;
  WaveSnapshot wave_snapshot;
  SkillTreeProgress skill_tree;
  int skill_tree_points_earned_last;
  int skill_tree_run_awarded;
  int show_skill_tree;
  int skill_tree_edit_mode;
  int skill_tree_drag_index;
  float skill_tree_drag_off_x;
  float skill_tree_drag_off_y;
  SDL_Rect skill_tree_button;
  SDL_Rect skill_tree_close_button;
  SDL_Rect skill_tree_debug_button;
  SDL_Rect skill_tree_item_rects[MAX_SKILL_TREE_UPGRADES];
  float skill_tree_xp_mult;
  float skill_tree_spawn_scale;
  float skill_tree_damage_bonus;
  float skill_tree_armor_bonus;

  float spawn_timer;
  int kills;
  int xp;
  int level;
  int xp_to_next;
  float game_time;
  int last_item_index;
  float item_popup_timer;
  char item_popup_name[64];

  LevelUpChoice choices[MAX_LEVELUP_CHOICES];
  int choice_count;
  SDL_Rect reroll_button;
  SDL_Rect highroll_button;
  SDL_Rect restart_button;
  int scythe_id_counter;
  float camera_x;
  float camera_y;
} Game;

extern const BossDef g_boss_defs[];
int boss_def_count(void);

extern FILE *g_log;
extern int g_log_combat;
extern FILE *g_combat_log;

void log_line(const char *msg);
void log_linef(const char *fmt, ...);
void log_combatf(Game *g, const char *fmt, ...);

SDL_Texture *load_texture_fallback(SDL_Renderer *r, const char *path);
LONG WINAPI crash_handler(EXCEPTION_POINTERS *e);

float clampf(float v, float a, float b);
float frandf(void);
void vec_norm(float *x, float *y);
float damage_after_armor(float dmg, float armor);

int weapon_is(const WeaponDef *w, const char *id);
WeaponStatusChances weapon_status_chances(const WeaponDef *w);

void skill_tree_progress_init(Game *g);
void skill_tree_progress_save(Game *g);
void skill_tree_apply_run_mods(Game *g);
int skill_tree_try_purchase_upgrade(Game *g, int upgrade_index);

void stats_clear(Stats *s);
void stats_add(Stats *a, Stats *b);
void stats_scale(Stats *s, float mul);

float player_slow_on_hit(Player *p, Database *db);
float player_burn_on_hit(Player *p, Database *db);
float player_slow_aura(Player *p, Database *db);
float player_burn_aura(Player *p, Database *db);
float player_thorns_percent(Player *p, Database *db);
float player_lifesteal_on_kill(Player *p, Database *db);
float player_slow_bonus_damage(Player *p, Database *db);
float player_legendary_amp(Player *p, Database *db);
float player_hp_regen_amp(Player *p, Database *db);
float player_xp_kill_chance(Player *p, Database *db);
Stats player_total_stats(Player *p, Database *db);
float player_roll_crit_damage(Stats *stats, WeaponDef *w, float dmg);
float player_apply_hit_mods(Game *g, Enemy *en, float dmg);
void player_try_item_proc(Game *g, int enemy_idx, Stats *stats);

void spawn_drop(Game *g, float x, float y, int type, float value);
void spawn_chest(Game *g, float x, float y);
void mark_enemy_hit(Enemy *en);

void update_window_view(Game *g);
void game_reset(Game *g);
void wave_start(Game *g);
void start_boss_event(Game *g);
void build_start_page(Game *g);
float start_scroll_max(Game *g);
void build_levelup_choices(Game *g);
void handle_levelup_click(Game *g, int mx, int my);
void toggle_pause(Game *g);
void activate_ultimate(Game *g);
void update_game(Game *g, float dt);
void update_boss_event(Game *g, float dt);

#endif

