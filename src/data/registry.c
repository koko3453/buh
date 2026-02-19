#include "data/registry.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/config.h"

#define JSMN_PARENT_LINKS
#include "jsmn/jsmn.h"

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

static void parse_stats_object(const char *json, jsmntok_t *t, int obj, Stats *out) {
  memset(out, 0, sizeof(*out));
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
    int ult_cdr = find_key(json, tokens, obj, "ultimate_cdr"); 
    int totem_spawn = find_key(json, tokens, obj, "totem_spawn_rate"); 
    int totem_dur = find_key(json, tokens, obj, "totem_duration_bonus"); 
    int chest_reroll = find_key(json, tokens, obj, "chest_reroll_bonus"); 
    if (burn_hit > 0) it->burn_on_hit = token_float(json, &tokens[burn_hit]); 
    if (burn_aura > 0) it->burn_aura = token_float(json, &tokens[burn_aura]); 
    if (thorns > 0) it->thorns_percent = token_float(json, &tokens[thorns]); 
    if (lifesteal > 0) it->lifesteal_on_kill = token_float(json, &tokens[lifesteal]); 
    if (rarity_bias > 0) it->rarity_bias = token_float(json, &tokens[rarity_bias]); 
    if (slow_bonus > 0) it->slow_bonus_damage = token_float(json, &tokens[slow_bonus]); 
    if (legendary_amp > 0) it->legendary_amp = token_float(json, &tokens[legendary_amp]); 
    if (hp_regen_amp > 0) it->hp_regen_amp = token_float(json, &tokens[hp_regen_amp]); 
    if (xp_kill > 0) it->xp_kill_chance = token_float(json, &tokens[xp_kill]); 
    if (ult_cdr > 0) it->ultimate_cdr = token_float(json, &tokens[ult_cdr]); 
    if (totem_spawn > 0) it->totem_spawn_rate = token_float(json, &tokens[totem_spawn]); 
    if (totem_dur > 0) it->totem_duration_bonus = token_float(json, &tokens[totem_dur]); 
    if (chest_reroll > 0) it->chest_reroll_bonus = token_int(json, &tokens[chest_reroll]); 

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
    int walk = find_key(json, tokens, obj, "walk_strip");
    int idle = find_key(json, tokens, obj, "idle_frame");
    int fps = find_key(json, tokens, obj, "anim_fps");
    if (idt > 0) token_string(json, &tokens[idt], c->id, (int)sizeof(c->id));
    if (nt > 0) token_string(json, &tokens[nt], c->name, (int)sizeof(c->name));
    if (pt > 0) token_string(json, &tokens[pt], c->portrait, (int)sizeof(c->portrait));
    if (wt > 0) token_string(json, &tokens[wt], c->weapon, (int)sizeof(c->weapon));
    if (rt > 0) token_string(json, &tokens[rt], c->rule, (int)sizeof(c->rule));
    if (ut > 0) token_string(json, &tokens[ut], c->ultimate, (int)sizeof(c->ultimate));
    else strcpy(c->ultimate, "kill_all"); /* default ultimate */
    if (walk > 0) token_string(json, &tokens[walk], c->walk_strip, (int)sizeof(c->walk_strip));
    else c->walk_strip[0] = '\0';
    if (idle > 0) c->idle_frame = token_int(json, &tokens[idle]);
    else c->idle_frame = 1;
    if (fps > 0) c->anim_fps = token_float(json, &tokens[fps]);
    else c->anim_fps = 6.0f;
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
