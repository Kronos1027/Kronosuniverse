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

using namespace krono;

// Colors for deep-tech theme
const float TEAL[4] = {0.13f, 0.83f, 0.93f, 1.0f};
const float AMBER[4] = {0.98f, 0.75f, 0.14f, 1.0f};
const float FG[4] = {0.96f, 0.96f, 0.98f, 1.0f};
const float ICE[4] = {0.7f, 0.85f, 0.95f, 0.6f};
const float MUD[4] = {0.35f, 0.25f, 0.15f, 0.8f};
const float STONE[4] = {0.4f, 0.4f, 0.45f, 0.8f};
const float PLAYER_COLOR[4] = {0.13f, 0.83f, 0.93f, 1.0f};

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
    physics.gravity_y = 9.81f * 50.0f; // game-scale gravity (pixels/s²)
    physics.linear_damping = 0.01f;

    MovementSystem movement;
    InputSystem input;

    float game_gravity = 9.81f * 50.0f;

    // ---- Create player ----
    Entity player = reg.create();
    reg.emplace<Position>(player, Position{WINDOW_W / 2.0f, 100});
    reg.emplace<Velocity>(player, Velocity{0, 0});
    reg.emplace<Mass>(player, Mass{70.0f});
    reg.emplace<RigidBody>(player, RigidBody{0.0f, 0.8f, false, false});
    reg.emplace<AABBCollider>(player, AABBCollider{32, 48});
    reg.emplace<CharacterController>(player, CharacterController{});
    reg.emplace<Health>(player, Health{100, 100});
    reg.emplace<AnimationState>(player, AnimationState{});
    reg.emplace<FallDamageTracker>(player, FallDamageTracker{});
    reg.emplace<TagPlayer>(player, TagPlayer{});

    // Player with Species
    Species human_species;
    human_species.name = "Human";
    human_species.base_mass = 70.0f;
    human_species.fall_damage_multiplier = 1.0f;
    reg.emplace<Species>(player, human_species);

    // Player with empty inventory
    InventoryWeight inv;
    inv.current_weight = 0.0f;
    inv.max_weight = 100.0f;
    reg.emplace<InventoryWeight>(player, inv);

    std::cout << "Player created at (" << WINDOW_W/2 << ", 100)" << std::endl;
    std::cout << "Controls: A/D=move, W/Space=jump, Shift=run, S=crouch" << std::endl;

    // ---- Create ground platforms with different surfaces ----
    struct Platform { Entity e; float x, y, w, h; SurfaceType type; const float color[4]; const char* name; };

    std::vector<Platform> platforms;

    auto create_platform = [&](float x, float y, float w, float h, SurfaceType type, const float color[4], const char* name) {
        Entity e = reg.create();
        reg.emplace<Position>(e, Position{x, y});
        reg.emplace<AABBCollider>(e, AABBCollider{w, h});
        reg.emplace<RigidBody>(e, RigidBody{0.3f, 1.0f, true, false});
        reg.emplace<Mass>(e, Mass{1000000});
        Surface s;
        s.type = type;
        s.friction = MovementSystem::surface_friction(type);
        reg.emplace<Surface>(e, s);
        reg.emplace<TagStructure>(e, TagStructure{});
        platforms.push_back({e, x, y, w, h, type, color, name});
    };

    // Main ground (stone)
    create_platform(0, WINDOW_H - 60, WINDOW_W, 60, SurfaceType::STONE, STONE, "Stone");

    // Ice patch
    create_platform(200, WINDOW_H - 60, 300, 60, SurfaceType::ICE, ICE, "Ice");

    // Mud patch
    create_platform(700, WINDOW_H - 60, 200, 60, SurfaceType::MUD, MUD, "Mud");

    // Floating platform
    create_platform(300, 450, 200, 30, SurfaceType::METAL, TEAL, "Metal Platform");

    // Higher platform
    create_platform(700, 300, 200, 30, SurfaceType::WOOD, AMBER, "Wood Platform");

    std::cout << "Created " << platforms.size() << " platforms" << std::endl;

    // ---- Game loop ----
    bool show_debug = true;
    int frame_count = 0;

    GameLoop loop(
        [&](double dt) {
            // Process SDL events (for jump edge detection)
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) { /* will quit */ }
                if (e.type == SDL_KEYDOWN) {
                    if (e.key.keysym.sym == SDLK_ESCAPE) {
                        // Quit
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

            // Check for quit via renderer
            // (renderer.poll_events() is not called — we handle events above)

            // Update input
            input.update(reg);

            // Update movement
            movement.update(reg, (float)dt, game_gravity);

            // Update physics
            physics.update(reg, (float)dt);

            // Check grounded status (simplified — check if player is on a platform)
            auto* ppos = reg.get<Position>(player);
            auto* pvel = reg.get<Velocity>(player);
            auto* ctrl = reg.get<CharacterController>(player);

            // Simple grounded check: if player is near ground level and falling
            bool was_grounded = ctrl->grounded;
            ctrl->grounded = false;

            // Check each platform
            for (auto& plat : platforms) {
                auto* ppos_plat = reg.get<Position>(plat.e);
                auto* pcol_plat = reg.get<AABBCollider>(plat.e);
                if (!ppos_plat || !pcol_plat) continue;

                // Player AABB
                float px = ppos->x;
                float py = ppos->y;
                float pw = 32, ph = 48;

                // Platform AABB
                float cx = ppos_plat->x;
                float cy = ppos_plat->y;
                float cw = pcol_plat->width;
                float ch = pcol_plat->height;

                // Check if player's feet are on platform top
                float feet_y = py + ph;
                bool x_overlap = (px + pw > cx) && (px < cx + cw);
                bool on_top = (feet_y >= cy - 5) && (feet_y <= cy + 10) && (pvel->y >= 0);

                if (x_overlap && on_top) {
                    // Snap to platform
                    ppos->y = cy - ph;
                    pvel->y = 0;
                    ctrl->grounded = true;

                    // Copy surface from platform
                    auto* surf = reg.get<Surface>(plat.e);
                    auto* player_surf = reg.get<Surface>(player);
                    if (surf) {
                        if (player_surf) {
                            player_surf->type = surf->type;
                            player_surf->friction = surf->friction;
                        } else {
                            reg.emplace<Surface>(player, *surf);
                        }
                    }
                    break;
                }
            }

            // If not grounded and has Surface, set to AIR
            if (!ctrl->grounded) {
                auto* player_surf = reg.get<Surface>(player);
                if (player_surf) {
                    player_surf->type = SurfaceType::AIR;
                    player_surf->friction = 0.01f;
                }
            }

            // Prevent player from going off-screen horizontally
            if (ppos->x < 0) ppos->x = 0;
            if (ppos->x > WINDOW_W - 32) ppos->x = WINDOW_W - 32;
            // Respawn if fell off
            if (ppos->y > WINDOW_H + 100) {
                ppos->x = WINDOW_W / 2.0f;
                ppos->y = 100;
                pvel->x = 0;
                pvel->y = 0;
                auto* hp = reg.get<Health>(player);
                if (hp) hp->current = hp->max; // heal on respawn
            }

            // End frame (clear edge-triggered inputs)
            input.end_frame(reg);

            frame_count++;
        },
        [&](double alpha) {
            // Render
            renderer.clear();
            renderer.set_ortho(WINDOW_W, WINDOW_H);

            // Draw platforms
            for (auto& plat : platforms) {
                auto* pos = reg.get<Position>(plat.e);
                renderer.draw_rect(pos->x, pos->y, plat.w, plat.h,
                    plat.color[0], plat.color[1], plat.color[2], plat.color[3]);
            }

            // Draw player
            auto* ppos = reg.get<Position>(player);
            auto* pvel = reg.get<Velocity>(player);
            auto* ctrl = reg.get<CharacterController>(player);
            auto* anim = reg.get<AnimationState>(player);
            auto* hp = reg.get<Health>(player);

            // Interpolated position
            float rx = ppos->x + pvel->x * (float)alpha;
            float ry = ppos->y + pvel->y * (float)alpha;

            // Player body (teal)
            renderer.draw_rect(rx, ry, 32, 48, PLAYER_COLOR[0], PLAYER_COLOR[1], PLAYER_COLOR[2]);

            // Direction indicator (amber dot on facing side)
            float dot_x = ctrl->facing_right ? rx + 24 : rx + 2;
            renderer.draw_rect(dot_x, ry + 8, 6, 6, AMBER[0], AMBER[1], AMBER[2]);

            // Debug info
            if (show_debug) {
                // Velocity vector
                renderer.draw_rect(rx + 16, ry + 24, pvel->x * 0.05f, 2,
                    1.0f, 0.75f, 0.14f, 0.5f);
                renderer.draw_rect(rx + 16, ry + 24, 2, pvel->y * 0.05f,
                    0.13f, 0.83f, 0.93f, 0.5f);
            }

            renderer.present();
        }
    );

    // Run
    std::thread game_thread([&]() { loop.run(); });

    // Wait for quit
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
