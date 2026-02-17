# Project Structure

## Root
- `CMakeLists.txt`: CMake build config for the game and tests.
- `build_run.bat`: Build + run Release, copies assets.
- `build.bat`: Minimal CMake build helper.
- `README.md`: Project overview and usage notes.
- `SYSTEM_DESCRIPTION.md`: System/design notes.
- `todo_ideas.md`: Backlog ideas.
- `compile_message.txt`: Build output log (generated).
- `log.txt`, `combat_log.txt`: Runtime logs (generated).
- `build/`: CMake build outputs (generated).
- `tools/`: Local tooling (vcpkg lives here).
- `tools/validate_data.py`: JSON data validator.
- `tools/validate_data.bat`: Windows wrapper for validator.
- `my-website/`: Unrelated web assets.

## Source
- `src/core/main.c`: Entry point, initialization, asset loading, game loop.
- `src/core/game.c`: Core game logic, state updates, UI rendering, helpers.
- `src/render/render.c`: Rendering helpers and render path.
- `src/systems/weapons.c`: Weapon behavior and firing logic.
- `src/systems/enemies.c`: Enemy AI/behavior.
- `include/core/game.h`: Shared types, constants, and function declarations.
- `src/data/registry.c`: JSON data loading and lookup.
- `include/data/registry.h`: JSON data loading API.
- `third_party/jsmn/jsmn.h`: JSON parser dependency.

## Data
- `data/characters.json`: Character definitions.
- `data/enemies.json`: Enemy definitions.
- `data/items.json`: Item definitions.
- `data/weapons.json`: Weapon definitions.
- `data/schema/`: JSON schema docs for data files.
- `data/assets/`: Images and sprites used by the game.
  - `data/assets/portraits/`: Character portraits.
  - `data/assets/weapons/`: Weapon sprites.
  - `data/assets/orbs_rarity/`: Rarity orb sprites.

## Tests
- `tests/test_game.c`: Game logic tests.
