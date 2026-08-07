// KronoUniverse — Main entry point
// Fase 1: Personagem e Movimento
//
// Demo jogável: personagem que anda, corre, pula, com física real.
// Controles:
//   A/D ou ←/→ = mover
//   W/↑/Space = pular
//   Shift = correr
//   S/↓ = agachar
//
// Plataformas estão no chão para teste de colisão e salto.
// Diferentes superfícies (gelo, lama, pedra) têm atrito diferente.

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "engine/character_components.hpp"
#include "engine/gameloop.hpp"
#include "engine/input_system.hpp"
#include "physics/physics.hpp"
#include "physics/movement_system.hpp"
#include "render/renderer.hpp"
#include <iostream>
#include <cmath>
#include <thread>

using namespace krono;

// Colors for deep-tech theme
const float TEAL[4] = {0.13f, 0.83f, 0.93f, 1.0f};
const float AMBER[4] = {0.98f, 0.75f, 0.14f, 1.0f};
const float FG[4] = {0.96f, 0.96f, 0.98f, 1.0f};
const float ICE_COL[4] = {0.7f, 0.85f, 0.95f, 0.6f};
const float MUD_COL[4] = {0.35f, 0.25f, 0.15f, 0.8f};
const float STONE_COL[4] = {0.4f, 0.4f, 0.45f, 0.8f};
const float PLAYER_COLOR[4] = {0.13f, 0.83f, 0.93f, 1.0f};

struct Platform {
    Entity e;
    float x, y, w, h;
    SurfaceType type;
    float color[4];
    const char* name;
};

int main() {
    std::cout << "=== KronoUniverse — Fase 1 (Personagem e Movimento) ===" << std::endl;

    const int WINDOW_W = 1280;
    const int WINDOW_H = 720;

    Renderer renderer;
    if (!renderer.init(WINDOW_W, WINDOW_H, "KronoUniverse — Fase 1")) {
        std::cerr << "Failed to init renderer" << std::endl;
        return 1;
    }
    renderer.set_ortho(WINDOW_W, WINDOW_H);

    Registry reg;
    PhysicsSystem physics;
    physics.gravity_y = 9.81f * 50.0f;
    physics.linear_damping = 0.01f;

    MovementSystem movement;
    InputSystem input;

    float game_gravity = 9.81f * 50.0f;

    // ---- Create player ----
    Entity player = reg.create();
    reg.emplace<Position>(player, Position{(float)(WINDOW_W / 2), 100.0f});
    reg.emplace<Velocity>(player, Velocity{0, 0});
    reg.emplace<Mass>(player, Mass{70.0f});
    reg.emplace<RigidBody>(player, RigidBody{0.0f, 0.8f, false, false});
    reg.emplace<AABBCollider>(player, AABBCollider{32, 48});
    reg.emplace<CharacterController>(player, CharacterController{});
    reg.emplace<Health>(player, Health{100, 100});
    reg.emplace<AnimationState>(player, AnimationState{});
    reg.emplace<FallDamageTracker>(player, FallDamageTracker{});
    reg.emplace<TagPlayer>(player, TagPlayer{});

    Species human_species;
    human_species.name = "Human";
    human_species.base_mass = 70.0f;
    human_species.fall_damage_multiplier = 1.0f;
    reg.emplace<Species>(player, std::move(human_species));

    InventoryWeight inv;
    inv.current_weight = 0.0f;
    inv.max_weight = 100.0f;
    reg.emplace<InventoryWeight>(player, std::move(inv));

    std::cout << "Player created at (" << WINDOW_W/2 << ", 100)" << std::endl;
    std::cout << "Controls: A/D=move, W/Space=jump, Shift=run, S=crouch" << std::endl;

    // ---- Create ground platforms with different surfaces ----
    std::vector<Platform> platforms;

    auto create_platform = [&](float x, float y, float w, float h, SurfaceType type, const float col[4], const char* name) {
        Entity e = reg.create();
        reg.emplace<Position>(e, Position{x, y});
        reg.emplace<AABBCollider>(e, AABBCollider{w, h});
        reg.emplace<RigidBody>(e, RigidBody{0.3f, 1.0f, true, false});
        reg.emplace<Mass>(e, Mass{1000000});
        Surface s;
        s.type = type;
        s.friction = MovementSystem::surface_friction(type);
        reg.emplace<Surface>(e, std::move(s));
        reg.emplace<TagStructure>(e, TagStructure{});

        Platform p;
        p.e = e;
        p.x = x; p.y = y; p.w = w; p.h = h;
        p.type = type;
        p.color[0] = col[0]; p.color[1] = col[1]; p.color[2] = col[2]; p.color[3] = col[3];
        p.name = name;
        platforms.push_back(std::move(p));
    };

    create_platform(0, WINDOW_H - 60, WINDOW_W, 60, SurfaceType::STONE, STONE_COL, "Stone");
    create_platform(200, WINDOW_H - 60, 300, 60, SurfaceType::ICE, ICE_COL, "Ice");
    create_platform(700, WINDOW_H - 60, 200, 60, SurfaceType::MUD, MUD_COL, "Mud");
    create_platform(300, 450, 200, 30, SurfaceType::METAL, TEAL, "Metal Platform");
    create_platform(700, 300, 200, 30, SurfaceType::WOOD, AMBER, "Wood Platform");

    std::cout << "Created " << platforms.size() << " platforms" << std::endl;

    // ---- Game loop ----
    bool show_debug = true;
    int frame_count = 0;

    GameLoop loop(
        [&](double dt) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) {
                    SDL_Event quit_ev;
                    quit_ev.type = SDL_QUIT;
                    SDL_PushEvent(&quit_ev);
                }
                if (e.type == SDL_KEYDOWN) {
                    if (e.key.keysym.sym == SDLK_ESCAPE) {
                        SDL_Event quit_ev;
                        quit_ev.type = SDL_QUIT;
                        SDL_PushEvent(&quit_ev);
                    }
                    if (e.key.keysym.sym == SDLK_F1) {
                        show_debug = !show_debug;
                    }
                }
                input.process_event(e, reg);
            }

            input.update(reg);
            movement.update(reg, (float)dt, game_gravity);
            physics.update(reg, (float)dt);

            auto* ppos = reg.get<Position>(player);
            auto* pvel = reg.get<Velocity>(player);
            auto* ctrl = reg.get<CharacterController>(player);

            ctrl->grounded = false;

            for (auto& plat : platforms) {
                auto* ppos_plat = reg.get<Position>(plat.e);
                auto* pcol_plat = reg.get<AABBCollider>(plat.e);
                if (!ppos_plat || !pcol_plat) continue;

                float px = ppos->x;
                float py = ppos->y;
                float pw = 32, ph = 48;

                float cx = ppos_plat->x;
                float cy = ppos_plat->y;
                float cw = pcol_plat->width;
                float ch = pcol_plat->height;

                float feet_y = py + ph;
                bool x_overlap = (px + pw > cx) && (px < cx + cw);
                bool on_top = (feet_y >= cy - 5) && (feet_y <= cy + 10) && (pvel->y >= 0);

                if (x_overlap && on_top) {
                    ppos->y = cy - ph;
                    pvel->y = 0;
                    ctrl->grounded = true;

                    auto* surf = reg.get<Surface>(plat.e);
                    auto* player_surf = reg.get<Surface>(player);
                    if (surf) {
                        if (player_surf) {
                            player_surf->type = surf->type;
                            player_surf->friction = surf->friction;
                        } else {
                            Surface copy = *surf;
                            reg.emplace<Surface>(player, std::move(copy));
                        }
                    }
                    break;
                }
            }

            if (!ctrl->grounded) {
                auto* player_surf = reg.get<Surface>(player);
                if (player_surf) {
                    player_surf->type = SurfaceType::AIR;
                    player_surf->friction = 0.01f;
                }
            }

            if (ppos->x < 0) ppos->x = 0;
            if (ppos->x > WINDOW_W - 32) ppos->x = WINDOW_W - 32;
            if (ppos->y > WINDOW_H + 100) {
                ppos->x = WINDOW_W / 2.0f;
                ppos->y = 100;
                pvel->x = 0;
                pvel->y = 0;
                auto* hp = reg.get<Health>(player);
                if (hp) hp->current = hp->max;
            }

            input.end_frame(reg);
            frame_count++;
        },
        [&](double alpha) {
            renderer.clear();
            renderer.set_ortho(WINDOW_W, WINDOW_H);

            for (auto& plat : platforms) {
                auto* pos = reg.get<Position>(plat.e);
                renderer.draw_rect(pos->x, pos->y, plat.w, plat.h,
                    plat.color[0], plat.color[1], plat.color[2], plat.color[3]);
            }

            auto* ppos = reg.get<Position>(player);
            auto* pvel = reg.get<Velocity>(player);
            auto* ctrl = reg.get<CharacterController>(player);

            float rx = ppos->x + pvel->x * (float)alpha;
            float ry = ppos->y + pvel->y * (float)alpha;

            renderer.draw_rect(rx, ry, 32, 48, PLAYER_COLOR[0], PLAYER_COLOR[1], PLAYER_COLOR[2]);

            float dot_x = ctrl->facing_right ? rx + 24 : rx + 2;
            renderer.draw_rect(dot_x, ry + 8, 6, 6, AMBER[0], AMBER[1], AMBER[2]);

            if (show_debug) {
                renderer.draw_rect(rx + 16, ry + 24, pvel->x * 0.05f, 2,
                    1.0f, 0.75f, 0.14f, 0.5f);
                renderer.draw_rect(rx + 16, ry + 24, 2, pvel->y * 0.05f,
                    0.13f, 0.83f, 0.93f, 0.5f);
            }

            renderer.present();
        }
    );

    std::thread game_thread([&]() { loop.run(); });

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;
            input.process_event(e, reg);
        }
        SDL_Delay(16);
    }
    loop.stop();
    game_thread.join();

    std::cout << "Game ended after " << frame_count << " frames" << std::endl;
    std::cout << "=== Fase 1 demo complete ===" << std::endl;
    return 0;
}
