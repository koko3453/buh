
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>
#include <windows.h>

static FILE *g_log = NULL;
static int g_frame_log = 0;
static int g_log_combat = 1;
static FILE *g_combat_log = NULL;

static void log_line(const char *msg) {
  if (!g_log) return;
  fputs(msg, g_log);
  fputs("\n", g_log);
  fflush(g_log);
}

static void log_linef(const char *fmt, ...) {
  if (!g_log) return;
  char buf[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  log_line(buf);
}

static SDL_Texture *load_texture_fallback(SDL_Renderer *r, const char *path) {
  SDL_Texture *tex = IMG_LoadTexture(r, path);
  if (tex) return tex;
  char alt[256];
  snprintf(alt, sizeof(alt), "../%s", path);
  tex = IMG_LoadTexture(r, alt);
  if (tex) return tex;
  snprintf(alt, sizeof(alt), "../../%s", path);
  return IMG_LoadTexture(r, alt);
}

static void draw_circle(SDL_Renderer *r, int cx, int cy, int radius, SDL_Color color) {
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

static void draw_filled_circle(SDL_Renderer *r, int cx, int cy, int radius, SDL_Color color) {
  SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
  for (int dy = -radius; dy <= radius; dy++) {
    int dx = (int)sqrtf((float)(radius * radius - dy * dy));
    SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
  }
}

static void draw_glow(SDL_Renderer *r, int cx, int cy, int radius, SDL_Color color) {
  SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
  for (int i = radius; i > 0; i -= 2) {
    int alpha = (int)(color.a * (float)i / (float)radius * 0.3f);
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, alpha);
    draw_circle(r, cx, cy, i, (SDL_Color){color.r, color.g, color.b, (Uint8)alpha});
  }
}

static void draw_diamond(SDL_Renderer *r, int cx, int cy, int size, SDL_Color color) {
  SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
  for (int i = 0; i < size; i++) {
    SDL_RenderDrawLine(r, cx - i, cy - size + i, cx + i, cy - size + i);
    SDL_RenderDrawLine(r, cx - i, cy + size - i, cx + i, cy + size - i);
  }
}

static LONG WINAPI crash_handler(EXCEPTION_POINTERS *e) {
  log_linef("Crash code: 0x%08lx", (unsigned long)e->ExceptionRecord->ExceptionCode);
  log_linef("Crash addr: %p", e->ExceptionRecord->ExceptionAddress);
  return EXCEPTION_EXECUTE_HANDLER;
}

#define JSMN_PARENT_LINKS
#include "third_party/jsmn.h"

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

static int weapon_is(const WeaponDef *w, const char *id) {
  return strcmp(w->id, id) == 0;
}

static void weapon_status_chances(const WeaponDef *w, float *bleed, float *burn, float *slow, float *stun, float *shred) {
  *bleed = 0.0f;
  *burn = 0.0f;
  *slow = 0.0f;
  *stun = 0.0f;
  *shred = 0.0f;
  if (weapon_is(w, "daggers")) *bleed = 0.6f;
  if (weapon_is(w, "sword") || weapon_is(w, "short_sword") || weapon_is(w, "longsword")) *bleed = 0.15f;
  if (weapon_is(w, "axe")) *shred = 0.5f;
  if (weapon_is(w, "hammer")) *stun = 0.35f;
  if (weapon_is(w, "scythe")) *bleed = 0.35f;
  if (weapon_is(w, "whip")) *slow = 0.35f;
  if (weapon_is(w, "chain_blades")) *slow = 0.25f;
  if (weapon_is(w, "wand")) *stun = 0.15f;
  if (weapon_is(w, "laser")) *burn = 0.4f;
  if (weapon_is(w, "greatsword")) *stun = 0.15f;
}

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
  float slow_on_hit;    /* chance to slow enemy on autoattack (0.0-1.0) */
  float slow_aura;      /* range for slowing nearby enemies (0 = disabled) */
  float burn_on_hit;    /* chance to burn enemy on hit (0.0-1.0) */
  float burn_aura;      /* range for burning nearby enemies (0 = disabled) */
  float thorns_percent; /* reflect percent of damage taken */
  float lifesteal_on_kill; /* flat heal on kill */
  float rarity_bias;    /* bias level-up rarity rolls (0.0-1.0) */
  float slow_bonus_damage; /* bonus damage vs slowed enemies (0.0-1.0) */
  float legendary_amp;  /* amplify legendary stats per secondary score */
  float hp_regen_amp;   /* amplify hp_regen stat (0.0-1.0+) */
  float xp_kill_chance; /* chance to kill enemy on XP pickup */
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
  char ultimate[32];  /* unique ultimate ability id */
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
  float burn_timer;
  float bleed_timer;
  int bleed_stacks;
  float slow_timer;
  float stun_timer;
  float armor_shred_timer;
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
  float magnet_speed; /* current attraction speed */
  int magnetized;     /* once true, keeps flying toward player */
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

/* Visual effect for weapon swings/attacks */
#define MAX_WEAPON_FX 16
typedef struct {
  int active;
  int type;       /* 0=scythe throw, 1=bite on enemy, 2=dagger projectile */
  float x, y;
  float angle;    /* direction of swing or projectile */
  float timer;    /* animation progress */
  float duration; /* total duration */
  int target_enemy; /* for bite effect - which enemy */
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
} BossSnapshot;

#define MAX_CHARACTERS 16

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
  int debug_show_items;   /* toggle with key 8 - shows item list */
  float ultimate_cd;
  int start_page;
  float start_scroll;      /* start screen scroll offset */
  int selected_character;  /* index into db.characters */
  int rerolls;             /* rerolls remaining this run */
  int high_roll_used;      /* 1 if high roll was used this run */
  float levelup_fade;      /* levelup fade start time */
  int levelup_chosen;      /* chosen index for fade out */
  int levelup_selected[MAX_LEVELUP_CHOICES]; /* selected indices for fade out */
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
  BossSnapshot boss_snapshot;

  float spawn_timer;
  int kills;
  int xp;
  int level;
  int xp_to_next;
  float game_time;
  int last_item_index; /* most recently received item index */
  float item_popup_timer;
  char item_popup_name[64];

  LevelUpChoice choices[MAX_LEVELUP_CHOICES];
  int choice_count;
  SDL_Rect reroll_button;  /* reroll button rect for levelup screen */
  SDL_Rect highroll_button; /* high roll button rect for levelup screen */
  int scythe_id_counter;
  float camera_x;
  float camera_y;
} Game;

static const char *enemy_label(Game *g, Enemy *e) {
  if (!g || !e) return "enemy";
  int idx = e->def_index;
  if (idx >= 0 && idx < g->db.enemy_count) return g->db.enemies[idx].name;
  return "enemy";
}

static void log_combatf(Game *g, const char *fmt, ...) {
  if (!g_combat_log || !g_log_combat || !g) return;
  char msg[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(msg, sizeof(msg), fmt, args);
  va_end(args);
  char line[600];
  snprintf(line, sizeof(line), "[%.2f] %s", g->game_time, msg);
  fputs(line, g_combat_log);
  fputs("\n", g_combat_log);
  fflush(g_combat_log);
}

static const BossDef g_boss_defs[] = {
  { "proto_beast", "Proto Behemoth", 1800.0f, 90.0f, 30.0f, 26.0f, 0.7f,
    1.1f, 900.0f, 120.0f, 22.0f,
    5.0f, 16, 220.0f,
    4.0f, 80.0f, 45.0f,
    12.0f, 5.0f, 60.0f, 70.0f }
};

static int boss_def_count(void) {
  return (int)(sizeof(g_boss_defs) / sizeof(g_boss_defs[0]));
}

static void boss_snapshot_save(Game *g) {
  if (!g) return;
  g->boss_snapshot.valid = 1;
  g->boss_snapshot.mode = g->mode;
  g->boss_snapshot.player = g->player;
  memcpy(g->boss_snapshot.enemies, g->enemies, sizeof(g->enemies));
  memcpy(g->boss_snapshot.bullets, g->bullets, sizeof(g->bullets));
  memcpy(g->boss_snapshot.drops, g->drops, sizeof(g->drops));
  memcpy(g->boss_snapshot.puddles, g->puddles, sizeof(g->puddles));
  memcpy(g->boss_snapshot.weapon_fx, g->weapon_fx, sizeof(g->weapon_fx));
  g->boss_snapshot.spawn_timer = g->spawn_timer;
  g->boss_snapshot.kills = g->kills;
  g->boss_snapshot.xp = g->xp;
  g->boss_snapshot.level = g->level;
  g->boss_snapshot.xp_to_next = g->xp_to_next;
  g->boss_snapshot.game_time = g->game_time;
  g->boss_snapshot.last_item_index = g->last_item_index;
  g->boss_snapshot.item_popup_timer = g->item_popup_timer;
  snprintf(g->boss_snapshot.item_popup_name, sizeof(g->boss_snapshot.item_popup_name), "%s", g->item_popup_name);
  g->boss_snapshot.camera_x = g->camera_x;
  g->boss_snapshot.camera_y = g->camera_y;
  g->boss_snapshot.ultimate_cd = g->ultimate_cd;
  g->boss_snapshot.time_scale = g->time_scale;
  g->boss_snapshot.rerolls = g->rerolls;
  g->boss_snapshot.high_roll_used = g->high_roll_used;
}

static void boss_snapshot_restore(Game *g) {
  if (!g || !g->boss_snapshot.valid) return;
  g->mode = g->boss_snapshot.mode;
  g->player = g->boss_snapshot.player;
  memcpy(g->enemies, g->boss_snapshot.enemies, sizeof(g->enemies));
  memcpy(g->bullets, g->boss_snapshot.bullets, sizeof(g->bullets));
  memcpy(g->drops, g->boss_snapshot.drops, sizeof(g->drops));
  memcpy(g->puddles, g->boss_snapshot.puddles, sizeof(g->puddles));
  memcpy(g->weapon_fx, g->boss_snapshot.weapon_fx, sizeof(g->weapon_fx));
  g->spawn_timer = g->boss_snapshot.spawn_timer;
  g->kills = g->boss_snapshot.kills;
  g->xp = g->boss_snapshot.xp;
  g->level = g->boss_snapshot.level;
  g->xp_to_next = g->boss_snapshot.xp_to_next;
  g->game_time = g->boss_snapshot.game_time;
  g->last_item_index = g->boss_snapshot.last_item_index;
  g->item_popup_timer = g->boss_snapshot.item_popup_timer;
  snprintf(g->item_popup_name, sizeof(g->item_popup_name), "%s", g->boss_snapshot.item_popup_name);
  g->camera_x = g->boss_snapshot.camera_x;
  g->camera_y = g->boss_snapshot.camera_y;
  g->ultimate_cd = g->boss_snapshot.ultimate_cd;
  g->time_scale = g->boss_snapshot.time_scale;
  g->rerolls = g->boss_snapshot.rerolls;
  g->high_roll_used = g->boss_snapshot.high_roll_used;
}

static int find_nearest_enemy(Game *g, float x, float y) {
  float best = 999999.0f;
  int idx = -1;
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!g->enemies[i].active) continue;
    float dx = g->enemies[i].x - x;
    float dy = g->enemies[i].y - y;
    float d2 = dx * dx + dy * dy;
    if (d2 < best) { best = d2; idx = i; }
  }
  return idx;
}
static float clampf(float v, float a, float b) {
  if (v < a) return a;
  if (v > b) return b;
  return v;
}

static float frandf(void) {
  return (float)rand() / (float)RAND_MAX;
}

static float frand_range(float a, float b) {
  return a + (b - a) * frandf();
}

static void toggle_pause(Game *g) {
  if (!g) return;
  if (g->mode == MODE_LEVELUP && (g->levelup_chosen >= 0 || g->levelup_selected_count > 0)) return;
  if (g->mode == MODE_PAUSE) {
    g->mode = g->pause_return_mode;
    return;
  }
  g->pause_return_mode = g->mode;
  g->mode = MODE_PAUSE;
}

static WeaponSlot *find_weapon_slot(Player *p, Database *db, const char *id);

static void vec_norm(float *x, float *y) {
  float len = sqrtf((*x) * (*x) + (*y) * (*y));
  if (len > 0.0001f) {
    *x /= len;
    *y /= len;
  }
}

static void update_sword_orbit(Game *g, float dt) {
  WeaponSlot *slot = find_weapon_slot(&g->player, &g->db, "sword");
  if (!slot) return;
  g->player.sword_orbit_angle += dt * SWORD_ORBIT_SPEED;
  if (g->player.sword_orbit_angle > 6.28318f) g->player.sword_orbit_angle -= 6.28318f;
}

static char *read_file(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buf = (char *)malloc(len + 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  fread(buf, 1, len, f);
  buf[len] = '\0';
  fclose(f);
  return buf;
}

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

static int token_span(jsmntok_t *t, int i) {
  if (i < 0) return 1;
  int count = 1;
  if (t[i].type == JSMN_ARRAY) {
    int n = t[i].size;
    int j = i + 1;
    for (int k = 0; k < n; k++) {
      int span = token_span(t, j);
      count += span;
      j += span;
    }
  } else if (t[i].type == JSMN_OBJECT) {
    /* JSMN with JSMN_PARENT_LINKS: size = total children (keys + values) */
    int n = t[i].size;
    int j = i + 1;
    for (int k = 0; k < n; k++) {
      int span = token_span(t, j);
      count += span;
      j += span;
    }
  }
  return count;
}

static int find_key(const char *json, jsmntok_t *t, int obj, const char *key) {
  if (t[obj].type != JSMN_OBJECT) return -1;
  /* JSMN with JSMN_PARENT_LINKS: size = total children (keys + values) */
  int n = t[obj].size;
  int i = obj + 1;
  /* iterate through key-value pairs (n/2 pairs, n total tokens) */
  for (int k = 0; k < n; k += 2) {
    if (jsoneq(json, &t[i], key) == 0) return i + 1;
    i += token_span(t, i);  /* skip key */
    i += token_span(t, i);  /* skip value */
  }
  return -1;
}

static void token_string(const char *json, jsmntok_t *tok, char *out, int out_len) {
  int len = tok->end - tok->start;
  if (len >= out_len) len = out_len - 1;
  memcpy(out, json + tok->start, len);
  out[len] = '\0';
}

static float token_float(const char *json, jsmntok_t *tok) {
  char buf[64];
  token_string(json, tok, buf, (int)sizeof(buf));
  return (float)atof(buf);
}

static int token_int(const char *json, jsmntok_t *tok) {
  return (int)token_float(json, tok);
}

static void stats_clear(Stats *s) {
  memset(s, 0, sizeof(Stats));
}

static void stats_add(Stats *dst, Stats *src) {
  dst->damage += src->damage;
  dst->max_hp += src->max_hp;
  dst->move_speed += src->move_speed;
  dst->attack_speed += src->attack_speed;
  dst->armor += src->armor;
  dst->dodge += src->dodge;
  dst->crit_chance += src->crit_chance;
  dst->crit_damage += src->crit_damage;
  dst->cooldown_reduction += src->cooldown_reduction;
  dst->xp_magnet += src->xp_magnet;
  dst->hp_regen += src->hp_regen;
}

static void stats_scale(Stats *s, float mul) {
  s->damage *= mul;
  s->max_hp *= mul;
  s->move_speed *= mul;
  s->attack_speed *= mul;
  s->armor *= mul;
  s->dodge *= mul;
  s->crit_chance *= mul;
  s->crit_damage *= mul;
  s->cooldown_reduction *= mul;
  s->xp_magnet *= mul;
  s->hp_regen *= mul;
}

static void parse_stats_object(const char *json, jsmntok_t *t, int obj, Stats *out) {
  stats_clear(out);
  if (obj < 0 || t[obj].type != JSMN_OBJECT) return;
  /* JSMN with JSMN_PARENT_LINKS: size = total children (keys + values) */
  int n = t[obj].size;
  int i = obj + 1;
  /* iterate through key-value pairs (n/2 pairs, n total tokens) */
  for (int k = 0; k < n; k += 2) {
    if (jsoneq(json, &t[i], "damage") == 0) out->damage = token_float(json, &t[i + 1]);
    else if (jsoneq(json, &t[i], "max_hp") == 0) out->max_hp = token_float(json, &t[i + 1]);
    else if (jsoneq(json, &t[i], "move_speed") == 0) out->move_speed = token_float(json, &t[i + 1]);
    else if (jsoneq(json, &t[i], "attack_speed") == 0) out->attack_speed = token_float(json, &t[i + 1]);
    else if (jsoneq(json, &t[i], "armor") == 0) out->armor = token_float(json, &t[i + 1]);
    else if (jsoneq(json, &t[i], "dodge") == 0) out->dodge = token_float(json, &t[i + 1]);
    else if (jsoneq(json, &t[i], "crit_chance") == 0) out->crit_chance = token_float(json, &t[i + 1]);
    else if (jsoneq(json, &t[i], "crit_damage") == 0) out->crit_damage = token_float(json, &t[i + 1]);
    else if (jsoneq(json, &t[i], "cooldown_reduction") == 0) out->cooldown_reduction = token_float(json, &t[i + 1]);
    else if (jsoneq(json, &t[i], "xp_magnet") == 0) out->xp_magnet = token_float(json, &t[i + 1]);
    i += token_span(t, i);  /* skip key */
    i += token_span(t, i);  /* skip value */
  }
}
static int load_weapons(Database *db, const char *path) {
  char *json = read_file(path);
  if (!json) return 0;
  jsmn_parser parser;
  jsmn_init(&parser);
  jsmntok_t tokens[1024];
  int count = jsmn_parse(&parser, json, strlen(json), tokens, 1024);
  if (count < 0) {
    free(json);
    return 0;
  }
  int arr = find_key(json, tokens, 0, "weapons");
  if (arr < 0 || tokens[arr].type != JSMN_ARRAY) {
    free(json);
    return 0;
  }
  int idx = arr + 1;
  int n = tokens[arr].size;
  db->weapon_count = 0;
  for (int i = 0; i < n && db->weapon_count < MAX_WEAPONS; i++) {
    int obj = idx;
    WeaponDef *w = &db->weapons[db->weapon_count++];
    memset(w, 0, sizeof(*w));
    int idt = find_key(json, tokens, obj, "id");
    int nt = find_key(json, tokens, obj, "name");
    int tt = find_key(json, tokens, obj, "type");
    int rt = find_key(json, tokens, obj, "rarity");
    if (idt > 0) token_string(json, &tokens[idt], w->id, (int)sizeof(w->id));
    if (nt > 0) token_string(json, &tokens[nt], w->name, (int)sizeof(w->name));
    if (tt > 0) token_string(json, &tokens[tt], w->type, (int)sizeof(w->type));
    if (rt > 0) token_string(json, &tokens[rt], w->rarity, (int)sizeof(w->rarity));
    int cd = find_key(json, tokens, obj, "cooldown");
    int dmg = find_key(json, tokens, obj, "damage");
    int range = find_key(json, tokens, obj, "range");
    int ps = find_key(json, tokens, obj, "projectile_speed");
    int pierce = find_key(json, tokens, obj, "pierce");
    int pellets = find_key(json, tokens, obj, "pellets");
    int spread = find_key(json, tokens, obj, "spread");
    int homing = find_key(json, tokens, obj, "homing");
    int crit = find_key(json, tokens, obj, "crit_multiplier");
    if (cd > 0) w->cooldown = token_float(json, &tokens[cd]);
    if (dmg > 0) w->damage = token_float(json, &tokens[dmg]);
    if (range > 0) w->range = token_float(json, &tokens[range]);
    if (ps > 0) w->projectile_speed = token_float(json, &tokens[ps]);
    if (pierce > 0) w->pierce = token_int(json, &tokens[pierce]);
    if (pellets > 0) w->pellets = token_int(json, &tokens[pellets]);
    if (spread > 0) w->spread = token_float(json, &tokens[spread]);
    if (homing > 0) w->homing = token_int(json, &tokens[homing]);
    if (crit > 0) w->crit_multiplier = token_float(json, &tokens[crit]);

    int scales = find_key(json, tokens, obj, "scales");
    if (scales > 0 && tokens[scales].type == JSMN_ARRAY) {
      int sidx = scales + 1;
      for (int s = 0; s < tokens[scales].size; s++) {
        if (jsoneq(json, &tokens[sidx], "damage") == 0) w->scale_damage = 1;
        if (jsoneq(json, &tokens[sidx], "attack_speed") == 0) w->scale_attack_speed = 1;
        if (jsoneq(json, &tokens[sidx], "range") == 0) w->scale_range = 1;
        if (jsoneq(json, &tokens[sidx], "crit") == 0) w->scale_crit = 1;
        sidx += token_span(tokens, sidx);
      }
    }
    idx += token_span(tokens, idx);
  }
  free(json);
  return 1;
}

static int load_items(Database *db, const char *path) {
  char *json = read_file(path);
  if (!json) return 0;
  jsmn_parser parser;
  jsmn_init(&parser);
  jsmntok_t tokens[2048];
  int count = jsmn_parse(&parser, json, strlen(json), tokens, 2048);
  if (count < 0) {
    free(json);
    return 0;
  }
  int arr = find_key(json, tokens, 0, "items");
  if (arr < 0 || tokens[arr].type != JSMN_ARRAY) {
    free(json);
    return 0;
  }
  int idx = arr + 1;
  int n = tokens[arr].size;
  db->item_count = 0;
  for (int i = 0; i < n && db->item_count < MAX_ITEMS; i++) {
    int obj = idx;
    ItemDef *it = &db->items[db->item_count++];
    memset(it, 0, sizeof(*it));
    int idt = find_key(json, tokens, obj, "id");
    int nt = find_key(json, tokens, obj, "name");
    int rt = find_key(json, tokens, obj, "rarity");
    int dt = find_key(json, tokens, obj, "desc");
    if (idt > 0) token_string(json, &tokens[idt], it->id, (int)sizeof(it->id));
    if (nt > 0) token_string(json, &tokens[nt], it->name, (int)sizeof(it->name));
    if (rt > 0) token_string(json, &tokens[rt], it->rarity, (int)sizeof(it->rarity));
    if (dt > 0) token_string(json, &tokens[dt], it->desc, (int)sizeof(it->desc));

    int stats = find_key(json, tokens, obj, "stats");
    if (stats > 0) parse_stats_object(json, tokens, stats, &it->stats);

    int proc = find_key(json, tokens, obj, "proc");
    if (proc > 0 && tokens[proc].type == JSMN_OBJECT) {
      it->has_proc = 1;
      int chance = find_key(json, tokens, proc, "chance");
      int dmg = find_key(json, tokens, proc, "damage");
      int bounces = find_key(json, tokens, proc, "bounces");
      int range = find_key(json, tokens, proc, "range");
      if (chance > 0) it->proc_chance = token_float(json, &tokens[chance]);
      if (dmg > 0) it->proc_damage = token_float(json, &tokens[dmg]);
      if (bounces > 0) it->proc_bounces = token_int(json, &tokens[bounces]);
      if (range > 0) it->proc_range = token_float(json, &tokens[range]);
    }
    
    /* Parse slow effects */
    int slow_hit = find_key(json, tokens, obj, "slow_on_hit");
    int slow_aura = find_key(json, tokens, obj, "slow_aura");
    if (slow_hit > 0) it->slow_on_hit = token_float(json, &tokens[slow_hit]);
    if (slow_aura > 0) it->slow_aura = token_float(json, &tokens[slow_aura]);

    int burn_hit = find_key(json, tokens, obj, "burn_on_hit");
    int burn_aura = find_key(json, tokens, obj, "burn_aura");
    int thorns = find_key(json, tokens, obj, "thorns_percent");
    int lifesteal = find_key(json, tokens, obj, "lifesteal_on_kill");
    int rarity_bias = find_key(json, tokens, obj, "rarity_bias");
    int slow_bonus = find_key(json, tokens, obj, "slow_bonus_damage");
    int legendary_amp = find_key(json, tokens, obj, "legendary_amp");
    int hp_regen_amp = find_key(json, tokens, obj, "hp_regen_amp");
    int xp_kill = find_key(json, tokens, obj, "xp_kill_chance");
    if (burn_hit > 0) it->burn_on_hit = token_float(json, &tokens[burn_hit]);
    if (burn_aura > 0) it->burn_aura = token_float(json, &tokens[burn_aura]);
    if (thorns > 0) it->thorns_percent = token_float(json, &tokens[thorns]);
    if (lifesteal > 0) it->lifesteal_on_kill = token_float(json, &tokens[lifesteal]);
    if (rarity_bias > 0) it->rarity_bias = token_float(json, &tokens[rarity_bias]);
    if (slow_bonus > 0) it->slow_bonus_damage = token_float(json, &tokens[slow_bonus]);
    if (legendary_amp > 0) it->legendary_amp = token_float(json, &tokens[legendary_amp]);
    if (hp_regen_amp > 0) it->hp_regen_amp = token_float(json, &tokens[hp_regen_amp]);
    if (xp_kill > 0) it->xp_kill_chance = token_float(json, &tokens[xp_kill]);

    idx += token_span(tokens, idx);
  }
  free(json);
  return 1;
}

static int load_enemies(Database *db, const char *path) {
  char *json = read_file(path);
  if (!json) return 0;
  jsmn_parser parser;
  jsmn_init(&parser);
  jsmntok_t tokens[2048];
  int count = jsmn_parse(&parser, json, strlen(json), tokens, 2048);
  if (count < 0) {
    free(json);
    return 0;
  }
  int arr = find_key(json, tokens, 0, "enemies");
  if (arr < 0 || tokens[arr].type != JSMN_ARRAY) {
    free(json);
    return 0;
  }
  int idx = arr + 1;
  int n = tokens[arr].size;
  db->enemy_count = 0;
  for (int i = 0; i < n && db->enemy_count < MAX_ITEMS; i++) {
    int obj = idx;
    EnemyDef *e = &db->enemies[db->enemy_count++];
    memset(e, 0, sizeof(*e));
    int idt = find_key(json, tokens, obj, "id");
    int nt = find_key(json, tokens, obj, "name");
    int rt = find_key(json, tokens, obj, "role");
    if (idt > 0) token_string(json, &tokens[idt], e->id, (int)sizeof(e->id));
    if (nt > 0) token_string(json, &tokens[nt], e->name, (int)sizeof(e->name));
    if (rt > 0) token_string(json, &tokens[rt], e->role, (int)sizeof(e->role));

    int hp = find_key(json, tokens, obj, "hp");
    int speed = find_key(json, tokens, obj, "speed");
    int dmg = find_key(json, tokens, obj, "damage");
    int cd = find_key(json, tokens, obj, "cooldown");
    int ps = find_key(json, tokens, obj, "projectile_speed");
    int charge_speed = find_key(json, tokens, obj, "charge_speed");
    int charge_cd = find_key(json, tokens, obj, "charge_cooldown");
    int explode = find_key(json, tokens, obj, "explode_radius");

    if (hp > 0) e->hp = token_float(json, &tokens[hp]);
    if (speed > 0) e->speed = token_float(json, &tokens[speed]);
    if (dmg > 0) e->damage = token_float(json, &tokens[dmg]);
    if (cd > 0) e->cooldown = token_float(json, &tokens[cd]);
    if (ps > 0) e->projectile_speed = token_float(json, &tokens[ps]);
    if (charge_speed > 0) e->charge_speed = token_float(json, &tokens[charge_speed]);
    if (charge_cd > 0) e->charge_cooldown = token_float(json, &tokens[charge_cd]);
    if (explode > 0) e->explode_radius = token_float(json, &tokens[explode]);

    idx += token_span(tokens, idx);
  }
  free(json);
  return 1;
}

static int load_characters(Database *db, const char *path) {
  char *json = read_file(path);
  if (!json) return 0;
  jsmn_parser parser;
  jsmn_init(&parser);
  jsmntok_t tokens[1024];
  int count = jsmn_parse(&parser, json, strlen(json), tokens, 1024);
  if (count < 0) {
    free(json);
    return 0;
  }
  int arr = find_key(json, tokens, 0, "characters");
  if (arr < 0 || tokens[arr].type != JSMN_ARRAY) {
    free(json);
    return 0;
  }
  int idx = arr + 1;
  int n = tokens[arr].size;
  db->character_count = 0;
  for (int i = 0; i < n && db->character_count < MAX_CHARACTERS; i++) {
    int obj = idx;
    CharacterDef *c = &db->characters[db->character_count++];
    memset(c, 0, sizeof(*c));
    int idt = find_key(json, tokens, obj, "id");
    int nt = find_key(json, tokens, obj, "name");
    int pt = find_key(json, tokens, obj, "portrait");
    int wt = find_key(json, tokens, obj, "weapon");
    int rt = find_key(json, tokens, obj, "rule");
    int ut = find_key(json, tokens, obj, "ultimate");
    if (idt > 0) token_string(json, &tokens[idt], c->id, (int)sizeof(c->id));
    if (nt > 0) token_string(json, &tokens[nt], c->name, (int)sizeof(c->name));
    if (pt > 0) token_string(json, &tokens[pt], c->portrait, (int)sizeof(c->portrait));
    if (wt > 0) token_string(json, &tokens[wt], c->weapon, (int)sizeof(c->weapon));
    if (rt > 0) token_string(json, &tokens[rt], c->rule, (int)sizeof(c->rule));
    if (ut > 0) token_string(json, &tokens[ut], c->ultimate, (int)sizeof(c->ultimate));
    else strcpy(c->ultimate, "kill_all"); /* default ultimate */
    int stats = find_key(json, tokens, obj, "stats");
    if (stats > 0) parse_stats_object(json, tokens, stats, &c->stats);
    idx += token_span(tokens, idx);
  }
  free(json);
  return 1;
}

static int db_load(Database *db) {
  if (!load_weapons(db, "data/weapons.json")) return 0;
  if (!load_items(db, "data/items.json")) return 0;
  if (!load_enemies(db, "data/enemies.json")) return 0;
  if (!load_characters(db, "data/characters.json")) return 0;
  return 1;
}

static int find_weapon(Database *db, const char *id) {
  for (int i = 0; i < db->weapon_count; i++) {
    if (strcmp(db->weapons[i].id, id) == 0) return i;
  }
  return -1;
}

static WeaponSlot *find_weapon_slot(Player *p, Database *db, const char *id) {
  if (!p || !db || !id) return NULL;
  for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
    if (!p->weapons[i].active) continue;
    WeaponDef *w = &db->weapons[p->weapons[i].def_index];
    if (weapon_is(w, id)) return &p->weapons[i];
  }
  return NULL;
}

static float player_hp_regen_amp(Player *p, Database *db);

static Stats player_total_stats(Player *p, Database *db) {
  Stats s = p->base;
  stats_add(&s, &p->bonus);
  if (p->ultimate_move_to_as_timer > 0.0f) {
    float ms = s.move_speed;
    s.move_speed = 0.0f;
    s.attack_speed += ms;
  }
  s.max_hp = clampf(s.max_hp, 1.0f, 9999.0f);
  s.attack_speed = clampf(s.attack_speed, -0.5f, 3.0f);
  s.move_speed = clampf(s.move_speed, -0.3f, 2.0f);
  s.dodge = clampf(s.dodge, 0.0f, 0.75f);
  s.crit_chance = clampf(s.crit_chance, 0.0f, 0.75f);
  s.crit_damage = clampf(s.crit_damage, 0.0f, 3.0f);
  s.cooldown_reduction = clampf(s.cooldown_reduction, 0.0f, 0.6f);
  s.xp_magnet = clampf(s.xp_magnet, 0.0f, 600.0f);
  s.hp_regen = clampf(s.hp_regen, 0.0f, 50.0f);
  if (db) {
    float regen_amp = player_hp_regen_amp(p, db);
    s.hp_regen = s.hp_regen * (1.0f + regen_amp);
  }
  s.hp_regen = clampf(s.hp_regen, 0.0f, 100.0f);
  return s;
}

/* Get total slow_on_hit chance from all passive items */
static float player_slow_on_hit(Player *p, Database *db) {
  float total = 0.0f;
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) {
      total += db->items[idx].slow_on_hit;
    }
  }
  return clampf(total, 0.0f, 1.0f);
}

/* Get max slow_aura range from all passive items */
static float player_slow_aura(Player *p, Database *db) {
  float max_range = 0.0f;
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) {
      if (db->items[idx].slow_aura > max_range) {
        max_range = db->items[idx].slow_aura;
      }
    }
  }
  return max_range;
}

static float player_burn_on_hit(Player *p, Database *db) {
  float total = 0.0f;
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) {
      total += db->items[idx].burn_on_hit;
    }
  }
  return clampf(total, 0.0f, 1.0f);
}

static float player_burn_aura(Player *p, Database *db) {
  float max_range = 0.0f;
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) {
      if (db->items[idx].burn_aura > max_range) {
        max_range = db->items[idx].burn_aura;
      }
    }
  }
  return max_range;
}

static float player_thorns_percent(Player *p, Database *db) {
  float total = 0.0f;
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) {
      total += db->items[idx].thorns_percent;
    }
  }
  return clampf(total, 0.0f, 0.9f);
}

static float player_lifesteal_on_kill(Player *p, Database *db) {
  float total = 0.0f;
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) {
      total += db->items[idx].lifesteal_on_kill;
    }
  }
  return total;
}

static float player_rarity_bias(Player *p, Database *db) {
  float total = 0.0f;
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) {
      total += db->items[idx].rarity_bias;
    }
  }
  return clampf(total, 0.0f, 0.9f);
}

static float player_slow_bonus_damage(Player *p, Database *db) {
  float total = 0.0f;
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) {
      total += db->items[idx].slow_bonus_damage;
    }
  }
  return clampf(total, 0.0f, 1.0f);
}

static float player_legendary_amp(Player *p, Database *db) {
  float total = 0.0f;
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) {
      total += db->items[idx].legendary_amp;
    }
  }
  return clampf(total, 0.0f, 0.2f);
}

static float player_hp_regen_amp(Player *p, Database *db) {
  float total = 0.0f;
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) {
      total += db->items[idx].hp_regen_amp;
    }
  }
  return clampf(total, 0.0f, 3.0f);
}

static float player_xp_kill_chance(Player *p, Database *db) {
  float total = 0.0f;
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) {
      total += db->items[idx].xp_kill_chance;
    }
  }
  return clampf(total, 0.0f, 1.0f);
}

static float player_roll_crit_damage(Stats *stats, WeaponDef *w, float dmg) {
  if (!w || !w->scale_crit) return dmg;
  if (stats->crit_chance <= 0.0f) return dmg;
  if (frandf() < stats->crit_chance) {
    float crit_mul = (w->crit_multiplier > 0.0f) ? w->crit_multiplier : 1.5f;
    crit_mul += stats->crit_damage;
    if (crit_mul < 1.1f) crit_mul = 1.1f;
    return dmg * crit_mul;
  }
  return dmg;
}

static void mark_enemy_hit(Enemy *en) {
  if (!en) return;
  en->hit_timer = (float)SDL_GetTicks() / 1000.0f;
}

static float player_apply_hit_mods(Game *g, Enemy *en, float dmg) {
  if (en->armor_shred_timer > 0.0f) dmg *= 1.2f;
  float slow_bonus = player_slow_bonus_damage(&g->player, &g->db);
  if (slow_bonus > 0.0f && en->slow_timer > 0.0f) {
    float extra = dmg * slow_bonus;
    log_combatf(g, "slow_bonus +%.1f dmg to %s", extra, enemy_label(g, en));
    dmg += extra;
  }
  return dmg;
}

static void proc_chain_lightning(Game *g, int start_idx, float dmg, int bounces, float range) {
  if (bounces <= 0 || range <= 0.0f) return;
  int visited[MAX_ENEMIES];
  memset(visited, 0, sizeof(visited));
  visited[start_idx] = 1;
  int current = start_idx;
  float range2 = range * range;

  for (int b = 0; b < bounces; b++) {
    int next = -1;
    float best = range2;
    float cx = g->enemies[current].x;
    float cy = g->enemies[current].y;
    for (int i = 0; i < MAX_ENEMIES; i++) {
      if (!g->enemies[i].active || visited[i]) continue;
      float dx = g->enemies[i].x - cx;
      float dy = g->enemies[i].y - cy;
      float d2 = dx * dx + dy * dy;
      if (d2 < best) { best = d2; next = i; }
    }
    if (next < 0) break;
    Enemy *en = &g->enemies[next];
    mark_enemy_hit(en);
    float hit = player_apply_hit_mods(g, en, dmg);
    en->hp -= hit;
    log_combatf(g, "chain_lightning hit %s for %.1f", enemy_label(g, en), hit);
    visited[next] = 1;
    current = next;
  }
}

static void player_try_item_proc(Game *g, int enemy_idx, Stats *stats) {
  for (int i = 0; i < g->player.passive_count; i++) {
    int idx = g->player.passive_items[i];
    if (idx < 0 || idx >= g->db.item_count) continue;
    ItemDef *it = &g->db.items[idx];
    if (!it->has_proc) continue;
    if (it->proc_chance <= 0.0f || it->proc_damage <= 0.0f || it->proc_bounces <= 0) continue;
    if (frandf() < it->proc_chance) {
      float range = (it->proc_range > 0.0f) ? it->proc_range : 140.0f;
      float dmg = it->proc_damage * (1.0f + stats->damage);
      log_combatf(g, "chain_lightning proc dmg %.1f bounces %d", dmg, it->proc_bounces);
      proc_chain_lightning(g, enemy_idx, dmg, it->proc_bounces, range);
    }
  }
}

static float damage_after_armor(float dmg, float armor) {
  float reduction = clampf(armor * 0.02f, 0.0f, 0.7f);
  return dmg * (1.0f - reduction);
}
static void spawn_enemy(Game *g, int def_index) {
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
      /* Spawn at the edge of the view */
      float margin = 20.0f;
      float cam_min_x = g->camera_x;
      float cam_max_x = g->camera_x + g->view_w;
      float cam_min_y = g->camera_y;
      float cam_max_y = g->camera_y + g->view_h;
      int side = rand() % 4; /* 0=left,1=right,2=top,3=bottom */
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
      /* Clamp to world bounds */
      x = clampf(x, 40.0f, ARENA_W - 40.0f);
      y = clampf(y, 40.0f, ARENA_H - 40.0f);
      e->x = x;
      e->y = y;
      return;
    }
  }
}

static void clear_boss_room(Game *g) {
  for (int i = 0; i < MAX_ENEMIES; i++) g->enemies[i].active = 0;
  for (int i = 0; i < MAX_BULLETS; i++) g->bullets[i].active = 0;
  for (int i = 0; i < MAX_DROPS; i++) g->drops[i].active = 0;
  for (int i = 0; i < MAX_PUDDLES; i++) g->puddles[i].active = 0;
  for (int i = 0; i < MAX_WEAPON_FX; i++) g->weapon_fx[i].active = 0;
}

static void spawn_boss(Game *g, float x, float y) {
  if (!g) return;
  int idx = g->boss_def_index;
  if (idx < 0 || idx >= boss_def_count()) idx = 0;
  const BossDef *def = &g_boss_defs[idx];
  g->boss.active = 1;
  g->boss.def_index = idx;
  g->boss.x = x;
  g->boss.y = y;
  g->boss.hp = def->hp;
  g->boss.max_hp = def->hp;
  g->boss.attack_timer = 0.0f;
  g->boss.beam_angle = frandf() * 6.28318f;
  g->boss.wave_cd = def->wave_cooldown * 0.5f;
  g->boss.slam_cd = def->slam_cooldown * 0.5f;
  g->boss.hazard_timer = 0.0f;
  g->boss.hazard_cd = def->hazard_cooldown * 0.5f;
  g->boss.sword_hit_cd = 0.0f;
  for (int i = 0; i < 3; i++) {
    g->boss.safe_x[i] = g->boss.x;
    g->boss.safe_y[i] = g->boss.y;
  }
}

static void start_boss_event(Game *g) {
  if (!g) return;
  boss_snapshot_save(g);
  g->mode = MODE_BOSS_EVENT;
  g->boss_countdown_timer = 3.0f;
  g->boss_timer = 180.0f;
  g->boss_timer_max = g->boss_timer;
  g->boss.active = 0;
  g->boss.def_index = 0;

  float margin = 200.0f;
  g->boss_room_x = margin + frandf() * (ARENA_W - margin * 2.0f);
  g->boss_room_y = margin + frandf() * (ARENA_H - margin * 2.0f);
  g->player.x = g->boss_room_x;
  g->player.y = g->boss_room_y;
  clear_boss_room(g);

  g->camera_x = g->player.x - g->view_w * 0.5f;
  g->camera_y = g->player.y - g->view_h * 0.5f;
  float max_cam_x = ARENA_W - g->view_w;
  float max_cam_y = ARENA_H - g->view_h;
  if (max_cam_x < 0.0f) max_cam_x = 0.0f;
  if (max_cam_y < 0.0f) max_cam_y = 0.0f;
  g->camera_x = clampf(g->camera_x, 0.0f, max_cam_x);
  g->camera_y = clampf(g->camera_y, 0.0f, max_cam_y);
}

static void build_boss_reward_choices(Game *g) {
  g->choice_count = 0;
  int legendary_indices[MAX_ITEMS];
  int legendary_count = 0;
  for (int i = 0; i < g->db.item_count; i++) {
    if (strcmp(g->db.items[i].rarity, "legendary") == 0) {
      legendary_indices[legendary_count++] = i;
    }
  }
  if (legendary_count == 0) return;

  int picks = legendary_count < 3 ? legendary_count : 3;
  for (int i = legendary_count - 1; i > 0; i--) {
    int j = rand() % (i + 1);
    int tmp = legendary_indices[i];
    legendary_indices[i] = legendary_indices[j];
    legendary_indices[j] = tmp;
  }
  for (int i = 0; i < picks; i++) {
    g->choices[g->choice_count++] = (LevelUpChoice){ .type = 0, .index = legendary_indices[i] };
  }
}

static void end_boss_event(Game *g, int success) {
  if (!g) return;
  g->boss.active = 0;
  g->boss_timer = 0.0f;
  g->boss_timer_max = 0.0f;
  g->boss_countdown_timer = 0.0f;
  boss_snapshot_restore(g);
  if (success) {
    build_boss_reward_choices(g);
    g->levelup_chosen = -1;
    g->levelup_selected_count = 0;
    g->levelup_fade = 0.0f;
    if (g->choice_count > 0) g->mode = MODE_LEVELUP;
    else g->mode = MODE_WAVE;
  } else {
    g->mode = MODE_WAVE;
  }
  g->boss_snapshot.valid = 0;
}

static void spawn_bullet(Game *g, float x, float y, float vx, float vy, float damage, int pierce, int homing, int from_player,
                         int weapon_index, float bleed_chance, float burn_chance, float slow_chance, float stun_chance, float armor_shred_chance) {
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

static void spawn_drop(Game *g, float x, float y, int type, float value) {
  for (int i = 0; i < MAX_DROPS; i++) {
    if (!g->drops[i].active) {
      Drop *d = &g->drops[i];
      memset(d, 0, sizeof(*d));
      d->active = 1;
      d->type = type;
      d->x = x;
      d->y = y;
      d->value = value;
      d->ttl = 10.0f;
      return;
    }
  }
}

static void spawn_chest(Game *g, float x, float y) {
  for (int i = 0; i < MAX_DROPS; i++) {
    if (!g->drops[i].active) {
      Drop *d = &g->drops[i];
      memset(d, 0, sizeof(*d));
      d->active = 1;
      d->type = 2;
      d->x = x;
      d->y = y;
      d->value = 0.0f;
      d->ttl = 9999.0f;
      return;
    }
  }
}

static void spawn_puddle(Game *g, float x, float y, float radius, float dps, float ttl) {
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

/* Spawn weapon visual effect */
static void spawn_weapon_fx(Game *g, int type, float x, float y, float angle, float duration, int target_enemy) {
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

static void spawn_scythe_fx(Game *g, float cx, float cy, float angle, float radial_speed, float angle_speed, float damage) {
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

static void update_weapon_fx(Game *g, float dt) {
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

static void update_puddles(Game *g, float dt) {
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
      Enemy *en = &g->enemies[e];
      if (!en->active) continue;
      float dx = en->x - p->x;
      float dy = en->y - p->y;
      if (dx * dx + dy * dy <= radius2) {
        float dmg = p->dps * dt;
        dmg = player_apply_hit_mods(g, en, dmg);
        en->hp -= dmg;
        if (p->log_timer <= 0.0f) {
          log_combatf(g, "puddle tick %s for %.1f", enemy_label(g, en), dmg);
          p->log_timer = 0.5f;
        }
      }
    }
  }
}

static void weapons_clear(Player *p) {
  for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
    p->weapons[i].active = 0;
  }
}

static void equip_weapon(Player *p, int def_index) {
  for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
    if (p->weapons[i].active && p->weapons[i].def_index == def_index) {
      p->weapons[i].level = (int)clampf(p->weapons[i].level + 1, 1, MAX_WEAPON_LEVEL);
      return;
    }
  }
  for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
    if (!p->weapons[i].active) {
      p->weapons[i].active = 1;
      p->weapons[i].def_index = def_index;
      p->weapons[i].level = 1;
      p->weapons[i].cd_timer = 0.0f;
      return;
    }
  }
}

static int weapon_slots_full(Player *p) {
  int count = 0;
  for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
    if (p->weapons[i].active) count++;
  }
  return count >= MAX_WEAPON_SLOTS;
}

static int weapon_is_owned(Player *p, int def_index, int *out_level) {
  for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
    if (p->weapons[i].active && p->weapons[i].def_index == def_index) {
      if (out_level) *out_level = p->weapons[i].level;
      return 1;
    }
  }
  return 0;
}

static int weapon_choice_allowed(Game *g, int def_index) {
  int level = 0;
  int owned = weapon_is_owned(&g->player, def_index, &level);
  if (owned) return level < MAX_WEAPON_LEVEL;
  if (weapon_slots_full(&g->player)) return 0;
  return 1;
}

static int count_allowed_weapons(Game *g) {
  int count = 0;
  for (int i = 0; i < g->db.weapon_count; i++) {
    if (weapon_choice_allowed(g, i)) count++;
  }
  return count;
}

static int roll_weapon_index_with_bias_filtered(Game *g) {
  int attempts = g->db.weapon_count * 2 + 5;
  for (int i = 0; i < attempts; i++) {
    int idx = roll_weapon_index_with_bias(g);
    if (weapon_choice_allowed(g, idx)) return idx;
  }
  int count = count_allowed_weapons(g);
  if (count <= 0) return -1;
  int pick = rand() % count;
  for (int i = 0; i < g->db.weapon_count; i++) {
    if (!weapon_choice_allowed(g, i)) continue;
    if (pick-- == 0) return i;
  }
  return -1;
}

static int can_equip_weapon(Player *p, int def_index) {
  for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
    if (p->weapons[i].active && p->weapons[i].def_index == def_index) return 1;
  }
  for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
    if (!p->weapons[i].active) return 1;
  }
  return 0;
}

static void player_recalc(Player *p, Database *db) {
  stats_clear(&p->bonus);
  float amp = player_legendary_amp(p, db);
  if (amp <= 0.0f) {
    for (int i = 0; i < p->passive_count; i++) {
      int idx = p->passive_items[i];
      if (idx >= 0 && idx < db->item_count) stats_add(&p->bonus, &db->items[idx].stats);
    }
    return;
  }

  /* First pass: apply non-legendary items */
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx < 0 || idx >= db->item_count) continue;
    ItemDef *it = &db->items[idx];
    if (strcmp(it->rarity, "legendary") != 0) {
      stats_add(&p->bonus, &it->stats);
    }
  }

  /* Compute secondary score from base + non-legendary stats */
  Stats total = p->base;
  stats_add(&total, &p->bonus);
  float secondary_score = 0.0f;
  secondary_score += total.max_hp / 100.0f;
  secondary_score += total.move_speed * 10.0f;
  secondary_score += total.armor;
  secondary_score += total.dodge * 20.0f;
  float legendary_bonus = clampf(amp * secondary_score, 0.0f, 0.75f);

  /* Second pass: apply amplified legendary items */
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx < 0 || idx >= db->item_count) continue;
    ItemDef *it = &db->items[idx];
    if (strcmp(it->rarity, "legendary") == 0) {
      Stats scaled = it->stats;
      if (legendary_bonus > 0.0f) stats_scale(&scaled, 1.0f + legendary_bonus);
      stats_add(&p->bonus, &scaled);
    }
  }
}

static void apply_item(Player *p, Database *db, ItemDef *it, int item_index) {
  if (p->passive_count < MAX_ITEMS) {
    p->passive_items[p->passive_count++] = item_index;
  }
  player_recalc(p, db);
}

static int levelup_choice_exists(Game *g, int type, int index) {
  for (int i = 0; i < g->choice_count; i++) {
    if (g->choices[i].type == type && g->choices[i].index == index) return 1;
  }
  return 0;
}

static void build_levelup_choices(Game *g) {
  g->choice_count = 0;
  if (g->db.item_count == 0 && g->db.weapon_count == 0) {
    log_line("Level up choices skipped: no items or weapons.");
    return;
  }
  int weapon_choices_available = count_allowed_weapons(g);
  /* Generate 3 random choices - mix of items and weapons */
  int num_choices = 3;
  int attempts = 0;
  int max_attempts = 32;
  while (g->choice_count < num_choices && attempts++ < max_attempts) {
    int type = rand() % 3; /* 0,1 = item (66%), 2 = weapon (33%) */
    if (type < 2 && g->db.item_count > 0) {
      int idx = roll_item_index_with_bias(g);
      if (!levelup_choice_exists(g, 0, idx)) {
        g->choices[g->choice_count++] = (LevelUpChoice){ .type = 0, .index = idx };
      }
    } else if (g->db.weapon_count > 0 && weapon_choices_available > 0) {
      int idx = roll_weapon_index_with_bias_filtered(g);
      if (idx >= 0 && !levelup_choice_exists(g, 1, idx)) {
        g->choices[g->choice_count++] = (LevelUpChoice){ .type = 1, .index = idx };
      } else if (g->db.item_count > 0) {
        int item_idx = roll_item_index_with_bias(g);
        if (!levelup_choice_exists(g, 0, item_idx)) {
          g->choices[g->choice_count++] = (LevelUpChoice){ .type = 0, .index = item_idx };
        }
      }
    } else if (g->db.item_count > 0) {
      int idx = roll_item_index_with_bias(g);
      if (!levelup_choice_exists(g, 0, idx)) {
        g->choices[g->choice_count++] = (LevelUpChoice){ .type = 0, .index = idx };
      }
    }
  }
  g->levelup_fade = 0.0f;
  g->levelup_chosen = -1;
  g->levelup_selected_count = 0;
}

static void build_start_page(Game *g) {
  g->choice_count = 0;
  int per_page = g->db.character_count;
  int total = g->db.character_count;
  int pages = (total + per_page - 1) / per_page;
  if (pages < 1) pages = 1;
  if (g->start_page < 0) g->start_page = 0;
  if (g->start_page >= pages) g->start_page = pages - 1;
  int start = g->start_page * per_page;
  int end = start + per_page;
  if (end > total) end = total;
  for (int i = start; i < end; i++) {
    /* Type 2 = Character */
    g->choices[g->choice_count++] = (LevelUpChoice){ .type = 2, .index = i };
  }
}

static float start_scroll_max(Game *g) {
  int split_x = (g->window_w * 2) / 3;
  int margin = 25;
  int cols = 4;
  int name_area_h = 32;
  int card_w = (split_x - margin * 2 - (cols - 1) * 20) / cols;
  int card_h = card_w + 60;
  int total_h = card_h + name_area_h;
  int start_y = 90;
  int shown = g->choice_count;
  int rows = (shown + cols - 1) / cols;
  int total_grid_h = rows * (total_h + 12) - 12;
  int view_h = g->window_h - start_y - 20;
  if (total_grid_h <= view_h) return 0.0f;
  return (float)(total_grid_h - view_h);
}

static SDL_Color rarity_color(const char *rarity) {
  if (strcmp(rarity, "uncommon") == 0) return (SDL_Color){ 75, 209, 160, 255 };
  if (strcmp(rarity, "rare") == 0) return (SDL_Color){ 91, 177, 255, 255 };
  if (strcmp(rarity, "epic") == 0) return (SDL_Color){ 210, 123, 255, 255 };
  if (strcmp(rarity, "legendary") == 0) return (SDL_Color){ 255, 179, 71, 255 };
  return (SDL_Color){ 230, 231, 234, 255 };
}

static SDL_Texture *rarity_orb_texture(Game *g, const char *rarity) {
  if (!g || !rarity) return NULL;
  if (strcmp(rarity, "uncommon") == 0) return g->tex_orb_uncommon;
  if (strcmp(rarity, "rare") == 0) return g->tex_orb_rare;
  if (strcmp(rarity, "epic") == 0) return g->tex_orb_epic;
  if (strcmp(rarity, "legendary") == 0) return g->tex_orb_legendary;
  return g->tex_orb_common;
}

static int levelup_orb_size(const SDL_Rect *rect) {
  int base = rect->w < rect->h ? rect->w : rect->h;
  return (int)(base * 2.4f);
}

static int rarity_rank(const char *rarity) {
  if (strcmp(rarity, "uncommon") == 0) return 1;
  if (strcmp(rarity, "rare") == 0) return 2;
  if (strcmp(rarity, "epic") == 0) return 3;
  if (strcmp(rarity, "legendary") == 0) return 4;
  return 0;
}

static int roll_rarity_rank(float bias) {
  float w_common = 50.0f;
  float w_uncommon = 35.0f;
  float w_rare = 20.0f;
  float w_epic = 10.0f;
  float w_legendary = 5.0f;

  if (bias > 0.0f) {
    float down = clampf(1.0f - bias * 0.6f, 0.2f, 1.0f);
    float up = 1.0f + bias;
    w_common *= down;
    w_uncommon *= down;
    w_rare *= up;
    w_epic *= up;
    w_legendary *= up;
  }

  float total = w_common + w_uncommon + w_rare + w_epic + w_legendary;
  float r = frandf() * total;
  if (r < w_common) return 0;
  r -= w_common;
  if (r < w_uncommon) return 1;
  r -= w_uncommon;
  if (r < w_rare) return 2;
  r -= w_rare;
  if (r < w_epic) return 3;
  return 4;
}

static int pick_item_index_by_rarity(Game *g, int rank) {
  int count = 0;
  for (int i = 0; i < g->db.item_count; i++) {
    if (rarity_rank(g->db.items[i].rarity) == rank) count++;
  }
  if (count == 0) return -1;
  int pick = rand() % count;
  for (int i = 0; i < g->db.item_count; i++) {
    if (rarity_rank(g->db.items[i].rarity) == rank) {
      if (pick-- == 0) return i;
    }
  }
  return -1;
}

static int pick_weapon_index_by_rarity(Game *g, int rank) {
  int count = 0;
  for (int i = 0; i < g->db.weapon_count; i++) {
    if (rarity_rank(g->db.weapons[i].rarity) == rank) count++;
  }
  if (count == 0) return -1;
  int pick = rand() % count;
  for (int i = 0; i < g->db.weapon_count; i++) {
    if (rarity_rank(g->db.weapons[i].rarity) == rank) {
      if (pick-- == 0) return i;
    }
  }
  return -1;
}

static int roll_item_index_with_bias(Game *g) {
  float bias = player_rarity_bias(&g->player, &g->db);
  int rank = roll_rarity_rank(bias);
  int idx = pick_item_index_by_rarity(g, rank);
  if (idx >= 0) return idx;
  for (int r = rank - 1; r >= 0; r--) {
    idx = pick_item_index_by_rarity(g, r);
    if (idx >= 0) return idx;
  }
  return rand() % g->db.item_count;
}

static int roll_weapon_index_with_bias(Game *g) {
  float bias = player_rarity_bias(&g->player, &g->db);
  int rank = roll_rarity_rank(bias);
  int idx = pick_weapon_index_by_rarity(g, rank);
  if (idx >= 0) return idx;
  for (int r = rank - 1; r >= 0; r--) {
    idx = pick_weapon_index_by_rarity(g, r);
    if (idx >= 0) return idx;
  }
  return rand() % g->db.weapon_count;
}

static void draw_text(SDL_Renderer *r, TTF_Font *font, int x, int y, SDL_Color color, const char *text) {
  if (!font) return;
  SDL_Surface *surf = TTF_RenderText_Blended(font, text, color);
  if (!surf) return;
  SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
  SDL_Rect dst = { x, y, surf->w, surf->h };
  SDL_FreeSurface(surf);
  SDL_RenderCopy(r, tex, NULL, &dst);
  SDL_DestroyTexture(tex);
}

static void draw_text_centered(SDL_Renderer *r, TTF_Font *font, int cx, int y, SDL_Color color, const char *text) {
  if (!font) return;
  SDL_Surface *surf = TTF_RenderText_Blended(font, text, color);
  if (!surf) return;
  SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
  SDL_Rect dst = { cx - surf->w / 2, y, surf->w, surf->h };
  SDL_FreeSurface(surf);
  SDL_RenderCopy(r, tex, NULL, &dst);
  SDL_DestroyTexture(tex);
}

static void draw_text_centered_outline(SDL_Renderer *r, TTF_Font *font, int cx, int y, SDL_Color text, SDL_Color outline, int thickness, const char *msg) {
  if (!font || !msg) return;
  for (int dy = -thickness; dy <= thickness; dy++) {
    for (int dx = -thickness; dx <= thickness; dx++) {
      if (dx == 0 && dy == 0) continue;
      draw_text_centered(r, font, cx + dx, y + dy, outline, msg);
    }
  }
  draw_text_centered(r, font, cx, y, text, msg);
}

static void draw_sword_orbit(Game *g, int offset_x, int offset_y, float cam_x, float cam_y) {
  WeaponSlot *slot = find_weapon_slot(&g->player, &g->db, "sword");
  if (!slot) return;
  WeaponDef *w = &g->db.weapons[slot->def_index];
  int count = slot->level;
  if (count < 1) count = 1;
  float orbit_radius = w->range * SWORD_ORBIT_RANGE_SCALE;
  float base_angle = g->player.sword_orbit_angle;

  for (int s = 0; s < count; s++) {
    float angle = base_angle + (6.28318f * (float)s / (float)count);
    float angle_hit = angle + 1.570796f;
    float sx = g->player.x + cosf(angle) * orbit_radius;
    float sy = g->player.y + sinf(angle) * orbit_radius;
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

static void trigger_item_popup(Game *g, ItemDef *it) {
  if (!g || !it) return;
  snprintf(g->item_popup_name, sizeof(g->item_popup_name), "%s", it->name);
  g->item_popup_timer = ITEM_POPUP_DURATION;
}

static void update_window_view(Game *g) {
  int w = WINDOW_W;
  int h = WINDOW_H;
  if (g && g->window) SDL_GetWindowSize(g->window, &w, &h);
  g->window_w = w;
  g->window_h = h;
  g->view_w = w;
  g->view_h = h;
  if (g->view_w < 200) g->view_w = 200;
  if (g->view_h < 200) g->view_h = 200;
}

static int strcasestr_simple(const char *haystack, const char *needle) {
  if (!haystack || !needle || !needle[0]) return 0;
  size_t nlen = strlen(needle);
  for (const char *h = haystack; *h; h++) {
    size_t i = 0;
    for (; i < nlen; i++) {
      if (!h[i]) return 0;
      if (tolower((unsigned char)h[i]) != tolower((unsigned char)needle[i])) break;
    }
    if (i == nlen) return 1;
  }
  return 0;
}

static void game_reset(Game *g) {
  update_window_view(g);
  g->spawn_timer = 0.0f;
  g->kills = 0;
  g->xp = 0;
  g->level = 1;
  g->xp_to_next = 10;
  g->game_time = 0.0f;
  g->last_item_index = -1;
  g->item_popup_timer = 0.0f;
  g->item_popup_name[0] = '\0';
  g->scythe_id_counter = 0;
  g->mode = MODE_START;
  g->pause_return_mode = MODE_START;
  g->time_scale = 1.0f;
  g->boss_event_cd = 0.0f;
  g->boss_countdown_timer = 0.0f;
  g->boss_timer = 0.0f;
  g->boss_timer_max = 0.0f;
  g->boss.active = 0;
  g->boss_def_index = 0;
  g->boss_snapshot.valid = 0;
  g->debug_show_range = 1;
  g->debug_show_items = 0;  /* hidden by default, toggle with key 8 */
  g->start_page = 0;
  g->start_scroll = 0.0f;
  g->ultimate_cd = 0.0f;
  g->choice_count = 0;
  g->selected_character = -1;
  g->rerolls = 2;  /* 2 rerolls per run */
  g->high_roll_used = 0;  /* high roll available once per run */
  for (int i = 0; i < MAX_ENEMIES; i++) g->enemies[i].active = 0;
  for (int i = 0; i < MAX_BULLETS; i++) g->bullets[i].active = 0;
  for (int i = 0; i < MAX_DROPS; i++) g->drops[i].active = 0;
  for (int i = 0; i < MAX_PUDDLES; i++) g->puddles[i].active = 0;

  Player *p = &g->player;
  p->x = ARENA_W * 0.5f;
  p->y = ARENA_H * 0.5f;
  /* Center camera on player */
  g->camera_x = p->x - g->view_w * 0.5f;
  g->camera_y = p->y - g->view_h * 0.5f;
  float max_cam_x = ARENA_W - g->view_w;
  float max_cam_y = ARENA_H - g->view_h;
  if (max_cam_x < 0.0f) max_cam_x = 0.0f;
  if (max_cam_y < 0.0f) max_cam_y = 0.0f;
  g->camera_x = clampf(g->camera_x, 0.0f, max_cam_x);
  g->camera_y = clampf(g->camera_y, 0.0f, max_cam_y);
  p->base = (Stats){0};
  p->base.max_hp = 1100;
  p->base.move_speed = 1.0f;
  p->hp = p->base.max_hp;
  p->ultimate_move_to_as_timer = 0.0f;
  p->move_dir_x = 0.0f;
  p->move_dir_y = 1.0f;
  p->is_moving = 0;
  p->scythe_throw_angle = 0.0f;
  p->sword_orbit_angle = -1.570796f;

  int chest_spawned = 0;
  int attempts = 0;
  while (chest_spawned < 3 && attempts++ < 200) {
    float x = 80.0f + frandf() * (ARENA_W - 160.0f);
    float y = 80.0f + frandf() * (ARENA_H - 160.0f);
    float dx = x - p->x;
    float dy = y - p->y;
    if (dx * dx + dy * dy < 600.0f * 600.0f) continue;
    int too_close = 0;
    for (int i = 0; i < MAX_DROPS; i++) {
      if (!g->drops[i].active) continue;
      if (g->drops[i].type != 2) continue;
      float ddx = x - g->drops[i].x;
      float ddy = y - g->drops[i].y;
      if (ddx * ddx + ddy * ddy < 400.0f * 400.0f) { too_close = 1; break; }
    }
    if (too_close) continue;
    spawn_chest(g, x, y);
    chest_spawned++;
  }
  stats_clear(&p->bonus);
  weapons_clear(p);
  p->passive_count = 0;
  build_start_page(g);
}

static void level_up(Game *g) {
  g->level += 1;
  g->xp_to_next = 10 + g->level * 5;
  g->mode = MODE_LEVELUP;
  build_levelup_choices(g);
}

static void wave_start(Game *g) {
  g->mode = MODE_WAVE;
  g->spawn_timer = 0.0f;
}

static void handle_player_pickups(Game *g, float dt) {
  Player *p = &g->player;
  Stats total = player_total_stats(p, &g->db);
  float xp_magnet_range = 150.0f + total.xp_magnet;  /* XP orbs start moving toward player */
  float pickup_range = 20.0f;        /* actual pickup distance */
  float health_pickup_range = 30.0f; /* health pickup distance */
  float chest_pickup_range = 26.0f;
  
  for (int i = 0; i < MAX_DROPS; i++) {
    Drop *d = &g->drops[i];
    if (!d->active) continue;
    float dx = d->x - p->x;
    float dy = d->y - p->y;
    float dist2 = dx * dx + dy * dy;
    float dist = sqrtf(dist2);
    
    float pickup_dist = (d->type == 0) ? pickup_range : (d->type == 1 ? health_pickup_range : chest_pickup_range);
    if (dist < pickup_dist) {
      if (d->type == 0) {
        g->xp += (int)d->value;
        if (g->xp >= g->xp_to_next) {
          g->xp -= g->xp_to_next;
          level_up(g);
        }
        float kill_chance = player_xp_kill_chance(p, &g->db);
        if (kill_chance > 0.0f && frandf() < kill_chance) {
          int nearest = find_nearest_enemy(g, p->x, p->y);
          if (nearest >= 0) {
            g->enemies[nearest].hp = 0.0f;
            log_combatf(g, "xp_kill proc on %s", enemy_label(g, &g->enemies[nearest]));
          }
        }
      } else if (d->type == 1) {
        p->hp = clampf(p->hp + d->value, 0.0f, total.max_hp);
      } else {
        level_up(g);
      }
      d->active = 0;
      continue;
    }

    /* Once magnetized, keep flying until picked up */
    if (d->type != 2 && (dist < xp_magnet_range || d->magnetized)) {
      d->magnetized = 1;
      /* Accelerate magnet speed over time */
      d->magnet_speed += 400.0f * dt;
      if (d->magnet_speed > 600.0f) d->magnet_speed = 600.0f;

      /* Move toward player */
      if (dist > 0.0001f) {
        float nx = -dx / dist;
        float ny = -dy / dist;
        d->x += nx * d->magnet_speed * dt;
        d->y += ny * d->magnet_speed * dt;
      }
    }
  }
}
static void update_bullets(Game *g, float dt) {
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

static void update_enemies(Game *g, float dt) {
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
    if (e->burn_timer > 0.0f) {
      e->burn_timer -= dt;
      e->hp -= 4.0f * dt;
    }
    if (e->bleed_timer > 0.0f) {
      e->bleed_timer -= dt;
      e->hp -= e->bleed_stacks * 1.5f * dt;
      if (e->bleed_timer <= 0.0f) e->bleed_stacks = 0;
    }
    if (e->slow_timer > 0.0f) e->slow_timer -= dt;
    if (e->stun_timer > 0.0f) e->stun_timer -= dt;
    if (e->armor_shred_timer > 0.0f) e->armor_shred_timer -= dt;
    if (e->sword_hit_cd > 0.0f) e->sword_hit_cd -= dt;
    
    /* Slow aura from items - constantly slows enemies in range */
    float aura_range = player_slow_aura(p, &g->db);
    if (aura_range > 0.0f && dist < aura_range) {
      e->slow_timer = 0.5f; /* Keep refreshing while in range */
    }

    /* Burn aura from items - constantly burns enemies in range */
    float burn_range = player_burn_aura(p, &g->db);
    if (burn_range > 0.0f && dist < burn_range) {
      if (e->burn_timer <= 0.0f) {
        log_combatf(g, "burn_aura applied to %s", enemy_label(g, e));
      }
      e->burn_timer = 0.5f; /* Keep refreshing while in range */
    }

    if (e->stun_timer <= 0.0f &&
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

    if (e->stun_timer <= 0.0f && strcmp(def->role, "charger") == 0) {
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

    if (e->stun_timer <= 0.0f && strcmp(def->role, "turret") != 0) {
      if (e->charge_time > 0.0f) {
        e->x += e->vx * dt;
        e->y += e->vy * dt;
        e->charge_time -= dt;
      } else {
        float vx = dx;
        float vy = dy;
        vec_norm(&vx, &vy);
        float slow_mul = (e->slow_timer > 0.0f) ? 0.5f : 1.0f;
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
        /* Drop XP orb */
        spawn_drop(g, e->x, e->y, 0, 1 + rand() % 2);
        /* Drop health pickup sometimes */
        if (frandf() < 0.05f) spawn_drop(g, e->x, e->y, 1, 10 + rand() % 10);
      }
    }
  }
}

static void fire_weapons(Game *g, float dt) {
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
            /* skip boss hit this tick */
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
    
    /* Add slow chance from items */
    slow_chance += player_slow_on_hit(p, &g->db);
    slow_chance = clampf(slow_chance, 0.0f, 1.0f);

    /* Add burn chance from items */
    burn_chance += item_burn;
    burn_chance = clampf(burn_chance, 0.0f, 1.0f);

    /* Lightning Zone - damages all enemies in circular range */
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
            /* Lightning has a chance to stun */
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

    /* Scythe - spiral throw toward map edge */
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

    /* Vampire Bite - pulsates around player, bites enemies in range */
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
          /* Spawn bite effect on enemy */
          spawn_weapon_fx(g, 1, en->x, en->y, 0.0f, 0.6f, e);
          /* Lifesteal */
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

    /* Daggers - throw at up to 3 targets in range */
    if (weapon_is(w, "daggers")) {
      float range = w->range;
      float range2 = range * range;
      int max_targets = 3 + (slot->level - 1); /* More targets at higher levels */
      if (max_targets > 6) max_targets = 6;
      
      /* Find up to max_targets enemies in range, sorted by distance */
      int targets[6] = {-1, -1, -1, -1, -1, -1};
      float dists[6] = {999999.0f, 999999.0f, 999999.0f, 999999.0f, 999999.0f, 999999.0f};
      
      for (int e = 0; e < MAX_ENEMIES; e++) {
        if (!g->enemies[e].active) continue;
        if (g->enemies[e].spawn_invuln > 0.0f) continue;
        float ex = g->enemies[e].x - p->x;
        float ey = g->enemies[e].y - p->y;
        float d2 = ex * ex + ey * ey;
        if (d2 > range2) continue;
        
        /* Insert into sorted list */
        for (int t = 0; t < max_targets; t++) {
          if (d2 < dists[t]) {
            /* Shift others down */
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
      
      /* Throw dagger at each target */
      float speed = w->projectile_speed > 0 ? w->projectile_speed : 400.0f;
      for (int t = 0; t < max_targets; t++) {
        if (targets[t] < 0) continue;
        Enemy *en = &g->enemies[targets[t]];
        float dx = en->x - p->x;
        float dy = en->y - p->y;
        vec_norm(&dx, &dy);
        float angle = atan2f(dy, dx);
        
        /* Spawn dagger visual and damage */
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

    float speed = w->projectile_speed > 0 ? w->projectile_speed : 400.0f;

    int pellets = (w->pellets > 0) ? w->pellets : 1;
    float spread = (w->spread > 0) ? w->spread : 0.0f;
    for (int pidx = 0; pidx < pellets; pidx++) {
      float angle = atan2f(ty, tx) + frand_range(-spread, spread) * (3.14159f / 180.0f);
      float vx = cosf(angle) * speed;
      float vy = sinf(angle) * speed;
      float shot_damage = player_roll_crit_damage(&stats, w, damage);
      spawn_bullet(g, p->x, p->y, vx, vy, shot_damage, w->pierce, w->homing, 1,
                   slot->def_index, bleed_chance, burn_chance, slow_chance, stun_chance, shred_chance);
    }

    float level_cd = clampf(1.0f - 0.05f * (slot->level - 1), 0.7f, 1.0f);
    slot->cd_timer = w->cooldown * cooldown_scale * level_cd;
  }
}

/* Ultimate abilities - each character can have a unique one */
static void ultimate_kill_all(Game *g) {
  /* Default ultimate - kills all enemies on screen */
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (g->enemies[i].active) {
      g->enemies[i].hp = 0.0f;
    }
  }
}

static void ultimate_shift_speed(Game *g) {
  if (!g) return;
  g->player.ultimate_move_to_as_timer = 30.0f;
}

static void activate_ultimate(Game *g) {
  /* Get character's ultimate type */
  const char *ult_type = "kill_all";
  if (g->selected_character >= 0 && g->selected_character < g->db.character_count) {
    ult_type = g->db.characters[g->selected_character].ultimate;
  }
  
  /* Dispatch to appropriate ultimate */
  if (strcmp(ult_type, "kill_all") == 0) {
    ultimate_kill_all(g);
  } else if (strcmp(ult_type, "shift_speed") == 0) {
    ultimate_shift_speed(g);
  } else {
    /* Unknown ultimate - fallback to kill_all */
    ultimate_kill_all(g);
  }
}

static void update_game(Game *g, float dt) {
  const Uint8 *keys = SDL_GetKeyboardState(NULL);
  Player *p = &g->player;
  Stats stats = player_total_stats(p, &g->db);

  /* Ultimate cooldown tick */
  if (g->ultimate_cd > 0.0f) g->ultimate_cd -= dt;
  if (g->ultimate_cd < 0.0f) g->ultimate_cd = 0.0f;
  if (g->item_popup_timer > 0.0f) g->item_popup_timer -= dt;
  if (p->ultimate_move_to_as_timer > 0.0f) {
    p->ultimate_move_to_as_timer -= dt;
    if (p->ultimate_move_to_as_timer < 0.0f) p->ultimate_move_to_as_timer = 0.0f;
  }

  float speed = 150.0f * (1.0f + stats.move_speed);
  float vx = 0.0f;
  float vy = 0.0f;
  if (keys[SDL_SCANCODE_W]) vy -= 1.0f;
  if (keys[SDL_SCANCODE_S]) vy += 1.0f;
  if (keys[SDL_SCANCODE_A]) vx -= 1.0f;
  if (keys[SDL_SCANCODE_D]) vx += 1.0f;
  vec_norm(&vx, &vy);
  p->is_moving = (vx != 0.0f || vy != 0.0f);
  if (p->is_moving) {
    p->move_dir_x = vx;
    p->move_dir_y = vy;
  }
  p->x = clampf(p->x + vx * speed * dt, 20.0f, ARENA_W - 20.0f);
  p->y = clampf(p->y + vy * speed * dt, 20.0f, ARENA_H - 20.0f);

  if (stats.hp_regen > 0.0f) {
    p->hp = clampf(p->hp + stats.hp_regen * dt, 0.0f, stats.max_hp);
  }

  /* Update camera - scroll when player reaches 4/5 of view edge */
  float scroll_margin_x = g->view_w * 0.2f;
  float scroll_margin_y = g->view_h * 0.2f;
  float player_screen_x = p->x - g->camera_x;
  float player_screen_y = p->y - g->camera_y;
  
  if (player_screen_x > g->view_w - scroll_margin_x) {
    g->camera_x = p->x - (g->view_w - scroll_margin_x);
  }
  if (player_screen_x < scroll_margin_x) {
    g->camera_x = p->x - scroll_margin_x;
  }
  if (player_screen_y > g->view_h - scroll_margin_y) {
    g->camera_y = p->y - (g->view_h - scroll_margin_y);
  }
  if (player_screen_y < scroll_margin_y) {
    g->camera_y = p->y - scroll_margin_y;
  }
  float max_cam_x = ARENA_W - g->view_w;
  float max_cam_y = ARENA_H - g->view_h;
  if (max_cam_x < 0.0f) max_cam_x = 0.0f;
  if (max_cam_y < 0.0f) max_cam_y = 0.0f;
  g->camera_x = clampf(g->camera_x, 0.0f, max_cam_x);
  g->camera_y = clampf(g->camera_y, 0.0f, max_cam_y);

  update_sword_orbit(g, dt);
  fire_weapons(g, dt);
  update_bullets(g, dt);
  update_weapon_fx(g, dt);
  update_puddles(g, dt);
  update_enemies(g, dt);

  if (g->mode == MODE_LEVELUP && (g->levelup_chosen >= 0 || g->levelup_selected_count > 0) && g->levelup_fade > 0.0f) {
    float now = (float)SDL_GetTicks() / 1000.0f;
    if (now - g->levelup_fade >= 0.5f) {
      g->mode = MODE_WAVE;
      g->levelup_chosen = -1;
      g->levelup_selected_count = 0;
    }
  }

  handle_player_pickups(g, dt);

  /* Update game time and spawn enemies continuously */
  g->game_time += dt;
  g->spawn_timer -= dt;
  if (g->spawn_timer <= 0.0f) {
    if (g->db.enemy_count > 0) {
      int def_index = rand() % g->db.enemy_count;
      /* Every 60 seconds, spawn tougher enemy type */
      int difficulty_tier = (int)(g->game_time / 60.0f);
      if (difficulty_tier > 0 && frandf() < 0.2f + difficulty_tier * 0.1f) {
        def_index = (def_index + difficulty_tier) % g->db.enemy_count;
      }
      spawn_enemy(g, def_index);
      /* Spawn rate increases over time */
      float base_rate = 1.0f - g->game_time * 0.005f;
      g->spawn_timer = clampf(base_rate, 0.15f, 1.0f);
    } else {
      g->spawn_timer = 1.0f;
    }
  }

  if (p->hp <= 0.0f) {
    g->mode = MODE_GAMEOVER;
  }
}

static void update_boss_event(Game *g, float dt) {
  const Uint8 *keys = SDL_GetKeyboardState(NULL);
  Player *p = &g->player;
  Stats stats = player_total_stats(p, &g->db);

  if (g->boss_event_cd > 0.0f) {
    g->boss_event_cd -= dt;
    if (g->boss_event_cd < 0.0f) g->boss_event_cd = 0.0f;
  }

  if (g->boss_countdown_timer > 0.0f) {
    g->boss_countdown_timer -= dt;
    if (g->boss_countdown_timer <= 0.0f) {
      float angle = frandf() * 6.28318f;
      float dist = 280.0f + frandf() * 80.0f;
      float bx = clampf(g->player.x + cosf(angle) * dist, 40.0f, ARENA_W - 40.0f);
      float by = clampf(g->player.y + sinf(angle) * dist, 40.0f, ARENA_H - 40.0f);
      spawn_boss(g, bx, by);
    }
  }

  if (g->boss_timer > 0.0f) {
    g->boss_timer -= dt;
    if (g->boss_timer < 0.0f) g->boss_timer = 0.0f;
  }
  if (g->boss.sword_hit_cd > 0.0f) {
    g->boss.sword_hit_cd -= dt;
    if (g->boss.sword_hit_cd < 0.0f) g->boss.sword_hit_cd = 0.0f;
  }

  /* Ultimate cooldown tick */
  if (g->ultimate_cd > 0.0f) g->ultimate_cd -= dt;
  if (g->ultimate_cd < 0.0f) g->ultimate_cd = 0.0f;
  if (g->item_popup_timer > 0.0f) g->item_popup_timer -= dt;
  if (g->boss_event_cd > 0.0f) {
    g->boss_event_cd -= dt;
    if (g->boss_event_cd < 0.0f) g->boss_event_cd = 0.0f;
  }
  if (p->ultimate_move_to_as_timer > 0.0f) {
    p->ultimate_move_to_as_timer -= dt;
    if (p->ultimate_move_to_as_timer < 0.0f) p->ultimate_move_to_as_timer = 0.0f;
  }

  float speed = 150.0f * (1.0f + stats.move_speed);
  float vx = 0.0f;
  float vy = 0.0f;
  if (keys[SDL_SCANCODE_W]) vy -= 1.0f;
  if (keys[SDL_SCANCODE_S]) vy += 1.0f;
  if (keys[SDL_SCANCODE_A]) vx -= 1.0f;
  if (keys[SDL_SCANCODE_D]) vx += 1.0f;
  vec_norm(&vx, &vy);
  p->is_moving = (vx != 0.0f || vy != 0.0f);
  if (p->is_moving) {
    p->move_dir_x = vx;
    p->move_dir_y = vy;
  }
  p->is_moving = (vx != 0.0f || vy != 0.0f);
  if (p->is_moving) {
    p->move_dir_x = vx;
    p->move_dir_y = vy;
  }
  p->x = clampf(p->x + vx * speed * dt, 20.0f, ARENA_W - 20.0f);
  p->y = clampf(p->y + vy * speed * dt, 20.0f, ARENA_H - 20.0f);

  if (stats.hp_regen > 0.0f) {
    p->hp = clampf(p->hp + stats.hp_regen * dt, 0.0f, stats.max_hp);
  }

  float scroll_margin_x = g->view_w * 0.2f;
  float scroll_margin_y = g->view_h * 0.2f;
  float player_screen_x = p->x - g->camera_x;
  float player_screen_y = p->y - g->camera_y;

  if (player_screen_x > g->view_w - scroll_margin_x) {
    g->camera_x = p->x - (g->view_w - scroll_margin_x);
  }
  if (player_screen_x < scroll_margin_x) {
    g->camera_x = p->x - scroll_margin_x;
  }
  if (player_screen_y > g->view_h - scroll_margin_y) {
    g->camera_y = p->y - (g->view_h - scroll_margin_y);
  }
  if (player_screen_y < scroll_margin_y) {
    g->camera_y = p->y - scroll_margin_y;
  }
  float max_cam_x = ARENA_W - g->view_w;
  float max_cam_y = ARENA_H - g->view_h;
  if (max_cam_x < 0.0f) max_cam_x = 0.0f;
  if (max_cam_y < 0.0f) max_cam_y = 0.0f;
  g->camera_x = clampf(g->camera_x, 0.0f, max_cam_x);
  g->camera_y = clampf(g->camera_y, 0.0f, max_cam_y);

  update_sword_orbit(g, dt);
  fire_weapons(g, dt);
  update_bullets(g, dt);
  update_weapon_fx(g, dt);
  update_puddles(g, dt);

  if (g->boss.active) {
    const BossDef *def = &g_boss_defs[g->boss.def_index];
    float dx = p->x - g->boss.x;
    float dy = p->y - g->boss.y;
    float dist2 = dx * dx + dy * dy;
    float dist = sqrtf(dist2);
    if (dist > 0.001f && g->boss.hazard_timer <= 0.0f) {
      float nx = dx / dist;
      float ny = dy / dist;
      g->boss.x += nx * def->speed * dt;
      g->boss.y += ny * def->speed * dt;
    }

    if (g->boss.attack_timer > 0.0f) g->boss.attack_timer -= dt;
    float hit_range = def->radius + 14.0f;
    if (dist < hit_range && g->boss.attack_timer <= 0.0f) {
      float dmg = damage_after_armor(def->damage, stats.armor);
      p->hp -= dmg;
      g->boss.attack_timer = def->attack_cooldown;
    }

    if (g->boss.wave_cd > 0.0f) g->boss.wave_cd -= dt;
    if (g->boss.slam_cd > 0.0f) g->boss.slam_cd -= dt;
    if (g->boss.hazard_cd > 0.0f) g->boss.hazard_cd -= dt;

    g->boss.beam_angle += def->beam_rot_speed * dt;
    if (g->boss.beam_angle > 6.28318f) g->boss.beam_angle -= 6.28318f;

    float angle = g->boss.beam_angle;
    float lx = cosf(angle);
    float ly = sinf(angle);
    float proj = dx * lx + dy * ly;
    if (proj > 0.0f && proj < def->beam_length) {
      float perp = fabsf(dx * (-ly) + dy * lx);
      if (perp <= def->beam_width * 0.5f) {
        float dmg = damage_after_armor(def->beam_dps * dt, stats.armor);
        p->hp -= dmg;
      }
    }

    int hazard_active = 0;
    if (g->boss.hazard_timer > 0.0f) {
      g->boss.hazard_timer -= dt;
      if (g->boss.hazard_timer < 0.0f) g->boss.hazard_timer = 0.0f;
      hazard_active = 1;

      float safe_r = def->hazard_safe_radius;
      int safe = 0;
      for (int i = 0; i < 3; i++) {
        float sx = g->boss.safe_x[i];
        float sy = g->boss.safe_y[i];
        float ddx = p->x - sx;
        float ddy = p->y - sy;
        if (ddx * ddx + ddy * ddy <= safe_r * safe_r) {
          safe = 1;
          break;
        }
      }
      if (!safe) {
        float dmg = damage_after_armor(def->hazard_dps * dt, stats.armor);
        p->hp -= dmg;
      }
    } else if (g->boss.hazard_cd <= 0.0f) {
      g->boss.hazard_timer = def->hazard_duration;
      g->boss.hazard_cd = def->hazard_cooldown;
      float radius = (g->view_w < g->view_h ? g->view_w : g->view_h) * 0.35f;
      if (radius < 120.0f) radius = 120.0f;
      for (int i = 0; i < 3; i++) {
        int attempts = 0;
        while (attempts++ < 20) {
          float angle = frandf() * 6.28318f;
          float dist = radius * (0.4f + frandf() * 0.6f);
          float sx = clampf(p->x + cosf(angle) * dist, 40.0f, ARENA_W - 40.0f);
          float sy = clampf(p->y + sinf(angle) * dist, 40.0f, ARENA_H - 40.0f);
          int ok = 1;
          for (int j = 0; j < i; j++) {
            float dxs = sx - g->boss.safe_x[j];
            float dys = sy - g->boss.safe_y[j];
            if (dxs * dxs + dys * dys < def->hazard_safe_radius * def->hazard_safe_radius * 3.0f) { ok = 0; break; }
          }
          if (!ok) continue;
          g->boss.safe_x[i] = sx;
          g->boss.safe_y[i] = sy;
          break;
        }
      }
      hazard_active = 1;
    }

    if (!hazard_active && g->boss.wave_cd <= 0.0f) {
      int n = def->wave_bullets;
      if (n < 6) n = 6;
      for (int i = 0; i < n; i++) {
        float a = (6.28318f * i) / n;
        float vx = cosf(a) * def->wave_speed;
        float vy = sinf(a) * def->wave_speed;
        spawn_bullet(g, g->boss.x, g->boss.y, vx, vy, def->damage * 0.7f, 0, 0, 0,
                     -1, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
      }
      g->boss.wave_cd = def->wave_cooldown;
    }

    if (!hazard_active && g->boss.slam_cd <= 0.0f && dist < def->slam_radius) {
      float dmg = damage_after_armor(def->slam_damage, stats.armor);
      p->hp -= dmg;
      if (dist > 0.001f) {
        float nx = dx / dist;
        float ny = dy / dist;
        p->x = clampf(p->x + nx * 80.0f, 20.0f, ARENA_W - 20.0f);
        p->y = clampf(p->y + ny * 80.0f, 20.0f, ARENA_H - 20.0f);
      }
      g->boss.slam_cd = def->slam_cooldown;
    }

    if (g->boss.hp <= 0.0f) {
      end_boss_event(g, 1);
      return;
    }
  }

  if (p->hp <= 0.0f || g->boss_timer <= 0.0f) {
    end_boss_event(g, 0);
  }
}

static void layout_levelup(Game *g, int screen_w, int screen_h) {
  int card_w = 260;
  int card_h = 120;
  int spacing = 40;
  int total_w = g->choice_count * card_w + (g->choice_count - 1) * spacing;
  int start_x = (screen_w - total_w) / 2;
  int start_y = (screen_h - card_h) / 2;
  int mid = g->choice_count / 2;
  for (int i = 0; i < g->choice_count; i++) {
    int y = start_y;
    if (g->choice_count >= 3 && i == mid) y -= 46;
    g->choices[i].rect = (SDL_Rect){ start_x + i * (card_w + spacing), y, card_w, card_h };
  }
}

static void render_game(Game *g) {
  SDL_SetRenderDrawColor(g->renderer, 8, 10, 16, 255);
  SDL_RenderClear(g->renderer);

  int win_w = g->window_w;
  int win_h = g->window_h;
  int view_w = g->view_w;
  int view_h = g->view_h;

  int cam_x = (int)g->camera_x;
  int cam_y = (int)g->camera_y;
  int offset_x = 0;
  int offset_y = 0;

  /* Arena background - tile 128x128 ground texture with camera offset */
  /* Buffer of 2 tiles beyond visible area for smoother scrolling */
  if (g->tex_ground) {
    int tile_size = 128;
    int buffer = tile_size * 2;  /* 2 extra tiles on each side */
    int start_tx = ((cam_x - buffer) / tile_size) * tile_size;
    int start_ty = ((cam_y - buffer) / tile_size) * tile_size;
    for (int ty = start_ty; ty < cam_y + view_h + buffer; ty += tile_size) {
      for (int tx = start_tx; tx < cam_x + view_w + buffer; tx += tile_size) {
        SDL_Rect dst = { offset_x + tx - cam_x, offset_y + ty - cam_y, tile_size, tile_size };
        SDL_RenderCopy(g->renderer, g->tex_ground, NULL, &dst);
      }
    }
  } else {
    SDL_Rect arena = { offset_x, offset_y, view_w, view_h };
    SDL_SetRenderDrawColor(g->renderer, 15, 18, 28, 255);
    SDL_RenderFillRect(g->renderer, &arena);
  }
  
  /* No wall border for infinite map - just clip rendering */
  SDL_Rect clip = { offset_x, offset_y, view_w, view_h };
  SDL_RenderSetClipRect(g->renderer, &clip);

  /* Alchemist puddles (above ground, behind player/enemies) */
  if (g->mode != MODE_LEVELUP) {
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
  }

  /* Player with sprite */
  int px = (int)(offset_x + g->player.x - cam_x);
  int py = (int)(offset_y + g->player.y - cam_y);
  int player_size = 90;
  
  /* Draw lightning zone effect if player has the weapon */
  for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
    if (!g->player.weapons[i].active) continue;
    WeaponDef *w = &g->db.weapons[g->player.weapons[i].def_index];
    if (weapon_is(w, "lightning_zone")) {
      float range = w->range * (1.0f + 0.1f * (g->player.weapons[i].level - 1));
      float cd_ratio = g->player.weapons[i].cd_timer / w->cooldown;
      /* Pulsing effect - brighter when about to fire */
      int alpha = (int)(40 + (1.0f - cd_ratio) * 60);
      SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
      /* Draw filled circle for the zone */
      SDL_Color zone_color = { 100, 150, 255, (Uint8)alpha };
      draw_filled_circle(g->renderer, px, py, (int)range, zone_color);
      /* Draw border */
      SDL_Color border_color = { 150, 200, 255, (Uint8)(alpha + 40) };
      draw_circle(g->renderer, px, py, (int)range, border_color);
      /* Flash effect when firing */
      if (cd_ratio > 0.95f) {
        SDL_Color flash = { 200, 220, 255, 120 };
        draw_filled_circle(g->renderer, px, py, (int)range, flash);
        /* Render lightning zone sprite */
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

  /* Enemies with sprite */
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!g->enemies[i].active) continue;
    EnemyDef *def = &g->db.enemies[g->enemies[i].def_index];
    int ex = (int)(offset_x + g->enemies[i].x - cam_x);
    int ey = (int)(offset_y + g->enemies[i].y - cam_y);
    
    int size = 64;
    if (strcmp(def->role, "boss") == 0) size = 96;
    
    /* Status effect visuals - burn glow only */
    if (g->enemies[i].burn_timer > 0.0f) {
      draw_glow(g->renderer, ex, ey, size/2 + 8, (SDL_Color){255, 100, 0, 100});
    }
    
    float now = (float)SDL_GetTicks() / 1000.0f;
    float hit_age = (g->enemies[i].hit_timer > 0.0f) ? (now - g->enemies[i].hit_timer) : 999.0f;
    int hit_flash = (hit_age >= 0.0f && hit_age < 0.5f);

    /* Draw enemy sprite with color tint for slow */
    if (g->tex_enemy) {
      float move_dx = 0.0f;
      float move_dy = 0.0f;
      SDL_RendererFlip enemy_flip = SDL_FLIP_NONE;
      if (strcmp(def->role, "turret") != 0) {
        if (g->enemies[i].charge_time > 0.0f) {
          move_dx = g->enemies[i].vx;
          move_dy = g->enemies[i].vy;
        } else if (g->enemies[i].stun_timer <= 0.0f) {
          move_dx = g->player.x - g->enemies[i].x;
          move_dy = g->player.y - g->enemies[i].y;
        }
        if (fabsf(move_dx) > 0.01f || fabsf(move_dy) > 0.01f) {
          if (move_dx < 0.0f) enemy_flip = SDL_FLIP_HORIZONTAL;
        }
      }
      /* Tint red when hit, blue when slowed */
      if (hit_flash) {
        SDL_SetTextureColorMod(g->tex_enemy, 255, 120, 120);
      } else if (g->enemies[i].slow_timer > 0.0f) {
        SDL_SetTextureColorMod(g->tex_enemy, 150, 180, 255);
      } else {
        SDL_SetTextureColorMod(g->tex_enemy, 255, 255, 255);
      }
      SDL_Rect dst = { ex - size/2, ey - size/2, size, size };
      SDL_RenderCopyEx(g->renderer, g->tex_enemy, NULL, &dst, 0.0, NULL, enemy_flip);
    } else {
      /* Fallback circle - tint blue when slowed */
      if (hit_flash) {
        draw_filled_circle(g->renderer, ex, ey, size/2, (SDL_Color){220, 100, 100, 255});
      } else if (g->enemies[i].slow_timer > 0.0f) {
        draw_filled_circle(g->renderer, ex, ey, size/2, (SDL_Color){100, 150, 200, 255});
      } else {
        draw_filled_circle(g->renderer, ex, ey, size/2, (SDL_Color){100, 200, 100, 255});
      }
    }

    /* Hit flash overlay for visibility */
    if (hit_flash) {
      float t = clampf(1.0f - (hit_age / 0.5f), 0.0f, 1.0f);
      Uint8 alpha = (Uint8)(120.0f * t + 40.0f);
      draw_glow(g->renderer, ex, ey, size/2 + 6, (SDL_Color){255, 80, 80, alpha});
    }

    /* Health bar for all enemies */
    float hp_pct = clampf(g->enemies[i].hp / g->enemies[i].max_hp, 0.0f, 1.0f);
    if (hp_pct < 1.0f) {
      int bar_w = size + 4;
      SDL_Rect bar_bg = { ex - bar_w/2, ey - size/2 - 8, bar_w, 4 };
      SDL_Rect bar_fg = { ex - bar_w/2, ey - size/2 - 8, (int)(bar_w * hp_pct), 4 };
      SDL_SetRenderDrawColor(g->renderer, 20, 20, 20, 200);
      SDL_RenderFillRect(g->renderer, &bar_bg);
      SDL_SetRenderDrawColor(g->renderer, 90, 220, 90, 255);
      SDL_RenderFillRect(g->renderer, &bar_fg);
    }
  }

  if (g->mode == MODE_BOSS_EVENT && g->boss.active && g->boss.hazard_timer > 0.0f) {
    SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g->renderer, 140, 30, 30, 50);
    SDL_Rect hazard = { offset_x, offset_y, view_w, view_h };
    SDL_RenderFillRect(g->renderer, &hazard);
  }

  if (g->mode == MODE_BOSS_EVENT && g->boss.active) {
    const BossDef *def = &g_boss_defs[g->boss.def_index];
    int bx = (int)(offset_x + g->boss.x - cam_x);
    int by = (int)(offset_y + g->boss.y - cam_y);
    int br = (int)def->radius;
    draw_glow(g->renderer, bx, by, br + 14, (SDL_Color){255, 120, 60, 140});
    draw_filled_circle(g->renderer, bx, by, br + 6, (SDL_Color){160, 60, 40, 255});
    draw_circle(g->renderer, bx, by, br + 6, (SDL_Color){240, 200, 120, 255});

    float hp_pct = clampf(g->boss.hp / g->boss.max_hp, 0.0f, 1.0f);
    int bar_w = br * 2 + 12;
    SDL_Rect bar_bg = { bx - bar_w/2, by - br - 16, bar_w, 6 };
    SDL_Rect bar_fg = { bx - bar_w/2, by - br - 16, (int)(bar_w * hp_pct), 6 };
    SDL_SetRenderDrawColor(g->renderer, 20, 20, 20, 220);
    SDL_RenderFillRect(g->renderer, &bar_bg);
    SDL_SetRenderDrawColor(g->renderer, 240, 120, 80, 255);
    SDL_RenderFillRect(g->renderer, &bar_fg);
  }

  if (g->mode == MODE_BOSS_EVENT && g->boss.active) {
    const BossDef *def = &g_boss_defs[g->boss.def_index];
    float angle = g->boss.beam_angle;
    int bx = (int)(offset_x + g->boss.x - cam_x);
    int by = (int)(offset_y + g->boss.y - cam_y);
    int beam_w = (int)(def->beam_width * 2.0f);
    int beam_len = (int)def->beam_length;
    if (beam_w < 8) beam_w = 8;
    if (beam_len < 200) beam_len = 200;

    SDL_Rect dst = { bx, by - beam_w / 2, beam_len, beam_w };
    SDL_Point pivot = { 0, beam_w / 2 };
    double angle_deg = angle * (180.0 / 3.14159);
    SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
    if (g->tex_laser_beam) {
      SDL_SetTextureAlphaMod(g->tex_laser_beam, 220);
      SDL_RenderCopyEx(g->renderer, g->tex_laser_beam, NULL, &dst, angle_deg, &pivot, SDL_FLIP_NONE);
      SDL_SetTextureAlphaMod(g->tex_laser_beam, 255);
    } else {
      SDL_SetRenderDrawColor(g->renderer, 120, 220, 255, 140);
      SDL_RenderDrawLine(g->renderer, bx, by,
                         bx + (int)(cosf(angle) * beam_len),
                         by + (int)(sinf(angle) * beam_len));
      SDL_SetRenderDrawColor(g->renderer, 60, 170, 255, 200);
      for (int i = 1; i <= beam_w / 6; i++) {
        int ox = (int)(-sinf(angle) * i * 3.0f);
        int oy = (int)(cosf(angle) * i * 3.0f);
        SDL_RenderDrawLine(g->renderer, bx + ox, by + oy,
                           bx + (int)(cosf(angle) * beam_len) + ox,
                           by + (int)(sinf(angle) * beam_len) + oy);
        SDL_RenderDrawLine(g->renderer, bx - ox, by - oy,
                           bx + (int)(cosf(angle) * beam_len) - ox,
                           by + (int)(sinf(angle) * beam_len) - oy);
      }
    }
  }

  if (g->mode == MODE_BOSS_EVENT && g->boss.active && g->boss.hazard_timer > 0.0f) {
    const BossDef *def = &g_boss_defs[g->boss.def_index];
    for (int i = 0; i < 3; i++) {
      int sx = (int)(offset_x + g->boss.safe_x[i] - cam_x);
      int sy = (int)(offset_y + g->boss.safe_y[i] - cam_y);
      draw_glow(g->renderer, sx, sy, (int)def->hazard_safe_radius + 12, (SDL_Color){80, 220, 140, 120});
      draw_circle(g->renderer, sx, sy, (int)def->hazard_safe_radius, (SDL_Color){120, 255, 170, 220});
    }
  }

  /* Bullets with trails */
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!g->bullets[i].active) continue;
    int bx = (int)(offset_x + g->bullets[i].x - cam_x);
    int by = (int)(offset_y + g->bullets[i].y - cam_y);
    if (g->bullets[i].from_player) {
      draw_glow(g->renderer, bx, by, 8, (SDL_Color){100, 220, 255, 80});
      draw_filled_circle(g->renderer, bx, by, 4, (SDL_Color){150, 230, 255, 255});
    } else {
      /* Enemy projectile - use goo_bolt sprite */
      if (g->tex_enemy_bolt) {
        SDL_Rect dst = { bx - 16, by - 16, 32, 32 };
        SDL_RenderCopy(g->renderer, g->tex_enemy_bolt, NULL, &dst);
      } else {
        draw_glow(g->renderer, bx, by, 8, (SDL_Color){255, 100, 100, 80});
        draw_filled_circle(g->renderer, bx, by, 4, (SDL_Color){255, 120, 120, 255});
      }
    }
  }

  /* Weapon visual effects */
  for (int i = 0; i < MAX_WEAPON_FX; i++) {
    if (!g->weapon_fx[i].active) continue;
    WeaponFX *fx = &g->weapon_fx[i];
    float progress = fx->timer / fx->duration;
    
    if (fx->type == 0) {
      float sx = fx->x + cosf(fx->angle) * fx->radius;
      float sy = fx->y + sinf(fx->angle) * fx->radius;
      int draw_x = (int)(offset_x + sx - cam_x);
      int draw_y = (int)(offset_y + sy - cam_y);
      int scythe_size = 64;
      int alpha = 220;
      if (g->tex_scythe) {
        SDL_Rect dst = { draw_x - scythe_size/2, draw_y - scythe_size/2, scythe_size, scythe_size };
        double angle_deg = fx->angle * (180.0 / 3.14159);
        SDL_SetTextureAlphaMod(g->tex_scythe, (Uint8)alpha);
        SDL_RenderCopyEx(g->renderer, g->tex_scythe, NULL, &dst, angle_deg + 90, NULL, SDL_FLIP_NONE);
        SDL_SetTextureAlphaMod(g->tex_scythe, 255);
      } else {
        SDL_SetRenderDrawColor(g->renderer, 200, 80, 220, (Uint8)alpha);
        SDL_RenderDrawLine(g->renderer, draw_x - 10, draw_y - 10, draw_x + 10, draw_y + 10);
        draw_filled_circle(g->renderer, draw_x, draw_y, 6, (SDL_Color){220, 100, 240, (Uint8)alpha});
      }
    }
    else if (fx->type == 1) {
      /* Vampire bite - appears on enemy */
      int target = fx->target_enemy;
      if (target >= 0 && target < MAX_ENEMIES && g->enemies[target].active) {
        int ex = (int)(offset_x + g->enemies[target].x - cam_x);
        int ey = (int)(offset_y + g->enemies[target].y - cam_y);
        int alpha = (int)(255 * (1.0f - progress));
        float scale = 0.5f + progress * 0.5f;  /* Grow from 0.5 to 1.0 */
        int size = (int)(96 * scale);  /* 3x bigger (was 32) */
        
        if (g->tex_bite) {
          SDL_Rect dst = { ex - size/2, ey - size/2, size, size };
          SDL_SetTextureAlphaMod(g->tex_bite, (Uint8)alpha);
          SDL_RenderCopy(g->renderer, g->tex_bite, NULL, &dst);
          SDL_SetTextureAlphaMod(g->tex_bite, 255);
        } else {
          /* Fallback - red fangs effect */
          SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
          draw_glow(g->renderer, ex, ey, size/2, (SDL_Color){180, 0, 40, (Uint8)(alpha * 0.6f)});
          /* Draw two fang marks */
          draw_filled_circle(g->renderer, ex - 12, ey - 8, 6, (SDL_Color){220, 20, 60, (Uint8)alpha});
          draw_filled_circle(g->renderer, ex + 12, ey - 8, 6, (SDL_Color){220, 20, 60, (Uint8)alpha});
        }
      }
    }
    else if (fx->type == 2) {
      /* Dagger throw - travels from player to target */
      int target = fx->target_enemy;
      if (target >= 0 && target < MAX_ENEMIES) {
        float start_x = fx->x;
        float start_y = fx->y;
        float end_x = g->enemies[target].active ? g->enemies[target].x : start_x + cosf(fx->angle) * 150.0f;
        float end_y = g->enemies[target].active ? g->enemies[target].y : start_y + sinf(fx->angle) * 150.0f;
        
        /* Interpolate position */
        float curr_x = start_x + (end_x - start_x) * progress;
        float curr_y = start_y + (end_y - start_y) * progress;
        
        int dx = (int)(offset_x + curr_x - cam_x);
        int dy = (int)(offset_y + curr_y - cam_y);
        int alpha = progress < 0.8f ? 255 : (int)(255 * (1.0f - (progress - 0.8f) / 0.2f));
        
        if (g->tex_dagger) {
          SDL_Rect dst = { dx - 12, dy - 12, 24, 24 };
          double angle_deg = fx->angle * (180.0 / 3.14159);
          SDL_SetTextureAlphaMod(g->tex_dagger, (Uint8)alpha);
          SDL_RenderCopyEx(g->renderer, g->tex_dagger, NULL, &dst, angle_deg + 90, NULL, SDL_FLIP_NONE);
          SDL_SetTextureAlphaMod(g->tex_dagger, 255);
        } else {
          /* Fallback - simple dagger shape */
          SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
          int tip_x = dx + (int)(cosf(fx->angle) * 10);
          int tip_y = dy + (int)(sinf(fx->angle) * 10);
          SDL_SetRenderDrawColor(g->renderer, 180, 180, 200, (Uint8)alpha);
          SDL_RenderDrawLine(g->renderer, dx, dy, tip_x, tip_y);
          draw_filled_circle(g->renderer, tip_x, tip_y, 3, (SDL_Color){200, 200, 220, (Uint8)alpha});
        }
      }
    }
  }

  /* Drops with sparkle effect */
  for (int i = 0; i < MAX_DROPS; i++) {
    if (!g->drops[i].active) continue;
    int dx = (int)(offset_x + g->drops[i].x - cam_x);
    int dy = (int)(offset_y + g->drops[i].y - cam_y);
    if (g->drops[i].type == 0) {
      /* XP orb (0.6x size) */
      if (g->tex_exp_orb) {
        SDL_Rect dst = { dx - 7, dy - 7, 14, 14 };
        SDL_RenderCopy(g->renderer, g->tex_exp_orb, NULL, &dst);
      } else {
        draw_glow(g->renderer, dx, dy, 6, (SDL_Color){80, 180, 255, 80});
        draw_filled_circle(g->renderer, dx, dy, 3, (SDL_Color){100, 200, 255, 255});
      }
    } else if (g->drops[i].type == 1) {
      /* Health pack */
      if (g->tex_health_flask) {
        SDL_Rect dst = { dx - 10, dy - 10, 20, 20 };
        SDL_RenderCopy(g->renderer, g->tex_health_flask, NULL, &dst);
      } else {
        draw_glow(g->renderer, dx, dy, 12, (SDL_Color){255, 80, 120, 100});
        draw_diamond(g->renderer, dx, dy, 6, (SDL_Color){255, 100, 130, 255});
      }
    } else {
      /* Chest */
      draw_glow(g->renderer, dx, dy, 16, (SDL_Color){255, 200, 90, 120});
      SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
      SDL_SetRenderDrawColor(g->renderer, 140, 90, 30, 255);
      SDL_Rect box = { dx - 13, dy - 10, 26, 20 };
      SDL_RenderFillRect(g->renderer, &box);
      SDL_SetRenderDrawColor(g->renderer, 230, 190, 90, 255);
      SDL_RenderDrawRect(g->renderer, &box);
      SDL_RenderDrawLine(g->renderer, dx - 10, dy, dx + 10, dy);
      SDL_RenderDrawLine(g->renderer, dx, dy - 10, dx, dy + 10);
    }
  }

  /* Reset clip rect for UI */
  SDL_RenderSetClipRect(g->renderer, NULL);

    SDL_Color text = { 230, 231, 234, 255 };
    char buf[128];
    Stats stats = player_total_stats(&g->player, &g->db);

    /* Top bar */
    SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g->renderer, 10, 12, 18, 210);
    SDL_Rect top_bar = { 0, 0, win_w, 38 };
    SDL_RenderFillRect(g->renderer, &top_bar);

    /* HP bar */
    float hp_pct = clampf(g->player.hp / stats.max_hp, 0.0f, 1.0f);
    SDL_Rect hp_bg = { 20, 10, 120, 18 };
    SDL_Rect hp_fill = { 20, 10, (int)(120 * hp_pct), 18 };
    SDL_SetRenderDrawColor(g->renderer, 40, 40, 48, 255);
    SDL_RenderFillRect(g->renderer, &hp_bg);
    SDL_SetRenderDrawColor(g->renderer, 200, 60, 60, 255);
    SDL_RenderFillRect(g->renderer, &hp_fill);
    SDL_SetRenderDrawColor(g->renderer, 90, 90, 110, 255);
    SDL_RenderDrawRect(g->renderer, &hp_bg);
    snprintf(buf, sizeof(buf), "Lv %d", g->level);
    draw_text(g->renderer, g->font, 150, 10, text, buf);
    float xp_pct = clampf((float)g->xp / (float)g->xp_to_next, 0.0f, 1.0f);
    SDL_Rect xp_bg = { 200, 10, 140, 18 };
    SDL_Rect xp_fill = { 200, 10, (int)(140 * xp_pct), 18 };
    SDL_SetRenderDrawColor(g->renderer, 30, 45, 60, 255);
    SDL_RenderFillRect(g->renderer, &xp_bg);
    SDL_SetRenderDrawColor(g->renderer, 80, 160, 230, 255);
    SDL_RenderFillRect(g->renderer, &xp_fill);
    SDL_SetRenderDrawColor(g->renderer, 80, 100, 130, 255);
    SDL_RenderDrawRect(g->renderer, &xp_bg);
    int mins = (int)(g->game_time / 60.0f);
    int secs = (int)g->game_time % 60;
    snprintf(buf, sizeof(buf), "Time %d:%02d", mins, secs);
    draw_text(g->renderer, g->font, 380, 10, text, buf);
    snprintf(buf, sizeof(buf), "Kills %d", g->kills);
    draw_text(g->renderer, g->font, 520, 10, text, buf);

    if (g->ultimate_cd > 0.0f) {
      snprintf(buf, sizeof(buf), "[SPACE] Ultimate: %.0fs", g->ultimate_cd);
    } else {
      snprintf(buf, sizeof(buf), "[SPACE] Ultimate: READY!");
    }
    draw_text(g->renderer, g->font, 20, 88, text, buf);
    draw_text(g->renderer, g->font, 20, 108, text, "TAB/P pause  F1 spawn  F2/F3 speed  F4 range");

  if (g->mode == MODE_BOSS_EVENT) {
      float time_pct = 0.0f;
      if (g->boss_timer_max > 0.0f) {
        time_pct = clampf(g->boss_timer / g->boss_timer_max, 0.0f, 1.0f);
      }
      int time_bar_w = 200;
      int time_bar_h = 8;
      int time_bar_x = win_w - time_bar_w - 20;
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
      draw_text(g->renderer, g->font, win_w - 220, 22, text, buf);

      if (g->boss.active) {
        const BossDef *def = &g_boss_defs[g->boss.def_index];
        float hp_pct = clampf(g->boss.hp / g->boss.max_hp, 0.0f, 1.0f);
        int bar_w = 200;
        SDL_Rect bar_bg = { win_w - bar_w - 20, 36, bar_w, 8 };
        SDL_Rect bar_fg = { win_w - bar_w - 20, 36, (int)(bar_w * hp_pct), 8 };
        SDL_SetRenderDrawColor(g->renderer, 20, 20, 20, 220);
        SDL_RenderFillRect(g->renderer, &bar_bg);
        SDL_SetRenderDrawColor(g->renderer, 240, 120, 80, 255);
        SDL_RenderFillRect(g->renderer, &bar_fg);
        draw_text(g->renderer, g->font, win_w - bar_w - 20, 48, text, def->name);
      }

      if (g->boss_countdown_timer > 0.0f) {
        int count = (int)ceilf(g->boss_countdown_timer);
        SDL_Color c = {255, 220, 120, 255};
        char cbuf[8];
        snprintf(cbuf, sizeof(cbuf), "%d", count);
        draw_text_centered_outline(g->renderer, g->font_title_big ? g->font_title_big : g->font_title,
                                   win_w / 2, win_h / 2 - 40, c, (SDL_Color){0, 0, 0, 200}, 2, cbuf);
      }
    }

    if (g->item_popup_timer > 0.0f && g->item_popup_name[0]) {
      float t = clampf(g->item_popup_timer / ITEM_POPUP_DURATION, 0.0f, 1.0f);
      SDL_Color c = {255, 220, 120, (Uint8)(255.0f * t)};
      int tw = 0, th = 0;
      if (g->font) TTF_SizeText(g->font, g->item_popup_name, &tw, &th);
      int tx = win_w - tw - 20;
      if (tx < 20) tx = 20;
      draw_text(g->renderer, g->font, tx, 50, c, g->item_popup_name);
    }

    int wx = 20;
    int wy = 130;
    draw_text(g->renderer, g->font, wx, wy, text, "Weapons:");
    wy += 18;
    for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
      if (!g->player.weapons[i].active) continue;
      WeaponDef *w = &g->db.weapons[g->player.weapons[i].def_index];
      snprintf(buf, sizeof(buf), "%s Lv%d", w->name, g->player.weapons[i].level);
      draw_text(g->renderer, g->font, wx, wy, text, buf);
      wy += 18;
    }

    if (g->debug_show_range) {
      float max_range = 0.0f;
      for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
        if (!g->player.weapons[i].active) continue;
        WeaponDef *w = &g->db.weapons[g->player.weapons[i].def_index];
        float r = w->range;
        if (r > max_range) max_range = r;
      }
      if (max_range > 0.0f) {
        SDL_Color c = { 80, 180, 220, 160 };
        draw_circle(g->renderer, px, py, (int)max_range, c);
      }
    }

  if (g->mode == MODE_LEVELUP) {
    SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g->renderer, 5, 8, 15, 200);
    SDL_Rect overlay = { 0, 0, win_w, win_h };
    SDL_RenderFillRect(g->renderer, &overlay);
    
    /* Level up title */
    SDL_Color gold = {255, 215, 100, 255};
    char lvl_str[64];
    snprintf(lvl_str, sizeof(lvl_str), "LEVEL UP! (Lv %d)", g->level);
    draw_text(g->renderer, g->font, win_w / 2 - 80, 200, gold, lvl_str);
    draw_text(g->renderer, g->font, win_w / 2 - 60, 230, text, "Choose one:");
    
    layout_levelup(g, win_w, win_h);
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    float fade = 1.0f;
    if ((g->levelup_chosen >= 0 || g->levelup_selected_count > 0) && g->levelup_fade > 0.0f) {
      float now = (float)SDL_GetTicks() / 1000.0f;
      fade = clampf((now - g->levelup_fade) / 0.5f, 0.0f, 1.0f);
    }
    for (int i = 0; i < g->choice_count; i++) {
      LevelUpChoice *choice = &g->choices[i];
      int base_orb_size = levelup_orb_size(&choice->rect);
      float cx = (float)(choice->rect.x + choice->rect.w / 2);
      float cy = (float)(choice->rect.y + choice->rect.h / 2);
      float radius = (float)base_orb_size * 0.5f;
      float dx = (float)mx - cx;
      float dy = (float)my - cy;
      int hovered = (dx * dx + dy * dy) <= (radius * radius);
      if (g->levelup_chosen >= 0 || g->levelup_selected_count > 0) hovered = 0;

      /* Rarity orb background (no card box) */
      const char *rarity_str = (choice->type == 0) ? g->db.items[choice->index].rarity : g->db.weapons[choice->index].rarity;
      SDL_Texture *orb_tex = rarity_orb_texture(g, rarity_str);
      if (orb_tex) {
        int orb_size = hovered ? (int)(base_orb_size * 1.08f) : base_orb_size;
        SDL_Rect orb_dst = {
          choice->rect.x + (choice->rect.w - orb_size) / 2,
          choice->rect.y + (choice->rect.h - orb_size) / 2,
          orb_size,
          orb_size
        };
        float alpha_mul = 1.0f;
        if (g->levelup_selected_count > 0) {
          int keep = 0;
          for (int k = 0; k < g->levelup_selected_count; k++) {
            if (g->levelup_selected[k] == i) { keep = 1; break; }
          }
          alpha_mul = keep ? 1.0f : (1.0f - fade);
        } else if (g->levelup_chosen >= 0) {
          alpha_mul = (i == g->levelup_chosen) ? 1.0f : (1.0f - fade);
        }
        SDL_SetTextureAlphaMod(orb_tex, (Uint8)(235.0f * alpha_mul));
        SDL_RenderCopy(g->renderer, orb_tex, NULL, &orb_dst);
        SDL_SetTextureAlphaMod(orb_tex, 255);
      }

      if (choice->type == 0) {
        ItemDef *it = &g->db.items[choice->index];
        float alpha_mul = 1.0f;
        if (g->levelup_selected_count > 0) {
          int keep = 0;
          for (int k = 0; k < g->levelup_selected_count; k++) {
            if (g->levelup_selected[k] == i) { keep = 1; break; }
          }
          alpha_mul = keep ? 1.0f : (1.0f - fade);
        } else if (g->levelup_chosen >= 0) {
          alpha_mul = (i == g->levelup_chosen) ? 1.0f : (1.0f - fade);
        }
        SDL_Color outline = {0, 0, 0, (Uint8)(220.0f * alpha_mul)};
        SDL_Color white = hovered ? (SDL_Color){255, 245, 210, (Uint8)(255.0f * alpha_mul)} : (SDL_Color){255, 255, 255, (Uint8)(255.0f * alpha_mul)};
        int tx = choice->rect.x + choice->rect.w / 2;
        SDL_Color tag_color = {200, 220, 255, (Uint8)(220.0f * alpha_mul)};
        draw_text_centered_outline(g->renderer, g->font, tx, choice->rect.y + 2, tag_color, outline, 1, "ITEM");
        int y = choice->rect.y + 18;
        TTF_Font *font_main = hovered ? (g->font_title_big ? g->font_title_big : g->font_title) : g->font_title;
        draw_text_centered_outline(g->renderer, font_main, tx, y, white, outline, 2, it->name);
        
        if (it->desc[0]) {
          int dy = choice->rect.y + 38;
          draw_text_centered_outline(g->renderer, font_main, tx, dy, white, outline, 2, it->desc);
        }
        
        char statline[128];
        Stats *s = &it->stats;
        int pos = 0;
        const char *desc = it->desc;
        statline[0] = 0;
        int show_damage = (s->damage != 0) && !strcasestr_simple(desc, "dmg") && !strcasestr_simple(desc, "damage");
        int show_hp = (s->max_hp != 0) && !strcasestr_simple(desc, "hp");
        int show_as = (s->attack_speed != 0) && !strcasestr_simple(desc, "attack speed") && !strcasestr_simple(desc, " as");
        int show_speed = (s->move_speed != 0) && !strcasestr_simple(desc, "speed") && !strcasestr_simple(desc, "spd");
        int show_armor = (s->armor != 0) && !strcasestr_simple(desc, "armor") && !strcasestr_simple(desc, "arm");
        int show_dodge = (s->dodge != 0) && !strcasestr_simple(desc, "dodge");
        int show_regen = (s->hp_regen != 0) && !strcasestr_simple(desc, "regen") && !strcasestr_simple(desc, "hp/s");
        if (show_damage) pos += snprintf(statline + pos, sizeof(statline) - pos, "DMG%+.0f%% ", s->damage * 100);
        if (show_hp) pos += snprintf(statline + pos, sizeof(statline) - pos, "HP%+.0f ", s->max_hp);
        if (show_as) pos += snprintf(statline + pos, sizeof(statline) - pos, "AS%+.0f%% ", s->attack_speed * 100);
        if (show_speed) pos += snprintf(statline + pos, sizeof(statline) - pos, "SPD%+.0f%% ", s->move_speed * 100);
        if (show_armor) pos += snprintf(statline + pos, sizeof(statline) - pos, "ARM%+.0f ", s->armor);
        if (show_dodge) pos += snprintf(statline + pos, sizeof(statline) - pos, "DDG%+.0f%% ", s->dodge * 100);
        if (show_regen) pos += snprintf(statline + pos, sizeof(statline) - pos, "REG%+.1f ", s->hp_regen);
        int sy = choice->rect.y + 60;
        if (pos > 0) draw_text_centered_outline(g->renderer, font_main, tx, sy, white, outline, 2, statline);
      } else {
        WeaponDef *w = &g->db.weapons[choice->index];
        float alpha_mul = 1.0f;
        if (g->levelup_selected_count > 0) {
          int keep = 0;
          for (int k = 0; k < g->levelup_selected_count; k++) {
            if (g->levelup_selected[k] == i) { keep = 1; break; }
          }
          alpha_mul = keep ? 1.0f : (1.0f - fade);
        } else if (g->levelup_chosen >= 0) {
          alpha_mul = (i == g->levelup_chosen) ? 1.0f : (1.0f - fade);
        }
        SDL_Color outline = {0, 0, 0, (Uint8)(220.0f * alpha_mul)};
        SDL_Color white = hovered ? (SDL_Color){255, 245, 210, (Uint8)(255.0f * alpha_mul)} : (SDL_Color){255, 255, 255, (Uint8)(255.0f * alpha_mul)};
        int tx = choice->rect.x + choice->rect.w / 2;
        SDL_Color tag_color = {255, 200, 140, (Uint8)(220.0f * alpha_mul)};
        draw_text_centered_outline(g->renderer, g->font, tx, choice->rect.y + 2, tag_color, outline, 1, "WEAPON");
        int y = choice->rect.y + 18;
        TTF_Font *font_main = hovered ? (g->font_title_big ? g->font_title_big : g->font_title) : g->font_title;
        draw_text_centered_outline(g->renderer, font_main, tx, y, white, outline, 2, w->name);
        char statline[128];
        snprintf(statline, sizeof(statline), "%s  DMG %.0f", w->type, w->damage);
        int dy = choice->rect.y + 38;
        draw_text_centered_outline(g->renderer, font_main, tx, dy, white, outline, 2, statline);
        snprintf(statline, sizeof(statline), "CD %.2fs  RNG %.0f", w->cooldown, w->range);
        int sy = choice->rect.y + 60;
        draw_text_centered_outline(g->renderer, font_main, tx, sy, white, outline, 2, statline);
      }
    }
    
    int max_orb_bottom = 0;
    for (int i = 0; i < g->choice_count; i++) {
      int bottom = g->choices[i].rect.y + g->choices[i].rect.h;
      if (bottom > max_orb_bottom) max_orb_bottom = bottom;
    }

    /* Reroll button */
    int btn_w = 140;
    int btn_h = 36;
    int btn_x = win_w / 2 - btn_w / 2;
    int btn_y = max_orb_bottom + 20;
    g->reroll_button = (SDL_Rect){ btn_x, btn_y, btn_w, btn_h };
    
    if (g->rerolls > 0) {
      /* Active button */
      SDL_SetRenderDrawColor(g->renderer, 50, 70, 100, 255);
      SDL_RenderFillRect(g->renderer, &g->reroll_button);
      SDL_SetRenderDrawColor(g->renderer, 100, 140, 200, 255);
      SDL_RenderDrawRect(g->renderer, &g->reroll_button);
      char reroll_text[32];
      snprintf(reroll_text, sizeof(reroll_text), "Reroll (%d)", g->rerolls);
      draw_text_centered(g->renderer, g->font, btn_x + btn_w / 2, btn_y + 10, (SDL_Color){200, 220, 255, 255}, reroll_text);
    } else {
      /* Disabled button */
      SDL_SetRenderDrawColor(g->renderer, 30, 35, 45, 255);
      SDL_RenderFillRect(g->renderer, &g->reroll_button);
      SDL_SetRenderDrawColor(g->renderer, 50, 55, 65, 255);
      SDL_RenderDrawRect(g->renderer, &g->reroll_button);
      draw_text_centered(g->renderer, g->font, btn_x + btn_w / 2, btn_y + 10, (SDL_Color){80, 85, 95, 255}, "No Rerolls");
    }
    
    /* High Roll button - below reroll button */
    int hr_btn_y = btn_y + btn_h + 10;
    g->highroll_button = (SDL_Rect){ btn_x, hr_btn_y, btn_w, btn_h };
    
    if (!g->high_roll_used) {
      /* Active button - golden/yellow color for gambling feel */
      SDL_SetRenderDrawColor(g->renderer, 90, 70, 20, 255);
      SDL_RenderFillRect(g->renderer, &g->highroll_button);
      SDL_SetRenderDrawColor(g->renderer, 200, 160, 60, 255);
      SDL_RenderDrawRect(g->renderer, &g->highroll_button);
      draw_text_centered(g->renderer, g->font, btn_x + btn_w / 2, hr_btn_y + 10, (SDL_Color){255, 220, 100, 255}, "High Roll!");
    } else {
      /* Disabled button */
      SDL_SetRenderDrawColor(g->renderer, 30, 35, 45, 255);
      SDL_RenderFillRect(g->renderer, &g->highroll_button);
      SDL_SetRenderDrawColor(g->renderer, 50, 55, 65, 255);
      SDL_RenderDrawRect(g->renderer, &g->highroll_button);
      draw_text_centered(g->renderer, g->font, btn_x + btn_w / 2, hr_btn_y + 10, (SDL_Color){80, 85, 95, 255}, "Used");
    }
  }

  if (g->mode == MODE_PAUSE) {
    SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g->renderer, 8, 8, 12, 200);
    SDL_Rect overlay = { 0, 0, win_w, win_h };
    SDL_RenderFillRect(g->renderer, &overlay);
    draw_text_centered(g->renderer, g->font_title, win_w / 2, 180, (SDL_Color){255, 215, 100, 255}, "PAUSED");

    int mx, my;
    SDL_GetMouseState(&mx, &my);

    Stats stats = player_total_stats(&g->player, &g->db);
    SDL_Color label = {170, 175, 190, 255};
    SDL_Color val = {230, 231, 234, 255};
    int x = win_w / 2 - 420;
    int y = 240;
    int line = 20;

    draw_text(g->renderer, g->font, x, y, label, "Max HP:");
    snprintf(buf, sizeof(buf), "%.0f", stats.max_hp);
    draw_text(g->renderer, g->font, x + 160, y, val, buf);
    y += line;

    draw_text(g->renderer, g->font, x, y, label, "Damage:");
    snprintf(buf, sizeof(buf), "+%.0f%%", stats.damage * 100.0f);
    draw_text(g->renderer, g->font, x + 160, y, val, buf);
    y += line;

    draw_text(g->renderer, g->font, x, y, label, "Attack Speed:");
    snprintf(buf, sizeof(buf), "+%.0f%%", stats.attack_speed * 100.0f);
    draw_text(g->renderer, g->font, x + 160, y, val, buf);
    y += line;

    draw_text(g->renderer, g->font, x, y, label, "Move Speed:");
    snprintf(buf, sizeof(buf), "+%.0f%%", stats.move_speed * 100.0f);
    draw_text(g->renderer, g->font, x + 160, y, val, buf);
    y += line;

    draw_text(g->renderer, g->font, x, y, label, "Armor:");
    snprintf(buf, sizeof(buf), "%.0f", stats.armor);
    draw_text(g->renderer, g->font, x + 160, y, val, buf);
    y += line;

    draw_text(g->renderer, g->font, x, y, label, "Dodge:");
    snprintf(buf, sizeof(buf), "%.0f%%", stats.dodge * 100.0f);
    draw_text(g->renderer, g->font, x + 160, y, val, buf);
    y += line;

    draw_text(g->renderer, g->font, x, y, label, "Crit Chance:");
    snprintf(buf, sizeof(buf), "%.0f%%", stats.crit_chance * 100.0f);
    draw_text(g->renderer, g->font, x + 160, y, val, buf);
    y += line;

    draw_text(g->renderer, g->font, x, y, label, "Crit Damage:");
    snprintf(buf, sizeof(buf), "+%.0f%%", stats.crit_damage * 100.0f);
    draw_text(g->renderer, g->font, x + 160, y, val, buf);
    y += line;

    draw_text(g->renderer, g->font, x, y, label, "Cooldown Red:");
    snprintf(buf, sizeof(buf), "%.0f%%", stats.cooldown_reduction * 100.0f);
    draw_text(g->renderer, g->font, x + 160, y, val, buf);
    y += line;

    draw_text(g->renderer, g->font, x, y, label, "XP Magnet:");
    snprintf(buf, sizeof(buf), "%.0f", stats.xp_magnet);
    draw_text(g->renderer, g->font, x + 160, y, val, buf);
    y += line;

    draw_text(g->renderer, g->font, x, y, label, "HP Regen:");
    snprintf(buf, sizeof(buf), "%.1f/s", stats.hp_regen);
    draw_text(g->renderer, g->font, x + 160, y, val, buf);
    y += line;

    int panel_w = 320;
    int panel_x = win_w / 2 + 40;
    int panel_y = 240;
    int line_h = 18;
    int max_visible = 12;
    int weapon_indices[MAX_WEAPON_SLOTS];
    int weapon_count = 0;
    for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
      if (!g->player.weapons[i].active) continue;
      weapon_indices[weapon_count++] = i;
    }
    int show_weapons = weapon_count;
    int extra_weapons = 0;
    if (show_weapons > max_visible) {
      show_weapons = max_visible;
      extra_weapons = 1;
    }
    int item_counts[MAX_ITEMS] = {0};
    int unique_indices[MAX_ITEMS];
    int unique_count = 0;
    for (int i = 0; i < g->player.passive_count; i++) {
      int idx = g->player.passive_items[i];
      if (idx < 0 || idx >= g->db.item_count) continue;
      if (item_counts[idx] == 0) unique_indices[unique_count++] = idx;
      item_counts[idx]++;
    }
    int show_items = unique_count;
    int extra_more = 0;
    if (show_items > max_visible) {
      show_items = max_visible;
      extra_more = 1;
    }
    int list_lines = 0;
    list_lines += 2; /* LAST ITEM header + value */
    list_lines += 1; /* spacer */
    list_lines += 1; /* WEAPONS header */
    list_lines += (show_weapons > 0 ? show_weapons : 1) + extra_weapons;
    list_lines += 1; /* spacer */
    list_lines += 1; /* ITEMS header */
    list_lines += (show_items > 0 ? show_items : 1) + extra_more;
    int panel_h = 20 + list_lines * line_h;

    SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g->renderer, 12, 14, 20, 210);
    SDL_Rect panel = { panel_x, panel_y, panel_w, panel_h };
    SDL_RenderFillRect(g->renderer, &panel);
    SDL_SetRenderDrawColor(g->renderer, 60, 70, 90, 255);
    SDL_RenderDrawRect(g->renderer, &panel);

    SDL_Color header = {255, 220, 100, 255};
    SDL_Color dim = {140, 145, 165, 255};
    int py = panel_y + 10;

    draw_text(g->renderer, g->font, panel_x + 10, py, header, "LAST ITEM");
    py += line_h;
    if (g->last_item_index >= 0 && g->last_item_index < g->db.item_count) {
      ItemDef *it = &g->db.items[g->last_item_index];
      SDL_Color rc = rarity_color(it->rarity);
      draw_text(g->renderer, g->font, panel_x + 12, py, rc, it->name);
    } else {
      draw_text(g->renderer, g->font, panel_x + 12, py, dim, "(none)");
    }
    py += line_h;

    draw_text(g->renderer, g->font, panel_x + 10, py, header, "WEAPONS");
    py += line_h + 2;
    if (weapon_count == 0) {
      draw_text(g->renderer, g->font, panel_x + 12, py, dim, "(none)");
      py += line_h;
    } else {
      for (int i = 0; i < weapon_count && i < show_weapons; i++) {
        int slot = weapon_indices[i];
        WeaponDef *w = &g->db.weapons[g->player.weapons[slot].def_index];
        SDL_Color rc = rarity_color(w->rarity);
        snprintf(buf, sizeof(buf), "%s Lv%d", w->name, g->player.weapons[slot].level);
        draw_text(g->renderer, g->font, panel_x + 12, py, rc, buf);
        py += line_h;
      }
      if (extra_weapons) {
        int remaining = weapon_count - show_weapons;
        snprintf(buf, sizeof(buf), "... +%d more", remaining);
        draw_text(g->renderer, g->font, panel_x + 12, py, dim, buf);
        py += line_h;
      }
    }
    py += line_h / 2;

    snprintf(buf, sizeof(buf), "ITEMS (%d)", g->player.passive_count);
    draw_text(g->renderer, g->font, panel_x + 10, py, header, buf);
    py += line_h + 2;

    int hovered_item_index = -1;
    if (unique_count == 0) {
      draw_text(g->renderer, g->font, panel_x + 12, py, dim, "(none)");
      py += line_h;
    } else {
      for (int i = 0; i < unique_count && i < show_items; i++) {
        int idx = unique_indices[i];
        ItemDef *it = &g->db.items[idx];
        SDL_Color rc = rarity_color(it->rarity);
        SDL_Rect row = { panel_x + 12, py, panel_w - 24, line_h };
        if (mx >= row.x && mx <= row.x + row.w && my >= row.y && my <= row.y + row.h) {
          hovered_item_index = idx;
        }
        if (item_counts[idx] > 1) {
          snprintf(buf, sizeof(buf), "%s x%d", it->name, item_counts[idx]);
          draw_text(g->renderer, g->font, panel_x + 12, py, rc, buf);
        } else {
          draw_text(g->renderer, g->font, panel_x + 12, py, rc, it->name);
        }
        py += line_h;
      }
      if (extra_more) {
        int remaining = unique_count - show_items;
        snprintf(buf, sizeof(buf), "... +%d more", remaining);
        draw_text(g->renderer, g->font, panel_x + 12, py, dim, buf);
        py += line_h;
      }
    }

    draw_text_centered(g->renderer, g->font, win_w / 2, y + 20, (SDL_Color){140, 140, 160, 255}, "Press TAB or P to resume");

    if (hovered_item_index >= 0 && hovered_item_index < g->db.item_count) {
      ItemDef *it = &g->db.items[hovered_item_index];
      if (it->desc[0]) {
        int name_w = 0, name_h = 0, desc_w = 0, desc_h = 0;
        if (g->font) {
          TTF_SizeText(g->font, it->name, &name_w, &name_h);
          TTF_SizeText(g->font, it->desc, &desc_w, &desc_h);
        }
        int pad = 8;
        int box_w = (name_w > desc_w ? name_w : desc_w) + pad * 2;
        int box_h = name_h + desc_h + pad * 3;
        int box_x = win_w - box_w - 20;
        int box_y = win_h - box_h - 20;
        if (box_x < 20) box_x = 20;
        if (box_y < 20) box_y = 20;
        SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g->renderer, 12, 14, 20, 235);
        SDL_Rect tip = { box_x, box_y, box_w, box_h };
        SDL_RenderFillRect(g->renderer, &tip);
        SDL_SetRenderDrawColor(g->renderer, 70, 80, 100, 255);
        SDL_RenderDrawRect(g->renderer, &tip);

        SDL_Color name_c = rarity_color(it->rarity);
        SDL_Color desc_c = (SDL_Color){190, 195, 210, 255};
        draw_text(g->renderer, g->font, box_x + pad, box_y + pad, name_c, it->name);
        draw_text(g->renderer, g->font, box_x + pad, box_y + pad * 2 + name_h, desc_c, it->desc);
      }
    }
  }


  if (g->mode == MODE_START) {
    /* Background */
    SDL_SetRenderDrawColor(g->renderer, 8, 8, 12, 255);
    SDL_RenderClear(g->renderer);

    int split_x = (win_w * 2) / 3;

    /* Right Panel Background */
    SDL_Rect right_panel = { split_x, 0, win_w - split_x, win_h };
    SDL_SetRenderDrawColor(g->renderer, 12, 14, 20, 255);
    SDL_RenderFillRect(g->renderer, &right_panel);
    SDL_SetRenderDrawColor(g->renderer, 40, 45, 60, 255);
    SDL_RenderDrawLine(g->renderer, split_x, 0, split_x, win_h);

    /* Left Panel: Character Grid */
    int margin = 25;
    int cols = 4;
    int name_area_h = 32;  /* Space below card for name */
    int card_w = (split_x - margin * 2 - (cols - 1) * 20) / cols;
    int card_h = card_w + 60;  /* Taller portrait cards */
    int total_h = card_h + name_area_h;
    int start_y = 90;
    
    /* Title */
    draw_text_centered(g->renderer, g->font_title, split_x / 2, 30, (SDL_Color){255, 215, 100, 255}, "SELECT YOUR CHARACTER");
    draw_text_centered(g->renderer, g->font, split_x / 2, 58, (SDL_Color){100, 100, 120, 255}, "Scroll to view more - Hover for details");
    
    /* Mouse for hover */
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    int hovered_idx = -1;

    int shown = g->choice_count;
    float scroll_max = start_scroll_max(g);
    g->start_scroll = clampf(g->start_scroll, 0.0f, scroll_max);
    for (int i = 0; i < shown; i++) {
      int col = i % cols;
      int row = i / cols;
      int card_x = margin + col * (card_w + 20);
      int card_y = start_y + row * (total_h + 12) - (int)g->start_scroll;
      SDL_Rect r = { card_x, card_y, card_w, card_h };
      
      /* Full clickable area includes name */
      SDL_Rect click_r = { card_x, card_y, card_w, total_h };
      g->choices[i].rect = click_r;

      if (card_y + total_h < start_y - 10 || card_y > win_h) {
        continue;
      }

      int hovered = (mx >= click_r.x && mx <= click_r.x + click_r.w && my >= click_r.y && my <= click_r.y + click_r.h);
      if (hovered) hovered_idx = i;

      int char_idx = g->choices[i].index;
      CharacterDef *c = &g->db.characters[char_idx];
      
      /* Portrait fills the entire card */
      if (g->tex_portraits[char_idx]) {
        SDL_RenderCopy(g->renderer, g->tex_portraits[char_idx], NULL, &r);
      } else {
        /* Fallback - draw placeholder */
        SDL_SetRenderDrawColor(g->renderer, 30, 35, 50, 255);
        SDL_RenderFillRect(g->renderer, &r);
        draw_text_centered(g->renderer, g->font_title, r.x + card_w/2, r.y + card_h/2 - 10, (SDL_Color){60, 65, 80, 255}, "?");
      }
      
      /* Border - thicker gold glow for hovered */
      if (hovered) {
        /* Outer glow */
        SDL_SetRenderDrawColor(g->renderer, 180, 150, 60, 100);
        SDL_Rect glow1 = { r.x - 3, r.y - 3, r.w + 6, r.h + 6 };
        SDL_RenderDrawRect(g->renderer, &glow1);
        SDL_Rect glow2 = { r.x - 2, r.y - 2, r.w + 4, r.h + 4 };
        SDL_RenderDrawRect(g->renderer, &glow2);
        /* Main border */
        SDL_SetRenderDrawColor(g->renderer, 220, 190, 90, 255);
        SDL_RenderDrawRect(g->renderer, &r);
      } else {
        SDL_SetRenderDrawColor(g->renderer, 50, 55, 70, 255);
        SDL_RenderDrawRect(g->renderer, &r);
      }
      
      /* Character name centered below portrait */
      SDL_Color name_color = hovered ? (SDL_Color){ 255, 220, 120, 255 } : (SDL_Color){ 180, 180, 200, 255 };
      int name_y = card_y + card_h + 6;
      draw_text_centered(g->renderer, g->font_title, card_x + card_w / 2, name_y, name_color, c->name);
    }

    /* Right Panel: Details */
    if (hovered_idx >= 0) {
      int char_idx = g->choices[hovered_idx].index;
      CharacterDef *c = &g->db.characters[char_idx];
      int rx = split_x + 20;
      int ry = 30;
      int panel_w = win_w - split_x - 40;
      
      /* Large portrait on right panel */
      int big_portrait_w = 200;
      int big_portrait_h = 260;
      SDL_Rect big_portrait = { rx + (panel_w - big_portrait_w) / 2, ry, big_portrait_w, big_portrait_h };
      if (g->tex_portraits[char_idx]) {
        SDL_RenderCopy(g->renderer, g->tex_portraits[char_idx], NULL, &big_portrait);
        /* Gold border */
        SDL_SetRenderDrawColor(g->renderer, 200, 170, 80, 255);
        SDL_RenderDrawRect(g->renderer, &big_portrait);
      }
      ry += big_portrait_h + 15;
      
      /* Character name centered */
      draw_text_centered(g->renderer, g->font_title, rx + panel_w / 2, ry, (SDL_Color){255, 215, 100, 255}, c->name);
      ry += 30;
      
      /* Passive Bonuses */
      draw_text(g->renderer, g->font, rx, ry, (SDL_Color){150, 150, 170, 255}, "Passive Bonuses:");
      ry += 20;
      char sbuf[128];
      Stats *s = &c->stats;
      int has_bonus = 0;
      if (s->max_hp != 0) { snprintf(sbuf, sizeof(sbuf), "Max HP: %+.0f", s->max_hp); draw_text(g->renderer, g->font, rx + 10, ry, s->max_hp > 0 ? (SDL_Color){100, 220, 100, 255} : (SDL_Color){220, 100, 100, 255}, sbuf); ry += 17; has_bonus = 1; }
      if (s->damage != 0) { snprintf(sbuf, sizeof(sbuf), "Damage: %+.0f%%", s->damage * 100); draw_text(g->renderer, g->font, rx + 10, ry, s->damage > 0 ? (SDL_Color){100, 220, 100, 255} : (SDL_Color){220, 100, 100, 255}, sbuf); ry += 17; has_bonus = 1; }
      if (s->attack_speed != 0) { snprintf(sbuf, sizeof(sbuf), "Atk Speed: %+.0f%%", s->attack_speed * 100); draw_text(g->renderer, g->font, rx + 10, ry, s->attack_speed > 0 ? (SDL_Color){100, 220, 100, 255} : (SDL_Color){220, 100, 100, 255}, sbuf); ry += 17; has_bonus = 1; }
      if (s->move_speed != 0) { snprintf(sbuf, sizeof(sbuf), "Speed: %+.0f%%", s->move_speed * 100); draw_text(g->renderer, g->font, rx + 10, ry, s->move_speed > 0 ? (SDL_Color){100, 220, 100, 255} : (SDL_Color){220, 100, 100, 255}, sbuf); ry += 17; has_bonus = 1; }
      if (s->armor != 0) { snprintf(sbuf, sizeof(sbuf), "Armor: %+.0f", s->armor); draw_text(g->renderer, g->font, rx + 10, ry, s->armor > 0 ? (SDL_Color){100, 220, 100, 255} : (SDL_Color){220, 100, 100, 255}, sbuf); ry += 17; has_bonus = 1; }
      if (s->dodge != 0) { snprintf(sbuf, sizeof(sbuf), "Dodge: %+.0f%%", s->dodge * 100); draw_text(g->renderer, g->font, rx + 10, ry, s->dodge > 0 ? (SDL_Color){100, 220, 100, 255} : (SDL_Color){220, 100, 100, 255}, sbuf); ry += 17; has_bonus = 1; }
      if (!has_bonus) { draw_text(g->renderer, g->font, rx + 10, ry, (SDL_Color){100, 100, 120, 255}, "None"); ry += 17; }
      
      ry += 12;
      draw_text(g->renderer, g->font, rx, ry, (SDL_Color){150, 150, 170, 255}, "Starting Weapon:");
      ry += 20;
      int widx = find_weapon(&g->db, c->weapon);
      if (widx >= 0) {
        WeaponDef *w = &g->db.weapons[widx];
        draw_text(g->renderer, g->font, rx + 10, ry, (SDL_Color){100, 180, 255, 255}, w->name);
        ry += 18;
        snprintf(sbuf, sizeof(sbuf), "DMG: %.0f  CD: %.2fs", w->damage, w->cooldown);
        draw_text(g->renderer, g->font, rx + 10, ry, text, sbuf);
        ry += 17;
        snprintf(sbuf, sizeof(sbuf), "Range: %.0f  Type: %s", w->range, w->type);
        draw_text(g->renderer, g->font, rx + 10, ry, text, sbuf);
      }
      
      if (c->rule[0]) {
        ry += 22;
        draw_text(g->renderer, g->font, rx, ry, (SDL_Color){255, 150, 100, 255}, "Special Rule:");
        ry += 20;
        draw_text(g->renderer, g->font, rx + 10, ry, (SDL_Color){200, 180, 150, 255}, c->rule);
      }
    } else {
      int rx = split_x + 20;
      int panel_w = win_w - split_x - 40;
      draw_text_centered(g->renderer, g->font, rx + panel_w / 2, win_h / 2 - 20, (SDL_Color){80, 80, 100, 255}, "Hover over a character");
      draw_text_centered(g->renderer, g->font, rx + panel_w / 2, win_h / 2 + 5, (SDL_Color){80, 80, 100, 255}, "to see details");
    }
  }

  if (g->mode == MODE_GAMEOVER) {
    SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g->renderer, 0, 0, 0, 180);
    SDL_Rect overlay = { 0, 0, win_w, win_h };
    SDL_RenderFillRect(g->renderer, &overlay);
    char buf[128];
    snprintf(buf, sizeof(buf), "Game Over - Level %d", g->level);
    draw_text(g->renderer, g->font, win_w / 2 - 80, win_h / 2 - 40, text, buf);
    int mins = (int)(g->game_time / 60.0f);
    int secs = (int)g->game_time % 60;
    snprintf(buf, sizeof(buf), "Survived %d:%02d  Kills: %d", mins, secs, g->kills);
    draw_text(g->renderer, g->font, win_w / 2 - 100, win_h / 2 - 10, text, buf);
    draw_text(g->renderer, g->font, win_w / 2 - 80, win_h / 2 + 20, text, "Press R to restart");
  }

  /* Debug item/weapon list panel (toggle with key 8) */
  if (g->debug_show_items) {
    int panel_w = 220;
    int panel_x = win_w - panel_w - 10;
    int panel_y = 10;
    int line_h = 18;
    
    /* Semi-transparent background */
    SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g->renderer, 0, 0, 0, 200);
    int panel_h = 40 + (g->player.passive_count + 1) * line_h + 20 + (MAX_WEAPON_SLOTS + 1) * line_h;
    SDL_Rect panel = { panel_x, panel_y, panel_w, panel_h };
    SDL_RenderFillRect(g->renderer, &panel);
    SDL_SetRenderDrawColor(g->renderer, 100, 100, 120, 255);
    SDL_RenderDrawRect(g->renderer, &panel);
    
    int y = panel_y + 8;
    SDL_Color header_color = {255, 220, 100, 255};
    SDL_Color item_color = {180, 200, 180, 255};
    SDL_Color weapon_color = {180, 180, 220, 255};
    SDL_Color count_color = {120, 120, 140, 255};
    
    /* Items section */
    char buf[64];
    snprintf(buf, sizeof(buf), "ITEMS (%d)", g->player.passive_count);
    draw_text(g->renderer, g->font, panel_x + 8, y, header_color, buf);
    y += line_h + 4;
    
    if (g->player.passive_count == 0) {
      draw_text(g->renderer, g->font, panel_x + 12, y, count_color, "(none)");
      y += line_h;
    } else {
      for (int i = 0; i < g->player.passive_count; i++) {
        int idx = g->player.passive_items[i];
        if (idx >= 0 && idx < g->db.item_count) {
          ItemDef *it = &g->db.items[idx];
          SDL_Color rc = rarity_color(it->rarity);
          draw_text(g->renderer, g->font, panel_x + 12, y, rc, it->name);
          y += line_h;
        }
      }
    }
    
    y += 10;
    
    /* Weapons section */
    int weapon_count = 0;
    for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
      if (g->player.weapons[i].active) weapon_count++;
    }
    snprintf(buf, sizeof(buf), "WEAPONS (%d)", weapon_count);
    draw_text(g->renderer, g->font, panel_x + 8, y, header_color, buf);
    y += line_h + 4;
    
    if (weapon_count == 0) {
      draw_text(g->renderer, g->font, panel_x + 12, y, count_color, "(none)");
    } else {
      for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
        if (!g->player.weapons[i].active) continue;
        int def_idx = g->player.weapons[i].def_index;
        if (def_idx >= 0 && def_idx < g->db.weapon_count) {
          WeaponDef *w = &g->db.weapons[def_idx];
          snprintf(buf, sizeof(buf), "%s (Lv%d)", w->name, g->player.weapons[i].level);
          SDL_Color rc = rarity_color(w->rarity);
          draw_text(g->renderer, g->font, panel_x + 12, y, rc, buf);
          y += line_h;
        }
      }
    }
  }

  SDL_RenderPresent(g->renderer);
}

static void handle_levelup_click(Game *g, int mx, int my) {
  if (g->levelup_chosen >= 0 || g->levelup_selected_count > 0) return;
  /* Check reroll button first */
  SDL_Rect rb = g->reroll_button;
  if (g->rerolls > 0 && mx >= rb.x && mx <= rb.x + rb.w && my >= rb.y && my <= rb.y + rb.h) {
    g->rerolls--;
    build_levelup_choices(g);
    return;
  }
  
  /* Check high roll button */
  SDL_Rect hr = g->highroll_button;
  if (!g->high_roll_used && mx >= hr.x && mx <= hr.x + hr.w && my >= hr.y && my <= hr.y + hr.h) {
    g->high_roll_used = 1;
    
    int items_to_grant = 1 + (rand() % 3);
    if (items_to_grant > g->choice_count) items_to_grant = g->choice_count;

    int indices[MAX_LEVELUP_CHOICES];
    for (int i = 0; i < g->choice_count; i++) indices[i] = i;
    for (int i = g->choice_count - 1; i > 0; i--) {
      int j = rand() % (i + 1);
      int tmp = indices[i];
      indices[i] = indices[j];
      indices[j] = tmp;
    }

    g->levelup_selected_count = 0;
    int active_weapons = 0;
    for (int w = 0; w < MAX_WEAPON_SLOTS; w++) {
      if (g->player.weapons[w].active) active_weapons++;
    }
    int free_slots = MAX_WEAPON_SLOTS - active_weapons;
    for (int k = 0; k < g->choice_count && g->levelup_selected_count < items_to_grant; k++) {
      int i = indices[k];
      if (g->choices[i].type == 0) {
        g->levelup_selected[g->levelup_selected_count++] = i;
        continue;
      }
      int level = 0;
      int wi = g->choices[i].index;
      int owned = weapon_is_owned(&g->player, wi, &level);
      if (owned) {
        if (level < MAX_WEAPON_LEVEL) {
          g->levelup_selected[g->levelup_selected_count++] = i;
        }
      } else if (free_slots > 0) {
        g->levelup_selected[g->levelup_selected_count++] = i;
        free_slots--;
      }
    }

    int applied_indices[MAX_LEVELUP_CHOICES];
    int applied_count = 0;
    for (int k = 0; k < g->levelup_selected_count; k++) {
      int i = g->levelup_selected[k];
      int applied = 0;
      if (g->choices[i].type == 0) {
        int before = g->player.passive_count;
        ItemDef *it = &g->db.items[g->choices[i].index];
        apply_item(&g->player, &g->db, it, g->choices[i].index);
        if (g->player.passive_count > before) {
          g->last_item_index = g->choices[i].index;
          trigger_item_popup(g, it);
          applied = 1;
        }
      } else {
        int wi = g->choices[i].index;
        int before_level = 0;
        int before_owned = weapon_is_owned(&g->player, wi, &before_level);
        if (can_equip_weapon(&g->player, wi)) {
          equip_weapon(&g->player, wi);
        } else {
          for (int w = 0; w < MAX_WEAPON_SLOTS; w++) {
            if (g->player.weapons[w].active && g->player.weapons[w].def_index == wi) {
              if (g->player.weapons[w].level < MAX_WEAPON_LEVEL) g->player.weapons[w].level += 1;
              break;
            }
          }
        }
        int after_level = 0;
        int after_owned = weapon_is_owned(&g->player, wi, &after_level);
        if ((!before_owned && after_owned) || (after_owned && after_level > before_level)) applied = 1;
      }
      if (applied) applied_indices[applied_count++] = i;
    }
    for (int k = 0; k < applied_count; k++) g->levelup_selected[k] = applied_indices[k];
    g->levelup_selected_count = applied_count;
    g->levelup_chosen = -1;
    g->levelup_fade = (float)SDL_GetTicks() / 1000.0f;
    return;
  }
  
  for (int i = 0; i < g->choice_count; i++) {
    SDL_Rect r = g->choices[i].rect;
    int orb_size = levelup_orb_size(&r);
    float cx = (float)(r.x + r.w / 2);
    float cy = (float)(r.y + r.h / 2);
    float radius = (float)orb_size * 0.5f;
    float dx = (float)mx - cx;
    float dy = (float)my - cy;
    if (dx * dx + dy * dy <= radius * radius) {
      if (g->choices[i].type == 0) {
        ItemDef *it = &g->db.items[g->choices[i].index];
        apply_item(&g->player, &g->db, it, g->choices[i].index);
        g->last_item_index = g->choices[i].index;
        trigger_item_popup(g, it);
      } else {
        int wi = g->choices[i].index;
        if (can_equip_weapon(&g->player, wi)) {
          equip_weapon(&g->player, wi);
        } else {
          /* Upgrade existing weapon if we already have it */
          for (int w = 0; w < MAX_WEAPON_SLOTS; w++) {
            if (g->player.weapons[w].active && g->player.weapons[w].def_index == wi) {
              if (g->player.weapons[w].level < MAX_WEAPON_LEVEL) g->player.weapons[w].level += 1;
              break;
            }
          }
        }
      }
      g->levelup_chosen = i;
      g->levelup_selected_count = 0;
      g->levelup_fade = (float)SDL_GetTicks() / 1000.0f;
      return;
    }
  }
}

#ifndef UNIT_TESTS
int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  g_log = fopen("log.txt", "w");
  g_combat_log = fopen("combat_log.txt", "w");
  log_line("Starting game...");
  SetUnhandledExceptionFilter(crash_handler);
  srand((unsigned int)time(NULL));

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
    log_linef("SDL init failed: %s", SDL_GetError());
    return 1;
  }
  log_line("SDL init ok");

  if (TTF_Init() != 0) {
    log_linef("TTF init failed: %s", TTF_GetError());
    return 1;
  }
  log_line("TTF init ok");

  Game game;
  memset(&game, 0, sizeof(game));
  if (!db_load(&game.db)) {
    log_line("Failed to load data.");
    return 1;
  }
  log_linef("Counts: weapons=%d items=%d enemies=%d characters=%d", game.db.weapon_count, game.db.item_count, game.db.enemy_count, game.db.character_count);
  log_line("Data load ok");

  game.window = SDL_CreateWindow("buh", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H,
                                 SDL_WINDOW_SHOWN | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_RESIZABLE);
  game.renderer = SDL_CreateRenderer(game.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  log_line("Window/renderer created");
  
  /* Load textures */
  IMG_Init(IMG_INIT_PNG);
  
  /* Set window icon */
  SDL_Surface *icon = IMG_Load("data/assets/game_icon.png");
  if (icon) {
    SDL_SetWindowIcon(game.window, icon);
    SDL_FreeSurface(icon);
    log_line("Window icon set");
  } else {
    log_linef("Failed to load game_icon.png: %s", IMG_GetError());
  }
  
  game.tex_ground = IMG_LoadTexture(game.renderer, "data/assets/hd_ground_tile.png");
  game.tex_wall = IMG_LoadTexture(game.renderer, "data/assets/wall.png");
  game.tex_enemy = IMG_LoadTexture(game.renderer, "data/assets/goo_green.png");
  game.tex_health_flask = IMG_LoadTexture(game.renderer, "data/assets/health_flask.png");
  if (game.tex_health_flask) log_line("Loaded health_flask.png");
  else log_linef("Failed to load health_flask.png: %s", IMG_GetError());
  if (game.tex_ground) log_line("Loaded hd_ground_tile.png");
  else log_linef("Failed to load ground.png: %s", IMG_GetError());
  if (game.tex_wall) log_line("Loaded wall.png");
  else log_linef("Failed to load wall.png: %s", IMG_GetError());
  if (game.tex_enemy) log_line("Loaded goo_green.png");
  else log_linef("Failed to load goo_green.png: %s", IMG_GetError());
  game.tex_player_front = IMG_LoadTexture(game.renderer, "data/assets/player_front.png");
  if (game.tex_player_front) log_line("Loaded player_front.png");
  else log_linef("Failed to load player_front.png: %s", IMG_GetError());
  game.tex_player_back = IMG_LoadTexture(game.renderer, "data/assets/player_back.png");
  if (game.tex_player_back) log_line("Loaded player_back.png");
  else log_linef("Failed to load player_back.png: %s", IMG_GetError());
  game.tex_player_right = IMG_LoadTexture(game.renderer, "data/assets/player_right.png");
  if (game.tex_player_right) log_line("Loaded player_right.png");
  else log_linef("Failed to load player_right.png: %s", IMG_GetError());
  game.tex_player_left = IMG_LoadTexture(game.renderer, "data/assets/player_left.png");
  if (game.tex_player_left) log_line("Loaded player_left.png");
  else log_linef("Failed to load player_left.png: %s", IMG_GetError());
  game.tex_enemy_bolt = IMG_LoadTexture(game.renderer, "data/assets/goo_bolt.png");
  if (game.tex_enemy_bolt) log_line("Loaded goo_bolt.png");
  else log_linef("Failed to load goo_bolt.png: %s", IMG_GetError());
  game.tex_laser_beam = IMG_LoadTexture(game.renderer, "data/assets/laser_beam.png");
  if (game.tex_laser_beam) log_line("Loaded laser_beam.png");
  else log_linef("Failed to load laser_beam.png: %s", IMG_GetError());
  game.tex_lightning_zone = IMG_LoadTexture(game.renderer, "data/assets/lightning_zone.png");
  if (game.tex_lightning_zone) log_line("Loaded lightning_zone.png");
  else log_linef("Failed to load lightning_zone.png: %s", IMG_GetError());
  
  /* Load weapon effect sprites */
  game.tex_scythe = IMG_LoadTexture(game.renderer, "data/assets/weapons/scythe.png");
  if (game.tex_scythe) log_line("Loaded scythe sprite");
  else log_linef("Failed to load scythe sprite: %s", IMG_GetError());
  game.tex_bite = IMG_LoadTexture(game.renderer, "data/assets/weapons/vampire_bite.png");
  if (game.tex_bite) log_line("Loaded vampire bite sprite");
  else log_linef("Failed to load vampire bite sprite: %s", IMG_GetError());
  game.tex_dagger = IMG_LoadTexture(game.renderer, "data/assets/weapons/dagger.png");
  if (game.tex_dagger) log_line("Loaded dagger sprite");
  else log_linef("Failed to load dagger sprite: %s", IMG_GetError());
  game.tex_alchemist_puddle = IMG_LoadTexture(game.renderer, "data/assets/weapons/alchemist_puddle.png");
  if (game.tex_alchemist_puddle) log_line("Loaded alchemist puddle sprite");
  else log_linef("Failed to load alchemist_puddle.png: %s", IMG_GetError());
  game.tex_exp_orb = IMG_LoadTexture(game.renderer, "data/assets/exp_orb.png");
  if (game.tex_exp_orb) log_line("Loaded exp_orb sprite");
  else log_linef("Failed to load exp_orb.png: %s", IMG_GetError());

  game.tex_orb_common = load_texture_fallback(game.renderer, "data/assets/orbs_rarity/common_orb.png");
  game.tex_orb_uncommon = load_texture_fallback(game.renderer, "data/assets/orbs_rarity/uncommon_orb.png");
  game.tex_orb_rare = load_texture_fallback(game.renderer, "data/assets/orbs_rarity/rare_orb.png");
  game.tex_orb_epic = load_texture_fallback(game.renderer, "data/assets/orbs_rarity/epic_orb.png");
  game.tex_orb_legendary = load_texture_fallback(game.renderer, "data/assets/orbs_rarity/legendary_orb.png");
  if (game.tex_orb_common) log_line("Loaded common_orb.png");
  else log_linef("Failed to load common_orb.png: %s", IMG_GetError());
  if (game.tex_orb_uncommon) log_line("Loaded uncommon_orb.png");
  else log_linef("Failed to load uncommon_orb.png: %s", IMG_GetError());
  if (game.tex_orb_rare) log_line("Loaded rare_orb.png");
  else log_linef("Failed to load rare_orb.png: %s", IMG_GetError());
  if (game.tex_orb_epic) log_line("Loaded epic_orb.png");
  else log_linef("Failed to load epic_orb.png: %s", IMG_GetError());
  if (game.tex_orb_legendary) log_line("Loaded legendary_orb.png");
  else log_linef("Failed to load legendary_orb.png: %s", IMG_GetError());
  
  /* Load character portraits */
  for (int i = 0; i < game.db.character_count && i < MAX_CHARACTERS; i++) {
    char portrait_path[128];
    snprintf(portrait_path, sizeof(portrait_path), "data/assets/portraits/%s", game.db.characters[i].portrait);
    game.tex_portraits[i] = IMG_LoadTexture(game.renderer, portrait_path);
    if (game.tex_portraits[i]) log_linef("Loaded portrait: %s", game.db.characters[i].portrait);
    else log_linef("Failed to load portrait %s: %s", game.db.characters[i].portrait, IMG_GetError());
  }
  
  game.font = TTF_OpenFont("C:/Windows/Fonts/verdana.ttf", 14);
  if (!game.font) {
    log_linef("Font load failed. Continuing without text. %s", TTF_GetError());
  }
  /* Try to load a nicer game font for titles - Segoe Script Bold */
  game.font_title = TTF_OpenFont("C:/Windows/Fonts/segoescb.ttf", 20);
  game.font_title_big = TTF_OpenFont("C:/Windows/Fonts/segoescb.ttf", 24);
  if (!game.font_title) {
    game.font_title = TTF_OpenFont("C:/Windows/Fonts/impact.ttf", 18);
  }
  if (!game.font_title_big) {
    game.font_title_big = TTF_OpenFont("C:/Windows/Fonts/impact.ttf", 22);
  }
  if (!game.font_title) {
    game.font_title = TTF_OpenFont("C:/Windows/Fonts/georgiab.ttf", 18);
  }
  if (!game.font_title_big) {
    game.font_title_big = TTF_OpenFont("C:/Windows/Fonts/georgiab.ttf", 22);
  }
  if (!game.font_title) {
    game.font_title = TTF_OpenFont("C:/Windows/Fonts/arialbd.ttf", 18);
  }
  if (!game.font_title) {
    game.font_title = game.font; /* Fallback to regular font */
  }
  if (!game.window || !game.renderer) {
    log_linef("Window/renderer creation failed: %s", SDL_GetError());
    return 1;
  }

  game_reset(&game);

  Uint64 now = SDL_GetPerformanceCounter();
  Uint64 last = 0;
  double accumulator = 0.0;
  double frequency = (double)SDL_GetPerformanceFrequency();

  while (game.running == 0) game.running = 1;
  log_line("Main loop start");
  while (game.running) {
    last = now;
    now = SDL_GetPerformanceCounter();
    double frame = (double)(now - last) / frequency;
    if (frame > 0.25) frame = 0.25;
    accumulator += frame;
    update_window_view(&game);

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) game.running = 0;
      if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_ESCAPE) game.running = 0;
        if (e.key.keysym.sym == SDLK_p) toggle_pause(&game);
        if (e.key.keysym.sym == SDLK_TAB) toggle_pause(&game);
        if (e.key.keysym.sym == SDLK_r && game.mode == MODE_GAMEOVER) game_reset(&game);
        if (e.key.keysym.sym == SDLK_F1) {
          if (game.db.enemy_count > 0) {
            for (int k = 0; k < 5; k++) spawn_enemy(&game, 0);
          }
        }
        if (e.key.keysym.sym == SDLK_F2) {
          game.time_scale = clampf(game.time_scale + 0.5f, 0.5f, 4.0f);
        }
        if (e.key.keysym.sym == SDLK_F3) {
          game.time_scale = clampf(game.time_scale - 0.5f, 0.5f, 4.0f);
        }
        if (e.key.keysym.sym == SDLK_F5) {
          toggle_pause(&game);
        }
        if (e.key.keysym.sym == SDLK_5) {
          if (game.mode == MODE_WAVE && game.boss_event_cd <= 0.0f) {
            game.boss_event_cd = 5.0f;
            start_boss_event(&game);
          }
        }
        if (e.key.keysym.sym == SDLK_8) {
          game.debug_show_items = !game.debug_show_items;
        }
        if (game.mode == MODE_START) {
          if (e.key.keysym.sym == SDLK_LEFT) {
            game.start_page -= 1;
            build_start_page(&game);
          }
          if (e.key.keysym.sym == SDLK_RIGHT) {
            game.start_page += 1;
            build_start_page(&game);
          }
        }
        if (game.mode == MODE_WAVE) {
          /* Ultimate ability - SPACE key, 2 minute cooldown */
          if (e.key.keysym.sym == SDLK_SPACE && game.ultimate_cd <= 0.0f) {
            activate_ultimate(&game);
            game.ultimate_cd = 120.0f;  /* 2 minutes */
          }
        }
      }
      if (e.type == SDL_KEYUP) {
        if (e.key.keysym.sym == SDLK_F4) {
          game.debug_show_range = !game.debug_show_range;
        }
      }
      if (e.type == SDL_MOUSEBUTTONDOWN && game.mode == MODE_START) {
        int mx = e.button.x;
        int my = e.button.y;
        int shown = game.choice_count;
        for (int i = 0; i < shown; i++) {
          SDL_Rect r = game.choices[i].rect;
          if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
            CharacterDef *c = &game.db.characters[game.choices[i].index];
            game.selected_character = game.choices[i].index;
            /* Apply character stats to base */
            stats_add(&game.player.base, &c->stats);
            /* Equip starting weapon */
            int widx = find_weapon(&game.db, c->weapon);
            if (widx >= 0) equip_weapon(&game.player, widx);
            wave_start(&game);
          }
        }
      }
      if (e.type == SDL_MOUSEWHEEL && game.mode == MODE_START) {
        float max_scroll = start_scroll_max(&game);
        if (max_scroll > 0.0f) {
          game.start_scroll -= (float)e.wheel.y * 40.0f;
          game.start_scroll = clampf(game.start_scroll, 0.0f, max_scroll);
        }
      }
      if (e.type == SDL_MOUSEBUTTONDOWN &&
          (game.mode == MODE_LEVELUP || (game.mode == MODE_PAUSE && game.pause_return_mode == MODE_LEVELUP))) {
        handle_levelup_click(&game, e.button.x, e.button.y);
      }
    }

    const double dt = 1.0 / 60.0;
    while (accumulator >= dt) {
      if (game.mode == MODE_WAVE) update_game(&game, (float)(dt * game.time_scale));
      if (game.mode == MODE_BOSS_EVENT) update_boss_event(&game, (float)(dt * game.time_scale));
      if (game.mode == MODE_LEVELUP && (game.levelup_chosen >= 0 || game.levelup_selected_count > 0) && game.levelup_fade > 0.0f) {
        float now = (float)SDL_GetTicks() / 1000.0f;
        if (now - game.levelup_fade >= 0.5f) {
          game.mode = MODE_WAVE;
          game.levelup_chosen = -1;
          game.levelup_selected_count = 0;
        }
      }
      accumulator -= dt;
    }

    render_game(&game);
    g_frame_log++;
    if ((g_frame_log % 600) == 0) {
      log_linef("Frame %d", g_frame_log);
    }
  }
  log_line("Main loop exit");

  if (game.tex_ground) SDL_DestroyTexture(game.tex_ground);
  if (game.tex_wall) SDL_DestroyTexture(game.tex_wall);
  if (game.tex_health_flask) SDL_DestroyTexture(game.tex_health_flask);
  if (game.tex_enemy) SDL_DestroyTexture(game.tex_enemy);
  if (game.tex_player_front) SDL_DestroyTexture(game.tex_player_front);
  if (game.tex_player_back) SDL_DestroyTexture(game.tex_player_back);
  if (game.tex_player_right) SDL_DestroyTexture(game.tex_player_right);
  if (game.tex_player_left) SDL_DestroyTexture(game.tex_player_left);
  if (game.tex_enemy_bolt) SDL_DestroyTexture(game.tex_enemy_bolt);
  if (game.tex_laser_beam) SDL_DestroyTexture(game.tex_laser_beam);
  if (game.tex_lightning_zone) SDL_DestroyTexture(game.tex_lightning_zone);
  if (game.tex_scythe) SDL_DestroyTexture(game.tex_scythe);
  if (game.tex_bite) SDL_DestroyTexture(game.tex_bite);
  if (game.tex_dagger) SDL_DestroyTexture(game.tex_dagger);
  if (game.tex_alchemist_puddle) SDL_DestroyTexture(game.tex_alchemist_puddle);
  if (game.tex_exp_orb) SDL_DestroyTexture(game.tex_exp_orb);
  if (game.tex_orb_common) SDL_DestroyTexture(game.tex_orb_common);
  if (game.tex_orb_uncommon) SDL_DestroyTexture(game.tex_orb_uncommon);
  if (game.tex_orb_rare) SDL_DestroyTexture(game.tex_orb_rare);
  if (game.tex_orb_epic) SDL_DestroyTexture(game.tex_orb_epic);
  if (game.tex_orb_legendary) SDL_DestroyTexture(game.tex_orb_legendary);
  for (int i = 0; i < MAX_CHARACTERS; i++) {
    if (game.tex_portraits[i]) SDL_DestroyTexture(game.tex_portraits[i]);
  }
  if (game.font_title_big) TTF_CloseFont(game.font_title_big);
  TTF_CloseFont(game.font);
  if (game.font_title) TTF_CloseFont(game.font_title);
  SDL_DestroyRenderer(game.renderer);
  SDL_DestroyWindow(game.window);
  IMG_Quit();
  TTF_Quit();
  SDL_Quit();
  if (g_combat_log) fclose(g_combat_log);
  g_combat_log = NULL;
  return 0;
}
#endif
