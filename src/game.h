#ifndef BUH_GAME_H
#define BUH_GAME_H

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

#define MAX_WEAPONS 32
#define MAX_ITEMS 128
#define MAX_ENEMIES 512
#define MAX_BULLETS 512
#define MAX_DROPS 256
#define MAX_WEAPON_SLOTS 6
#define MAX_WEAPON_LEVEL 4
#define MAX_SHOP_SLOTS 12
#define MAX_PUDDLES 64

#define WINDOW_W 1280
#define WINDOW_H 720
#define ARENA_W 4000
#define ARENA_H 4000
#define VIEW_W 1180
#define VIEW_H 640
#define ITEM_POPUP_DURATION 4.5f
#define SWORD_ORBIT_SPEED 2.4f
#define SWORD_ORBIT_RANGE_SCALE 1.25f
#define SWORD_ORBIT_WIDTH 22
#define SWORD_ORBIT_HIT_COOLDOWN 0.2f

#define MAX_LEVELUP_CHOICES 16
#define MAX_WEAPON_FX 16
#define MAX_CHARACTERS 16
#define MAX_META_UPGRADES 5

typedef struct {
  float damage;
  float max_hp;
  float move_speed;
  float attack_speed;
  float armor;
  float dodge;
  float crit_chance;
  float crit_damage;
  float cooldown_reduction;
  float xp_magnet;
  float hp_regen;
} Stats;

typedef struct {
  float bleed;
  float burn;
  float slow;
  float stun;
  float shred;
} WeaponStatusChances;

typedef struct {
  int points;
  int total_points;
  int upgrades[MAX_META_UPGRADES];
} MetaProgress;

typedef struct WeaponDef {
  char id[32];
  char name[32];
  char type[16];
  char rarity[16];
  float cooldown;
  float damage;
  float range;
  float projectile_speed;
  float crit_multiplier;
  int pierce;
  int pellets;
  float spread;
  int homing;
  int scale_damage;
  int scale_attack_speed;
  int scale_range;
  int scale_crit;
} WeaponDef;

typedef struct {
  char id[32];
  char name[32];
  char rarity[16];
  char desc[64];
  Stats stats;
  int has_proc;
  int proc_bounces;
  float proc_chance;
  float proc_damage;
  float proc_range;
  float slow_on_hit;
  float slow_aura;
  float burn_on_hit;
  float burn_aura;
  float thorns_percent;
  float lifesteal_on_kill;
  float rarity_bias;
  float slow_bonus_damage;
  float legendary_amp;
  float hp_regen_amp;
  float xp_kill_chance;
} ItemDef;

typedef struct {
  char id[32];
  char name[32];
  char role[16];
  float hp;
  float speed;
  float damage;
  float cooldown;
  float projectile_speed;
  float charge_speed;
  float charge_cooldown;
  float explode_radius;
} EnemyDef;

typedef struct {
  char id[32];
  char name[32];
  float hp;
  float speed;
  float damage;
  float radius;
  float attack_cooldown;
  float beam_rot_speed;
  float beam_length;
  float beam_dps;
  float beam_width;
  float wave_cooldown;
  int wave_bullets;
  float wave_speed;
  float slam_cooldown;
  float slam_radius;
  float slam_damage;
  float hazard_cooldown;
  float hazard_duration;
  float hazard_dps;
  float hazard_safe_radius;
} BossDef;

typedef struct {
  char id[32];
  char name[32];
  char portrait[48];
  char weapon[32];
  Stats stats;
  char rule[24];
  char ultimate[32];
} CharacterDef;

typedef struct {
  int active;
  int def_index;
  int level;
  float cd_timer;
} WeaponSlot;

typedef struct {
  float x;
  float y;
  float hp;
  Stats base;
  Stats bonus;
  WeaponSlot weapons[MAX_WEAPON_SLOTS];
  int passive_items[MAX_ITEMS];
  int passive_count;
  float ultimate_move_to_as_timer;
  float move_dir_x;
  float move_dir_y;
  int is_moving;
  float scythe_throw_angle;
  float sword_orbit_angle;
} Player;

typedef struct {
  int active;
  int def_index;
  float x;
  float y;
  float hp;
  float max_hp;
  float attack_timer;
  float beam_angle;
  float wave_cd;
  float slam_cd;
  float hazard_timer;
  float hazard_cd;
  float sword_hit_cd;
  float safe_x[3];
  float safe_y[3];
} Boss;

typedef struct {
  float burn_timer;
  float bleed_timer;
  int bleed_stacks;
  float slow_timer;
  float stun_timer;
  float armor_shred_timer;
} EnemyDebuffs;

typedef struct {
  int active;
  int def_index;
  float x;
  float y;
  float vx;
  float vy;
  float hp;
  float max_hp;
  float spawn_invuln;
  float cooldown;
  float charge_timer;
  float charge_time;
  EnemyDebuffs debuffs;
  float hit_timer;
  float sword_hit_cd;
  int scythe_hit_id;
} Enemy;

typedef struct {
  int active;
  int from_player;
  float x;
  float y;
  float vx;
  float vy;
  float damage;
  float radius;
  float lifetime;
  int pierce;
  int homing;
  int weapon_index;
  float bleed_chance;
  float burn_chance;
  float slow_chance;
  float stun_chance;
  float armor_shred_chance;
} Bullet;

typedef struct {
  int active;
  int type; /* 0 xp orb, 1 heal, 2 chest */
  float x;
  float y;
  float value;
  float ttl;
  float magnet_speed;
  int magnetized;
} Drop;

typedef struct {
  int active;
  float x;
  float y;
  float radius;
  float dps;
  float ttl;
  float log_timer;
} Puddle;

typedef struct {
  int type; /* 0 item, 1 weapon */
  int index;
  SDL_Rect rect;
} LevelUpChoice;

typedef struct {
  int active;
  int type;       /* 0=scythe throw, 1=bite on enemy, 2=dagger projectile */
  float x, y;
  float angle;
  float timer;
  float duration;
  int target_enemy;
  float radius;
  float radial_speed;
  float angle_speed;
  float damage;
  int scythe_id;
  int scythe_hit_boss;
  float start_angle;
} WeaponFX;

typedef enum {
  MODE_START,
  MODE_WAVE,
  MODE_LEVELUP,
  MODE_PAUSE,
  MODE_BOSS_EVENT,
  MODE_GAMEOVER
} GameMode;

typedef struct {
  int valid;
  GameMode mode;
  Player player;
  Enemy enemies[MAX_ENEMIES];
  Bullet bullets[MAX_BULLETS];
  Drop drops[MAX_DROPS];
  Puddle puddles[MAX_PUDDLES];
  WeaponFX weapon_fx[MAX_WEAPON_FX];
  float spawn_timer;
  int kills;
  int xp;
  int level;
  int xp_to_next;
  float game_time;
  int last_item_index;
  float item_popup_timer;
  char item_popup_name[64];
  float camera_x;
  float camera_y;
  float ultimate_cd;
  float time_scale;
  int rerolls;
  int high_roll_used;
} WaveSnapshot;

typedef struct {
  WeaponDef weapons[MAX_WEAPONS];
  int weapon_count;
  ItemDef items[MAX_ITEMS];
  int item_count;
  EnemyDef enemies[MAX_ITEMS];
  int enemy_count;
  CharacterDef characters[MAX_CHARACTERS];
  int character_count;
} Database;

typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
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
  MetaProgress meta;
  int meta_points_earned_last;
  int meta_run_awarded;
  int show_skill_tree;
  SDL_Rect skill_tree_button;
  SDL_Rect skill_tree_close_button;
  SDL_Rect skill_tree_debug_button;
  SDL_Rect skill_tree_item_rects[MAX_META_UPGRADES];
  float meta_xp_mult;
  float meta_spawn_scale;
  float meta_damage_bonus;
  float meta_armor_bonus;

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
int find_weapon(Database *db, const char *id);
int db_load(Database *db);

void meta_progress_init(Game *g);
void meta_progress_save(Game *g);
void meta_apply_run_mods(Game *g);
int meta_try_purchase_upgrade(Game *g, int upgrade_index);

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

void mark_enemy_hit(Enemy *en);
const char *enemy_label(Game *g, Enemy *e);

void weapons_clear(Player *p);
void equip_weapon(Player *p, int def_index);
int weapon_is_owned(Player *p, int def_index, int *out_level);
int weapon_choice_allowed(Game *g, int def_index);
void apply_item(Player *p, Database *db, ItemDef *it, int item_index);

void spawn_drop(Game *g, float x, float y, int type, float value);
void spawn_chest(Game *g, float x, float y);
void spawn_puddle(Game *g, float x, float y, float radius, float dps, float ttl);
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

void spawn_enemy(Game *g, int def_index);
void update_enemies(Game *g, float dt);

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
