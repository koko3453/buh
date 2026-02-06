# Hack n Slash Survivor (C + SDL2)

A Vampire Survivors-style endless survival game built in C with SDL2. Features:
- Endless auto-attacking combat with infinite scrolling map
- Level up system - choose 1 of 4 upgrades on each level
- Enemies spawn continuously with scaling difficulty over time
- Ultimate ability (SPACE) with 2-minute cooldown
- Data-driven content via JSON
 - Item effects: regen, chain procs, XP pickup effects

## Gameplay

Kill enemies to collect XP orbs. When you level up, choose 1 of 3 random upgrades:
- **Items** - Passive stat boosts (damage, health, speed, armor, etc.)
- **Weapons** - Add new weapons or upgrade existing ones


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
Create `data/assets/` folder and add your sprites

### 4. Build
```bash
set VCPKG_ROOT=tools\vcpkg
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

### 5. Run
```bash
build\Release\buh.exe
```

## Controls
| Key | Action |
|-----|--------|
| WASD | Move |
| SPACE | Ultimate (kills all enemies, 2 min cooldown) |
| P | Pause |
| Mouse | Select upgrades on level up |

## Stats
The game uses these core stats:
- **Damage** - Attack damage
- **Health** - Maximum HP
- **Move Speed** - Movement speed
- **Attack Speed** - Auto-attack rate
- **Armor** - Damage reduction
- **Dodge** - Chance to avoid damage
 - **Crit Chance** - Chance to crit (if weapon scales crit)
 - **Crit Damage** - Bonus crit multiplier
 - **Cooldown Reduction** - Faster weapon cooldowns
 - **XP Magnet** - XP pull range
 - **HP Regen** - HP per second

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
<img width="820" height="340" alt="{89E12881-F2FB-4C23-9EB1-876C57DC99F5}" src="https://github.com/user-attachments/assets/0534edc0-016c-476b-9434-eaea61268abe" />


## License
MIT

Note: This is a prototype; balance values are starter tuning and meant to be tweaked.
