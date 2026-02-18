import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

CHAR_PATH = ROOT / "data" / "characters.json"
ENEMY_PATH = ROOT / "data" / "enemies.json"
ASSETS = ROOT / "data" / "assets"


def load_json(path: Path):
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def file_exists(rel_path: str) -> bool:
    return (ASSETS / rel_path).exists()


def main() -> None:
    data = load_json(CHAR_PATH)
    chars = data.get("characters", [])
    enemies = load_json(ENEMY_PATH).get("enemies", [])

    missing_chars = []
    for c in chars:
        cid = c.get("id", "<unknown>")
        portrait = c.get("portrait", "")
        walk_strip = c.get("walk_strip", "")
        missing = []
        if portrait:
            if not file_exists(f"portraits/{portrait}"):
                missing.append(f"portraits/{portrait}")
        else:
            missing.append("portrait (missing field)")
        if walk_strip:
            if not file_exists(walk_strip):
                missing.append(walk_strip)
        if missing:
            missing_chars.append((cid, missing))

    # Enemy sprite expectations
    base_enemy = "enemies/goo_enemy.png"
    base_exists = file_exists(base_enemy)
    special = {
        "eye": "enemies/eye_enemy.png",
        "ghost": "enemies/ghost_enemy.png",
        "charger": "enemies/reaper_enemy.png",
    }

    missing_enemies = []
    for e in enemies:
        eid = e.get("id", "<unknown>")
        missing = []
        if eid in special:
            if not file_exists(special[eid]):
                missing.append(special[eid])
        else:
            if not base_exists:
                missing.append(base_enemy)
        missing_enemies.append((eid, missing))

    print("Missing hero sprites:")
    if not missing_chars:
        print("  (none)")
    else:
        for cid, missing in missing_chars:
            print(f"  {cid}:")
            for m in missing:
                print(f"    - {m}")

    print("\nMissing enemy sprites:")
    any_missing = False
    for eid, missing in missing_enemies:
        if not missing:
            continue
        any_missing = True
        print(f"  {eid}:")
        for m in missing:
            print(f"    - {m}")
    if not any_missing:
        print("  (none)")


if __name__ == "__main__":
    main()
