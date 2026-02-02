
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

#define WAVE_COUNT 10

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
  char weapon[32];
  Stats stats;
  char rule[24];
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
  int type; /* 0 currency, 1 heal */
  float x;
  float y;
  float value;
  float ttl;
  float magnet_speed; /* current attraction speed */
} Drop;

typedef struct {
  int type; /* 0 item, 1 weapon */
  int index;
  int price;
  SDL_Rect rect;
  int sold;
} ShopSlot;

typedef enum {
  MODE_START,
  MODE_WAVE,
  MODE_SHOP,
  MODE_PAUSE,
  MODE_GAMEOVER
} GameMode;

typedef struct {
  WeaponDef weapons[MAX_WEAPONS];
  int weapon_count;
  ItemDef items[MAX_ITEMS];
  int item_count;
  EnemyDef enemies[MAX_ITEMS];
  int enemy_count;
  CharacterDef characters[MAX_ITEMS];
  int character_count;
} Database;

typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  TTF_Font *font;
  SDL_Texture *tex_ground;
  SDL_Texture *tex_wall;
  SDL_Texture *tex_health_flask;
  SDL_Texture *tex_enemy;
  SDL_Texture *tex_player;
  SDL_Texture *tex_enemy_bolt;
  int running;
  GameMode mode;
  float time_scale;
  int debug_show_range;
  float ultimate_cd;
  int start_page;

  Database db;
  Player player;
  Enemy enemies[MAX_ENEMIES];
  Bullet bullets[MAX_BULLETS];
  Drop drops[MAX_DROPS];

  int wave;
  float wave_time;
  float wave_duration;
  float spawn_timer;
  int currency;
  int reroll_cost;
  int lock_shop;
  int kills;

  ShopSlot shop[MAX_SHOP_SLOTS];
  int shop_count;
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
  for (int i = 0; i < n && db->character_count < MAX_ITEMS; i++) {
    int obj = idx;
    CharacterDef *c = &db->characters[db->character_count++];
    memset(c, 0, sizeof(*c));
    int idt = find_key(json, tokens, obj, "id");
    int nt = find_key(json, tokens, obj, "name");
    int wt = find_key(json, tokens, obj, "weapon");
    int rt = find_key(json, tokens, obj, "rule");
    if (idt > 0) token_string(json, &tokens[idt], c->id, (int)sizeof(c->id));
    if (nt > 0) token_string(json, &tokens[nt], c->name, (int)sizeof(c->name));
    if (wt > 0) token_string(json, &tokens[wt], c->weapon, (int)sizeof(c->weapon));
    if (rt > 0) token_string(json, &tokens[rt], c->rule, (int)sizeof(c->rule));
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

static void build_shop(Game *g) {
  g->shop_count = 0;
  int desired_items = 8;
  int desired_weapons = 4;
  if (g->db.item_count == 0 || g->db.weapon_count == 0) {
    log_line("Shop build skipped: missing items or weapons.");
    return;
  }
  for (int i = 0; i < desired_items && g->shop_count < MAX_SHOP_SLOTS; i++) {
    int idx = rand() % g->db.item_count;
    int price = 15 + rand() % 20 + g->wave * 2;
    g->shop[g->shop_count++] = (ShopSlot){ .type = 0, .index = idx, .price = price, .sold = 0 };
  }
  for (int i = 0; i < desired_weapons && g->shop_count < MAX_SHOP_SLOTS; i++) {
    int idx = rand() % g->db.weapon_count;
    int price = 25 + rand() % 15 + g->wave * 3;
    g->shop[g->shop_count++] = (ShopSlot){ .type = 1, .index = idx, .price = price, .sold = 0 };
  }
}

static void build_start_page(Game *g) {
  g->shop_count = 0;
  int per_page = 16;
  int total = g->db.weapon_count;
  int pages = (total + per_page - 1) / per_page;
  if (pages < 1) pages = 1;
  if (g->start_page < 0) g->start_page = 0;
  if (g->start_page >= pages) g->start_page = pages - 1;
  int start = g->start_page * per_page;
  int end = start + per_page;
  if (end > total) end = total;
  for (int i = start; i < end; i++) {
    g->shop[g->shop_count++] = (ShopSlot){ .type = 1, .index = i, .price = 0, .sold = 0 };
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

static void game_reset(Game *g) {
  g->wave = 1;
  g->wave_duration = 25.0f;
  g->wave_time = g->wave_duration;
  g->spawn_timer = 0.0f;
  g->currency = 60;
  g->reroll_cost = 5;
  g->lock_shop = 0;
  g->kills = 0;
  g->mode = MODE_START;
  g->time_scale = 1.0f;
  g->debug_show_range = 1;
  g->start_page = 0;
  g->ultimate_cd = 0.0f;
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
  p->base.max_hp = 1000;
  p->base.move_speed = 1.0f;
  p->hp = p->base.max_hp;
  stats_clear(&p->bonus);
  weapons_clear(p);
  for (int i = 0; i < SLOT_COUNT; i++) p->gear[i] = -1;
  p->passive_count = 0;
  if (g->db.character_count > 0) {
    CharacterDef *c = &g->db.characters[0];
    stats_add(&p->base, &c->stats);
  }
  build_start_page(g);
}

static void shop_enter(Game *g) {
  g->mode = MODE_SHOP;
  if (!g->lock_shop) build_shop(g);
}

static void wave_start(Game *g) {
  g->mode = MODE_WAVE;
  g->wave_time = g->wave_duration;
  g->spawn_timer = 0.0f;
  g->currency += (int)(20 + g->wave * 2);
}

static void handle_player_pickups(Game *g, float dt) {
  Player *p = &g->player;
  Stats total = player_total_stats(p);
  float coin_magnet_range = 150.0f;  /* coins start moving toward player */
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
      /* Coin - magnet attraction with accelerating speed */
      if (dist < pickup_range) {
        g->currency += (int)d->value;
        d->active = 0;
        continue;
      }
      if (dist < coin_magnet_range) {
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
        spawn_drop(g, e->x, e->y, 0, 5 + rand() % 6);
        if (frandf() < 0.15f) spawn_drop(g, e->x, e->y, 1, 15 + rand() % 10);
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

    if (weapon_is(w, "sword") || weapon_is(w, "short_sword") || weapon_is(w, "longsword") || weapon_is(w, "axe") ||
        weapon_is(w, "greatsword") || weapon_is(w, "hammer") || weapon_is(w, "scythe") || weapon_is(w, "daggers") ||
        weapon_is(w, "fists")) {
      float range = w->range;
      float arc_deg = weapon_is(w, "axe") || weapon_is(w, "greatsword") || weapon_is(w, "hammer") || weapon_is(w, "scythe") ? 110.0f : 80.0f;
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
          if (weapon_is(w, "scythe") && en->hp <= 0.0f) {
            p->hp = clampf(p->hp + 6.0f, 0.0f, stats.max_hp);
          }
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

/* Ultimate ability - kills all enemies on screen, 2 minute cooldown */
static void ultimate_kill_all(Game *g) {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (g->enemies[i].active) {
      g->enemies[i].hp = 0.0f;
    }
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
  update_enemies(g, dt);
  handle_player_pickups(g, dt);

  g->wave_time -= dt;
  g->spawn_timer -= dt;
  if (g->spawn_timer <= 0.0f) {
    int tier = g->wave;
    if (g->db.enemy_count == 0) {
      g->spawn_timer = 1.0f;
      return;
    }
    int def_index = rand() % g->db.enemy_count;
    if (tier % 5 == 0) def_index = g->db.enemy_count - 1;
    spawn_enemy(g, def_index);
    g->spawn_timer = clampf(0.75f - 0.03f * g->wave, 0.2f, 1.0f);
  }

  if (g->wave_time <= 0.0f) {
    shop_enter(g);
  }

  if (p->hp <= 0.0f) {
    g->mode = MODE_GAMEOVER;
  }
}
static void layout_shop(Game *g, int screen_w, int screen_h) {
  int cols = 4;
  int rows = 3;
  int card_w = 280;
  int card_h = 100;
  int start_x = (screen_w - cols * card_w - (cols - 1) * 10) / 2;
  int start_y = 120;
  for (int i = 0; i < g->shop_count; i++) {
    int col = i % cols;
    int row = i / cols;
    g->shop[i].rect = (SDL_Rect){ start_x + col * (card_w + 10), start_y + row * (card_h + 10), card_w, card_h };
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
    
    /* Status effect visuals */
    if (g->enemies[i].burn_timer > 0.0f) {
      draw_glow(g->renderer, ex, ey, size/2 + 8, (SDL_Color){255, 100, 0, 100});
    }
    if (g->enemies[i].slow_timer > 0.0f) {
      draw_circle(g->renderer, ex, ey, size/2 + 4, (SDL_Color){100, 150, 255, 150});
    }
    
    /* Draw enemy sprite */
    if (g->tex_enemy) {
      SDL_Rect dst = { ex - size/2, ey - size/2, size, size };
      SDL_RenderCopy(g->renderer, g->tex_enemy, NULL, &dst);
    } else {
      draw_filled_circle(g->renderer, ex, ey, size/2, (SDL_Color){100, 200, 100, 255});
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

  /* Drops with sparkle effect */
  for (int i = 0; i < MAX_DROPS; i++) {
    if (!g->drops[i].active) continue;
    int dx = (int)(offset_x + g->drops[i].x - cam_x);
    int dy = (int)(offset_y + g->drops[i].y - cam_y);
    if (g->drops[i].type == 0) {
      /* Gold coin */
      draw_glow(g->renderer, dx, dy, 10, (SDL_Color){255, 215, 0, 80});
      draw_filled_circle(g->renderer, dx, dy, 5, (SDL_Color){255, 215, 0, 255});
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
    snprintf(buf, sizeof(buf), "Wave %d / %d", g->wave, WAVE_COUNT);
    draw_text(g->renderer, g->font, 260, 12, text, buf);
    snprintf(buf, sizeof(buf), "Time %.0f", g->wave_time);
    draw_text(g->renderer, g->font, 430, 12, text, buf);
    snprintf(buf, sizeof(buf), "$ %d", g->currency);
    draw_text(g->renderer, g->font, 560, 12, text, buf);
    snprintf(buf, sizeof(buf), "Kills %d", g->kills);
    draw_text(g->renderer, g->font, 680, 12, text, buf);
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

  if (g->mode == MODE_SHOP) {
    SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g->renderer, 5, 8, 15, 220);
    SDL_Rect overlay = { 0, 0, WINDOW_W, WINDOW_H };
    SDL_RenderFillRect(g->renderer, &overlay);
    
    /* Shop panel with glow border */
    SDL_Rect panel = { 60, 60, WINDOW_W - 120, WINDOW_H - 120 };
    SDL_SetRenderDrawColor(g->renderer, 20, 26, 40, 255);
    SDL_RenderFillRect(g->renderer, &panel);
    SDL_SetRenderDrawColor(g->renderer, 60, 80, 120, 255);
    SDL_RenderDrawRect(g->renderer, &panel);
    
    /* Shop title */
    SDL_Color gold = {255, 215, 100, 255};
    draw_text(g->renderer, g->font, 80, 72, gold, "SHOP - Click to Buy");
    char money_str[64];
    snprintf(money_str, sizeof(money_str), "Gold: $%d", g->currency);
    draw_text(g->renderer, g->font, WINDOW_W - 200, 72, gold, money_str);
    
    layout_shop(g, WINDOW_W, WINDOW_H);
    for (int i = 0; i < g->shop_count; i++) {
      ShopSlot *slot = &g->shop[i];
      if (slot->sold) continue;
      
      /* Card background with gradient effect */
      SDL_SetRenderDrawColor(g->renderer, 28, 34, 50, 255);
      SDL_RenderFillRect(g->renderer, &slot->rect);
      
      /* Highlight border based on rarity */
      const char *rarity_str = (slot->type == 0) ? g->db.items[slot->index].rarity : g->db.weapons[slot->index].rarity;
      SDL_Color border = {60, 70, 90, 255};
      if (strcmp(rarity_str, "uncommon") == 0) border = (SDL_Color){80, 180, 120, 255};
      else if (strcmp(rarity_str, "rare") == 0) border = (SDL_Color){80, 140, 255, 255};
      else if (strcmp(rarity_str, "epic") == 0) border = (SDL_Color){180, 100, 255, 255};
      else if (strcmp(rarity_str, "legendary") == 0) border = (SDL_Color){255, 180, 60, 255};
      SDL_SetRenderDrawColor(g->renderer, border.r, border.g, border.b, border.a);
      SDL_RenderDrawRect(g->renderer, &slot->rect);

      if (slot->type == 0) {
        ItemDef *it = &g->db.items[slot->index];
        SDL_Color rc = rarity_color(it->rarity);
        draw_text(g->renderer, g->font, slot->rect.x + 8, slot->rect.y + 6, rc, it->name);
        
        /* Show description if available */
        if (it->desc[0]) {
          SDL_Color desc_color = {180, 180, 190, 255};
          draw_text(g->renderer, g->font, slot->rect.x + 8, slot->rect.y + 26, desc_color, it->desc);
        }
        
        /* Show key stats on third line */
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
        draw_text(g->renderer, g->font, slot->rect.x + 8, slot->rect.y + 46, stat_color, statline);
      } else {
        WeaponDef *w = &g->db.weapons[slot->index];
        SDL_Color rc = rarity_color(w->rarity);
        draw_text(g->renderer, g->font, slot->rect.x + 8, slot->rect.y + 6, rc, w->name);
        char statline[128];
        snprintf(statline, sizeof(statline), "%s - DMG %.0f  CD %.2fs  RNG %.0f", w->type, w->damage, w->cooldown, w->range);
        SDL_Color wep_color = {180, 180, 190, 255};
        draw_text(g->renderer, g->font, slot->rect.x + 8, slot->rect.y + 26, wep_color, statline);
        
        /* Show projectile info */
        char proj_info[64];
        snprintf(proj_info, sizeof(proj_info), "Pellets: %d  Speed: %.0f", w->pellets, w->projectile_speed);
        draw_text(g->renderer, g->font, slot->rect.x + 8, slot->rect.y + 46, text, proj_info);
      }
      
      /* Price with affordability color */
      char price[32];
      snprintf(price, sizeof(price), "$%d", slot->price);
      SDL_Color price_color = (g->currency >= slot->price) ? (SDL_Color){100, 255, 100, 255} : (SDL_Color){255, 100, 100, 255};
      draw_text(g->renderer, g->font, slot->rect.x + 8, slot->rect.y + slot->rect.h - 22, price_color, price);
    }
    SDL_Rect start = { WINDOW_W - 240, WINDOW_H - 120, 160, 48 };
    SDL_SetRenderDrawColor(g->renderer, 50, 90, 80, 255);
    SDL_RenderFillRect(g->renderer, &start);
    draw_text(g->renderer, g->font, start.x + 20, start.y + 12, text, "Start Wave");
  }

  if (g->mode == MODE_START) {
    SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g->renderer, 10, 10, 20, 220);
    SDL_Rect overlay = { 0, 0, WINDOW_W, WINDOW_H };
    SDL_RenderFillRect(g->renderer, &overlay);
    SDL_SetRenderDrawColor(g->renderer, 24, 30, 44, 255);
    SDL_Rect panel = { 140, 100, WINDOW_W - 280, WINDOW_H - 200 };
    SDL_RenderFillRect(g->renderer, &panel);
    draw_text(g->renderer, g->font, 170, 120, text, "Choose your starting weapon");

    int per_page = 8;
    int total = g->db.weapon_count;
    int pages = (total + per_page - 1) / per_page;
    if (pages < 1) pages = 1;
    char pagebuf[64];
    snprintf(pagebuf, sizeof(pagebuf), "Page %d / %d (Left/Right)", g->start_page + 1, pages);
    draw_text(g->renderer, g->font, 170, 140, text, pagebuf);

    int shown = g->shop_count;
    int cols = 2;
    int rows = (shown + cols - 1) / cols;
    int card_w = 240;
    int card_h = 90;
    int start_x = 200;
    int start_y = 180;
    for (int i = 0; i < shown; i++) {
      int col = i % cols;
      int row = i / cols;
      SDL_Rect r = { start_x + col * (card_w + 16), start_y + row * (card_h + 14), card_w, card_h };
      SDL_SetRenderDrawColor(g->renderer, 30, 36, 50, 255);
      SDL_RenderFillRect(g->renderer, &r);
      SDL_SetRenderDrawColor(g->renderer, 80, 90, 110, 255);
      SDL_RenderDrawRect(g->renderer, &r);
      WeaponDef *w = &g->db.weapons[g->shop[i].index];
      SDL_Color rc = rarity_color(w->rarity);
      draw_text(g->renderer, g->font, r.x + 8, r.y + 8, rc, w->name);
      char info[64];
      snprintf(info, sizeof(info), "DMG %.0f  CD %.2f", w->damage, w->cooldown);
      draw_text(g->renderer, g->font, r.x + 8, r.y + 36, text, info);
      g->shop[i].rect = r;
    }
  }

  if (g->mode == MODE_GAMEOVER) {
    SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(g->renderer, 0, 0, 0, 180);
    SDL_Rect overlay = { 0, 0, WINDOW_W, WINDOW_H };
    SDL_RenderFillRect(g->renderer, &overlay);
    draw_text(g->renderer, g->font, WINDOW_W / 2 - 80, WINDOW_H / 2 - 20, text, "Run Complete");
    draw_text(g->renderer, g->font, WINDOW_W / 2 - 120, WINDOW_H / 2 + 10, text, "Press R to restart");
  }

  SDL_RenderPresent(g->renderer);
}

static void handle_shop_click(Game *g, int mx, int my) {
  for (int i = 0; i < g->shop_count; i++) {
    SDL_Rect r = g->shop[i].rect;
    if (g->shop[i].sold) continue;
    if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
      if (g->shop[i].type == 0) {
        ItemDef *it = &g->db.items[g->shop[i].index];
        if (g->currency >= g->shop[i].price) {
          g->currency -= g->shop[i].price;
          apply_item(&g->player, &g->db, it, g->shop[i].index);
          g->shop[i].sold = 1;
        } else {
          log_line("Shop: not enough currency for item.");
        }
      } else {
        WeaponDef *w = &g->db.weapons[g->shop[i].index];
        if (g->currency >= g->shop[i].price && can_equip_weapon(&g->player, g->shop[i].index)) {
          g->currency -= g->shop[i].price;
          int wi = find_weapon(&g->db, w->id);
          if (wi >= 0) equip_weapon(&g->player, wi);
          g->shop[i].sold = 1;
        } else {
          log_line("Shop: not enough currency or no weapon slot.");
        }
      }
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
  game.tex_player = IMG_LoadTexture(game.renderer, "data/assets/player..png");
  if (game.tex_player) log_line("Loaded player.png");
  else log_linef("Failed to load player.png: %s", IMG_GetError());
  game.tex_enemy_bolt = IMG_LoadTexture(game.renderer, "data/assets/goo_bolt.png");
  if (game.tex_enemy_bolt) log_line("Loaded goo_bolt.png");
  else log_linef("Failed to load goo_bolt.png: %s", IMG_GetError());
  
  game.font = TTF_OpenFont("C:/Windows/Fonts/verdana.ttf", 16);
  if (!game.font) {
    log_linef("Font load failed. Continuing without text. %s", TTF_GetError());
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
            ultimate_kill_all(&game);
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
        int shown = game.shop_count;
        for (int i = 0; i < shown; i++) {
          SDL_Rect r = game.shop[i].rect;
          if (e.button.x >= r.x && e.button.x <= r.x + r.w && e.button.y >= r.y && e.button.y <= r.y + r.h) {
            equip_weapon(&game.player, game.shop[i].index);
            wave_start(&game);
          }
        }
      }
      if (e.type == SDL_MOUSEBUTTONDOWN && game.mode == MODE_SHOP) {
        handle_shop_click(&game, e.button.x, e.button.y);
        SDL_Rect start = { WINDOW_W - 240, WINDOW_H - 120, 160, 48 };
        if (e.button.x >= start.x && e.button.x <= start.x + start.w && e.button.y >= start.y && e.button.y <= start.y + start.h) {
          if (game.wave >= WAVE_COUNT) {
            game.mode = MODE_GAMEOVER;
          } else {
            game.wave += 1;
            wave_start(&game);
          }
        }
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
  TTF_CloseFont(game.font);
  SDL_DestroyRenderer(game.renderer);
  SDL_DestroyWindow(game.window);
  IMG_Quit();
  TTF_Quit();
  SDL_Quit();
  return 0;
}
#endif
