
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <windows.h>

static FILE *g_log = NULL;
static int g_frame_log = 0;

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
#define MAX_SHOP_SLOTS 12
#define MAX_TAGS 6

enum {
  SLOT_HELMET = 0,
  SLOT_CHEST,
  SLOT_GLOVES,
  SLOT_BOOTS,
  SLOT_RING1,
  SLOT_RING2,
  SLOT_AMULET,
  SLOT_RELIC,
  SLOT_OFFHAND,
  SLOT_COUNT
};

#define WINDOW_W 1280
#define WINDOW_H 720
#define ARENA_W 4000
#define ARENA_H 4000
#define VIEW_W 1180
#define VIEW_H 640

#define MAX_LEVELUP_CHOICES 16

typedef struct {
  float damage;
  float max_hp;
  float move_speed;
  float attack_speed;
  float armor;
  float dodge;
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
  char slot[16];
  char desc[64];
  char tags[MAX_TAGS][16];
  int tag_count;
  Stats stats;
  int has_proc;
  int proc_bounces;
  float proc_chance;
  float proc_damage;
  float slow_on_hit;    /* chance to slow enemy on autoattack (0.0-1.0) */
  float slow_aura;      /* range for slowing nearby enemies (0 = disabled) */
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
  int gear[SLOT_COUNT];
  int passive_items[MAX_ITEMS];
  int passive_count;
} Player;

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
  float bleed_chance;
  float burn_chance;
  float slow_chance;
  float stun_chance;
  float armor_shred_chance;
} Bullet;

typedef struct {
  int active;
  int type; /* 0 xp orb, 1 heal */
  float x;
  float y;
  float value;
  float ttl;
  float magnet_speed; /* current attraction speed */
  int magnetized;     /* once true, keeps flying toward player */
} Drop;

typedef struct {
  int type; /* 0 item, 1 weapon */
  int index;
  SDL_Rect rect;
} LevelUpChoice;

/* Visual effect for weapon swings/attacks */
#define MAX_WEAPON_FX 16
typedef struct {
  int active;
  int type;       /* 0=scythe swing, 1=bite on enemy, 2=dagger projectile */
  float x, y;
  float angle;    /* direction of swing or projectile */
  float timer;    /* animation progress */
  float duration; /* total duration */
  int target_enemy; /* for bite effect - which enemy */
} WeaponFX;

typedef enum {
  MODE_START,
  MODE_WAVE,
  MODE_LEVELUP,
  MODE_PAUSE,
  MODE_GAMEOVER
} GameMode;

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
  SDL_Texture *tex_ground;
  SDL_Texture *tex_wall;
  SDL_Texture *tex_health_flask;
  SDL_Texture *tex_enemy;
  SDL_Texture *tex_player;
  SDL_Texture *tex_enemy_bolt;
  SDL_Texture *tex_lightning_zone;
  SDL_Texture *tex_portraits[MAX_CHARACTERS];
  SDL_Texture *tex_scythe;
  SDL_Texture *tex_bite;
  SDL_Texture *tex_dagger;
  SDL_Texture *tex_exp_orb;
  int running;
  GameMode mode;
  float time_scale;
  int debug_show_range;
  float ultimate_cd;
  int start_page;
  int selected_character;  /* index into db.characters */
  int rerolls;             /* rerolls remaining this run */

  Database db;
  Player player;
  Enemy enemies[MAX_ENEMIES];
  Bullet bullets[MAX_BULLETS];
  Drop drops[MAX_DROPS];
  WeaponFX weapon_fx[MAX_WEAPON_FX];

  float spawn_timer;
  int kills;
  int xp;
  int level;
  int xp_to_next;
  float game_time;

  LevelUpChoice choices[MAX_LEVELUP_CHOICES];
  int choice_count;
  SDL_Rect reroll_button;  /* reroll button rect for levelup screen */
  float camera_x;
  float camera_y;
} Game;

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

static void vec_norm(float *x, float *y) {
  float len = sqrtf((*x) * (*x) + (*y) * (*y));
  if (len > 0.0001f) {
    *x /= len;
    *y /= len;
  }
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
    if (cd > 0) w->cooldown = token_float(json, &tokens[cd]);
    if (dmg > 0) w->damage = token_float(json, &tokens[dmg]);
    if (range > 0) w->range = token_float(json, &tokens[range]);
    if (ps > 0) w->projectile_speed = token_float(json, &tokens[ps]);
    if (pierce > 0) w->pierce = token_int(json, &tokens[pierce]);
    if (pellets > 0) w->pellets = token_int(json, &tokens[pellets]);
    if (spread > 0) w->spread = token_float(json, &tokens[spread]);
    if (homing > 0) w->homing = token_int(json, &tokens[homing]);

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
    int st = find_key(json, tokens, obj, "slot");
    int dt = find_key(json, tokens, obj, "desc");
    if (idt > 0) token_string(json, &tokens[idt], it->id, (int)sizeof(it->id));
    if (nt > 0) token_string(json, &tokens[nt], it->name, (int)sizeof(it->name));
    if (rt > 0) token_string(json, &tokens[rt], it->rarity, (int)sizeof(it->rarity));
    if (st > 0) token_string(json, &tokens[st], it->slot, (int)sizeof(it->slot));
    if (dt > 0) token_string(json, &tokens[dt], it->desc, (int)sizeof(it->desc));
    if (it->slot[0] == '\0') strcpy(it->slot, "passive");

    int tags = find_key(json, tokens, obj, "tags");
    if (tags > 0 && tokens[tags].type == JSMN_ARRAY) {
      int tidx = tags + 1;
      for (int t = 0; t < tokens[tags].size && it->tag_count < MAX_TAGS; t++) {
        token_string(json, &tokens[tidx], it->tags[it->tag_count++], 16);
        tidx += token_span(tokens, tidx);
      }
    }

    int stats = find_key(json, tokens, obj, "stats");
    if (stats > 0) parse_stats_object(json, tokens, stats, &it->stats);

    int proc = find_key(json, tokens, obj, "proc");
    if (proc > 0 && tokens[proc].type == JSMN_OBJECT) {
      it->has_proc = 1;
      int chance = find_key(json, tokens, proc, "chance");
      int dmg = find_key(json, tokens, proc, "damage");
      int bounces = find_key(json, tokens, proc, "bounces");
      if (chance > 0) it->proc_chance = token_float(json, &tokens[chance]);
      if (dmg > 0) it->proc_damage = token_float(json, &tokens[dmg]);
      if (bounces > 0) it->proc_bounces = token_int(json, &tokens[bounces]);
    }
    
    /* Parse slow effects */
    int slow_hit = find_key(json, tokens, obj, "slow_on_hit");
    int slow_aura = find_key(json, tokens, obj, "slow_aura");
    if (slow_hit > 0) it->slow_on_hit = token_float(json, &tokens[slow_hit]);
    if (slow_aura > 0) it->slow_aura = token_float(json, &tokens[slow_aura]);

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

static Stats player_total_stats(Player *p) {
  Stats s = p->base;
  stats_add(&s, &p->bonus);
  s.max_hp = clampf(s.max_hp, 1.0f, 9999.0f);
  s.attack_speed = clampf(s.attack_speed, -0.5f, 3.0f);
  s.move_speed = clampf(s.move_speed, -0.3f, 2.0f);
  s.dodge = clampf(s.dodge, 0.0f, 0.75f);
  return s;
}

/* Get total slow_on_hit chance from all equipped/passive items */
static float player_slow_on_hit(Player *p, Database *db) {
  float total = 0.0f;
  for (int i = 0; i < SLOT_COUNT; i++) {
    int idx = p->gear[i];
    if (idx >= 0 && idx < db->item_count) {
      total += db->items[idx].slow_on_hit;
    }
  }
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) {
      total += db->items[idx].slow_on_hit;
    }
  }
  return clampf(total, 0.0f, 1.0f);
}

/* Get max slow_aura range from all equipped/passive items */
static float player_slow_aura(Player *p, Database *db) {
  float max_range = 0.0f;
  for (int i = 0; i < SLOT_COUNT; i++) {
    int idx = p->gear[i];
    if (idx >= 0 && idx < db->item_count) {
      if (db->items[idx].slow_aura > max_range) {
        max_range = db->items[idx].slow_aura;
      }
    }
  }
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
      float x = 0.0f;
      float y = 0.0f;
      /* Spawn enemies around the player, just outside the view */
      float spawn_dist = 400.0f + frandf() * 200.0f;
      for (int tries = 0; tries < 12; tries++) {
        float angle = frandf() * 6.28318f;
        x = g->player.x + cosf(angle) * spawn_dist;
        y = g->player.y + sinf(angle) * spawn_dist;
        /* Clamp to world bounds */
        x = clampf(x, 40.0f, ARENA_W - 40.0f);
        y = clampf(y, 40.0f, ARENA_H - 40.0f);
        float dx = x - g->player.x;
        float dy = y - g->player.y;
        if (dx * dx + dy * dy > 200.0f * 200.0f) break;
      }
      e->x = x;
      e->y = y;
      return;
    }
  }
}

static void spawn_bullet(Game *g, float x, float y, float vx, float vy, float damage, int pierce, int homing, int from_player,
                         float bleed_chance, float burn_chance, float slow_chance, float stun_chance, float armor_shred_chance) {
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

static void update_weapon_fx(Game *g, float dt) {
  for (int i = 0; i < MAX_WEAPON_FX; i++) {
    if (!g->weapon_fx[i].active) continue;
    WeaponFX *fx = &g->weapon_fx[i];
    fx->timer += dt;
    if (fx->timer >= fx->duration) {
      fx->active = 0;
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
      p->weapons[i].level = (int)clampf(p->weapons[i].level + 1, 1, 4);
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

static int can_equip_weapon(Player *p, int def_index) {
  for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
    if (p->weapons[i].active && p->weapons[i].def_index == def_index) return 1;
  }
  for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
    if (!p->weapons[i].active) return 1;
  }
  return 0;
}

static int slot_from_string(const char *s) {
  if (!s || s[0] == '\0') return -1;
  if (strcmp(s, "helmet") == 0) return SLOT_HELMET;
  if (strcmp(s, "chest") == 0) return SLOT_CHEST;
  if (strcmp(s, "gloves") == 0) return SLOT_GLOVES;
  if (strcmp(s, "boots") == 0) return SLOT_BOOTS;
  if (strcmp(s, "amulet") == 0) return SLOT_AMULET;
  if (strcmp(s, "relic") == 0) return SLOT_RELIC;
  if (strcmp(s, "offhand") == 0) return SLOT_OFFHAND;
  if (strcmp(s, "ring") == 0) return SLOT_RING1;
  if (strcmp(s, "passive") == 0) return -1;
  return -1;
}

static void player_recalc(Player *p, Database *db) {
  stats_clear(&p->bonus);
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) stats_add(&p->bonus, &db->items[idx].stats);
  }
  for (int i = 0; i < SLOT_COUNT; i++) {
    int idx = p->gear[i];
    if (idx >= 0 && idx < db->item_count) stats_add(&p->bonus, &db->items[idx].stats);
  }
}

static void apply_item(Player *p, Database *db, ItemDef *it, int item_index) {
  int slot = slot_from_string(it->slot);
  if (slot == -1) {
    if (p->passive_count < MAX_ITEMS) {
      p->passive_items[p->passive_count++] = item_index;
    }
  } else if (slot == SLOT_RING1) {
    if (p->gear[SLOT_RING1] == -1) p->gear[SLOT_RING1] = item_index;
    else if (p->gear[SLOT_RING2] == -1) p->gear[SLOT_RING2] = item_index;
    else p->gear[SLOT_RING1] = item_index;
  } else {
    p->gear[slot] = item_index;
  }
  player_recalc(p, db);
}

static void build_levelup_choices(Game *g) {
  g->choice_count = 0;
  if (g->db.item_count == 0 && g->db.weapon_count == 0) {
    log_line("Level up choices skipped: no items or weapons.");
    return;
  }
  
  /* Generate 3 random choices - mix of items and weapons */
  int num_choices = 3;
  for (int i = 0; i < num_choices; i++) {
    int type = rand() % 3; /* 0,1 = item (66%), 2 = weapon (33%) */
    if (type < 2 && g->db.item_count > 0) {
      int idx = rand() % g->db.item_count;
      g->choices[g->choice_count++] = (LevelUpChoice){ .type = 0, .index = idx };
    } else if (g->db.weapon_count > 0) {
      int idx = rand() % g->db.weapon_count;
      g->choices[g->choice_count++] = (LevelUpChoice){ .type = 1, .index = idx };
    } else if (g->db.item_count > 0) {
      int idx = rand() % g->db.item_count;
      g->choices[g->choice_count++] = (LevelUpChoice){ .type = 0, .index = idx };
    }
  }
}

static void build_start_page(Game *g) {
  g->choice_count = 0;
  int per_page = 16;
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

static SDL_Color rarity_color(const char *rarity) {
  if (strcmp(rarity, "uncommon") == 0) return (SDL_Color){ 75, 209, 160, 255 };
  if (strcmp(rarity, "rare") == 0) return (SDL_Color){ 91, 177, 255, 255 };
  if (strcmp(rarity, "epic") == 0) return (SDL_Color){ 210, 123, 255, 255 };
  if (strcmp(rarity, "legendary") == 0) return (SDL_Color){ 255, 179, 71, 255 };
  return (SDL_Color){ 230, 231, 234, 255 };
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

static void game_reset(Game *g) {
  g->spawn_timer = 0.0f;
  g->kills = 0;
  g->xp = 0;
  g->level = 1;
  g->xp_to_next = 10;
  g->game_time = 0.0f;
  g->mode = MODE_START;
  g->time_scale = 1.0f;
  g->debug_show_range = 1;
  g->start_page = 0;
  g->ultimate_cd = 0.0f;
  g->choice_count = 0;
  g->selected_character = -1;
  g->rerolls = 2;  /* 2 rerolls per run */
  for (int i = 0; i < MAX_ENEMIES; i++) g->enemies[i].active = 0;
  for (int i = 0; i < MAX_BULLETS; i++) g->bullets[i].active = 0;
  for (int i = 0; i < MAX_DROPS; i++) g->drops[i].active = 0;

  Player *p = &g->player;
  p->x = ARENA_W * 0.5f;
  p->y = ARENA_H * 0.5f;
  /* Center camera on player */
  g->camera_x = p->x - VIEW_W * 0.5f;
  g->camera_y = p->y - VIEW_H * 0.5f;
  g->camera_x = clampf(g->camera_x, 0.0f, ARENA_W - VIEW_W);
  g->camera_y = clampf(g->camera_y, 0.0f, ARENA_H - VIEW_H);
  p->base = (Stats){0};
  p->base.max_hp = 1100;
  p->base.move_speed = 1.0f;
  p->hp = p->base.max_hp;
  stats_clear(&p->bonus);
  weapons_clear(p);
  for (int i = 0; i < SLOT_COUNT; i++) p->gear[i] = -1;
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
  Stats total = player_total_stats(p);
  float xp_magnet_range = 150.0f;  /* XP orbs start moving toward player */
  float pickup_range = 20.0f;        /* actual pickup distance */
  float health_pickup_range = 30.0f; /* health pickup distance */
  
  for (int i = 0; i < MAX_DROPS; i++) {
    Drop *d = &g->drops[i];
    if (!d->active) continue;
    float dx = d->x - p->x;
    float dy = d->y - p->y;
    float dist2 = dx * dx + dy * dy;
    float dist = sqrtf(dist2);
    
    if (d->type == 0) {
      /* XP orb - magnet attraction with accelerating speed */
      if (dist < pickup_range) {
        g->xp += (int)d->value;
        if (g->xp >= g->xp_to_next) {
          g->xp -= g->xp_to_next;
          level_up(g);
        }
        d->active = 0;
        continue;
      }
      /* Once magnetized, keep flying until picked up */
      if (dist < xp_magnet_range || d->magnetized) {
        d->magnetized = 1;
        /* Accelerate magnet speed over time */
        d->magnet_speed += 400.0f * dt;
        if (d->magnet_speed > 600.0f) d->magnet_speed = 600.0f;
        
        /* Move toward player */
        float nx = -dx / dist;
        float ny = -dy / dist;
        d->x += nx * d->magnet_speed * dt;
        d->y += ny * d->magnet_speed * dt;
      }
    } else {
      /* Health pack - standard pickup */
      if (dist < health_pickup_range) {
        p->hp = clampf(p->hp + d->value, 0.0f, total.max_hp);
        d->active = 0;
      }
    }
  }
}
static void update_bullets(Game *g, float dt) {
  Player *p = &g->player;
  Stats stats = player_total_stats(p);
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
          float dmg = b->damage;
          if (en->armor_shred_timer > 0.0f) dmg *= 1.2f;
          en->hp -= dmg;
          if (b->bleed_chance > 0.0f && frandf() < b->bleed_chance) {
            en->bleed_stacks = (en->bleed_stacks < 5) ? en->bleed_stacks + 1 : 5;
            en->bleed_timer = 4.0f;
          }
          if (b->burn_chance > 0.0f && frandf() < b->burn_chance) en->burn_timer = 4.0f;
          if (b->slow_chance > 0.0f && frandf() < b->slow_chance) en->slow_timer = 2.5f;
          if (b->stun_chance > 0.0f && frandf() < b->stun_chance) en->stun_timer = 0.6f;
          if (b->armor_shred_chance > 0.0f && frandf() < b->armor_shred_chance) en->armor_shred_timer = 3.0f;
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
  Stats stats = player_total_stats(p);
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
    
    /* Slow aura from items - constantly slows enemies in range */
    float aura_range = player_slow_aura(p, &g->db);
    if (aura_range > 0.0f && dist < aura_range) {
      e->slow_timer = 0.5f; /* Keep refreshing while in range */
    }

    if (e->stun_timer <= 0.0f &&
        (strcmp(def->role, "ranged") == 0 || strcmp(def->role, "boss") == 0 || strcmp(def->role, "turret") == 0)) {
      e->cooldown -= dt;
      if (e->cooldown <= 0.0f) {
        float vx = dx;
        float vy = dy;
        vec_norm(&vx, &vy);
        spawn_bullet(g, e->x, e->y, vx * def->projectile_speed, vy * def->projectile_speed, def->damage, 0, 0, 0,
                     0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
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
      p->hp -= dmg * dt;
    }

    if (strcmp(def->role, "exploder") == 0 && dist < 28.0f) {
      float dmg = damage_after_armor(def->damage, stats.armor);
      p->hp -= dmg * 2.0f;
      e->hp = 0;
    }

    if (e->hp <= 0.0f) {
      e->active = 0;
      g->kills += 1;
      if (e->spawn_invuln <= 0.0f) {
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
  Stats stats = player_total_stats(p);
  float attack_speed = 1.0f + stats.attack_speed;
  float cooldown_scale = 1.0f;

  for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
    WeaponSlot *slot = &p->weapons[i];
    if (!slot->active) continue;
    WeaponDef *w = &g->db.weapons[slot->def_index];
    slot->cd_timer -= dt * attack_speed;
    if (slot->cd_timer > 0.0f) continue;

    float best = 999999.0f;
    int target = -1;
    for (int e = 0; e < MAX_ENEMIES; e++) {
      if (!g->enemies[e].active) continue;
      if (g->enemies[e].spawn_invuln > 0.0f) continue;
      float dx = g->enemies[e].x - p->x;
      float dy = g->enemies[e].y - p->y;
      float d2 = dx * dx + dy * dy;
      if (d2 < best) { best = d2; target = e; }
    }
    if (target < 0) continue;

    float range = w->range;
    if (range > 0.0f && best > range * range) continue;

    float tx = g->enemies[target].x - p->x;
    float ty = g->enemies[target].y - p->y;
    vec_norm(&tx, &ty);

    float level_mul = 1.0f + 0.2f * (slot->level - 1);
    float damage = w->damage * level_mul * (1.0f + stats.damage);

    float bleed_chance, burn_chance, slow_chance, stun_chance, shred_chance;
    weapon_status_chances(w, &bleed_chance, &burn_chance, &slow_chance, &stun_chance, &shred_chance);
    
    /* Add slow chance from items */
    slow_chance += player_slow_on_hit(p, &g->db);
    slow_chance = clampf(slow_chance, 0.0f, 1.0f);

    /* Lightning Zone - damages all enemies in circular range */
    if (weapon_is(w, "lightning_zone")) {
      float range = w->range * (1.0f + 0.1f * (slot->level - 1));
      float range2 = range * range;
      for (int e = 0; e < MAX_ENEMIES; e++) {
        if (!g->enemies[e].active) continue;
        Enemy *en = &g->enemies[e];
        if (en->spawn_invuln > 0.0f) continue;
        float ex = en->x - p->x;
        float ey = en->y - p->y;
        float d2 = ex * ex + ey * ey;
        if (d2 <= range2) {
          float final_dmg = damage;
          if (en->armor_shred_timer > 0.0f) final_dmg *= 1.2f;
          en->hp -= final_dmg;
          /* Lightning has a chance to stun */
          if (frandf() < 0.15f) en->stun_timer = 0.3f;
        }
      }
      float level_cd = clampf(1.0f - 0.05f * (slot->level - 1), 0.7f, 1.0f);
      slot->cd_timer = w->cooldown * cooldown_scale * level_cd;
      continue;
    }

    if (weapon_is(w, "laser") || weapon_is(w, "whip") || weapon_is(w, "chain_blades")) {
      float range = w->range;
      float half_width = weapon_is(w, "whip") ? 8.0f : 10.0f;
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
          float final_dmg = damage;
          if (en->armor_shred_timer > 0.0f) final_dmg *= 1.2f;
          en->hp -= final_dmg;
          if (bleed_chance > 0.0f && frandf() < bleed_chance) {
            en->bleed_stacks = (en->bleed_stacks < 5) ? en->bleed_stacks + 1 : 5;
            en->bleed_timer = 4.0f;
          }
          if (burn_chance > 0.0f && frandf() < burn_chance) en->burn_timer = 4.0f;
          if (slow_chance > 0.0f && frandf() < slow_chance) en->slow_timer = 2.5f;
          if (stun_chance > 0.0f && frandf() < stun_chance) en->stun_timer = 0.6f;
          if (shred_chance > 0.0f && frandf() < shred_chance) en->armor_shred_timer = 3.0f;
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

    /* Scythe - 180 degree half-circle swing */
    if (weapon_is(w, "scythe")) {
      float range = w->range;
      float arc_deg = 180.0f;  /* Half circle */
      float arc_cos = cosf(arc_deg * 0.5f * (3.14159f / 180.0f)); /* cos(90) = 0, so anything in front half */
      float base_angle = atan2f(ty, tx);
      
      /* Spawn swing visual effect */
      spawn_weapon_fx(g, 0, p->x, p->y, base_angle, 0.35f, -1);
      
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
        if (dot >= arc_cos) {  /* In front 180 degrees */
          Enemy *en = &g->enemies[e];
          if (en->spawn_invuln > 0.0f) continue;
          float final_dmg = damage;
          if (en->armor_shred_timer > 0.0f) final_dmg *= 1.2f;
          en->hp -= final_dmg;
          if (bleed_chance > 0.0f && frandf() < bleed_chance) {
            en->bleed_stacks = (en->bleed_stacks < 5) ? en->bleed_stacks + 1 : 5;
            en->bleed_timer = 4.0f;
          }
          /* Lifesteal on kill */
          if (en->hp <= 0.0f) {
            p->hp = clampf(p->hp + 6.0f, 0.0f, stats.max_hp);
          }
        }
      }
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
          float final_dmg = damage;
          if (en->armor_shred_timer > 0.0f) final_dmg *= 1.2f;
          en->hp -= final_dmg;
          /* Spawn bite effect on enemy */
          spawn_weapon_fx(g, 1, en->x, en->y, 0.0f, 0.6f, e);
          /* Lifesteal */
          p->hp = clampf(p->hp + final_dmg * 0.15f, 0.0f, stats.max_hp);
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
        
        float final_dmg = damage;
        if (en->armor_shred_timer > 0.0f) final_dmg *= 1.2f;
        en->hp -= final_dmg;
        if (bleed_chance > 0.0f && frandf() < bleed_chance) {
          en->bleed_stacks = (en->bleed_stacks < 5) ? en->bleed_stacks + 1 : 5;
          en->bleed_timer = 4.0f;
        }
      }
      
      float level_cd = clampf(1.0f - 0.05f * (slot->level - 1), 0.7f, 1.0f);
      slot->cd_timer = w->cooldown * cooldown_scale * level_cd;
      continue;
    }

    if (weapon_is(w, "sword") || weapon_is(w, "short_sword") || weapon_is(w, "longsword") || weapon_is(w, "axe") ||
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
          float final_dmg = damage;
          if (en->armor_shred_timer > 0.0f) final_dmg *= 1.2f;
          en->hp -= final_dmg;
          if (bleed_chance > 0.0f && frandf() < bleed_chance) {
            en->bleed_stacks = (en->bleed_stacks < 5) ? en->bleed_stacks + 1 : 5;
            en->bleed_timer = 4.0f;
          }
          if (burn_chance > 0.0f && frandf() < burn_chance) en->burn_timer = 4.0f;
          if (slow_chance > 0.0f && frandf() < slow_chance) en->slow_timer = 2.5f;
          if (stun_chance > 0.0f && frandf() < stun_chance) en->stun_timer = 0.6f;
          if (shred_chance > 0.0f && frandf() < shred_chance) en->armor_shred_timer = 3.0f;
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
      spawn_bullet(g, p->x, p->y, vx, vy, damage, w->pierce, w->homing, 1,
                   bleed_chance, burn_chance, slow_chance, stun_chance, shred_chance);
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

static void activate_ultimate(Game *g) {
  /* Get character's ultimate type */
  const char *ult_type = "kill_all";
  if (g->selected_character >= 0 && g->selected_character < g->db.character_count) {
    ult_type = g->db.characters[g->selected_character].ultimate;
  }
  
  /* Dispatch to appropriate ultimate */
  if (strcmp(ult_type, "kill_all") == 0) {
    ultimate_kill_all(g);
  } else {
    /* Unknown ultimate - fallback to kill_all */
    ultimate_kill_all(g);
  }
}

static void update_game(Game *g, float dt) {
  const Uint8 *keys = SDL_GetKeyboardState(NULL);
  Player *p = &g->player;
  Stats stats = player_total_stats(p);

  /* Ultimate cooldown tick */
  if (g->ultimate_cd > 0.0f) g->ultimate_cd -= dt;
  if (g->ultimate_cd < 0.0f) g->ultimate_cd = 0.0f;

  float speed = 150.0f * (1.0f + stats.move_speed);
  float vx = 0.0f;
  float vy = 0.0f;
  if (keys[SDL_SCANCODE_W]) vy -= 1.0f;
  if (keys[SDL_SCANCODE_S]) vy += 1.0f;
  if (keys[SDL_SCANCODE_A]) vx -= 1.0f;
  if (keys[SDL_SCANCODE_D]) vx += 1.0f;
  vec_norm(&vx, &vy);
  p->x = clampf(p->x + vx * speed * dt, 20.0f, ARENA_W - 20.0f);
  p->y = clampf(p->y + vy * speed * dt, 20.0f, ARENA_H - 20.0f);

  /* Update camera - scroll when player reaches 4/5 of view edge */
  float scroll_margin_x = VIEW_W * 0.2f;
  float scroll_margin_y = VIEW_H * 0.2f;
  float player_screen_x = p->x - g->camera_x;
  float player_screen_y = p->y - g->camera_y;
  
  if (player_screen_x > VIEW_W - scroll_margin_x) {
    g->camera_x = p->x - (VIEW_W - scroll_margin_x);
  }
  if (player_screen_x < scroll_margin_x) {
    g->camera_x = p->x - scroll_margin_x;
  }
  if (player_screen_y > VIEW_H - scroll_margin_y) {
    g->camera_y = p->y - (VIEW_H - scroll_margin_y);
  }
  if (player_screen_y < scroll_margin_y) {
    g->camera_y = p->y - scroll_margin_y;
  }
  g->camera_x = clampf(g->camera_x, 0.0f, ARENA_W - VIEW_W);
  g->camera_y = clampf(g->camera_y, 0.0f, ARENA_H - VIEW_H);

  fire_weapons(g, dt);
  update_bullets(g, dt);
  update_weapon_fx(g, dt);
  update_enemies(g, dt);
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

static void layout_levelup(Game *g, int screen_w, int screen_h) {
  int card_w = 260;
  int card_h = 120;
  int spacing = 20;
  int total_w = g->choice_count * card_w + (g->choice_count - 1) * spacing;
  int start_x = (screen_w - total_w) / 2;
  int start_y = (screen_h - card_h) / 2;
  for (int i = 0; i < g->choice_count; i++) {
    g->choices[i].rect = (SDL_Rect){ start_x + i * (card_w + spacing), start_y, card_w, card_h };
  }
}

static void render_game(Game *g) {
  SDL_SetRenderDrawColor(g->renderer, 8, 10, 16, 255);
  SDL_RenderClear(g->renderer);

  int cam_x = (int)g->camera_x;
  int cam_y = (int)g->camera_y;
  int offset_x = 50;
  int offset_y = 40;

  /* Arena background - tile 128x128 ground texture with camera offset */
  /* Buffer of 2 tiles beyond visible area for smoother scrolling */
  if (g->tex_ground) {
    int tile_size = 128;
    int buffer = tile_size * 2;  /* 2 extra tiles on each side */
    int start_tx = ((cam_x - buffer) / tile_size) * tile_size;
    int start_ty = ((cam_y - buffer) / tile_size) * tile_size;
    for (int ty = start_ty; ty < cam_y + VIEW_H + buffer; ty += tile_size) {
      for (int tx = start_tx; tx < cam_x + VIEW_W + buffer; tx += tile_size) {
        SDL_Rect dst = { offset_x + tx - cam_x, offset_y + ty - cam_y, tile_size, tile_size };
        SDL_RenderCopy(g->renderer, g->tex_ground, NULL, &dst);
      }
    }
  } else {
    SDL_Rect arena = { offset_x, offset_y, VIEW_W, VIEW_H };
    SDL_SetRenderDrawColor(g->renderer, 15, 18, 28, 255);
    SDL_RenderFillRect(g->renderer, &arena);
  }
  
  /* No wall border for infinite map - just clip rendering */
  SDL_Rect clip = { offset_x, offset_y, VIEW_W, VIEW_H };
  SDL_RenderSetClipRect(g->renderer, &clip);

  /* Player with sprite */
  int px = (int)(offset_x + g->player.x - cam_x);
  int py = (int)(offset_y + g->player.y - cam_y);
  int player_size = 32;
  
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
  
  if (g->tex_player) {
    SDL_Rect dst = { px - player_size/2, py - player_size/2, player_size, player_size };
    SDL_RenderCopy(g->renderer, g->tex_player, NULL, &dst);
  } else {
    draw_glow(g->renderer, px, py, 25, (SDL_Color){255, 200, 80, 100});
    draw_filled_circle(g->renderer, px, py, 12, (SDL_Color){255, 200, 80, 255});
    draw_circle(g->renderer, px, py, 12, (SDL_Color){255, 230, 150, 255});
  }

  /* Enemies with sprite */
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!g->enemies[i].active) continue;
    EnemyDef *def = &g->db.enemies[g->enemies[i].def_index];
    int ex = (int)(offset_x + g->enemies[i].x - cam_x);
    int ey = (int)(offset_y + g->enemies[i].y - cam_y);
    
    int size = 32;
    if (strcmp(def->role, "boss") == 0) size = 48;
    
    /* Status effect visuals - burn glow only */
    if (g->enemies[i].burn_timer > 0.0f) {
      draw_glow(g->renderer, ex, ey, size/2 + 8, (SDL_Color){255, 100, 0, 100});
    }
    
    /* Draw enemy sprite with color tint for slow */
    if (g->tex_enemy) {
      /* Tint blue when slowed */
      if (g->enemies[i].slow_timer > 0.0f) {
        SDL_SetTextureColorMod(g->tex_enemy, 150, 180, 255);
      } else {
        SDL_SetTextureColorMod(g->tex_enemy, 255, 255, 255);
      }
      SDL_Rect dst = { ex - size/2, ey - size/2, size, size };
      SDL_RenderCopy(g->renderer, g->tex_enemy, NULL, &dst);
    } else {
      /* Fallback circle - tint blue when slowed */
      if (g->enemies[i].slow_timer > 0.0f) {
        draw_filled_circle(g->renderer, ex, ey, size/2, (SDL_Color){100, 150, 200, 255});
      } else {
        draw_filled_circle(g->renderer, ex, ey, size/2, (SDL_Color){100, 200, 100, 255});
      }
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
        SDL_Rect dst = { bx - 8, by - 8, 16, 16 };
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
      /* Scythe swing - half circle arc from player */
      int fxx = (int)(offset_x + fx->x - cam_x);
      int fxy = (int)(offset_y + fx->y - cam_y);
      float base_angle = fx->angle;
      float swing_progress = progress;  /* 0 to 1 over duration */
      /* Swing from -90 to +90 degrees relative to facing direction */
      float start_offset = -90.0f * (3.14159f / 180.0f);
      float end_offset = 90.0f * (3.14159f / 180.0f);
      float current_offset = start_offset + (end_offset - start_offset) * swing_progress;
      float swing_angle = base_angle + current_offset;
      
      /* Draw arc trail */
      int alpha = (int)(200 * (1.0f - progress));
      SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
      
      /* Draw multiple lines for the swing arc trail */
      float range = 120.0f;  /* Scythe range */
      for (float trail = 0.0f; trail <= swing_progress; trail += 0.05f) {
        float trail_offset = start_offset + (end_offset - start_offset) * trail;
        float trail_angle = base_angle + trail_offset;
        int trail_alpha = (int)(alpha * (trail / swing_progress) * 0.5f);
        SDL_SetRenderDrawColor(g->renderer, 180, 60, 200, (Uint8)trail_alpha);
        int ex = fxx + (int)(cosf(trail_angle) * range);
        int ey = fxy + (int)(sinf(trail_angle) * range);
        SDL_RenderDrawLine(g->renderer, fxx, fxy, ex, ey);
      }
      
      /* Draw scythe sprite at current swing position */
      if (g->tex_scythe) {
        int sx = fxx + (int)(cosf(swing_angle) * range * 0.6f);
        int sy = fxy + (int)(sinf(swing_angle) * range * 0.6f);
        int scythe_size = 67;  /* 48 * 1.4 = ~67 */
        SDL_Rect dst = { sx - scythe_size/2, sy - scythe_size/2, scythe_size, scythe_size };
        /* Rotate sprite to match swing angle */
        double angle_deg = swing_angle * (180.0 / 3.14159);
        SDL_SetTextureAlphaMod(g->tex_scythe, (Uint8)alpha);
        SDL_RenderCopyEx(g->renderer, g->tex_scythe, NULL, &dst, angle_deg + 90, NULL, SDL_FLIP_NONE);
        SDL_SetTextureAlphaMod(g->tex_scythe, 255);
      } else {
        /* Fallback - draw a line for the scythe blade */
        int ex = fxx + (int)(cosf(swing_angle) * range);
        int ey = fxy + (int)(sinf(swing_angle) * range);
        SDL_SetRenderDrawColor(g->renderer, 200, 80, 220, (Uint8)alpha);
        SDL_RenderDrawLine(g->renderer, fxx, fxy, ex, ey);
        /* Draw blade tip */
        draw_filled_circle(g->renderer, ex, ey, 6, (SDL_Color){220, 100, 240, (Uint8)alpha});
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
    } else {
      /* Health pack */
      if (g->tex_health_flask) {
        SDL_Rect dst = { dx - 10, dy - 10, 20, 20 };
        SDL_RenderCopy(g->renderer, g->tex_health_flask, NULL, &dst);
      } else {
        draw_glow(g->renderer, dx, dy, 12, (SDL_Color){255, 80, 120, 100});
        draw_diamond(g->renderer, dx, dy, 6, (SDL_Color){255, 100, 130, 255});
      }
    }
  }

  /* Reset clip rect for UI */
  SDL_RenderSetClipRect(g->renderer, NULL);

    SDL_Color text = { 230, 231, 234, 255 };
    char buf[128];
    Stats stats = player_total_stats(&g->player);
    snprintf(buf, sizeof(buf), "HP %.0f / %.0f", g->player.hp, stats.max_hp);
    draw_text(g->renderer, g->font, 70, 12, text, buf);
    snprintf(buf, sizeof(buf), "Lv %d  XP %d/%d", g->level, g->xp, g->xp_to_next);
    draw_text(g->renderer, g->font, 260, 12, text, buf);
    int mins = (int)(g->game_time / 60.0f);
    int secs = (int)g->game_time % 60;
    snprintf(buf, sizeof(buf), "Time %d:%02d", mins, secs);
    draw_text(g->renderer, g->font, 430, 12, text, buf);
    snprintf(buf, sizeof(buf), "Kills %d", g->kills);
    draw_text(g->renderer, g->font, 560, 12, text, buf);
    snprintf(buf, sizeof(buf), "HP %.0f  Dmg +%.0f%%  AS +%.0f%%", stats.max_hp, stats.damage * 100.0f, stats.attack_speed * 100.0f);
    draw_text(g->renderer, g->font, 70, 32, text, buf);
    snprintf(buf, sizeof(buf), "Armor %.0f  Speed +%.0f%%  Dodge %.0f%%", stats.armor, stats.move_speed * 100.0f, stats.dodge * 100.0f);
    draw_text(g->renderer, g->font, 70, 52, text, buf);
    snprintf(buf, sizeof(buf), "Speed x%.1f  DebugRange %s", g->time_scale, g->debug_show_range ? "ON" : "OFF");
    draw_text(g->renderer, g->font, 70, 72, text, buf);
    if (g->ultimate_cd > 0.0f) {
      snprintf(buf, sizeof(buf), "[SPACE] Ultimate: %.0fs", g->ultimate_cd);
    } else {
      snprintf(buf, sizeof(buf), "[SPACE] Ultimate: READY!");
    }
    draw_text(g->renderer, g->font, 70, 92, text, buf);
    draw_text(g->renderer, g->font, 70, 112, text, "F1 spawn  F2/F3 speed  F4 range  F5 pause  P pause");

    int wx = 70;
    int wy = 132;
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
    SDL_Rect overlay = { 0, 0, WINDOW_W, WINDOW_H };
    SDL_RenderFillRect(g->renderer, &overlay);
    
    /* Level up title */
    SDL_Color gold = {255, 215, 100, 255};
    char lvl_str[64];
    snprintf(lvl_str, sizeof(lvl_str), "LEVEL UP! (Lv %d)", g->level);
    draw_text(g->renderer, g->font, WINDOW_W / 2 - 80, 200, gold, lvl_str);
    draw_text(g->renderer, g->font, WINDOW_W / 2 - 60, 230, text, "Choose one:");
    
    layout_levelup(g, WINDOW_W, WINDOW_H);
    for (int i = 0; i < g->choice_count; i++) {
      LevelUpChoice *choice = &g->choices[i];
      
      /* Card background */
      SDL_SetRenderDrawColor(g->renderer, 28, 34, 50, 255);
      SDL_RenderFillRect(g->renderer, &choice->rect);
      
      /* Border based on rarity */
      const char *rarity_str = (choice->type == 0) ? g->db.items[choice->index].rarity : g->db.weapons[choice->index].rarity;
      SDL_Color border = {60, 70, 90, 255};
      if (strcmp(rarity_str, "uncommon") == 0) border = (SDL_Color){80, 180, 120, 255};
      else if (strcmp(rarity_str, "rare") == 0) border = (SDL_Color){80, 140, 255, 255};
      else if (strcmp(rarity_str, "epic") == 0) border = (SDL_Color){180, 100, 255, 255};
      else if (strcmp(rarity_str, "legendary") == 0) border = (SDL_Color){255, 180, 60, 255};
      SDL_SetRenderDrawColor(g->renderer, border.r, border.g, border.b, border.a);
      SDL_RenderDrawRect(g->renderer, &choice->rect);

      if (choice->type == 0) {
        ItemDef *it = &g->db.items[choice->index];
        SDL_Color rc = rarity_color(it->rarity);
        draw_text(g->renderer, g->font, choice->rect.x + 8, choice->rect.y + 8, rc, it->name);
        
        if (it->desc[0]) {
          SDL_Color desc_color = {180, 180, 190, 255};
          draw_text(g->renderer, g->font, choice->rect.x + 8, choice->rect.y + 30, desc_color, it->desc);
        }
        
        char statline[128];
        Stats *s = &it->stats;
        int pos = 0;
        statline[0] = 0;
        if (s->damage != 0) pos += snprintf(statline + pos, sizeof(statline) - pos, "DMG%+.0f%% ", s->damage * 100);
        if (s->max_hp != 0) pos += snprintf(statline + pos, sizeof(statline) - pos, "HP%+.0f ", s->max_hp);
        if (s->attack_speed != 0) pos += snprintf(statline + pos, sizeof(statline) - pos, "AS%+.0f%% ", s->attack_speed * 100);
        if (s->move_speed != 0) pos += snprintf(statline + pos, sizeof(statline) - pos, "SPD%+.0f%% ", s->move_speed * 100);
        if (s->armor != 0) pos += snprintf(statline + pos, sizeof(statline) - pos, "ARM%+.0f ", s->armor);
        if (s->dodge != 0) pos += snprintf(statline + pos, sizeof(statline) - pos, "DDG%+.0f%% ", s->dodge * 100);
        SDL_Color stat_color = {140, 200, 140, 255};
        draw_text(g->renderer, g->font, choice->rect.x + 8, choice->rect.y + 52, stat_color, statline);
        
        SDL_Color slot_color = {150, 150, 160, 255};
        draw_text(g->renderer, g->font, choice->rect.x + 8, choice->rect.y + 95, slot_color, it->slot);
      } else {
        WeaponDef *w = &g->db.weapons[choice->index];
        SDL_Color rc = rarity_color(w->rarity);
        draw_text(g->renderer, g->font, choice->rect.x + 8, choice->rect.y + 8, rc, w->name);
        char statline[128];
        snprintf(statline, sizeof(statline), "%s  DMG %.0f", w->type, w->damage);
        SDL_Color wep_color = {180, 180, 190, 255};
        draw_text(g->renderer, g->font, choice->rect.x + 8, choice->rect.y + 30, wep_color, statline);
        snprintf(statline, sizeof(statline), "CD %.2fs  RNG %.0f", w->cooldown, w->range);
        draw_text(g->renderer, g->font, choice->rect.x + 8, choice->rect.y + 52, wep_color, statline);
        
        SDL_Color type_color = {150, 150, 160, 255};
        draw_text(g->renderer, g->font, choice->rect.x + 8, choice->rect.y + 95, type_color, "WEAPON");
      }
    }
    
    /* Reroll button */
    int btn_w = 140;
    int btn_h = 36;
    int btn_x = WINDOW_W / 2 - btn_w / 2;
    int btn_y = 520;
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
  }

  if (g->mode == MODE_START) {
    /* Background */
    SDL_SetRenderDrawColor(g->renderer, 8, 8, 12, 255);
    SDL_RenderClear(g->renderer);

    int split_x = (WINDOW_W * 2) / 3;

    /* Right Panel Background */
    SDL_Rect right_panel = { split_x, 0, WINDOW_W - split_x, WINDOW_H };
    SDL_SetRenderDrawColor(g->renderer, 12, 14, 20, 255);
    SDL_RenderFillRect(g->renderer, &right_panel);
    SDL_SetRenderDrawColor(g->renderer, 40, 45, 60, 255);
    SDL_RenderDrawLine(g->renderer, split_x, 0, split_x, WINDOW_H);

    /* Left Panel: Character Grid */
    int margin = 25;
    int cols = 4;
    int name_area_h = 32;  /* Space below card for name */
    int card_w = (split_x - margin * 2 - (cols - 1) * 20) / cols;
    int card_h = card_w + 20;  /* Slightly taller than wide for portrait */
    int total_h = card_h + name_area_h;
    int start_y = 90;
    
    /* Title */
    draw_text_centered(g->renderer, g->font_title, split_x / 2, 30, (SDL_Color){255, 215, 100, 255}, "SELECT YOUR CHARACTER");
    draw_text_centered(g->renderer, g->font, split_x / 2, 58, (SDL_Color){100, 100, 120, 255}, "Click to choose - Hover for details");
    
    /* Mouse for hover */
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    int hovered_idx = -1;

    int shown = g->choice_count;
    for (int i = 0; i < shown; i++) {
      int col = i % cols;
      int row = i / cols;
      int card_x = margin + col * (card_w + 20);
      int card_y = start_y + row * (total_h + 12);
      SDL_Rect r = { card_x, card_y, card_w, card_h };
      
      /* Full clickable area includes name */
      SDL_Rect click_r = { card_x, card_y, card_w, total_h };
      g->choices[i].rect = click_r;

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
      int panel_w = WINDOW_W - split_x - 40;
      
      /* Large portrait on right panel */
      int big_portrait_size = 200;
      SDL_Rect big_portrait = { rx + (panel_w - big_portrait_size) / 2, ry, big_portrait_size, big_portrait_size };
      if (g->tex_portraits[char_idx]) {
        SDL_RenderCopy(g->renderer, g->tex_portraits[char_idx], NULL, &big_portrait);
        /* Gold border */
        SDL_SetRenderDrawColor(g->renderer, 200, 170, 80, 255);
        SDL_RenderDrawRect(g->renderer, &big_portrait);
      }
      ry += big_portrait_size + 15;
      
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
      int panel_w = WINDOW_W - split_x - 40;
      draw_text_centered(g->renderer, g->font, rx + panel_w / 2, WINDOW_H / 2 - 20, (SDL_Color){80, 80, 100, 255}, "Hover over a character");
      draw_text_centered(g->renderer, g->font, rx + panel_w / 2, WINDOW_H / 2 + 5, (SDL_Color){80, 80, 100, 255}, "to see details");
    }
  }

  if (g->mode == MODE_GAMEOVER) {
    SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g->renderer, 0, 0, 0, 180);
    SDL_Rect overlay = { 0, 0, WINDOW_W, WINDOW_H };
    SDL_RenderFillRect(g->renderer, &overlay);
    char buf[128];
    snprintf(buf, sizeof(buf), "Game Over - Level %d", g->level);
    draw_text(g->renderer, g->font, WINDOW_W / 2 - 80, WINDOW_H / 2 - 40, text, buf);
    int mins = (int)(g->game_time / 60.0f);
    int secs = (int)g->game_time % 60;
    snprintf(buf, sizeof(buf), "Survived %d:%02d  Kills: %d", mins, secs, g->kills);
    draw_text(g->renderer, g->font, WINDOW_W / 2 - 100, WINDOW_H / 2 - 10, text, buf);
    draw_text(g->renderer, g->font, WINDOW_W / 2 - 80, WINDOW_H / 2 + 20, text, "Press R to restart");
  }

  SDL_RenderPresent(g->renderer);
}

static void handle_levelup_click(Game *g, int mx, int my) {
  /* Check reroll button first */
  SDL_Rect rb = g->reroll_button;
  if (g->rerolls > 0 && mx >= rb.x && mx <= rb.x + rb.w && my >= rb.y && my <= rb.y + rb.h) {
    g->rerolls--;
    build_levelup_choices(g);
    return;
  }
  
  for (int i = 0; i < g->choice_count; i++) {
    SDL_Rect r = g->choices[i].rect;
    if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
      if (g->choices[i].type == 0) {
        ItemDef *it = &g->db.items[g->choices[i].index];
        apply_item(&g->player, &g->db, it, g->choices[i].index);
      } else {
        int wi = g->choices[i].index;
        if (can_equip_weapon(&g->player, wi)) {
          equip_weapon(&g->player, wi);
        } else {
          /* Upgrade existing weapon if we already have it */
          for (int w = 0; w < MAX_WEAPON_SLOTS; w++) {
            if (g->player.weapons[w].active && g->player.weapons[w].def_index == wi) {
              g->player.weapons[w].level += 1;
              break;
            }
          }
        }
      }
      g->mode = MODE_WAVE;
      return;
    }
  }
}

#ifndef UNIT_TESTS
int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  g_log = fopen("log.txt", "w");
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

  game.window = SDL_CreateWindow("buh", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN);
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
  game.tex_player = IMG_LoadTexture(game.renderer, "data/assets/player.png");
  if (game.tex_player) log_line("Loaded player.png");
  else log_linef("Failed to load player.png: %s", IMG_GetError());
  game.tex_enemy_bolt = IMG_LoadTexture(game.renderer, "data/assets/goo_bolt.png");
  if (game.tex_enemy_bolt) log_line("Loaded goo_bolt.png");
  else log_linef("Failed to load goo_bolt.png: %s", IMG_GetError());
  game.tex_lightning_zone = IMG_LoadTexture(game.renderer, "data/assets/lightning_zone.png");
  if (game.tex_lightning_zone) log_line("Loaded lightning_zone.png");
  else log_linef("Failed to load lightning_zone.png: %s", IMG_GetError());
  
  /* Load weapon effect sprites */
  game.tex_scythe = IMG_LoadTexture(game.renderer, "data/assets/weapons/sythe.png");
  if (game.tex_scythe) log_line("Loaded scythe sprite");
  else log_linef("Failed to load scythe sprite: %s", IMG_GetError());
  game.tex_bite = IMG_LoadTexture(game.renderer, "data/assets/weapons/vampire_bite.png");
  if (game.tex_bite) log_line("Loaded vampire bite sprite");
  else log_linef("Failed to load vampire bite sprite: %s", IMG_GetError());
  game.tex_dagger = IMG_LoadTexture(game.renderer, "data/assets/weapons/dagger.png");
  if (game.tex_dagger) log_line("Loaded dagger sprite");
  else log_linef("Failed to load dagger sprite: %s", IMG_GetError());
  game.tex_exp_orb = IMG_LoadTexture(game.renderer, "data/assets/exp_orb.png");
  if (game.tex_exp_orb) log_line("Loaded exp_orb sprite");
  else log_linef("Failed to load exp_orb.png: %s", IMG_GetError());
  
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
  /* Try to load a nicer game font for titles - Impact or Georgia Bold */
  game.font_title = TTF_OpenFont("C:/Windows/Fonts/impact.ttf", 18);
  if (!game.font_title) {
    game.font_title = TTF_OpenFont("C:/Windows/Fonts/georgiab.ttf", 18);
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

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) game.running = 0;
      if (e.type == SDL_KEYDOWN) {
        if (e.key.keysym.sym == SDLK_ESCAPE) game.running = 0;
        if (e.key.keysym.sym == SDLK_p) game.mode = (game.mode == MODE_PAUSE ? MODE_WAVE : MODE_PAUSE);
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
          game.mode = (game.mode == MODE_PAUSE ? MODE_WAVE : MODE_PAUSE);
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
        int shown = game.choice_count;
        for (int i = 0; i < shown; i++) {
          SDL_Rect r = game.choices[i].rect;
          if (e.button.x >= r.x && e.button.x <= r.x + r.w && e.button.y >= r.y && e.button.y <= r.y + r.h) {
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
      if (e.type == SDL_MOUSEBUTTONDOWN && game.mode == MODE_LEVELUP) {
        handle_levelup_click(&game, e.button.x, e.button.y);
      }
    }

    const double dt = 1.0 / 60.0;
    while (accumulator >= dt) {
      if (game.mode == MODE_WAVE) update_game(&game, (float)(dt * game.time_scale));
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
  if (game.tex_player) SDL_DestroyTexture(game.tex_player);
  if (game.tex_enemy_bolt) SDL_DestroyTexture(game.tex_enemy_bolt);
  if (game.tex_lightning_zone) SDL_DestroyTexture(game.tex_lightning_zone);
  if (game.tex_scythe) SDL_DestroyTexture(game.tex_scythe);
  if (game.tex_bite) SDL_DestroyTexture(game.tex_bite);
  if (game.tex_dagger) SDL_DestroyTexture(game.tex_dagger);
  if (game.tex_exp_orb) SDL_DestroyTexture(game.tex_exp_orb);
  for (int i = 0; i < MAX_CHARACTERS; i++) {
    if (game.tex_portraits[i]) SDL_DestroyTexture(game.tex_portraits[i]);
  }
  TTF_CloseFont(game.font);
  if (game.font_title) TTF_CloseFont(game.font_title);
  SDL_DestroyRenderer(game.renderer);
  SDL_DestroyWindow(game.window);
  IMG_Quit();
  TTF_Quit();
  SDL_Quit();
  return 0;
}
#endif
