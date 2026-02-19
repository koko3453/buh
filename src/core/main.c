#include "core/game.h"
#include "data/registry.h"
#include "render/render.h"
#include "systems/enemies.h"
#include "systems/skill_tree.h"

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;
  SetUnhandledExceptionFilter(crash_handler);

  g_log = fopen("log.txt", "w");
  g_combat_log = fopen("combat_log.txt", "w");
  if (g_log) log_line("Starting game...");

  Game game;
  memset(&game, 0, sizeof(game));

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    log_linef("SDL init failed: %s", SDL_GetError());
    return 1;
  }
  log_line("SDL init ok");

  if (TTF_Init() != 0) {
    log_linef("TTF init failed: %s", TTF_GetError());
  } else {
    log_line("TTF init ok");
  }

  if (IMG_Init(IMG_INIT_PNG) == 0) {
    log_linef("IMG init failed: %s", IMG_GetError());
  }

  if (!db_load(&game.db)) {
    log_line("Failed to load data.");
    return 1;
  }
  log_linef("Counts: weapons=%d items=%d enemies=%d characters=%d",
            game.db.weapon_count, game.db.item_count, game.db.enemy_count, game.db.character_count);
  log_line("Data load ok");
  skill_tree_layout_load(); 
  skill_tree_progress_init(&game); 

  game.window = SDL_CreateWindow("buh", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 WINDOW_W, WINDOW_H, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);
  game.renderer = SDL_CreateRenderer(game.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!game.window || !game.renderer) {
    log_linef("Window/renderer creation failed: %s", SDL_GetError());
    return 1;
  }
  log_line("Window/renderer created");

  SDL_Surface *icon = IMG_Load("data/assets/game_icon.png");
  if (icon) {
    SDL_SetWindowIcon(game.window, icon);
    SDL_FreeSurface(icon);
    log_line("Window icon set");
  } else {
    log_linef("Failed to load game_icon.png: %s", IMG_GetError());
  }

  SDL_Surface *cursor_surface = IMG_Load("data/assets/env/cursor.png");
  if (cursor_surface) {
    int cursor_w = cursor_surface->w / 2;
    int cursor_h = cursor_surface->h / 2;
    if (cursor_w < 1) cursor_w = 1;
    if (cursor_h < 1) cursor_h = 1;
    SDL_Surface *cursor_scaled =
        SDL_CreateRGBSurfaceWithFormat(0, cursor_w, cursor_h, 32, SDL_PIXELFORMAT_RGBA32);
    if (cursor_scaled) {
      SDL_Rect dst = {0, 0, cursor_w, cursor_h};
      SDL_BlitScaled(cursor_surface, NULL, cursor_scaled, &dst);
      game.cursor = SDL_CreateColorCursor(cursor_scaled, 0, 0);
      SDL_FreeSurface(cursor_scaled);
    } else {
      game.cursor = SDL_CreateColorCursor(cursor_surface, 0, 0);
    }
    SDL_FreeSurface(cursor_surface);
    if (game.cursor) {
      SDL_SetCursor(game.cursor);
      SDL_ShowCursor(SDL_ENABLE);
      log_line("Custom cursor set");
    } else {
      log_linef("Failed to create cursor: %s", SDL_GetError());
    }
  } else {
    log_linef("Failed to load cursor.png: %s", IMG_GetError());
  }

  game.tex_ground = IMG_LoadTexture(game.renderer, "data/assets/hd_ground_tile.png");
  game.tex_wall = IMG_LoadTexture(game.renderer, "data/assets/wall.png");
  game.tex_enemy = IMG_LoadTexture(game.renderer, "data/assets/enemies/goo_enemy.png");
  game.tex_enemy_eye = IMG_LoadTexture(game.renderer, "data/assets/enemies/eye_enemy.png");
  game.tex_enemy_ghost = IMG_LoadTexture(game.renderer, "data/assets/enemies/ghost_enemy.png");
  game.tex_enemy_charger = IMG_LoadTexture(game.renderer, "data/assets/enemies/reaper_enemy.png");
  game.tex_health_flask = IMG_LoadTexture(game.renderer, "data/assets/health_flask.png");
  if (game.tex_health_flask) log_line("Loaded health_flask.png");
  else log_linef("Failed to load health_flask.png: %s", IMG_GetError());
  if (game.tex_ground) log_line("Loaded hd_ground_tile.png");
  else log_linef("Failed to load ground.png: %s", IMG_GetError());
  if (game.tex_wall) log_line("Loaded wall.png");
  else log_linef("Failed to load wall.png: %s", IMG_GetError());
  if (game.tex_enemy) log_line("Loaded goo_enemy.png");
  else log_linef("Failed to load goo_enemy.png: %s", IMG_GetError());
  if (game.tex_enemy_eye) log_line("Loaded enemies/eye_enemy.png");
  else log_linef("Failed to load enemies/eye_enemy.png: %s", IMG_GetError());
  if (game.tex_enemy_ghost) log_line("Loaded enemies/ghost_enemy.png");
  else log_linef("Failed to load enemies/ghost_enemy.png: %s", IMG_GetError());
  if (game.tex_enemy_charger) log_line("Loaded reaper_enemy.png");
  else log_linef("Failed to load reaper_enemy.png: %s", IMG_GetError());
  game.tex_boss = load_texture_fallback(game.renderer, "data/assets/fire_goo_boss.png");
  if (game.tex_boss) log_line("Loaded fire_goo_boss.png");
  else log_linef("Failed to load fire_goo_boss.png: %s", IMG_GetError());
  game.tex_player_front = IMG_LoadTexture(game.renderer, "data/assets/player_front.png");
  if (game.tex_player_front) log_line("Loaded player_front.png");
  else log_linef("Failed to load player_front.png: %s", IMG_GetError());
  game.tex_player_back = IMG_LoadTexture(game.renderer, "data/assets/player_back.png");
  if (game.tex_player_back) log_line("Loaded player_back.png");
  else log_linef("Failed to load player_back.png: %s", IMG_GetError());
  game.tex_player_right = IMG_LoadTexture(game.renderer, "data/assets/player_right.png");
  if (game.tex_player_right) log_line("Loaded player_right.png");
  else log_linef("Failed to load player_right.png: %s", IMG_GetError());
  game.tex_player_left = IMG_LoadTexture(game.renderer, "data/assets/player_left.png");
  if (game.tex_player_left) log_line("Loaded player_left.png");
  else log_linef("Failed to load player_left.png: %s", IMG_GetError());
  for (int i = 0; i < game.db.character_count && i < MAX_CHARACTERS; i++) {
    game.tex_character_walk[i] = NULL;
    if (game.db.characters[i].walk_strip[0]) {
      char walk_path[160];
      snprintf(walk_path, sizeof(walk_path), "data/assets/%s", game.db.characters[i].walk_strip);
      game.tex_character_walk[i] = IMG_LoadTexture(game.renderer, walk_path);
      if (game.tex_character_walk[i]) log_linef("Loaded walk strip: %s", walk_path);
      else log_linef("Failed to load walk strip %s: %s", walk_path, IMG_GetError());
    }
  }
  game.tex_enemy_bolt = IMG_LoadTexture(game.renderer, "data/assets/goo_bolt.png");
  if (game.tex_enemy_bolt) log_line("Loaded goo_bolt.png");
  else log_linef("Failed to load goo_bolt.png: %s", IMG_GetError());
  game.tex_laser_beam = IMG_LoadTexture(game.renderer, "data/assets/laser_beam.png");
  if (game.tex_laser_beam) log_line("Loaded laser_beam.png");
  else log_linef("Failed to load laser_beam.png: %s", IMG_GetError());
  game.tex_lightning_zone = IMG_LoadTexture(game.renderer, "data/assets/lightning_zone.png");
  if (game.tex_lightning_zone) log_line("Loaded lightning_zone.png");
  else log_linef("Failed to load lightning_zone.png: %s", IMG_GetError());
  game.tex_chest = IMG_LoadTexture(game.renderer, "data/assets/env/chest.png");
  if (game.tex_chest) log_line("Loaded chest.png");
  else log_linef("Failed to load chest.png: %s", IMG_GetError());
  game.tex_totem_freeze = IMG_LoadTexture(game.renderer, "data/assets/env/freeze_totem.png");
  if (game.tex_totem_freeze) log_line("Loaded freeze_totem.png");
  else log_linef("Failed to load freeze_totem.png: %s", IMG_GetError());
  game.tex_totem_curse = IMG_LoadTexture(game.renderer, "data/assets/env/curse_totem.png");
  if (game.tex_totem_curse) log_line("Loaded curse_totem.png");
  else log_linef("Failed to load curse_totem.png: %s", IMG_GetError());
  game.tex_totem_damage = IMG_LoadTexture(game.renderer, "data/assets/env/damage_totem.png");
  if (game.tex_totem_damage) log_line("Loaded damage_totem.png");
  else log_linef("Failed to load damage_totem.png: %s", IMG_GetError());

  game.tex_scythe = IMG_LoadTexture(game.renderer, "data/assets/weapons/scythe.png");
  if (game.tex_scythe) log_line("Loaded scythe sprite");
  else log_linef("Failed to load scythe sprite: %s", IMG_GetError());
  game.tex_bite = IMG_LoadTexture(game.renderer, "data/assets/weapons/vampire_bite.png");
  if (game.tex_bite) log_line("Loaded vampire bite sprite");
  else log_linef("Failed to load vampire bite sprite: %s", IMG_GetError());
  game.tex_dagger = IMG_LoadTexture(game.renderer, "data/assets/weapons/dagger.png");
  if (game.tex_dagger) log_line("Loaded dagger sprite");
  else log_linef("Failed to load dagger sprite: %s", IMG_GetError());
  game.tex_alchemist_puddle = IMG_LoadTexture(game.renderer, "data/assets/weapons/alchemist_puddle.png");
  if (game.tex_alchemist_puddle) log_line("Loaded alchemist puddle sprite");
  else log_linef("Failed to load alchemist_puddle.png: %s", IMG_GetError());
  game.tex_fire_trail = IMG_LoadTexture(game.renderer, "data/assets/heroes/molten/fire_trail.png");
  if (game.tex_fire_trail) log_line("Loaded fire_trail.png");
  else log_linef("Failed to load fire_trail.png: %s", IMG_GetError());
  game.tex_alchemist_ult = IMG_LoadTexture(game.renderer, "data/assets/alchemist_ult.png");
  if (game.tex_alchemist_ult) log_line("Loaded alchemist ult sprite");
  else log_linef("Failed to load alchemist_ult.png: %s", IMG_GetError());
  game.tex_exp_orb = IMG_LoadTexture(game.renderer, "data/assets/exp_orb.png");
  if (game.tex_exp_orb) log_line("Loaded exp_orb sprite");
  else log_linef("Failed to load exp_orb.png: %s", IMG_GetError());

  game.tex_orb_common = load_texture_fallback(game.renderer, "data/assets/orbs_rarity/common_orb.png");
  game.tex_orb_uncommon = load_texture_fallback(game.renderer, "data/assets/orbs_rarity/uncommon_orb.png");
  game.tex_orb_rare = load_texture_fallback(game.renderer, "data/assets/orbs_rarity/rare_orb.png");
  game.tex_orb_epic = load_texture_fallback(game.renderer, "data/assets/orbs_rarity/epic_orb.png");
  game.tex_orb_legendary = load_texture_fallback(game.renderer, "data/assets/orbs_rarity/legendary_orb.png");
  if (game.tex_orb_common) log_line("Loaded common_orb.png");
  else log_linef("Failed to load common_orb.png: %s", IMG_GetError());
  if (game.tex_orb_uncommon) log_line("Loaded uncommon_orb.png");
  else log_linef("Failed to load uncommon_orb.png: %s", IMG_GetError());
  if (game.tex_orb_rare) log_line("Loaded rare_orb.png");
  else log_linef("Failed to load rare_orb.png: %s", IMG_GetError());
  if (game.tex_orb_epic) log_line("Loaded epic_orb.png");
  else log_linef("Failed to load epic_orb.png: %s", IMG_GetError());
  if (game.tex_orb_legendary) log_line("Loaded legendary_orb.png");
  else log_linef("Failed to load legendary_orb.png: %s", IMG_GetError());

  for (int i = 0; i < game.db.character_count && i < MAX_CHARACTERS; i++) {
    char portrait_path[128];
    snprintf(portrait_path, sizeof(portrait_path), "data/assets/portraits/%s", game.db.characters[i].portrait);
    game.tex_portraits[i] = IMG_LoadTexture(game.renderer, portrait_path);
    if (game.tex_portraits[i]) log_linef("Loaded portrait: %s", game.db.characters[i].portrait);
    else log_linef("Failed to load portrait %s: %s", game.db.characters[i].portrait, IMG_GetError());
  }

  game.font = TTF_OpenFont("C:/Windows/Fonts/verdana.ttf", 14);
  if (!game.font) {
    log_linef("Font load failed. Continuing without text. %s", TTF_GetError());
  }
  game.font_title = TTF_OpenFont("C:/Windows/Fonts/segoescb.ttf", 20);
  game.font_title_big = TTF_OpenFont("C:/Windows/Fonts/segoescb.ttf", 24);
  if (!game.font_title) {
    game.font_title = TTF_OpenFont("C:/Windows/Fonts/impact.ttf", 18);
  }
  if (!game.font_title_big) {
    game.font_title_big = TTF_OpenFont("C:/Windows/Fonts/impact.ttf", 22);
  }
  if (!game.font_title) {
    game.font_title = TTF_OpenFont("C:/Windows/Fonts/georgiab.ttf", 18);
  }
  if (!game.font_title_big) {
    game.font_title_big = TTF_OpenFont("C:/Windows/Fonts/georgiab.ttf", 22);
  }
  if (!game.font_title) {
    game.font_title = TTF_OpenFont("C:/Windows/Fonts/arialbd.ttf", 18);
  }
  if (!game.font_title) {
    game.font_title = game.font;
  }

  game_reset(&game);

  Uint64 now = SDL_GetPerformanceCounter();
  Uint64 last = 0;
  double accumulator = 0.0;
  double frequency = (double)SDL_GetPerformanceFrequency();
  int frame_log = 0;

  while (game.running == 0) game.running = 1;
  log_line("Main loop start");
  while (game.running) {
    last = now;
    now = SDL_GetPerformanceCounter();
    double frame = (double)(now - last) / frequency;
    if (frame > 0.25) frame = 0.25;
    accumulator += frame;
    update_window_view(&game);

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) game.running = 0;
      if (e.type == SDL_TEXTINPUT && game.skill_tree_text_active) { 
        size_t len = strlen(game.skill_tree_text_buf); 
        size_t add = strlen(e.text.text); 
        if (len + add < sizeof(game.skill_tree_text_buf) - 1) { 
          strncat(game.skill_tree_text_buf, e.text.text, sizeof(game.skill_tree_text_buf) - len - 1); 
        } 
        continue; 
      } 
      if (e.type == SDL_KEYDOWN) { 
        if (game.skill_tree_text_active) { 
          if (e.key.keysym.sym == SDLK_BACKSPACE) { 
            size_t len = strlen(game.skill_tree_text_buf); 
            if (len > 0) game.skill_tree_text_buf[len - 1] = '\0'; 
          } else if (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER) {
            if (game.skill_tree_text_kind == 1) {
              if (game.skill_tree_text_field == 1) {
                skill_tree_override_set_name(game.skill_tree_selected_index, game.skill_tree_text_buf);
              } else {
                skill_tree_override_set_desc(game.skill_tree_selected_index, game.skill_tree_text_buf);
              }
            } else if (game.skill_tree_text_kind == 2) {
              if (game.skill_tree_text_field == 1) {
                int idx = game.skill_tree_selected_index;
                if (idx >= 0) skill_tree_custom_set_name(idx, game.skill_tree_text_buf);
              } else {
                int idx = game.skill_tree_selected_index;
                if (idx >= 0) skill_tree_custom_set_desc(idx, game.skill_tree_text_buf);
              }
            }
            game.skill_tree_text_active = 0;
            SDL_StopTextInput();
          } else if (e.key.keysym.sym == SDLK_ESCAPE) {
            game.skill_tree_text_active = 0;
            SDL_StopTextInput();
          } 
          continue; 
        } 
        if (e.key.keysym.sym == SDLK_ESCAPE) { 
          if (game.mode == MODE_START && game.show_skill_tree) { 
            game.show_skill_tree = 0; 
            game.skill_tree_pan_x = 0.0f; 
            game.skill_tree_pan_y = 0.0f;
          } else {
            game.running = 0;
          }
        }
        if (e.key.keysym.sym == SDLK_F9) { 
          if (game.mode == MODE_START && game.show_skill_tree) { 
            game.skill_tree_edit_mode = !game.skill_tree_edit_mode; 
            if (!game.skill_tree_edit_mode) { 
              game.skill_tree_drag_index = -1; 
              game.skill_tree_drag_custom_index = -1; 
              game.skill_tree_connect_kind = 0; 
              game.skill_tree_connect_index = -1; 
            } 
          } 
        } 
        if (e.key.keysym.sym == SDLK_p) {
          if (game.mode == MODE_WAVE || game.mode == MODE_BOSS_EVENT || game.mode == MODE_PAUSE) toggle_pause(&game);
        }
        if (e.key.keysym.sym == SDLK_TAB) {
          if (game.mode == MODE_WAVE || game.mode == MODE_BOSS_EVENT || game.mode == MODE_PAUSE) toggle_pause(&game);
        }
        if (e.key.keysym.sym == SDLK_g && game.mode == MODE_GAMEOVER) game_reset(&game);
        if (e.key.keysym.sym == SDLK_F1) {
          if (game.db.enemy_count > 0) {
            for (int k = 0; k < 5; k++) spawn_enemy(&game, 0);
          }
        }
        if (e.key.keysym.sym == SDLK_F2) {
          game.time_scale = clampf(game.time_scale + 0.5f, 0.5f, 4.0f);
        }
        if (e.key.keysym.sym == SDLK_F3) {
          game.time_scale = clampf(game.time_scale - 0.5f, 0.5f, 4.0f);
        }
        if (e.key.keysym.sym == SDLK_F5) {
          /* no-op: pause is only via P/TAB in wave/boss */
        }
        if (e.key.keysym.sym == SDLK_5) {
          if (game.mode == MODE_WAVE && game.boss_event_cd <= 0.0f) {
            game.boss_event_cd = 5.0f;
            start_boss_event(&game);
          }
        }
        if (e.key.keysym.sym == SDLK_8) {
          game.debug_show_items = !game.debug_show_items;
        }
        if (game.mode == MODE_START) {
          if (e.key.keysym.sym == SDLK_LEFT) {
            game.start_page -= 1;
            build_start_page(&game);
          }
          if (e.key.keysym.sym == SDLK_RIGHT) {
            game.start_page += 1;
            build_start_page(&game);
          }
        }
        if (game.mode == MODE_WAVE || game.mode == MODE_BOSS_EVENT) { 
          if (e.key.keysym.sym == SDLK_SPACE && game.ultimate_cd <= 0.0f) { 
            activate_ultimate(&game); 
            float ult_cdr = player_ultimate_cdr(&game.player, &game.db); 
            game.ultimate_cd = 120.0f * (1.0f - ult_cdr); 
          } 
        } 
      }
      if (e.type == SDL_KEYUP) {
        if (e.key.keysym.sym == SDLK_F4) {
          game.debug_show_range = !game.debug_show_range;
        }
      }
      if (e.type == SDL_MOUSEBUTTONDOWN && game.mode == MODE_START) {
        int mx = e.button.x;
        int my = e.button.y;

        if (game.show_skill_tree) {
          if (game.skill_tree_edit_mode && e.button.button == SDL_BUTTON_LEFT) {
            game.skill_tree_drag_index = -1;
            game.skill_tree_drag_custom_index = -1;
            for (int i = 0; i < MAX_SKILL_TREE_UPGRADES; i++) {
              SDL_Rect r = game.skill_tree_item_rects[i];
              if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
                game.skill_tree_drag_index = i;
                game.skill_tree_drag_off_x = (float)mx - (r.x + r.w * 0.5f);
                game.skill_tree_drag_off_y = (float)my - (r.y + r.h * 0.5f);
                game.skill_tree_selected_kind = 1;
                game.skill_tree_selected_index = i;
                break;
              }
            }
            if (game.skill_tree_drag_index < 0) {
              int custom_count = skill_tree_custom_count();
              for (int i = 0; i < custom_count; i++) {
                SDL_Rect r = game.skill_tree_custom_rects[i];
                if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
                  game.skill_tree_drag_custom_index = i;
                  game.skill_tree_drag_off_x = (float)mx - (r.x + r.w * 0.5f);
                  game.skill_tree_drag_off_y = (float)my - (r.y + r.h * 0.5f);
                  game.skill_tree_selected_kind = 2;
                  game.skill_tree_selected_index = i;
                  break;
                }
              }
            }
            if (game.skill_tree_connect_kind != 0 && game.skill_tree_selected_kind != 0) {
              int same = (game.skill_tree_selected_kind == game.skill_tree_connect_kind &&
                          game.skill_tree_selected_index == game.skill_tree_connect_index);
              if (!same) {
                if (game.skill_tree_selected_kind == 1) {
                  skill_tree_override_set_parent(game.skill_tree_selected_index,
                                                 game.skill_tree_connect_kind,
                                                 game.skill_tree_connect_index);
                } else if (game.skill_tree_selected_kind == 2) {
                  skill_tree_custom_set_parent(game.skill_tree_selected_index,
                                               game.skill_tree_connect_kind,
                                               game.skill_tree_connect_index);
                }
              }
              game.skill_tree_connect_kind = 0;
              game.skill_tree_connect_index = -1;
            }
          }
          if (game.skill_tree_edit_mode && e.button.button == SDL_BUTTON_RIGHT) {
            int hit_existing = 0;
            for (int i = 0; i < MAX_SKILL_TREE_UPGRADES; i++) {
              SDL_Rect r = game.skill_tree_item_rects[i];
              if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
                hit_existing = 1;
                break;
              }
            }
            if (!hit_existing) {
              int custom_count = skill_tree_custom_count();
              for (int i = 0; i < custom_count; i++) {
                SDL_Rect r = game.skill_tree_custom_rects[i];
                if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
                  hit_existing = 1;
                  break;
                }
              }
            }
            if (!hit_existing) {
              float zoom = (game.skill_tree_zoom > 0.1f) ? game.skill_tree_zoom : 1.0f;
              float panel_x = 0.0f;
              float panel_y = 0.0f;
              float panel_w = (float)game.window_w;
              float panel_h = (float)game.window_h;
              float center_x = panel_x + panel_w * 0.5f;
              float center_y = panel_y + panel_h * 0.5f;
              float base_x = center_x + ((float)mx - center_x) / zoom;
              float base_y = center_y + ((float)my - center_y) / zoom;
              float nx = (base_x - panel_x) / panel_w;
              float ny = (base_y - panel_y) / panel_h;
              int new_idx = skill_tree_custom_add(nx, ny); 
              if (new_idx >= 0) { 
                game.skill_tree.custom_upgrades[new_idx] = 0; 
                game.skill_tree_selected_kind = 2; 
                game.skill_tree_selected_index = new_idx; 
              } 
            }
          }
          SDL_Rect close_btn = game.skill_tree_close_button;
          SDL_Rect reset_btn = game.skill_tree_reset_button;
          if (mx >= close_btn.x && mx <= close_btn.x + close_btn.w && my >= close_btn.y && my <= close_btn.y + close_btn.h) {
            game.show_skill_tree = 0;
            game.skill_tree_pan_x = 0.0f;
            game.skill_tree_pan_y = 0.0f;
          } else if (mx >= reset_btn.x && mx <= reset_btn.x + reset_btn.w && my >= reset_btn.y && my <= reset_btn.y + reset_btn.h) {
            skill_tree_reset_upgrades(&game);
          } else if (!game.skill_tree_edit_mode && e.button.button == SDL_BUTTON_LEFT) {
            int clicked_node = 0;
            for (int i = 0; i < MAX_SKILL_TREE_UPGRADES; i++) {
              SDL_Rect r = game.skill_tree_item_rects[i];
              if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
                clicked_node = 1;
                skill_tree_try_purchase_upgrade(&game, i);
                break;
              }
            }
            if (!clicked_node) { 
              int custom_count = skill_tree_custom_count(); 
              for (int i = 0; i < custom_count; i++) { 
                SDL_Rect r = game.skill_tree_custom_rects[i]; 
                if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) { 
                  clicked_node = 1; 
                  skill_tree_try_purchase_custom(&game, i); 
                  break; 
                } 
              } 
            } 
            if (!clicked_node) {
              game.skill_tree_pan_drag = 1;
              game.skill_tree_pan_start_x = (float)mx;
              game.skill_tree_pan_start_y = (float)my;
              game.skill_tree_pan_base_x = game.skill_tree_pan_x;
              game.skill_tree_pan_base_y = game.skill_tree_pan_y;
            }
          } else {
            for (int i = 0; i < MAX_SKILL_TREE_UPGRADES; i++) {
              SDL_Rect r = game.skill_tree_item_rects[i];
              if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
                if (!game.skill_tree_edit_mode) {
                  skill_tree_try_purchase_upgrade(&game, i);
                }
                break;
              }
            }
          }
        } else {
          SDL_Rect tree_btn = game.skill_tree_button;
          SDL_Rect dbg_btn = game.skill_tree_debug_button;
          if (mx >= dbg_btn.x && mx <= dbg_btn.x + dbg_btn.w && my >= dbg_btn.y && my <= dbg_btn.y + dbg_btn.h) {
            game.skill_tree.points += 10;
            game.skill_tree.total_points += 10;
            skill_tree_progress_save(&game);
          } else if (mx >= tree_btn.x && mx <= tree_btn.x + tree_btn.w && my >= tree_btn.y && my <= tree_btn.y + tree_btn.h) {
            game.show_skill_tree = 1;
            game.skill_tree_pan_x = 0.0f;
            game.skill_tree_pan_y = 0.0f;
          } else {
            int shown = game.choice_count;
            for (int i = 0; i < shown; i++) {
              SDL_Rect r = game.choices[i].rect;
              if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
                CharacterDef *c = &game.db.characters[game.choices[i].index];
                game.selected_character = game.choices[i].index;
                stats_add(&game.player.base, &c->stats);
                skill_tree_apply_run_mods(&game);
                int widx = find_weapon(&game.db, c->weapon);
                if (widx >= 0) equip_weapon(&game.player, widx);
                wave_start(&game);
              }
            }
          }
        }
      }
      if (e.type == SDL_MOUSEWHEEL && game.mode == MODE_START) {
        if (game.show_skill_tree) {
          float zoom = (game.skill_tree_zoom > 0.1f) ? game.skill_tree_zoom : 1.0f;
          zoom += (float)e.wheel.y * 0.1f;
          zoom = clampf(zoom, 0.5f, 2.5f);
          game.skill_tree_zoom = zoom;
          continue;
        }
        float max_scroll = start_scroll_max(&game);
        if (max_scroll > 0.0f) {
          game.start_scroll -= (float)e.wheel.y * 40.0f;
          game.start_scroll = clampf(game.start_scroll, 0.0f, max_scroll);
        }
      }
      if (e.type == SDL_KEYDOWN && game.mode == MODE_START && game.show_skill_tree && game.skill_tree_edit_mode) { 
        if (e.key.keysym.sym == SDLK_n || e.key.keysym.sym == SDLK_d) { 
          if (game.skill_tree_selected_kind != 0 && game.skill_tree_selected_index >= 0) { 
            game.skill_tree_text_active = 1; 
            game.skill_tree_text_kind = game.skill_tree_selected_kind; 
            game.skill_tree_text_field = (e.key.keysym.sym == SDLK_n) ? 1 : 2; 
            game.skill_tree_text_buf[0] = '\0';
            if (game.skill_tree_text_kind == 1) {
              const char *src = (game.skill_tree_text_field == 1)
                                    ? skill_tree_ui_name(game.skill_tree_selected_index)
                                    : skill_tree_ui_desc(game.skill_tree_selected_index);
              snprintf(game.skill_tree_text_buf, sizeof(game.skill_tree_text_buf), "%s", src);
            } else {
              const char *src = (game.skill_tree_text_field == 1)
                                    ? skill_tree_custom_name(game.skill_tree_selected_index)
                                    : skill_tree_custom_desc(game.skill_tree_selected_index);
              snprintf(game.skill_tree_text_buf, sizeof(game.skill_tree_text_buf), "%s", src);
            }
            SDL_StartTextInput();
          }
        }
        if (e.key.keysym.sym == SDLK_c) {
          if (game.skill_tree_selected_kind != 0 && game.skill_tree_selected_index >= 0) {
            if (game.skill_tree_connect_kind == 0) {
              game.skill_tree_connect_kind = game.skill_tree_selected_kind;
              game.skill_tree_connect_index = game.skill_tree_selected_index;
            } else {
              game.skill_tree_connect_kind = 0;
              game.skill_tree_connect_index = -1;
            }
          }
        }
        if (e.key.keysym.sym == SDLK_LEFTBRACKET) {
          if (game.skill_tree_selected_kind == 1) {
            int cur = skill_tree_upgrade_max_rank(game.skill_tree_selected_index);
            skill_tree_override_set_max_rank(game.skill_tree_selected_index, cur - 1);
          } else if (game.skill_tree_selected_kind == 2) {
            int cur = skill_tree_custom_max_rank(game.skill_tree_selected_index);
            skill_tree_custom_set_max_rank(game.skill_tree_selected_index, cur - 1);
          }
        }
        if (e.key.keysym.sym == SDLK_RIGHTBRACKET) {
          if (game.skill_tree_selected_kind == 1) {
            int cur = skill_tree_upgrade_max_rank(game.skill_tree_selected_index);
            skill_tree_override_set_max_rank(game.skill_tree_selected_index, cur + 1);
          } else if (game.skill_tree_selected_kind == 2) {
            int cur = skill_tree_custom_max_rank(game.skill_tree_selected_index);
            skill_tree_custom_set_max_rank(game.skill_tree_selected_index, cur + 1);
          }
        }
        if (e.key.keysym.sym == SDLK_s) { 
          skill_tree_layout_save(); 
        } 
        if (e.key.keysym.sym == SDLK_r) { 
          skill_tree_layout_clear(); 
          memset(game.skill_tree.custom_upgrades, 0, sizeof(game.skill_tree.custom_upgrades)); 
        } 
        if (e.key.keysym.sym == SDLK_x) { 
          if (game.skill_tree_selected_kind == 1) { 
            skill_tree_override_clear_parent(game.skill_tree_selected_index); 
            skill_tree_layout_save(); 
          } else if (game.skill_tree_selected_kind == 2) { 
            skill_tree_custom_set_parent(game.skill_tree_selected_index, 0, -1); 
            skill_tree_layout_save(); 
          } 
        } 
        if (e.key.keysym.sym == SDLK_DELETE || e.key.keysym.sym == SDLK_BACKSPACE) { 
          if (game.skill_tree_selected_kind == 2) { 
            int del = game.skill_tree_selected_index; 
            skill_tree_custom_remove(del); 
            for (int i = del; i < MAX_SKILL_TREE_CUSTOM_NODES - 1; i++) { 
              game.skill_tree.custom_upgrades[i] = game.skill_tree.custom_upgrades[i + 1]; 
            } 
            game.skill_tree.custom_upgrades[MAX_SKILL_TREE_CUSTOM_NODES - 1] = 0; 
            game.skill_tree_selected_kind = 0; 
            game.skill_tree_selected_index = -1; 
            game.skill_tree_drag_custom_index = -1; 
            game.skill_tree_connect_kind = 0; 
            game.skill_tree_connect_index = -1; 
            skill_tree_layout_save(); 
          } else if (game.skill_tree_selected_kind == 1) { 
            skill_tree_override_clear_parent(game.skill_tree_selected_index); 
            skill_tree_layout_save(); 
          } 
        } 
      } 
      if (e.type == SDL_MOUSEBUTTONDOWN &&
          (game.mode == MODE_LEVELUP || (game.mode == MODE_PAUSE && game.pause_return_mode == MODE_LEVELUP))) {
        handle_levelup_click(&game, e.button.x, e.button.y);
      }
      if (e.type == SDL_MOUSEBUTTONUP && game.mode == MODE_START && game.show_skill_tree && !game.skill_tree_edit_mode) {
        if (e.button.button == SDL_BUTTON_LEFT) {
          game.skill_tree_pan_drag = 0;
        }
      }
      if (e.type == SDL_MOUSEBUTTONDOWN && game.mode == MODE_GAMEOVER) {
        int mx = e.button.x;
        int my = e.button.y;
        SDL_Rect r = game.restart_button;
        if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
          game_reset(&game);
        }
      }
      if (e.type == SDL_MOUSEBUTTONDOWN && game.mode == MODE_PAUSE) {
        int mx = e.button.x;
        int my = e.button.y;
        SDL_Rect r = game.pause_end_run_button;
        if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
          game_reset(&game);
        }
      }
    }

    const double dt = 1.0 / 60.0;
    while (accumulator >= dt) {
      if (game.mode == MODE_WAVE) update_game(&game, (float)(dt * game.time_scale));
      if (game.mode == MODE_BOSS_EVENT) update_boss_event(&game, (float)(dt * game.time_scale));
      if (game.mode == MODE_LEVELUP && (game.levelup_chosen >= 0 || game.levelup_selected_count > 0) && game.levelup_fade > 0.0f) {
        float now_s = (float)SDL_GetTicks() / 1000.0f;
        if (now_s - game.levelup_fade >= 0.5f) {
          game.mode = MODE_WAVE;
          game.levelup_chosen = -1;
          game.levelup_selected_count = 0;
        }
      }
      accumulator -= dt;
    }

    render_game(&game);
    frame_log++;
    if ((frame_log % 600) == 0) {
      log_linef("Frame %d", frame_log);
    }
  }
  log_line("Main loop exit");

  if (game.tex_ground) SDL_DestroyTexture(game.tex_ground);
  if (game.tex_wall) SDL_DestroyTexture(game.tex_wall);
  if (game.tex_health_flask) SDL_DestroyTexture(game.tex_health_flask);
  if (game.tex_enemy) SDL_DestroyTexture(game.tex_enemy);
  if (game.tex_enemy_eye) SDL_DestroyTexture(game.tex_enemy_eye);
  if (game.tex_enemy_ghost) SDL_DestroyTexture(game.tex_enemy_ghost);
  if (game.tex_boss) SDL_DestroyTexture(game.tex_boss);
  if (game.tex_player_front) SDL_DestroyTexture(game.tex_player_front);
  if (game.tex_player_back) SDL_DestroyTexture(game.tex_player_back);
  if (game.tex_player_right) SDL_DestroyTexture(game.tex_player_right);
  if (game.tex_player_left) SDL_DestroyTexture(game.tex_player_left);
  for (int i = 0; i < game.db.character_count && i < MAX_CHARACTERS; i++) {
    if (game.tex_character_walk[i]) SDL_DestroyTexture(game.tex_character_walk[i]);
  }
  if (game.tex_enemy_bolt) SDL_DestroyTexture(game.tex_enemy_bolt);
  if (game.tex_laser_beam) SDL_DestroyTexture(game.tex_laser_beam);
  if (game.tex_lightning_zone) SDL_DestroyTexture(game.tex_lightning_zone);
  if (game.tex_chest) SDL_DestroyTexture(game.tex_chest);
  if (game.tex_totem_freeze) SDL_DestroyTexture(game.tex_totem_freeze);
  if (game.tex_totem_curse) SDL_DestroyTexture(game.tex_totem_curse);
  if (game.tex_totem_damage) SDL_DestroyTexture(game.tex_totem_damage);
  if (game.tex_enemy_charger) SDL_DestroyTexture(game.tex_enemy_charger);
  if (game.tex_scythe) SDL_DestroyTexture(game.tex_scythe);
  if (game.tex_bite) SDL_DestroyTexture(game.tex_bite);
  if (game.tex_dagger) SDL_DestroyTexture(game.tex_dagger);
  if (game.tex_alchemist_puddle) SDL_DestroyTexture(game.tex_alchemist_puddle);
  if (game.tex_fire_trail) SDL_DestroyTexture(game.tex_fire_trail);
  if (game.tex_alchemist_ult) SDL_DestroyTexture(game.tex_alchemist_ult);
  if (game.tex_exp_orb) SDL_DestroyTexture(game.tex_exp_orb);
  if (game.tex_orb_common) SDL_DestroyTexture(game.tex_orb_common);
  if (game.tex_orb_uncommon) SDL_DestroyTexture(game.tex_orb_uncommon);
  if (game.tex_orb_rare) SDL_DestroyTexture(game.tex_orb_rare);
  if (game.tex_orb_epic) SDL_DestroyTexture(game.tex_orb_epic);
  if (game.tex_orb_legendary) SDL_DestroyTexture(game.tex_orb_legendary);
  for (int i = 0; i < MAX_CHARACTERS; i++) {
    if (game.tex_portraits[i]) SDL_DestroyTexture(game.tex_portraits[i]);
  }
  if (game.cursor) SDL_FreeCursor(game.cursor);
  if (game.font) TTF_CloseFont(game.font);
  if (game.font_title && game.font_title != game.font) TTF_CloseFont(game.font_title);
  if (game.font_title_big && game.font_title_big != game.font && game.font_title_big != game.font_title) TTF_CloseFont(game.font_title_big);
  if (game.renderer) SDL_DestroyRenderer(game.renderer);
  if (game.window) SDL_DestroyWindow(game.window);
  IMG_Quit();
  TTF_Quit();
  SDL_Quit();

  if (g_log) fclose(g_log);
  g_log = NULL;
  if (g_combat_log) fclose(g_combat_log);
  g_combat_log = NULL;

  return 0;
}
