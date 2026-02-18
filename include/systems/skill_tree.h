#ifndef BUH_SYSTEMS_SKILL_TREE_H
#define BUH_SYSTEMS_SKILL_TREE_H

typedef enum {
  SKILL_TREE_BRANCH_BOSS,
  SKILL_TREE_BRANCH_HORDE,
  SKILL_TREE_BRANCH_MAP_ENV,
  SKILL_TREE_BRANCH_ROLLS,
  SKILL_TREE_BRANCH_STATS,
  SKILL_TREE_BRANCH_COUNT
} SkillTreeBranch;

typedef enum {
  SKILL_TREE_EFF_NONE,
  SKILL_TREE_EFF_DAMAGE_PCT,
  SKILL_TREE_EFF_ARMOR_FLAT,
  SKILL_TREE_EFF_XP_MULT,
  SKILL_TREE_EFF_SPAWN_SCALE
} SkillTreeEffectType;

typedef struct {
  const char *key;
  const char *name;
  const char *desc;
  int max_rank;
  int parent; /* -1 = root */
  SkillTreeBranch branch;
  int depth;
  SkillTreeEffectType effect;
  float value_per_rank;
} SkillTreeNode;

int skill_tree_node_count(void);
const SkillTreeNode *skill_tree_node_get(int index);
const char *skill_tree_branch_name(int branch);
int skill_tree_upgrade_max_rank(int idx);
int skill_tree_upgrade_cost(int rank);
int skill_tree_layout_load(void);
void skill_tree_layout_save(void);
int skill_tree_layout_get(int index, float *x, float *y);
void skill_tree_layout_set(int index, float x, float y);
void skill_tree_layout_clear(void);
int skill_tree_custom_count(void);
int skill_tree_custom_add(float x, float y);
int skill_tree_custom_get(int index, float *x, float *y);
void skill_tree_custom_set(int index, float x, float y);
const char *skill_tree_custom_name(int index);
const char *skill_tree_custom_desc(int index);
void skill_tree_custom_set_name(int index, const char *name);
void skill_tree_custom_set_desc(int index, const char *desc);
int skill_tree_custom_max_rank(int index);
void skill_tree_custom_set_max_rank(int index, int max_rank);
int skill_tree_custom_parent_kind(int index);
int skill_tree_custom_parent_index(int index);
void skill_tree_custom_set_parent(int index, int parent_kind, int parent_index);
void skill_tree_override_set_max_rank(int index, int max_rank);
int skill_tree_override_max_rank(int index);
void skill_tree_override_set_parent(int index, int parent_kind, int parent_index);
int skill_tree_ui_parent_kind(int index);
int skill_tree_ui_parent_index(int index);
const char *skill_tree_ui_name(int index);
const char *skill_tree_ui_desc(int index);
void skill_tree_override_set_name(int index, const char *name);
void skill_tree_override_set_desc(int index, const char *desc);

#endif
