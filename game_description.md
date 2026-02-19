# Game Description

Hack n Slash Survivor is a fast, top-down survival game. You pick a hero, survive endless waves, collect XP, and choose upgrades as you level. Runs can shift between normal wave fights and special boss events.

## Core Loop

- Move and kite to survive while your weapons attack automatically.
- Collect XP orbs to level up and choose upgrades.
- Difficulty ramps up over time and enemy types evolve.
- Boss events pull you into a focused fight with big attacks to dodge.

## Heroes & Ultimates

Each hero has a unique vibe and a signature ultimate:

- **Mage** — Ultimate: big area blast that wipes regular enemies.
- **Reaper** — Ultimate: big area blast that wipes regular enemies.
- **Rogue** — Ultimate: big area blast that wipes regular enemies.
- **Shapeshifter** — Ultimate: converts movement speed into attack speed for a short time.
- **Vampire** — Ultimate: big area blast that wipes regular enemies.
- **Gladiator** — Ultimate: big area blast that wipes regular enemies.
- **Gunslinger** — Ultimate: big area blast that wipes regular enemies.
- **Engineer** — Ultimate: big area blast that wipes regular enemies.
- **Paladin** — Ultimate: big area blast that wipes regular enemies.
- **Priestess** — Ultimate: big area blast that wipes regular enemies.
- **Sorceress** — Ultimate: big area blast that wipes regular enemies.
- **Molten** — Ultimate: overdrive with huge movement speed and heavy damage reduction.
- **Alchemist** — Ultimate: drains you to 1 HP over a few seconds, explodes to kill minions, then heals you back to full.

## Weapons & Upgrades

Weapons auto-fire and scale with your stats. Upgrades can add raw power, survivability, or utility like burn, slow, lifesteal, and thorns.

## Totems (Map Events)

Totems randomly spawn and must be destroyed (not picked up). Each totem affects the whole battlefield:

- **Freeze Totem** — Freezes all enemies for a short time.
- **Curse Totem** — Applies a strong damage-over-time effect to all enemies.
- **Damage Totem** — Instantly kills all non-boss enemies on screen.

## Enemies

Enemy roles vary to keep pressure high:

- **Grunt** — melee
- **Bruiser** — melee
- **Ranger** — ranged
- **Charger** — charging attacker
- **Exploder** — rushes and explodes
- **Sentry Tower** — turret
- **Eye** — ranged
- **Ghost** — melee
- **Arena Warden** — boss

## Skill Tree

A persistent skill tree lets you spend points earned across runs. It is split into themed branches and supports experimenting with layouts and connections in the editor.

## Items (with Quality, Stats, and Perks)

| Item | Rarity | Stats | Perks |
|---|---|---|---|
| Quick Hands | common | attack_speed: 0.12 | — |
| Glass Cannon | rare | damage: 0.22, max_hp: -15 | — |
| Runner | common | move_speed: 0.1 | — |
| Thick Hide | uncommon | armor: 2 | — |
| Toughness | common | max_hp: 20 | — |
| Power Surge | uncommon | damage: 0.1 | — |
| Swift Boots | common | move_speed: 0.12 | — |
| Iron Helm | common | armor: 3, max_hp: 10 | — |
| Steel Chest | uncommon | armor: 4, max_hp: 30 | — |
| Spiked Gloves | uncommon | damage: 0.08, attack_speed: 0.05 | — |
| Swift Boots+ | uncommon | move_speed: 0.15, dodge: 0.03 | — |
| Ring of Power | uncommon | damage: 0.12 | — |
| Ring of Speed | uncommon | attack_speed: 0.1 | — |
| Amulet of Fortitude | rare | max_hp: 40, armor: 2 | — |
| War Relic | rare | damage: 0.15, attack_speed: 0.08 | — |
| Buckler Shield | uncommon | armor: 3, dodge: 0.04 | — |
| Berserker Rage | epic | damage: 0.25, attack_speed: 0.15, armor: -3 | — |
| Iron Skin | rare | armor: 5, max_hp: 30 | — |
| Storm Crystal | epic | attack_speed: 0.2, damage: 0.12 | — |
| Shadow Cloak | rare | dodge: 0.08, move_speed: 0.15 | — |
| Titan Gauntlets | legendary | damage: 0.2, armor: 3 | — |
| Crown of Might | epic | max_hp: 50, armor: 4 | — |
| Dragon Heart | legendary | max_hp: 80, armor: 6 | — |
| Boots of Hermes | legendary | move_speed: 0.25, dodge: 0.1 | — |
| Ring of Doom | epic | damage: 0.2, max_hp: -25 | — |
| Amulet of Defense | rare | armor: 4, dodge: 0.05 | — |
| Relic of Destruction | legendary | damage: 0.3, attack_speed: 0.15 | — |
| Mystic Orb | rare | damage: 0.12, attack_speed: 0.08 | — |
| Warrior's Belt | uncommon | max_hp: 25, damage: 0.05 | — |
| Featherweight Charm | common | move_speed: 0.08, dodge: 0.02 | — |
| Heavy Plate | epic | armor: 8, move_speed: -0.1 | — |
| Bloodlust Amulet | legendary | damage: 0.25, attack_speed: 0.2 | — |
| Tank Armor | legendary | armor: 10, max_hp: 60, move_speed: -0.15 | — |
| Agility Ring | rare | attack_speed: 0.12, dodge: 0.05 | — |
| Bulwark Shield | epic | armor: 6, max_hp: 30 | — |
| Assassin Garb | epic | dodge: 0.12, damage: 0.1, armor: -2 | — |
| Speed Demon | rare | move_speed: 0.2, attack_speed: 0.1 | — |
| Fortress Helm | rare | armor: 5, max_hp: 20 | — |
| Brawler Gloves | rare | damage: 0.12, attack_speed: 0.08 | — |
| Frost Blade | rare | damage: 0.08 | slow_on_hit: 0.35 |
| Frozen Heart | epic | max_hp: 30, armor: 2 | slow_aura: 120 |
| Immolating Core | epic | damage: 0.06 | burn_on_hit: 0.25, burn_aura: 110 |
| Elder Sigil | epic | — | legendary_amp: 0.01 |
| Thorn Plating | uncommon | armor: 2 | thorns_percent: 0.2 |
| Blood Vial | rare | — | lifesteal_on_kill: 12 |
| Sharpened Edge | uncommon | crit_chance: 0.1, crit_damage: 0.3 | — |
| Magnetic Core | common | xp_magnet: 120 | — |
| Temporal Gears | rare | cooldown_reduction: 0.12 | — |
| Lucky Charm | rare | — | rarity_bias: 0.25 |
| Frostbite Charm | rare | — | slow_on_hit: 0.2, slow_bonus_damage: 0.2 |
| Shock Coil | epic | — | proc: chance 0.15, dmg 12, bounces 3, range 140 |
| Herbal Tonic | common | hp_regen: 0.6 | — |
| Vital Bloom | rare | hp_regen: 1.2 | — |
| Emerald Font | epic | hp_regen: 2.0, max_hp: 20 | — |
| Eternal Spring | legendary | hp_regen: 1.0 | hp_regen_amp: 0.5 |
| Verdant Crown | legendary | max_hp: 40 | hp_regen_amp: 1.0 |
| Executioner's Sigil | legendary | — | xp_kill_chance: 0.10 |
| Master Key | uncommon | — | chest_reroll_bonus: 1 |
| Totem Beacon | rare | — | totem_spawn_rate: 0.2 |
| Totem Core | epic | — | totem_duration_bonus: 0.5 |
| Chrono Sigil | epic | — | ultimate_cdr: 0.2 |
| Vault Pass | rare | — | chest_reroll_bonus: 2 |
| Totem Lure | uncommon | — | totem_spawn_rate: 0.35 |
| Everlasting Totem | legendary | — | totem_duration_bonus: 1.0 |
| Time Sheath | legendary | — | ultimate_cdr: 0.35 |
| Marksman Focus | uncommon | crit_chance: 0.08 | — |
| Blood Quartz | rare | hp_regen: 0.8, max_hp: 10 | — |
| Storm Shard | rare | attack_speed: 0.14, cooldown_reduction: 0.06 | — |
| Gale Band | uncommon | move_speed: 0.12, xp_magnet: 60 | — |
| Ironroots | epic | armor: 6, hp_regen: 0.6 | — |
| Savage Pact | epic | damage: 0.18, crit_damage: 0.4 | — |
| Silent Steps | rare | dodge: 0.08, move_speed: 0.08 | — |
| Arcane Well | epic | damage: 0.08, xp_magnet: 120 | — |
