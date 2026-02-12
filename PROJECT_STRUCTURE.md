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
- `my-website/`: Unrelated web assets.

## Source
- `src/main.c`: Entry point, initialization, asset loading, game loop.
- `src/game.c`: Core game logic, state updates, UI rendering, helpers.
- `src/render.c`: Rendering helpers and render path.
- `src/weapons.c`: Weapon behavior and firing logic.
- `src/enemies.c`: Enemy AI/behavior.
- `src/game.h`: Shared types, constants, and function declarations.
- `src/third_party/jsmn.h`: JSON parser dependency.

## Data
- `data/characters.json`: Character definitions.
- `data/enemies.json`: Enemy definitions.
- `data/items.json`: Item definitions.
- `data/weapons.json`: Weapon definitions.
- `data/assets/`: Images and sprites used by the game.
  - `data/assets/portraits/`: Character portraits.
  - `data/assets/weapons/`: Weapon sprites.
  - `data/assets/orbs_rarity/`: Rarity orb sprites.

## Tests
- `tests/test_game.c`: Game logic tests.
