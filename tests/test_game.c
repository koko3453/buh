#define UNIT_TESTS
#include "../src/main.c"
#include <assert.h>

static void test_db_load() {
  Database db;
  memset(&db, 0, sizeof(db));
  assert(db_load(&db));
  assert(db.weapon_count > 0);
  assert(db.item_count > 0);
  assert(db.enemy_count > 0);
  for (int i = 0; i < db.enemy_count; i++) {
    assert(db.enemies[i].hp > 0.0f);
  }
}

static void test_weapon_upgrade() {
  Player p;
  memset(&p, 0, sizeof(p));
  weapons_clear(&p);
  equip_weapon(&p, 0);
  assert(p.weapons[0].active);
  int lvl = p.weapons[0].level;
  equip_weapon(&p, 0);
  assert(p.weapons[0].level == lvl + 1);
  for (int i = 0; i < 10; i++) equip_weapon(&p, 0);
  assert(p.weapons[0].level == 4);
}

static void test_stats_scaling() {
  Database db;
  memset(&db, 0, sizeof(db));
  db.item_count = 1;
  ItemDef *it = &db.items[0];
  memset(it, 0, sizeof(*it));
  it->stats.damage = 0.1f;
  it->stats.attack_speed = 0.1f;
  it->stats.max_hp = 50;

  Player p;
  memset(&p, 0, sizeof(p));
  p.base.max_hp = 100;

  apply_item(&p, &db, it, 0);
  Stats total = player_total_stats(&p, &db);
  assert(total.damage > 0.0f);
  assert(total.attack_speed > 0.0f);
  assert(total.max_hp > 100.0f);
}

static void test_kill_count() {
  Game g;
  memset(&g, 0, sizeof(g));
  g.player.base.max_hp = 100;
  g.db.enemy_count = 1;
  strcpy(g.db.enemies[0].role, "grunt");
  g.db.enemies[0].hp = 10;
  g.enemies[0].active = 1;
  g.enemies[0].def_index = 0;
  g.enemies[0].hp = 0.0f;
  g.enemies[0].max_hp = 10.0f;
  update_enemies(&g, 0.016f);
  assert(g.kills == 1);
}

static void test_json_item_stats_apply() {
  Database db;
  memset(&db, 0, sizeof(db));
  assert(db_load(&db));
  int ring_idx = -1;
  for (int i = 0; i < db.item_count; i++) {
    if (strcmp(db.items[i].id, "ring_power") == 0) { ring_idx = i; break; }
  }
  assert(ring_idx >= 0);

  Player p;
  memset(&p, 0, sizeof(p));
  p.base.max_hp = 100;

  apply_item(&p, &db, &db.items[ring_idx], ring_idx);
  Stats total = player_total_stats(&p, &db);
  assert(total.damage >= 0.0f);
}

int main(void) {
  test_db_load();
  test_weapon_upgrade();
  test_stats_scaling();
  test_kill_count();
  test_json_item_stats_apply();
  return 0;
}
