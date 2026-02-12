
#include "game.h"

FILE *g_log = NULL;
int g_log_combat = 1;
FILE *g_combat_log = NULL;

void log_line(const char *msg) {
  if (!g_log) return;
  fputs(msg, g_log);
  fputs("\n", g_log);
  fflush(g_log);
}

void log_linef(const char *fmt, ...) {
  if (!g_log) return;
  char buf[512];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  log_line(buf);
}

SDL_Texture *load_texture_fallback(SDL_Renderer *r, const char *path) {
  SDL_Texture *tex = IMG_LoadTexture(r, path);
  if (tex) return tex;
  char alt[256];
  snprintf(alt, sizeof(alt), "../%s", path);
  tex = IMG_LoadTexture(r, alt);
  if (tex) return tex;
  snprintf(alt, sizeof(alt), "../../%s", path);
  return IMG_LoadTexture(r, alt);
}

LONG WINAPI crash_handler(EXCEPTION_POINTERS *e) {
  log_linef("Crash code: 0x%08lx", (unsigned long)e->ExceptionRecord->ExceptionCode);
  log_linef("Crash addr: %p", e->ExceptionRecord->ExceptionAddress);
  return EXCEPTION_EXECUTE_HANDLER;
}

#define JSMN_PARENT_LINKS
#include "third_party/jsmn.h"

int weapon_is(const WeaponDef *w, const char *id) {
  return strcmp(w->id, id) == 0;
}

void weapon_status_chances(const WeaponDef *w, float *bleed, float *burn, float *slow, float *stun, float *shred) {
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

void log_combatf(Game *g, const char *fmt, ...) {
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

const BossDef g_boss_defs[] = {
  { "proto_beast", "Proto Behemoth", 1800.0f, 90.0f, 30.0f, 26.0f, 0.7f,
    1.1f, 900.0f, 120.0f, 22.0f,
    5.0f, 16, 220.0f,
    4.0f, 80.0f, 45.0f,
    12.0f, 5.0f, 60.0f, 70.0f }
};

int boss_def_count(void) {
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
float clampf(float v, float a, float b) {
  if (v < a) return a;
  if (v > b) return b;
  return v;
}

float frandf(void) {
  return (float)rand() / (float)RAND_MAX;
}

static float frand_range(float a, float b) {
  return a + (b - a) * frandf();
}

void toggle_pause(Game *g) {
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

void vec_norm(float *x, float *y) {
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

void stats_clear(Stats *s) {
  memset(s, 0, sizeof(Stats));
}

void stats_add(Stats *dst, Stats *src) {
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

void stats_scale(Stats *s, float mul) {
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

int db_load(Database *db) {
  if (!load_weapons(db, "data/weapons.json")) return 0;
  if (!load_items(db, "data/items.json")) return 0;
  if (!load_enemies(db, "data/enemies.json")) return 0;
  if (!load_characters(db, "data/characters.json")) return 0;
  return 1;
}

int find_weapon(Database *db, const char *id) {
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

static int roll_item_index_with_bias(Game *g);
static int roll_weapon_index_with_bias(Game *g);
float player_hp_regen_amp(Player *p, Database *db);

Stats player_total_stats(Player *p, Database *db) {
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
float player_slow_on_hit(Player *p, Database *db) {
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
float player_slow_aura(Player *p, Database *db) {
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

float player_burn_on_hit(Player *p, Database *db) {
  float total = 0.0f;
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) {
      total += db->items[idx].burn_on_hit;
    }
  }
  return clampf(total, 0.0f, 1.0f);
}

float player_burn_aura(Player *p, Database *db) {
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

float player_thorns_percent(Player *p, Database *db) {
  float total = 0.0f;
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) {
      total += db->items[idx].thorns_percent;
    }
  }
  return clampf(total, 0.0f, 0.9f);
}

float player_lifesteal_on_kill(Player *p, Database *db) {
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

float player_slow_bonus_damage(Player *p, Database *db) {
  float total = 0.0f;
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) {
      total += db->items[idx].slow_bonus_damage;
    }
  }
  return clampf(total, 0.0f, 1.0f);
}

float player_legendary_amp(Player *p, Database *db) {
  float total = 0.0f;
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) {
      total += db->items[idx].legendary_amp;
    }
  }
  return clampf(total, 0.0f, 0.2f);
}

float player_hp_regen_amp(Player *p, Database *db) {
  float total = 0.0f;
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) {
      total += db->items[idx].hp_regen_amp;
    }
  }
  return clampf(total, 0.0f, 3.0f);
}

float player_xp_kill_chance(Player *p, Database *db) {
  float total = 0.0f;
  for (int i = 0; i < p->passive_count; i++) {
    int idx = p->passive_items[i];
    if (idx >= 0 && idx < db->item_count) {
      total += db->items[idx].xp_kill_chance;
    }
  }
  return clampf(total, 0.0f, 1.0f);
}

float player_roll_crit_damage(Stats *stats, WeaponDef *w, float dmg) {
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

void mark_enemy_hit(Enemy *en) {
  if (!en) return;
  en->hit_timer = (float)SDL_GetTicks() / 1000.0f;
}

float player_apply_hit_mods(Game *g, Enemy *en, float dmg) {
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

void player_try_item_proc(Game *g, int enemy_idx, Stats *stats) {
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

float damage_after_armor(float dmg, float armor) {
  float reduction = clampf(armor * 0.02f, 0.0f, 0.7f);
  return dmg * (1.0f - reduction);
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

void start_boss_event(Game *g) {
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

void spawn_drop(Game *g, float x, float y, int type, float value) {
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

void spawn_chest(Game *g, float x, float y) {
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

void weapons_clear(Player *p) {
  for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
    p->weapons[i].active = 0;
  }
}

void equip_weapon(Player *p, int def_index) {
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

int weapon_is_owned(Player *p, int def_index, int *out_level) {
  for (int i = 0; i < MAX_WEAPON_SLOTS; i++) {
    if (p->weapons[i].active && p->weapons[i].def_index == def_index) {
      if (out_level) *out_level = p->weapons[i].level;
      return 1;
    }
  }
  return 0;
}

int weapon_choice_allowed(Game *g, int def_index) {
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

void apply_item(Player *p, Database *db, ItemDef *it, int item_index) {
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

void build_levelup_choices(Game *g) {
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

void build_start_page(Game *g) {
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

float start_scroll_max(Game *g) {
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

static void trigger_item_popup(Game *g, ItemDef *it) {
  if (!g || !it) return;
  snprintf(g->item_popup_name, sizeof(g->item_popup_name), "%s", it->name);
  g->item_popup_timer = ITEM_POPUP_DURATION;
}

void update_window_view(Game *g) {
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

void game_reset(Game *g) {
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

void wave_start(Game *g) {
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

void activate_ultimate(Game *g) {
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

void update_game(Game *g, float dt) {
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

void update_boss_event(Game *g, float dt) {
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

void render_game(Game *g) {
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
    if (g->tex_boss) {
      SDL_Rect dst = { bx - br, by - br, br * 2, br * 2 };
      SDL_RenderCopy(g->renderer, g->tex_boss, NULL, &dst);
    } else {
      draw_filled_circle(g->renderer, bx, by, br + 6, (SDL_Color){160, 60, 40, 255});
      draw_circle(g->renderer, bx, by, br + 6, (SDL_Color){240, 200, 120, 255});
    }

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

void handle_levelup_click(Game *g, int mx, int my) {
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

