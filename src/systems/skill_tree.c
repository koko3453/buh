#include "core/game.h"
#include "systems/skill_tree.h"

#define JSMN_PARENT_LINKS
#include "jsmn/jsmn.h"

static const char *skill_tree_branch_names[SKILL_TREE_BRANCH_COUNT] = {
  "Boss",
  "Horde",
  "Map/Env",
  "Rolls",
  "Stats"
};

static const SkillTreeNode skill_tree_nodes[] = {
  /* Boss branch */
  { "boss_root", "Boss Focus", "Boss modifiers", 3, -1, SKILL_TREE_BRANCH_BOSS, 0, SKILL_TREE_EFF_NONE, 0.0f },
  { "boss_armor", "Less Boss Armor", "Placeholder", 3, 0, SKILL_TREE_BRANCH_BOSS, 1, SKILL_TREE_EFF_NONE, 0.0f },
  { "boss_damage", "Less Boss Damage", "Placeholder", 3, 0, SKILL_TREE_BRANCH_BOSS, 1, SKILL_TREE_EFF_NONE, 0.0f },

  /* Horde branch */
  { "horde_root", "Horde Tactics", "Horde modifiers", 3, -1, SKILL_TREE_BRANCH_HORDE, 0, SKILL_TREE_EFF_NONE, 0.0f },
  { "horde_size", "Horde Density", "Placeholder", 3, 3, SKILL_TREE_BRANCH_HORDE, 1, SKILL_TREE_EFF_NONE, 0.0f },
  { "horde_elites", "Elite Packs", "Placeholder", 3, 3, SKILL_TREE_BRANCH_HORDE, 1, SKILL_TREE_EFF_NONE, 0.0f },

  /* Map/Env branch */
  { "map_root", "Map/Env", "Map and environment", 3, -1, SKILL_TREE_BRANCH_MAP_ENV, 0, SKILL_TREE_EFF_NONE, 0.0f },
  { "map_size", "Arena Scale", "Placeholder", 3, 6, SKILL_TREE_BRANCH_MAP_ENV, 1, SKILL_TREE_EFF_NONE, 0.0f },
  { "map_hazards", "Environmental Hazards", "Placeholder", 3, 6, SKILL_TREE_BRANCH_MAP_ENV, 1, SKILL_TREE_EFF_NONE, 0.0f },

  /* Rolls branch */
  { "rolls_root", "Roll Control", "Roll modifiers", 3, -1, SKILL_TREE_BRANCH_ROLLS, 0, SKILL_TREE_EFF_NONE, 0.0f },
  { "rolls_reroll", "Extra Reroll", "Placeholder", 3, 9, SKILL_TREE_BRANCH_ROLLS, 1, SKILL_TREE_EFF_NONE, 0.0f },
  { "rolls_highroll", "High Roll Boost", "Placeholder", 3, 9, SKILL_TREE_BRANCH_ROLLS, 1, SKILL_TREE_EFF_NONE, 0.0f },

  /* Stats branch */
  { "stats_root", "Stats Core", "Stat modifiers", 3, -1, SKILL_TREE_BRANCH_STATS, 0, SKILL_TREE_EFF_NONE, 0.0f },
  { "stats_damage", "Damage", "Placeholder", 3, 12, SKILL_TREE_BRANCH_STATS, 1, SKILL_TREE_EFF_NONE, 0.0f },
  { "stats_attack_speed", "Attack Speed", "Placeholder", 3, 13, SKILL_TREE_BRANCH_STATS, 2, SKILL_TREE_EFF_NONE, 0.0f },
  { "stats_crit", "Crit Chance", "Placeholder", 3, 13, SKILL_TREE_BRANCH_STATS, 2, SKILL_TREE_EFF_NONE, 0.0f }
};

static const char *skill_tree_checksum_salt = "buh_skill_tree_v1";
static const char *skill_tree_layout_path = "data/skill_tree_layout.json";

static float skill_tree_layout_x[MAX_SKILL_TREE_UPGRADES];
static float skill_tree_layout_y[MAX_SKILL_TREE_UPGRADES];
static int skill_tree_layout_is_set[MAX_SKILL_TREE_UPGRADES];
static float skill_tree_custom_x[MAX_SKILL_TREE_CUSTOM_NODES];
static float skill_tree_custom_y[MAX_SKILL_TREE_CUSTOM_NODES];
static int skill_tree_custom_count_value = 0;
static int skill_tree_custom_parent_kind_arr[MAX_SKILL_TREE_CUSTOM_NODES];
static int skill_tree_custom_parent_index_arr[MAX_SKILL_TREE_CUSTOM_NODES];
static int skill_tree_custom_max_rank_arr[MAX_SKILL_TREE_CUSTOM_NODES];
static char skill_tree_custom_name_buf[MAX_SKILL_TREE_CUSTOM_NODES][32];
static char skill_tree_custom_desc_buf[MAX_SKILL_TREE_CUSTOM_NODES][64];
static int skill_tree_override_parent_kind_arr[MAX_SKILL_TREE_UPGRADES];
static int skill_tree_override_parent_index_arr[MAX_SKILL_TREE_UPGRADES];
static int skill_tree_override_has_parent_arr[MAX_SKILL_TREE_UPGRADES];
static int skill_tree_override_max_rank_arr[MAX_SKILL_TREE_UPGRADES];
static int skill_tree_override_has_max_rank_arr[MAX_SKILL_TREE_UPGRADES];
static int skill_tree_override_has_name_arr[MAX_SKILL_TREE_UPGRADES];
static int skill_tree_override_has_desc_arr[MAX_SKILL_TREE_UPGRADES];
static char skill_tree_override_name_buf[MAX_SKILL_TREE_UPGRADES][32];
static char skill_tree_override_desc_buf[MAX_SKILL_TREE_UPGRADES][64];

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

static int write_file(const char *path, const char *data) {
  FILE *f = fopen(path, "wb");
  if (!f) return 0;
  size_t len = strlen(data);
  if (fwrite(data, 1, len, f) != len) {
    fclose(f);
    return 0;
  }
  fclose(f);
  return 1;
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
  int n = t[obj].size;
  int i = obj + 1;
  for (int k = 0; k < n; k += 2) {
    if (jsoneq(json, &t[i], key) == 0) return i + 1;
    i += token_span(t, i);
    i += token_span(t, i);
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

static unsigned int skill_tree_checksum(const char *payload) {
  unsigned int hash = 2166136261u;
  for (const char *p = skill_tree_checksum_salt; *p; p++) {
    hash ^= (unsigned char)(*p);
    hash *= 16777619u;
  }
  for (const char *p = payload; *p; p++) {
    hash ^= (unsigned char)(*p);
    hash *= 16777619u;
  }
  return hash;
}

static void skill_tree_build_payload(const SkillTreeProgress *progress, char *out, size_t out_len) {
  if (!out || out_len == 0) return;
  size_t used = 0;
  used += (size_t)snprintf(out + used, out_len - used, "points=%d\n", progress->points);
  used += (size_t)snprintf(out + used, out_len - used, "total=%d\n", progress->total_points);
  int count = skill_tree_node_count();
  for (int i = 0; i < count; i++) {
    used += (size_t)snprintf(out + used, out_len - used, "%s=%d\n", skill_tree_nodes[i].key, progress->upgrades[i]);
    if (used >= out_len) {
      out[out_len - 1] = '\0';
      break;
    }
  }
}

int skill_tree_node_count(void) {
  int count = (int)(sizeof(skill_tree_nodes) / sizeof(skill_tree_nodes[0]));
  if (count > MAX_SKILL_TREE_UPGRADES) count = MAX_SKILL_TREE_UPGRADES;
  return count;
}

const SkillTreeNode *skill_tree_node_get(int index) {
  if (index < 0 || index >= skill_tree_node_count()) return NULL;
  return &skill_tree_nodes[index];
}

const char *skill_tree_branch_name(int branch) {
  if (branch < 0 || branch >= SKILL_TREE_BRANCH_COUNT) return "";
  return skill_tree_branch_names[branch];
}

int skill_tree_upgrade_max_rank(int idx) {
  if (idx < 0 || idx >= skill_tree_node_count()) return 0;
  if (skill_tree_override_has_max_rank_arr[idx]) return skill_tree_override_max_rank_arr[idx];
  return skill_tree_nodes[idx].max_rank;
}

int skill_tree_upgrade_cost(int rank) {
  return 1 + rank;
}

void skill_tree_layout_clear(void) {
  for (int i = 0; i < MAX_SKILL_TREE_UPGRADES; i++) {
    skill_tree_layout_x[i] = 0.0f;
    skill_tree_layout_y[i] = 0.0f;
    skill_tree_layout_is_set[i] = 0;
    skill_tree_override_has_parent_arr[i] = 0;
    skill_tree_override_parent_kind_arr[i] = 0;
    skill_tree_override_parent_index_arr[i] = -1;
    skill_tree_override_has_max_rank_arr[i] = 0;
    skill_tree_override_max_rank_arr[i] = 0;
    skill_tree_override_has_name_arr[i] = 0;
    skill_tree_override_has_desc_arr[i] = 0;
    skill_tree_override_name_buf[i][0] = '\0';
    skill_tree_override_desc_buf[i][0] = '\0';
  }
  skill_tree_custom_count_value = 0;
}

int skill_tree_layout_get(int index, float *x, float *y) {
  if (index < 0 || index >= MAX_SKILL_TREE_UPGRADES) return 0;
  if (!skill_tree_layout_is_set[index]) return 0;
  if (x) *x = skill_tree_layout_x[index];
  if (y) *y = skill_tree_layout_y[index];
  return 1;
}

void skill_tree_layout_set(int index, float x, float y) {
  if (index < 0 || index >= MAX_SKILL_TREE_UPGRADES) return;
  if (x < 0.0f) x = 0.0f;
  if (x > 1.0f) x = 1.0f;
  if (y < 0.0f) y = 0.0f;
  if (y > 1.0f) y = 1.0f;
  skill_tree_layout_x[index] = x;
  skill_tree_layout_y[index] = y;
  skill_tree_layout_is_set[index] = 1;
}

int skill_tree_custom_count(void) {
  return skill_tree_custom_count_value;
}

int skill_tree_custom_add(float x, float y) {
  if (skill_tree_custom_count_value >= MAX_SKILL_TREE_CUSTOM_NODES) return -1;
  if (x < 0.0f) x = 0.0f;
  if (x > 1.0f) x = 1.0f;
  if (y < 0.0f) y = 0.0f;
  if (y > 1.0f) y = 1.0f;
  int idx = skill_tree_custom_count_value++;
  skill_tree_custom_x[idx] = x;
  skill_tree_custom_y[idx] = y;
  skill_tree_custom_parent_kind_arr[idx] = 0;
  skill_tree_custom_parent_index_arr[idx] = -1;
  skill_tree_custom_max_rank_arr[idx] = 3;
  snprintf(skill_tree_custom_name_buf[idx], sizeof(skill_tree_custom_name_buf[idx]), "Custom %d", idx + 1);
  snprintf(skill_tree_custom_desc_buf[idx], sizeof(skill_tree_custom_desc_buf[idx]), "Custom node");
  return idx;
}

int skill_tree_custom_get(int index, float *x, float *y) {
  if (index < 0 || index >= skill_tree_custom_count_value) return 0;
  if (x) *x = skill_tree_custom_x[index];
  if (y) *y = skill_tree_custom_y[index];
  return 1;
}

void skill_tree_custom_set(int index, float x, float y) {
  if (index < 0 || index >= skill_tree_custom_count_value) return;
  if (x < 0.0f) x = 0.0f;
  if (x > 1.0f) x = 1.0f;
  if (y < 0.0f) y = 0.0f;
  if (y > 1.0f) y = 1.0f;
  skill_tree_custom_x[index] = x;
  skill_tree_custom_y[index] = y;
}

const char *skill_tree_custom_name(int index) {
  if (index < 0 || index >= skill_tree_custom_count_value) return "";
  return skill_tree_custom_name_buf[index];
}

const char *skill_tree_custom_desc(int index) {
  if (index < 0 || index >= skill_tree_custom_count_value) return "";
  return skill_tree_custom_desc_buf[index];
}

void skill_tree_custom_set_name(int index, const char *name) {
  if (index < 0 || index >= skill_tree_custom_count_value) return;
  if (!name) return;
  snprintf(skill_tree_custom_name_buf[index], sizeof(skill_tree_custom_name_buf[index]), "%s", name);
}

void skill_tree_custom_set_desc(int index, const char *desc) {
  if (index < 0 || index >= skill_tree_custom_count_value) return;
  if (!desc) return;
  snprintf(skill_tree_custom_desc_buf[index], sizeof(skill_tree_custom_desc_buf[index]), "%s", desc);
}

int skill_tree_custom_max_rank(int index) {
  if (index < 0 || index >= skill_tree_custom_count_value) return 0;
  return skill_tree_custom_max_rank_arr[index];
}

void skill_tree_custom_set_max_rank(int index, int max_rank) {
  if (index < 0 || index >= skill_tree_custom_count_value) return;
  if (max_rank < 1) max_rank = 1;
  if (max_rank > 20) max_rank = 20;
  skill_tree_custom_max_rank_arr[index] = max_rank;
}

int skill_tree_custom_parent_kind(int index) {
  if (index < 0 || index >= skill_tree_custom_count_value) return 0;
  return skill_tree_custom_parent_kind_arr[index];
}

int skill_tree_custom_parent_index(int index) {
  if (index < 0 || index >= skill_tree_custom_count_value) return -1;
  return skill_tree_custom_parent_index_arr[index];
}

void skill_tree_custom_set_parent(int index, int parent_kind, int parent_index) {
  if (index < 0 || index >= skill_tree_custom_count_value) return;
  if (parent_kind < 0 || parent_kind > 2) return;
  skill_tree_custom_parent_kind_arr[index] = parent_kind;
  skill_tree_custom_parent_index_arr[index] = parent_index;
}

void skill_tree_override_set_max_rank(int index, int max_rank) {
  if (index < 0 || index >= skill_tree_node_count()) return;
  if (max_rank < 1) max_rank = 1;
  if (max_rank > 20) max_rank = 20;
  skill_tree_override_has_max_rank_arr[index] = 1;
  skill_tree_override_max_rank_arr[index] = max_rank;
}

int skill_tree_override_max_rank(int index) {
  if (index < 0 || index >= skill_tree_node_count()) return 0;
  if (!skill_tree_override_has_max_rank_arr[index]) return 0;
  return skill_tree_override_max_rank_arr[index];
}

void skill_tree_override_set_parent(int index, int parent_kind, int parent_index) {
  if (index < 0 || index >= skill_tree_node_count()) return;
  skill_tree_override_has_parent_arr[index] = 1;
  skill_tree_override_parent_kind_arr[index] = parent_kind;
  skill_tree_override_parent_index_arr[index] = parent_index;
}

int skill_tree_ui_parent_kind(int index) {
  if (index < 0 || index >= skill_tree_node_count()) return 0;
  if (skill_tree_override_has_parent_arr[index]) return skill_tree_override_parent_kind_arr[index];
  return (skill_tree_nodes[index].parent >= 0) ? 1 : 0;
}

int skill_tree_ui_parent_index(int index) {
  if (index < 0 || index >= skill_tree_node_count()) return -1;
  if (skill_tree_override_has_parent_arr[index]) return skill_tree_override_parent_index_arr[index];
  return skill_tree_nodes[index].parent;
}

const char *skill_tree_ui_name(int index) {
  if (index < 0 || index >= skill_tree_node_count()) return "";
  if (skill_tree_override_has_name_arr[index]) return skill_tree_override_name_buf[index];
  return skill_tree_nodes[index].name;
}

const char *skill_tree_ui_desc(int index) {
  if (index < 0 || index >= skill_tree_node_count()) return "";
  if (skill_tree_override_has_desc_arr[index]) return skill_tree_override_desc_buf[index];
  return skill_tree_nodes[index].desc;
}

void skill_tree_override_set_name(int index, const char *name) {
  if (index < 0 || index >= skill_tree_node_count()) return;
  if (!name) return;
  skill_tree_override_has_name_arr[index] = 1;
  snprintf(skill_tree_override_name_buf[index], sizeof(skill_tree_override_name_buf[index]), "%s", name);
}

void skill_tree_override_set_desc(int index, const char *desc) {
  if (index < 0 || index >= skill_tree_node_count()) return;
  if (!desc) return;
  skill_tree_override_has_desc_arr[index] = 1;
  snprintf(skill_tree_override_desc_buf[index], sizeof(skill_tree_override_desc_buf[index]), "%s", desc);
}

int skill_tree_layout_load(void) {
  skill_tree_layout_clear();
  char *json = read_file(skill_tree_layout_path);
  if (!json) return 0;

  jsmn_parser parser;
  jsmn_init(&parser);
  jsmntok_t tokens[512];
  int count = jsmn_parse(&parser, json, strlen(json), tokens, 512);
  if (count < 0) {
    free(json);
    return 0;
  }

  int nodes = find_key(json, tokens, 0, "nodes");
  if (nodes > 0 && tokens[nodes].type == JSMN_OBJECT) {
    int node_count = skill_tree_node_count();
    for (int i = 0; i < node_count; i++) {
      int nt = find_key(json, tokens, nodes, skill_tree_nodes[i].key);
      if (nt <= 0 || tokens[nt].type != JSMN_OBJECT) continue;
      int xt = find_key(json, tokens, nt, "x");
      int yt = find_key(json, tokens, nt, "y");
      if (xt > 0 && yt > 0) {
        float x = token_float(json, &tokens[xt]);
        float y = token_float(json, &tokens[yt]);
        skill_tree_layout_set(i, x, y);
      }
    }
  }

  int custom = find_key(json, tokens, 0, "custom_nodes");
  if (custom > 0 && tokens[custom].type == JSMN_ARRAY) {
    int n = tokens[custom].size;
    int idx = custom + 1;
    for (int i = 0; i < n && skill_tree_custom_count_value < MAX_SKILL_TREE_CUSTOM_NODES; i++) {
      int obj = idx;
      if (tokens[obj].type == JSMN_OBJECT) {
        int xt = find_key(json, tokens, obj, "x");
        int yt = find_key(json, tokens, obj, "y");
        int nt = find_key(json, tokens, obj, "name");
        int dt = find_key(json, tokens, obj, "desc");
        int mt = find_key(json, tokens, obj, "max_rank");
        int pkt = find_key(json, tokens, obj, "parent_kind");
        int pit = find_key(json, tokens, obj, "parent_index");
        if (xt > 0 && yt > 0) {
          float x = token_float(json, &tokens[xt]);
          float y = token_float(json, &tokens[yt]);
          int ci = skill_tree_custom_add(x, y);
          if (ci >= 0) {
            if (nt > 0) token_string(json, &tokens[nt], skill_tree_custom_name_buf[ci], (int)sizeof(skill_tree_custom_name_buf[ci]));
            if (dt > 0) token_string(json, &tokens[dt], skill_tree_custom_desc_buf[ci], (int)sizeof(skill_tree_custom_desc_buf[ci]));
            if (mt > 0) skill_tree_custom_max_rank_arr[ci] = token_int(json, &tokens[mt]);
            if (pkt > 0) skill_tree_custom_parent_kind_arr[ci] = token_int(json, &tokens[pkt]);
            if (pit > 0) skill_tree_custom_parent_index_arr[ci] = token_int(json, &tokens[pit]);
          }
        }
      }
      idx += token_span(tokens, idx);
    }
  }

  int overrides = find_key(json, tokens, 0, "node_overrides");
  if (overrides > 0 && tokens[overrides].type == JSMN_OBJECT) {
    int node_count = skill_tree_node_count();
    for (int i = 0; i < node_count; i++) {
      int ot = find_key(json, tokens, overrides, skill_tree_nodes[i].key);
      if (ot <= 0 || tokens[ot].type != JSMN_OBJECT) continue;
      int mt = find_key(json, tokens, ot, "max_rank");
      int pkt = find_key(json, tokens, ot, "parent_kind");
      int pit = find_key(json, tokens, ot, "parent_index");
      int nt = find_key(json, tokens, ot, "name");
      int dt = find_key(json, tokens, ot, "desc");
      if (mt > 0) {
        skill_tree_override_has_max_rank_arr[i] = 1;
        skill_tree_override_max_rank_arr[i] = token_int(json, &tokens[mt]);
      }
      if (pkt > 0 || pit > 0) {
        skill_tree_override_has_parent_arr[i] = 1;
        if (pkt > 0) skill_tree_override_parent_kind_arr[i] = token_int(json, &tokens[pkt]);
        if (pit > 0) skill_tree_override_parent_index_arr[i] = token_int(json, &tokens[pit]);
      }
      if (nt > 0) {
        skill_tree_override_has_name_arr[i] = 1;
        token_string(json, &tokens[nt], skill_tree_override_name_buf[i], (int)sizeof(skill_tree_override_name_buf[i]));
      }
      if (dt > 0) {
        skill_tree_override_has_desc_arr[i] = 1;
        token_string(json, &tokens[dt], skill_tree_override_desc_buf[i], (int)sizeof(skill_tree_override_desc_buf[i]));
      }
    }
  }

  free(json);
  return 1;
}

void skill_tree_layout_save(void) {
  char buf[8192];
  int used = 0;
  used += snprintf(buf + used, sizeof(buf) - used, "{\n  \"nodes\": {\n");
  int count = skill_tree_node_count();
  int written = 0;
  for (int i = 0; i < count; i++) {
    if (!skill_tree_layout_is_set[i]) continue;
    const char *comma = (written == 0) ? "" : ",\n";
    used += snprintf(buf + used, sizeof(buf) - used,
                     "%s    \"%s\": { \"x\": %.4f, \"y\": %.4f }",
                     comma, skill_tree_nodes[i].key,
                     skill_tree_layout_x[i], skill_tree_layout_y[i]);
    written++;
  }
  if (written > 0) used += snprintf(buf + used, sizeof(buf) - used, "\n");
  used += snprintf(buf + used, sizeof(buf) - used, "  },\n  \"custom_nodes\": [\n");
  int custom_count = skill_tree_custom_count_value;
  for (int i = 0; i < custom_count; i++) {
    const char *comma = (i == 0) ? "" : ",\n";
    used += snprintf(buf + used, sizeof(buf) - used,
                     "%s    { \"x\": %.4f, \"y\": %.4f, \"name\": \"%s\", \"desc\": \"%s\", \"max_rank\": %d, \"parent_kind\": %d, \"parent_index\": %d }",
                     comma, skill_tree_custom_x[i], skill_tree_custom_y[i],
                     skill_tree_custom_name_buf[i], skill_tree_custom_desc_buf[i],
                     skill_tree_custom_max_rank_arr[i], skill_tree_custom_parent_kind_arr[i], skill_tree_custom_parent_index_arr[i]);
  }
  if (custom_count > 0) used += snprintf(buf + used, sizeof(buf) - used, "\n");
  used += snprintf(buf + used, sizeof(buf) - used, "  ],\n  \"node_overrides\": {\n");
  int override_written = 0;
  for (int i = 0; i < count; i++) {
    if (!skill_tree_override_has_parent_arr[i] && !skill_tree_override_has_max_rank_arr[i] &&
        !skill_tree_override_has_name_arr[i] && !skill_tree_override_has_desc_arr[i]) {
      continue;
    }
    const char *comma = (override_written == 0) ? "" : ",\n";
    used += snprintf(buf + used, sizeof(buf) - used,
                     "%s    \"%s\": { \"max_rank\": %d, \"parent_kind\": %d, \"parent_index\": %d, \"name\": \"%s\", \"desc\": \"%s\" }",
                     comma, skill_tree_nodes[i].key,
                     skill_tree_override_has_max_rank_arr[i] ? skill_tree_override_max_rank_arr[i] : 0,
                     skill_tree_override_has_parent_arr[i] ? skill_tree_override_parent_kind_arr[i] : 0,
                     skill_tree_override_has_parent_arr[i] ? skill_tree_override_parent_index_arr[i] : -1,
                     skill_tree_override_has_name_arr[i] ? skill_tree_override_name_buf[i] : "",
                     skill_tree_override_has_desc_arr[i] ? skill_tree_override_desc_buf[i] : "");
    override_written++;
  }
  if (override_written > 0) used += snprintf(buf + used, sizeof(buf) - used, "\n");
  used += snprintf(buf + used, sizeof(buf) - used, "  }\n}\n");
  write_file(skill_tree_layout_path, buf);
}

static void skill_tree_recalculate(Game *g) {
  g->skill_tree_damage_bonus = 0.0f;
  g->skill_tree_armor_bonus = 0.0f;
  g->skill_tree_xp_mult = 1.0f;
  g->skill_tree_spawn_scale = 1.0f;

  int count = skill_tree_node_count();
  for (int i = 0; i < count; i++) {
    int rank = g->skill_tree.upgrades[i];
    if (rank <= 0) continue;
    const SkillTreeNode *node = &skill_tree_nodes[i];
    switch (node->effect) {
      case SKILL_TREE_EFF_DAMAGE_PCT:
        g->skill_tree_damage_bonus += node->value_per_rank * (float)rank;
        break;
      case SKILL_TREE_EFF_ARMOR_FLAT:
        g->skill_tree_armor_bonus += node->value_per_rank * (float)rank;
        break;
      case SKILL_TREE_EFF_XP_MULT:
        g->skill_tree_xp_mult += node->value_per_rank * (float)rank;
        break;
      case SKILL_TREE_EFF_SPAWN_SCALE:
        g->skill_tree_spawn_scale *= (1.0f + node->value_per_rank * (float)rank);
        break;
      case SKILL_TREE_EFF_NONE:
      default:
        break;
    }
  }
  g->skill_tree_spawn_scale = clampf(g->skill_tree_spawn_scale, 0.5f, 2.0f);
}

void skill_tree_apply_run_mods(Game *g) {
  if (!g) return;
  Player *p = &g->player;
  p->base.damage += g->skill_tree_damage_bonus;
  p->base.armor += g->skill_tree_armor_bonus;
}

void skill_tree_progress_save(Game *g) {
  if (!g) return;
  char payload[1024];
  skill_tree_build_payload(&g->skill_tree, payload, sizeof(payload));
  unsigned int checksum = skill_tree_checksum(payload);
  char buf[2048];
  int used = 0;
  used += snprintf(buf + used, sizeof(buf) - used,
                   "{\n  \"points\": %d,\n  \"total_points\": %d,\n  \"checksum\": %u,\n  \"upgrades\": {\n",
                   g->skill_tree.points,
                   g->skill_tree.total_points,
                   checksum);
  int count = skill_tree_node_count();
  for (int i = 0; i < count; i++) {
    const char *comma = (i == count - 1) ? "" : ",";
    used += snprintf(buf + used, sizeof(buf) - used, "    \"%s\": %d%s\n",
                     skill_tree_nodes[i].key, g->skill_tree.upgrades[i], comma);
  }
  used += snprintf(buf + used, sizeof(buf) - used, "  }\n}\n");
  write_file("data/skill_tree_progress.json", buf);
}

void skill_tree_progress_init(Game *g) {
  if (!g) return;
  memset(&g->skill_tree, 0, sizeof(g->skill_tree));
  g->skill_tree.points = 0;
  g->skill_tree.total_points = 0;

  char *json = read_file("data/skill_tree_progress.json");
  if (!json) {
    skill_tree_recalculate(g);
    skill_tree_progress_save(g);
    return;
  }

  jsmn_parser parser;
  jsmn_init(&parser);
  jsmntok_t tokens[256];
  int count = jsmn_parse(&parser, json, strlen(json), tokens, 256);
  if (count < 0) {
    free(json);
    skill_tree_recalculate(g);
    return;
  }

  int pt = find_key(json, tokens, 0, "points");
  int tt = find_key(json, tokens, 0, "total_points");
  int ct = find_key(json, tokens, 0, "checksum");
  if (pt > 0) g->skill_tree.points = token_int(json, &tokens[pt]);
  if (tt > 0) g->skill_tree.total_points = token_int(json, &tokens[tt]);
  unsigned int saved_checksum = 0;
  if (ct > 0) saved_checksum = (unsigned int)token_int(json, &tokens[ct]);

  int upgrades = find_key(json, tokens, 0, "upgrades");
  if (upgrades > 0 && tokens[upgrades].type == JSMN_OBJECT) {
    int node_count = skill_tree_node_count();
    for (int i = 0; i < node_count; i++) {
      int ut = find_key(json, tokens, upgrades, skill_tree_nodes[i].key);
      if (ut > 0) g->skill_tree.upgrades[i] = token_int(json, &tokens[ut]);
    }
  }

  free(json);
  skill_tree_recalculate(g);

  char payload[1024];
  skill_tree_build_payload(&g->skill_tree, payload, sizeof(payload));
  unsigned int checksum = skill_tree_checksum(payload);
  if (ct <= 0) {
    skill_tree_progress_save(g);
  } else if (saved_checksum != checksum) {
    memset(&g->skill_tree, 0, sizeof(g->skill_tree));
    skill_tree_recalculate(g);
    skill_tree_progress_save(g);
  }
}

int skill_tree_try_purchase_upgrade(Game *g, int upgrade_index) {
  if (!g) return 0;
  int count = skill_tree_node_count();
  if (upgrade_index < 0 || upgrade_index >= count) return 0;
  int pk = skill_tree_ui_parent_kind(upgrade_index);
  int parent = skill_tree_ui_parent_index(upgrade_index);
  if (pk == 1 && parent >= 0) {
    int parent_max = skill_tree_upgrade_max_rank(parent);
    if (g->skill_tree.upgrades[parent] < parent_max) return 0;
  }
  int rank = g->skill_tree.upgrades[upgrade_index];
  int max_rank = skill_tree_upgrade_max_rank(upgrade_index);
  if (rank >= max_rank) return 0;
  int cost = skill_tree_upgrade_cost(rank);
  if (g->skill_tree.points < cost) return 0;
  g->skill_tree.points -= cost;
  g->skill_tree.upgrades[upgrade_index] += 1;
  skill_tree_recalculate(g);
  skill_tree_progress_save(g);
  return 1;
}

void skill_tree_reset_upgrades(Game *g) {
  if (!g) return;
  int count = skill_tree_node_count();
  for (int i = 0; i < count; i++) g->skill_tree.upgrades[i] = 0;
  g->skill_tree.points = g->skill_tree.total_points;
  skill_tree_recalculate(g);
  skill_tree_progress_save(g);
}




