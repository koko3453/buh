#ifndef BUH_CORE_TYPES_H
#define BUH_CORE_TYPES_H

#include "core/config.h"

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
  int upgrades[MAX_SKILL_TREE_UPGRADES];
} SkillTreeProgress;

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
  int alch_ult_phase; /* 0=inactive, 1=drain, 2=heal */
  float alch_ult_timer;
  float alch_ult_start_hp;
  float alch_ult_max_hp;
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
  int kind; /* 0=alchemist, 1=ultimate */
} Puddle;

typedef struct {
  int active;
  int type;       /* 0=scythe throw, 1=bite on enemy, 2=dagger projectile, 3=alchemist ult */
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

#endif

