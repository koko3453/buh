#!/usr/bin/env python3
import json
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DATA_DIR = os.path.join(ROOT, "data")

WEAPONS_PATH = os.path.join(DATA_DIR, "weapons.json")
ITEMS_PATH = os.path.join(DATA_DIR, "items.json")
ENEMIES_PATH = os.path.join(DATA_DIR, "enemies.json")
CHARACTERS_PATH = os.path.join(DATA_DIR, "characters.json")

STATS_KEYS = {
    "damage",
    "max_hp",
    "move_speed",
    "attack_speed",
    "armor",
    "dodge",
    "crit_chance",
    "crit_damage",
    "cooldown_reduction",
    "xp_magnet",
    "hp_regen",
}

SCALES_ALLOWED = {"damage", "attack_speed", "range", "crit"}


def load_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def err(errors, path, msg):
    errors.append(f"{path}: {msg}")


def is_number(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def require(obj, key, path, errors):
    if key not in obj:
        err(errors, path, f"missing '{key}'")
        return None
    return obj[key]


def check_stats(stats, path, errors):
    if not isinstance(stats, dict):
        err(errors, path, "stats must be an object")
        return
    for key, value in stats.items():
        if key not in STATS_KEYS:
            err(errors, path, f"unknown stat '{key}'")
            continue
        if not is_number(value):
            err(errors, path, f"stat '{key}' must be a number")


def check_unique_ids(items, path, errors):
    seen = set()
    for idx, item in enumerate(items):
        item_path = f"{path}[{idx}]"
        if not isinstance(item, dict):
            err(errors, item_path, "entry must be an object")
            continue
        item_id = item.get("id")
        if not isinstance(item_id, str) or not item_id:
            err(errors, item_path, "id must be a non-empty string")
            continue
        if item_id in seen:
            err(errors, item_path, f"duplicate id '{item_id}'")
        seen.add(item_id)


def validate_weapons(data, errors):
    path = "data/weapons.json"
    weapons = require(data, "weapons", path, errors)
    if weapons is None:
        return
    if not isinstance(weapons, list):
        err(errors, path, "'weapons' must be an array")
        return

    check_unique_ids(weapons, path, errors)

    for idx, weapon in enumerate(weapons):
        wpath = f"{path}#weapons[{idx}]"
        if not isinstance(weapon, dict):
            err(errors, wpath, "entry must be an object")
            continue
        for key in ("id", "name", "type", "rarity"):
            val = require(weapon, key, wpath, errors)
            if val is not None and not isinstance(val, str):
                err(errors, wpath, f"'{key}' must be a string")
        for key in ("cooldown", "damage", "range", "projectile_speed"):
            val = require(weapon, key, wpath, errors)
            if val is not None and not is_number(val):
                err(errors, wpath, f"'{key}' must be a number")
        for key in ("pierce", "pellets", "homing"):
            if key in weapon and not isinstance(weapon[key], int):
                err(errors, wpath, f"'{key}' must be an integer")
        if "spread" in weapon and not is_number(weapon["spread"]):
            err(errors, wpath, "'spread' must be a number")
        if "crit_multiplier" in weapon and not is_number(weapon["crit_multiplier"]):
            err(errors, wpath, "'crit_multiplier' must be a number")
        if "scales" in weapon:
            scales = weapon["scales"]
            if not isinstance(scales, list):
                err(errors, wpath, "'scales' must be an array")
            else:
                for s in scales:
                    if not isinstance(s, str):
                        err(errors, wpath, "'scales' entries must be strings")
                    elif s not in SCALES_ALLOWED:
                        err(errors, wpath, f"unknown scale '{s}'")


def validate_items(data, errors):
    path = "data/items.json"
    items = require(data, "items", path, errors)
    if items is None:
        return
    if not isinstance(items, list):
        err(errors, path, "'items' must be an array")
        return

    check_unique_ids(items, path, errors)

    for idx, item in enumerate(items):
        ipath = f"{path}#items[{idx}]"
        if not isinstance(item, dict):
            err(errors, ipath, "entry must be an object")
            continue
        for key in ("id", "name", "rarity", "desc"):
            val = require(item, key, ipath, errors)
            if val is not None and not isinstance(val, str):
                err(errors, ipath, f"'{key}' must be a string")
        if "stats" in item:
            check_stats(item["stats"], f"{ipath}.stats", errors)
        if "proc" in item:
            proc = item["proc"]
            if not isinstance(proc, dict):
                err(errors, ipath, "'proc' must be an object")
            else:
                if "chance" in proc and not is_number(proc["chance"]):
                    err(errors, ipath, "'proc.chance' must be a number")
                if "damage" in proc and not is_number(proc["damage"]):
                    err(errors, ipath, "'proc.damage' must be a number")
                if "bounces" in proc and not isinstance(proc["bounces"], int):
                    err(errors, ipath, "'proc.bounces' must be an integer")
                if "range" in proc and not is_number(proc["range"]):
                    err(errors, ipath, "'proc.range' must be a number")
        for key in ( 
            "slow_on_hit", 
            "slow_aura", 
            "burn_on_hit", 
            "burn_aura", 
            "thorns_percent", 
            "lifesteal_on_kill", 
            "rarity_bias", 
            "slow_bonus_damage", 
            "legendary_amp", 
            "hp_regen_amp", 
            "xp_kill_chance", 
            "ultimate_cdr", 
            "totem_spawn_rate", 
            "totem_duration_bonus", 
        ): 
            if key in item and not is_number(item[key]): 
                err(errors, ipath, f"'{key}' must be a number") 
        if "chest_reroll_bonus" in item and not isinstance(item["chest_reroll_bonus"], int): 
            err(errors, ipath, "'chest_reroll_bonus' must be an integer") 


def validate_enemies(data, errors):
    path = "data/enemies.json"
    enemies = require(data, "enemies", path, errors)
    if enemies is None:
        return
    if not isinstance(enemies, list):
        err(errors, path, "'enemies' must be an array")
        return

    check_unique_ids(enemies, path, errors)

    for idx, enemy in enumerate(enemies):
        epath = f"{path}#enemies[{idx}]"
        if not isinstance(enemy, dict):
            err(errors, epath, "entry must be an object")
            continue
        for key in ("id", "name", "role"):
            val = require(enemy, key, epath, errors)
            if val is not None and not isinstance(val, str):
                err(errors, epath, f"'{key}' must be a string")
        for key in ("hp", "speed", "damage"):
            val = require(enemy, key, epath, errors)
            if val is not None and not is_number(val):
                err(errors, epath, f"'{key}' must be a number")
        for key in ("cooldown", "projectile_speed", "charge_speed", "charge_cooldown", "explode_radius"):
            if key in enemy and not is_number(enemy[key]):
                err(errors, epath, f"'{key}' must be a number")


def validate_characters(data, weapon_ids, errors):
    path = "data/characters.json"
    chars = require(data, "characters", path, errors)
    if chars is None:
        return
    if not isinstance(chars, list):
        err(errors, path, "'characters' must be an array")
        return

    check_unique_ids(chars, path, errors)

    for idx, char in enumerate(chars):
        cpath = f"{path}#characters[{idx}]"
        if not isinstance(char, dict):
            err(errors, cpath, "entry must be an object")
            continue
        for key in ("id", "name", "portrait", "weapon", "rule", "ultimate"):
            val = require(char, key, cpath, errors)
            if val is not None and not isinstance(val, str):
                err(errors, cpath, f"'{key}' must be a string")
        if "stats" in char:
            check_stats(char["stats"], f"{cpath}.stats", errors)
        weapon = char.get("weapon")
        if isinstance(weapon, str) and weapon and weapon not in weapon_ids:
            err(errors, cpath, f"weapon '{weapon}' not found in weapons.json")


def main():
    errors = []

    try:
        weapons = load_json(WEAPONS_PATH)
        validate_weapons(weapons, errors)
        weapon_ids = {w.get("id") for w in weapons.get("weapons", []) if isinstance(w, dict)}
    except Exception as exc:
        err(errors, "data/weapons.json", f"failed to load: {exc}")
        weapon_ids = set()

    try:
        items = load_json(ITEMS_PATH)
        validate_items(items, errors)
    except Exception as exc:
        err(errors, "data/items.json", f"failed to load: {exc}")

    try:
        enemies = load_json(ENEMIES_PATH)
        validate_enemies(enemies, errors)
    except Exception as exc:
        err(errors, "data/enemies.json", f"failed to load: {exc}")

    try:
        chars = load_json(CHARACTERS_PATH)
        validate_characters(chars, weapon_ids, errors)
    except Exception as exc:
        err(errors, "data/characters.json", f"failed to load: {exc}")

    if errors:
        print("Data validation failed:")
        for msg in errors:
            print(f"- {msg}")
        return 1

    print("Data validation OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
