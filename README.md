# Hack n Slash Game (C + SDL2)

A hack-and-slash arena survival game built in C with SDL2. Features:
- Auto-attacking combat with infinite scrolling map
- Wave-based enemy spawns with scaling difficulty
- Shop between waves (items, weapons, reroll, heal)
- Ultimate ability (SPACE) with 2-minute cooldown
- Data-driven content via JSON

## Setup

### 1. Clone the repository
```bash
  git clone https://github.com/koko3453/buh.git
cd buh
```

### 2. Setup vcpkg (package manager)
```bash
git clone https://github.com/microsoft/vcpkg tools/vcpkg
tools\vcpkg\bootstrap-vcpkg.bat
tools\vcpkg\vcpkg install sdl2 sdl2-ttf sdl2-image --triplet=x64-windows
```

### 3. Add assets
Create `data/assets/` folder and add your sprites:
- `player..png` - Player sprite
- `goo_green.png` - Enemy sprite  
- `goo_bolt.png` - Enemy projectile
- `hd_ground_tile.png` - Ground tile
- `wall.png` - Wall tile
- `health_flask.png` - Health pickup

### 4. Build
```bash
set VCPKG_ROOT=tools\vcpkg
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

### 5. Run
```bash
build\Release\hack_arena.exe
```

## Controls
| Key | Action |
|-----|--------|
| WASD | Move |
| SPACE | Ultimate (kills all enemies, 2 min cooldown) |
| P | Pause |
| Mouse | Shop interaction |

## Stats
The game uses 6 core stats:
- **Damage** - Attack damage
- **Health** - Maximum HP
- **Move Speed** - Movement speed
- **Attack Speed** - Auto-attack rate
- **Armor** - Damage reduction
- **Dodge** - Chance to avoid damage

## Project Structure
```
├── src/main.c           # Game code
├── src/third_party/     # JSMN JSON parser
├── data/*.json          # Game data (weapons, items, enemies, characters)
├── data/assets/         # Sprites (not included - add your own)
├── tests/               # Unit tests
├── CMakeLists.txt       # Build config
└── tools/vcpkg/         # Package manager (clone separately)
```

## License
MIT

Note: This is a prototype; balance values are starter tuning and meant to be tweaked.
