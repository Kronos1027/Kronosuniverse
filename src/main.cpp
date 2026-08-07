// KronoUniverse — Main entry point (v0.1 playable)
// Primeira versão jogável: personagem explorando um planeta procedural.
//
// Controles:
//   A/D ou ←/→ = mover
//   W/↑/Space = pular
//   Shift = correr
//   S/↓ = agachar
//   Mouse wheel = zoom in/out
//   F1 = debug overlay
//   F2 = trocar bloco (destruir com botão esquerdo, colocar com direito)
//   ESC = sair
//
// O mundo é gerado proceduralmente com biomas, cavernas, minérios e árvores.

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "engine/character_components.hpp"
#include "engine/gameloop.hpp"
#include "engine/input_system.hpp"
#include "physics/physics.hpp"
#include "physics/movement_system.hpp"
#include "procedural/world.hpp"
#include "render/renderer.hpp"
#include <iostream>
#include <cmath>
#include <thread>
#include <algorithm>

using namespace krono;

// ---- Config ----
static constexpr int WINDOW_W = 1280;
static constexpr int WINDOW_H = 720;
// BLOCK_SIZE is defined in world.hpp as 16
static constexpr float GAME_GRAVITY = 9.81f * 50.0f;

// ---- Camera ----
struct Camera {
    float x = 0, y = 0;
    float zoom = 1.0f;
    int screen_w = WINDOW_W;
    int screen_h = WINDOW_H;

    void follow(float target_x, float target_y, float dt) {
        // Smooth follow
        float target_cx = target_x - (screen_w / zoom) / 2.0f;
        float target_cy = target_y - (screen_h / zoom) / 2.0f;
        x += (target_cx - x) * 5.0f * dt;
        y += (target_cy - y) * 5.0f * dt;
    }

    // World to screen coordinates
    float world_to_screen_x(float wx) const { return (wx - x) * zoom; }
    float world_to_screen_y(float wy) const { return (wy - y) * zoom; }

    // Screen to world coordinates
    float screen_to_world_x(float sx) const { return sx / zoom + x; }
    float screen_to_world_y(float sy) const { return sy / zoom + y; }

    // Visible world bounds
    float left() const { return x; }
    float top() const { return y; }
    float right() const { return x + screen_w / zoom; }
    float bottom() const { return y + screen_h / zoom; }
};

// ---- Block colors (RGB for pixel art) ----
struct BlockColor { float r, g, b; };

BlockColor get_block_color(BlockType type) {
    switch (type) {
        case BlockType::AIR:      return {0.04f, 0.043f, 0.059f}; // bg
        case BlockType::DIRT:     return {0.47f, 0.31f, 0.20f};
        case BlockType::GRASS:    return {0.39f, 0.63f, 0.24f};
        case BlockType::STONE:    return {0.51f, 0.51f, 0.55f};
        case BlockType::SAND:     return {0.78f, 0.71f, 0.39f};
        case BlockType::WOOD:     return {0.55f, 0.39f, 0.24f};
        case BlockType::LEAVES:   return {0.31f, 0.55f, 0.20f};
        case BlockType::METAL:    return {0.71f, 0.71f, 0.78f};
        case BlockType::ICE:      return {0.71f, 0.86f, 0.94f,};
        case BlockType::LAVA:     return {0.86f, 0.31f, 0.08f};
        case BlockType::WATER:    return {0.24f, 0.39f, 0.71f};
        case BlockType::BEDROCK:  return {0.24f, 0.24f, 0.24f};
        case BlockType::CRYSTAL:  return {0.59f, 0.78f, 1.0f};
        case BlockType::ANCIENT:  return {0.39f, 0.31f, 0.55f};
        default:                  return {0.5f, 0.5f, 0.5f};
    }
}

// ---- Player creation ----
static Entity create_player(Registry& reg, float x, float y) {
    Entity player = reg.create();
    reg.emplace<Position>(player, Position{x, y});
    reg.emplace<Velocity>(player, Velocity{0, 0});
    reg.emplace<Mass>(player, Mass{70.0f});
    reg.emplace<RigidBody>(player, RigidBody{0.0f, 0.8f, false, false});
    reg.emplace<AABBCollider>(player, AABBCollider{24, 40});
    reg.emplace<CharacterController>(player, CharacterController{});
    reg.emplace<Health>(player, Health{100, 100});
    reg.emplace<AnimationState>(player, AnimationState{});
    reg.emplace<FallDamageTracker>(player, FallDamageTracker{});
    reg.emplace<TagPlayer>(player, TagPlayer{});

    Species human;
    human.name = "Human";
    human.base_mass = 70.0f;
    reg.emplace<Species>(player, std::move(human));

    InventoryWeight inv;
    inv.max_weight = 100.0f;
    reg.emplace<InventoryWeight>(player, std::move(inv));

    return player;
}

// ---- Collision with world blocks ----
static void resolve_world_collision(Registry& reg, Entity player, World& world, CharacterController& ctrl) {
    auto* pos = reg.get<Position>(player);
    auto* vel = reg.get<Velocity>(player);
    auto* col = reg.get<AABBCollider>(player);
    if (!pos || !vel || !col) return;

    float px = pos->x;
    float py = pos->y;
    float pw = col->width;
    float ph = col->height;

    // Check blocks around player
    int min_bx = (int)(px / BLOCK_SIZE) - 1;
    int max_bx = (int)((px + pw) / BLOCK_SIZE) + 1;
    int min_by = (int)(py / BLOCK_SIZE) - 1;
    int max_by = (int)((py + ph) / BLOCK_SIZE) + 1;

    bool was_grounded = ctrl.grounded;
    ctrl.grounded = false;

    for (int bx = min_bx; bx <= max_bx; bx++) {
        for (int by = min_by; by <= max_by; by++) {
            Block* block = world.get_block(bx, by);
            if (!block || !block->is_solid()) continue;

            float bx_world = bx * BLOCK_SIZE;
            float by_world = by * BLOCK_SIZE;

            // AABB overlap
            float overlap_x = std::min(px + pw, bx_world + BLOCK_SIZE) - std::max(px, bx_world);
            float overlap_y = std::min(py + ph, by_world + BLOCK_SIZE) - std::max(py, by_world);

            if (overlap_x > 0 && overlap_y > 0) {
                // Resolve along smallest overlap
                if (overlap_x < overlap_y) {
                    // Horizontal collision
                    if (px + pw/2 < bx_world + BLOCK_SIZE/2) {
                        pos->x = bx_world - pw;
                    } else {
                        pos->x = bx_world + BLOCK_SIZE;
                    }
                    vel->x = 0;
                } else {
                    // Vertical collision
                    if (py + ph/2 < by_world + BLOCK_SIZE/2) {
                        // Hit head
                        pos->y = by_world - ph;
                        vel->y = 0;
                    } else {
                        // Landed on top
                        pos->y = by_world - ph;
                        vel->y = 0;
                        ctrl.grounded = true;

                        // Set surface
                        auto* surf = reg.get<Surface>(player);
                        const auto& props = get_block_props(block->type);
                        if (surf) {
                            surf->type = block->type == BlockType::AIR ? SurfaceType::AIR : SurfaceType::STONE;
                            surf->friction = props.friction;
                        } else {
                            Surface s;
                            s.type = SurfaceType::STONE;
                            s.friction = props.friction;
                            reg.emplace<Surface>(player, std::move(s));
                        }
                    }
                }
                px = pos->x;
                py = pos->y;
            }
        }
    }

    // If not grounded, set surface to air
    if (!ctrl.grounded) {
        auto* surf = reg.get<Surface>(player);
        if (surf) {
            surf->type = SurfaceType::AIR;
            surf->friction = 0.01f;
        }
    }
}

int main() {
    std::cout << "=== KronoUniverse v0.1 — Playable Demo ===" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  A/D or arrows = move" << std::endl;
    std::cout << "  W/Space = jump" << std::endl;
    std::cout << "  Shift = run" << std::endl;
    std::cout << "  S = crouch" << std::endl;
    std::cout << "  Mouse wheel = zoom" << std::endl;
    std::cout << "  Left click = mine block" << std::endl;
    std::cout << "  Right click = place block" << std::endl;
    std::cout << "  F1 = debug info" << std::endl;
    std::cout << "  ESC = quit" << std::endl;
    std::cout << std::endl;

    Renderer renderer;
    if (!renderer.init(WINDOW_W, WINDOW_H, "KronoUniverse v0.1")) {
        std::cerr << "Failed to init renderer" << std::endl;
        return 1;
    }

    Registry reg;
    PhysicsSystem physics;
    physics.gravity_y = GAME_GRAVITY;
    physics.linear_damping = 0.01f;

    MovementSystem movement;
    InputSystem input;

    // Create world with seed
    uint32_t seed = 42;
    World world(seed);
    std::cout << "World created with seed " << seed << std::endl;

    // Find spawn point (top of terrain at x=0)
    int spawn_x = 0;
    int spawn_y = 0;
    for (int y = 0; y < CHUNK_H; y++) {
        Block* b = world.get_block(spawn_x / BLOCK_SIZE, y);
        if (b && b->is_solid()) {
            spawn_y = (y * BLOCK_SIZE) - 48; // place player above surface
            break;
        }
    }
    std::cout << "Spawn point: (" << spawn_x << ", " << spawn_y << ")" << std::endl;

    // Create player
    Entity player = create_player(reg, spawn_x, spawn_y);
    Camera camera;
    camera.x = spawn_x - WINDOW_W / 2;
    camera.y = spawn_y - WINDOW_H / 2;

    // Mouse state
    int mouse_x = 0, mouse_y = 0;
    bool mouse_left = false;
    bool mouse_right = false;
    bool show_debug = true;
    BlockType selected_block = BlockType::DIRT;

    // Block selection palette
    BlockType block_palette[] = {BlockType::DIRT, BlockType::STONE, BlockType::WOOD,
                                  BlockType::METAL, BlockType::SAND, BlockType::ICE};
    int palette_idx = 0;

    std::cout << "Entering game loop..." << std::endl;

    GameLoop loop(
        [&](double dt) {
            // ---- Input ----
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_QUIT) {
                    SDL_Event q; q.type = SDL_QUIT; SDL_PushEvent(&q);
                }
                if (e.type == SDL_KEYDOWN) {
                    if (e.key.keysym.sym == SDLK_ESCAPE) {
                        SDL_Event q; q.type = SDL_QUIT; SDL_PushEvent(&q);
                    }
                    if (e.key.keysym.sym == SDLK_F1) show_debug = !show_debug;
                    if (e.key.keysym.sym == SDLK_F2) {
                        palette_idx = (palette_idx + 1) % 6;
                        selected_block = block_palette[palette_idx];
                    }
                }
                if (e.type == SDL_MOUSEMOTION) {
                    mouse_x = e.motion.x;
                    mouse_y = e.motion.y;
                }
                if (e.type == SDL_MOUSEBUTTONDOWN) {
                    if (e.button.button == SDL_BUTTON_LEFT) mouse_left = true;
                    if (e.button.button == SDL_BUTTON_RIGHT) mouse_right = true;
                }
                if (e.type == SDL_MOUSEWHEEL) {
                    camera.zoom = std::max(0.5f, std::min(3.0f, camera.zoom + e.wheel.y * 0.1f));
                }
                input.process_event(e, reg);
            }

            input.update(reg);
            movement.update(reg, (float)dt, GAME_GRAVITY);

            // Custom physics (skip default collision, use world collision)
            auto* pos = reg.get<Position>(player);
            auto* vel = reg.get<Velocity>(player);
            auto* mass = reg.get<Mass>(player);
            auto* rb = reg.get<RigidBody>(player);

            if (pos && vel && mass && rb && !rb->is_static) {
                // Apply gravity
                vel->y += GAME_GRAVITY * (float)dt;
                // Apply damping
                vel->x *= (1.0f - 0.01f * (float)dt);
                // Integrate position
                pos->x += vel->x * (float)dt;
                pos->y += vel->y * (float)dt;
            }

            // World collision
            auto* ctrl = reg.get<CharacterController>(player);
            if (ctrl) resolve_world_collision(reg, player, world, *ctrl);

            // Mining / placing blocks
            float world_mx = camera.screen_to_world_x(mouse_x);
            float world_my = camera.screen_to_world_y(mouse_y);
            int target_bx = (int)(world_mx / BLOCK_SIZE);
            int target_by = (int)(world_my / BLOCK_SIZE);

            if (mouse_left) {
                Block* b = world.get_block(target_bx, target_by);
                if (b && b->is_solid() && b->type != BlockType::BEDROCK) {
                    world.destroy_block(target_bx, target_by);
                }
                mouse_left = false;
            }
            if (mouse_right) {
                Block* b = world.get_block(target_bx, target_by);
                if (b && b->is_air()) {
                    // Don't place block inside player
                    auto* ppos = reg.get<Position>(player);
                    auto* pcol = reg.get<AABBCollider>(player);
                    float pbx1 = ppos->x, pby1 = ppos->y;
                    float pbx2 = ppos->x + pcol->width, pby2 = ppos->y + pcol->height;
                    float bbx1 = target_bx * BLOCK_SIZE, bby1 = target_by * BLOCK_SIZE;
                    float bbx2 = bbx1 + BLOCK_SIZE, bby2 = bby1 + BLOCK_SIZE;
                    bool overlap = (pbx1 < bbx2 && pbx2 > bbx1 && pby1 < bby2 && pby2 > bby1);
                    if (!overlap) {
                        world.set_block(target_bx, target_by, selected_block);
                    }
                }
                mouse_right = false;
            }

            // Camera follow
            camera.follow(pos->x, pos->y, (float)dt);

            // Prevent falling infinitely
            if (pos->y > CHUNK_H * BLOCK_SIZE + 1000) {
                pos->x = spawn_x;
                pos->y = spawn_y;
                vel->x = 0;
                vel->y = 0;
            }

            input.end_frame(reg);
        },
        [&](double alpha) {
            renderer.clear();

            // Set camera-based ortho
            camera.screen_w = WINDOW_W;
            camera.screen_h = WINDOW_H;
            float view_w = WINDOW_W / camera.zoom;
            float view_h = WINDOW_H / camera.zoom;

            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(camera.x, camera.x + view_w, camera.y + view_h, camera.y, -1, 1);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();

            // ---- Draw world blocks ----
            int min_bx = (int)(camera.left() / BLOCK_SIZE) - 1;
            int max_bx = (int)(camera.right() / BLOCK_SIZE) + 1;
            int min_by = (int)(camera.top() / BLOCK_SIZE) - 1;
            int max_by = (int)(camera.bottom() / BLOCK_SIZE) + 1;

            // Clamp to world bounds
            min_by = std::max(0, min_by);
            max_by = std::min(CHUNK_H - 1, max_by);

            for (int bx = min_bx; bx <= max_bx; bx++) {
                for (int by = min_by; by <= max_by; by++) {
                    Block* block = world.get_block(bx, by);
                    if (!block || block->is_air()) continue;

                    BlockColor c = get_block_color(block->type);
                    float fx = bx * BLOCK_SIZE;
                    float fy = by * BLOCK_SIZE;

                    // Damage tint (cracked blocks are darker)
                    if (block->hp < block->max_hp && block->max_hp > 0) {
                        float ratio = (float)block->hp / block->max_hp;
                        c.r *= 0.5f + ratio * 0.5f;
                        c.g *= 0.5f + ratio * 0.5f;
                        c.b *= 0.5f + ratio * 0.5f;
                    }

                    renderer.draw_rect(fx, fy, BLOCK_SIZE, BLOCK_SIZE, c.r, c.g, c.b);
                }
            }

            // ---- Draw player ----
            auto* ppos = reg.get<Position>(player);
            auto* pvel = reg.get<Velocity>(player);
            auto* ctrl = reg.get<CharacterController>(player);
            auto* hp = reg.get<Health>(player);

            // Interpolated position
            float rx = ppos->x + pvel->x * (float)alpha;
            float ry = ppos->y + pvel->y * (float)alpha;

            // Body (teal)
            renderer.draw_rect(rx, ry, 24, 40, 0.13f, 0.83f, 0.93f);

            // Direction indicator (amber dot)
            float dot_x = ctrl->facing_right ? rx + 18 : rx;
            renderer.draw_rect(dot_x, ry + 4, 6, 6, 0.98f, 0.75f, 0.14f);

            // Health bar
            if (hp && hp->current < hp->max) {
                float bar_w = 24;
                float bar_h = 3;
                float hp_ratio = hp->current / hp->max;
                renderer.draw_rect(rx, ry - 8, bar_w, bar_h, 0.3f, 0.3f, 0.3f);
                renderer.draw_rect(rx, ry - 8, bar_w * hp_ratio, bar_h,
                    1.0f - hp_ratio, hp_ratio, 0.0f);
            }

            // ---- Draw block highlight (mouse target) ----
            float world_mx = camera.screen_to_world_x(mouse_x);
            float world_my = camera.screen_to_world_y(mouse_y);
            int hbx = (int)(world_mx / BLOCK_SIZE);
            int hby = (int)(world_my / BLOCK_SIZE);
            renderer.draw_rect(hbx * BLOCK_SIZE, hby * BLOCK_SIZE,
                BLOCK_SIZE, BLOCK_SIZE, 1.0f, 1.0f, 1.0f, 0.3f);

            // ---- Draw selected block indicator ----
            BlockColor sc = get_block_color(selected_block);
            renderer.draw_rect(10, 10, 20, 20, sc.r, sc.g, sc.b);
            renderer.draw_rect(10, 10, 20, 20, 1.0f, 1.0f, 1.0f, 0.3f);

            // ---- Debug overlay ----
            if (show_debug) {
                // Switch to screen-space for text-like debug info
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                glOrtho(0, WINDOW_W, WINDOW_H, 0, -1, 1);
                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();

                // Debug background
                renderer.draw_rect(0, 0, 300, 100, 0.0f, 0.0f, 0.0f, 0.7f);

                // Position dots for debug info
                float dx = 10, dy = 15;
                renderer.draw_rect(dx, dy, 4, 4, 0.13f, 0.83f, 0.93f); // teal dot
                // We can't draw text with immediate mode OpenGL, so we use colored bars
                // In a real build, we'd use SDL_ttf or bitmap fonts

                // Player velocity indicator
                renderer.draw_rect(150, 15, std::abs(pvel->x) * 0.1f, 4, 0.98f, 0.75f, 0.14f);
                renderer.draw_rect(150, 22, std::abs(pvel->y) * 0.1f, 4, 0.13f, 0.83f, 0.93f);
            }

            renderer.present();
        }
    );

    // Run in thread
    std::thread game_thread([&]() { loop.run(); });

    // Wait for quit
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) running = false;
            if (e.type == SDL_MOUSEMOTION) { mouse_x = e.motion.x; mouse_y = e.motion.y; }
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_LEFT) mouse_left = true;
                if (e.button.button == SDL_BUTTON_RIGHT) mouse_right = true;
            }
            if (e.type == SDL_MOUSEWHEEL) {
                camera.zoom = std::max(0.5f, std::min(3.0f, camera.zoom + e.wheel.y * 0.1f));
            }
            input.process_event(e, reg);
        }
        SDL_Delay(16);
    }
    loop.stop();
    game_thread.join();

    std::cout << "Game ended." << std::endl;
    return 0;
}
