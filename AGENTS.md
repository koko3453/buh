# Project Summary

Hack n Slash Survivor is a Vampire Survivors-style endless survival game written in C with SDL2. The player moves freely while weapons auto-fire; enemies spawn continuously with scaling difficulty, drop XP orbs, and the player levels up to pick 1 of 3 random upgrades (items or weapons). Content (weapons, items, enemies, characters) is data-driven via JSON. The build uses CMake with vcpkg for SDL2/SDL2_image/SDL2_ttf.

# Core Loop

- Spawn enemies around the player outside the viewport.
- Auto-attacks and special weapon behaviors run continuously.
- XP orbs and health pickups drop; collecting XP triggers level-ups.
- Level-up: choose from items/weapons; rerolls and a one-time "High Roll" exist.
- Difficulty/time scale up indefinitely; a boss event can trigger a special arena.

# Key Systems

- Game modes: start, wave, level-up, pause, game over.
- Player stats with clamps (damage, hp, move speed, attack speed, armor, dodge, crit, cooldown reduction, xp magnet, regen).
- Weapons are data-defined with runtime special behaviors (e.g., lightning zone, laser/whip line hits, scythe arc, daggers multi-target).
- Items are passive and always apply; Legendary Amplification can scale legendary stats.
- Status effects: bleed, burn, slow, stun, armor shred.

# Repo Layout

- `src/core/main.c`: Entry point, initialization, asset loading, game loop.
- `src/core/game.c`: Core logic, UI rendering, helpers.
- `src/render/render.c`: Rendering helpers.
- `src/systems/weapons.c`: Weapon behavior/firing.
- `src/systems/enemies.c`: Enemy AI/behavior.
- `include/core/game.h`: Shared types/constants.
- `src/data/registry.c`: Data loading/lookup for JSON content.
- `include/data/registry.h`: Data loading/lookup API.
- `third_party/jsmn/jsmn.h`: JSON parser.
- `data/*.json`: Characters, enemies, items, weapons.
- `data/schema/`: JSON schema docs for data files.
- `data/assets/`: Sprites and art assets (user-supplied).
- `tests/test_game.c`: Game logic tests.
- `tools/`: Tooling (vcpkg lives here).
- `tools/validate_data.py`: JSON data validator.
- `tools/validate_data.bat`: Windows wrapper for validator.
- `build/`: CMake outputs (generated).
- `my-website/`: Unrelated web assets.

# Build/Run (Windows)

1. Install dependencies via vcpkg:
   - `tools\vcpkg\bootstrap-vcpkg.bat`
   - `tools\vcpkg\vcpkg install sdl2 sdl2-ttf sdl2-image --triplet=x64-windows`
2. Configure and build:
   - `cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake`
   - `cmake --build build --config Release`
3. Run:
   - `build\Release\buh.exe`
