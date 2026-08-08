// KronoUniverse — Main entry point (v0.1 playable)
// Primeira versão jogável: personagem explorando um planeta procedural.
//
// CORREÇÕES vs versão anterior:
// 1. Não usa mais threads — game loop roda na thread principal (evita race condition com SDL)
// 2. OpenGL Compatibility Profile (suporta glBegin/glEnd)
// 3. Event polling numa única thread
// 4. Render e update integrados no mesmo loop
//
// Controles:
//   A/D ou ←/→ = mover
//   W/↑/Space = pular
//   Shift = correr
//   S/↓ = agachar
//   Mouse wheel = zoom in/out
//   Left click = cavar bloco
//   Right click = colocar bloco
//   F1 = debug overlay
//   F2 = trocar bloco selecionado
//   ESC = sair

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "engine/character_components.hpp"
#include "engine/input_system.hpp"
#include "physics/physics.hpp"
#include "physics/movement_system.hpp"
#include "procedural/world.hpp"
#include "render/renderer.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>

using namespace krono;

// ---- Config ----
static constexpr int WINDOW_W = 1280;
static constexpr int WINDOW_H = 720;
static constexpr float GAME_GRAVITY = 9.81f * 50.0f;

// ---- Camera ----
struct Camera {
    float x = 0, y = 0;
    float zoom = 1.0f;

    void follow(float target_x, float target_y, float dt) {
        float target_cx = target_x - (WINDOW_W / zoom) / 2.0f;
        float target_cy = target_y - (WINDOW_H / zoom) / 2.0f;
        x += (target_cx - x) * 5.0f * dt;
        y += (target_cy - y) * 5.0f * dt;
    }

    float screen_to_world_x(float sx) const { return sx / zoom + x; }
    float screen_to_world_y(float sy) const { return sy / zoom + y; }
    float left() const { return x; }
    float top() const { return y; }
    float right() const { return x + WINDOW_W / zoom; }
    float bottom() const { return y + WINDOW_H / zoom; }
};

// ---- Block colors ----
struct BlockColor { float r, g, b; };

BlockColor get_block_color(BlockType type) {
    switch (type) {
        case BlockType::AIR:      return {0.04f, 0.043f, 0.059f};
        case BlockType::DIRT:     return {0.47f, 0.31f, 0.20f};
        case BlockType::GRASS:    return {0.39f, 0.63f, 0.24f};
        case BlockType::STONE:    return {0.51f, 0.51f, 0.55f};
        case BlockType::SAND:     return {0.78f, 0.71f, 0.39f};
        case BlockType::WOOD:     return {0.55f, 0.39f, 0.24f};
        case BlockType::LEAVES:   return {0.31f, 0.55f, 0.20f};
        case BlockType::METAL:    return {0.71f, 0.71f, 0.78f};
        case BlockType::ICE:      return {0.71f, 0.86f, 0.94f};
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

// ---- World collision ----
static void resolve_world_collision(Registry& reg, Entity player, World& world, CharacterController& ctrl) {
    auto* pos = reg.get<Position>(player);
    auto* vel = reg.get<Velocity>(player);
    auto* col = reg.get<AABBCollider>(player);
    if (!pos || !vel || !col) return;

    float px = pos->x;
    float py = pos->y;
    float pw = col->width;
    float ph = col->height;

    int min_bx = (int)(px / BLOCK_SIZE) - 1;
    int max_bx = (int)((px + pw) / BLOCK_SIZE) + 1;
    int min_by = (int)(py / BLOCK_SIZE) - 1;
    int max_by = (int)((py + ph) / BLOCK_SIZE) + 1;

    ctrl.grounded = false;

    for (int bx = min_bx; bx <= max_bx; bx++) {
        for (int by = min_by; by <= max_by; by++) {
            Block* block = world.get_block(bx, by);
            if (!block || !block->is_solid()) continue;

            float bx_world = bx * BLOCK_SIZE;
            float by_world = by * BLOCK_SIZE;

            float overlap_x = std::min(px + pw, bx_world + BLOCK_SIZE) - std::max(px, bx_world);
            float overlap_y = std::min(py + ph, by_world + BLOCK_SIZE) - std::max(py, by_world);

            if (overlap_x > 0 && overlap_y > 0) {
                if (overlap_x < overlap_y) {
                    if (px + pw/2 < bx_world + BLOCK_SIZE/2) {
                        pos->x = bx_world - pw;
                    } else {
                        pos->x = bx_world + BLOCK_SIZE;
                    }
                    vel->x = 0;
                } else {
                    if (py + ph/2 < by_world + BLOCK_SIZE/2) {
                        pos->y = by_world - ph;
                        vel->y = 0;
                    } else {
                        pos->y = by_world - ph;
                        vel->y = 0;
                        ctrl.grounded = true;

                        auto* surf = reg.get<Surface>(player);
                        const auto& props = get_block_props(block->type);
                        if (surf) {
                            surf->type = SurfaceType::STONE;
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

    // ---- Init renderer ----
    Renderer renderer;
    if (!renderer.init(WINDOW_W, WINDOW_H, "KronoUniverse v0.1")) {
        std::cerr << "Failed to init renderer!" << std::endl;
        return 1;
    }

    // ---- Init game systems ----
    Registry reg;
    PhysicsSystem physics;
    physics.gravity_y = GAME_GRAVITY;
    physics.linear_damping = 0.01f;

    MovementSystem movement;
    InputSystem input;

    // ---- Create world ----
    uint32_t seed = 42;
    World world(seed);
    std::cout << "World created with seed " << seed << std::endl;

    // ---- Find spawn point ----
    int spawn_bx = 0;
    int spawn_by = 0;
    for (int y = 0; y < CHUNK_H; y++) {
        Block* b = world.get_block(spawn_bx, y);
        if (b && b->is_solid()) {
            spawn_by = y - 3; // 3 blocks above surface
            break;
        }
    }
    float spawn_x = spawn_bx * BLOCK_SIZE;
    float spawn_y = spawn_by * BLOCK_SIZE;
    std::cout << "Spawn: (" << spawn_x << ", " << spawn_y << ")" << std::endl;

    // ---- Create player ----
    Entity player = create_player(reg, spawn_x, spawn_y);
    Camera camera;
    camera.x = spawn_x - WINDOW_W / 2;
    camera.y = spawn_y - WINDOW_H / 2;

    // ---- Input state ----
    int mouse_x = 0, mouse_y = 0;
    bool mouse_left = false;
    bool mouse_right = false;
    bool show_debug = true;

    BlockType block_palette[] = {BlockType::DIRT, BlockType::STONE, BlockType::WOOD,
                                  BlockType::METAL, BlockType::SAND, BlockType::ICE};
    int palette_idx = 0;
    BlockType selected_block = block_palette[0];

    // ---- Game loop (SINGLE THREADED — no race conditions!) ----
    bool running = true;
    Uint32 last_time = SDL_GetTicks();
    float accumulator = 0.0f;
    const float FIXED_DT = 1.0f / 60.0f;
    int frame_count = 0;

    std::cout << "Entering game loop..." << std::endl;

    while (running) {
        // ---- Calculate frame time ----
        Uint32 current_time = SDL_GetTicks();
        float frame_time = (current_time - last_time) / 1000.0f;
        last_time = current_time;
        if (frame_time > 0.25f) frame_time = 0.25f; // prevent spiral of death
        accumulator += frame_time;

        // ---- Event polling (SINGLE THREAD) ----
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) running = false;
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

        // ---- Fixed timestep updates ----
        while (accumulator >= FIXED_DT) {
            // Input
            input.update(reg);

            // Movement
            movement.update(reg, FIXED_DT, GAME_GRAVITY);

            // Custom physics (gravity + integration, skip default collision)
            auto* pos = reg.get<Position>(player);
            auto* vel = reg.get<Velocity>(player);
            auto* mass = reg.get<Mass>(player);
            auto* rb = reg.get<RigidBody>(player);

            if (pos && vel && mass && rb && !rb->is_static) {
                vel->y += GAME_GRAVITY * FIXED_DT;
                vel->x *= (1.0f - 0.01f * FIXED_DT);
                pos->x += vel->x * FIXED_DT;
                pos->y += vel->y * FIXED_DT;
            }

            // World collision
            auto* ctrl = reg.get<CharacterController>(player);
            if (ctrl) resolve_world_collision(reg, player, world, *ctrl);

            // Mining / placing
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
                    auto* ppos = reg.get<Position>(player);
                    auto* pcol = reg.get<AABBCollider>(player);
                    bool overlap = (ppos->x < (target_bx+1)*BLOCK_SIZE && ppos->x+pcol->width > target_bx*BLOCK_SIZE &&
                                   ppos->y < (target_by+1)*BLOCK_SIZE && ppos->y+pcol->height > target_by*BLOCK_SIZE);
                    if (!overlap) world.set_block(target_bx, target_by, selected_block);
                }
                mouse_right = false;
            }

            // Camera follow
            camera.follow(pos->x, pos->y, FIXED_DT);

            // Respawn if fell
            if (pos->y > CHUNK_H * BLOCK_SIZE + 1000) {
                pos->x = spawn_x;
                pos->y = spawn_y;
                vel->x = 0;
                vel->y = 0;
            }

            input.end_frame(reg);
            accumulator -= FIXED_DT;
        }

        // ---- Render ----
        renderer.clear();

        // Set camera-based ortho projection
        float view_w = WINDOW_W / camera.zoom;
        float view_h = WINDOW_H / camera.zoom;
        renderer.set_ortho(camera.x, camera.x + view_w, camera.y + view_h, camera.y);

        // ---- Draw world blocks ----
        int min_bx = (int)(camera.left() / BLOCK_SIZE) - 1;
        int max_bx = (int)(camera.right() / BLOCK_SIZE) + 1;
        int min_by = std::max(0, (int)(camera.top() / BLOCK_SIZE) - 1);
        int max_by = std::min(CHUNK_H - 1, (int)(camera.bottom() / BLOCK_SIZE) + 1);

        for (int bx = min_bx; bx <= max_bx; bx++) {
            for (int by = min_by; by <= max_by; by++) {
                Block* block = world.get_block(bx, by);
                if (!block || block->is_air()) continue;

                BlockColor c = get_block_color(block->type);
                float fx = bx * BLOCK_SIZE;
                float fy = by * BLOCK_SIZE;

                // Damage tint
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
        auto* pctrl = reg.get<CharacterController>(player);
        auto* php = reg.get<Health>(player);

        // Body (teal)
        renderer.draw_rect(ppos->x, ppos->y, 24, 40, 0.13f, 0.83f, 0.93f);

        // Direction indicator (amber dot)
        float dot_x = pctrl->facing_right ? ppos->x + 18 : ppos->x;
        renderer.draw_rect(dot_x, ppos->y + 4, 6, 6, 0.98f, 0.75f, 0.14f);

        // Health bar
        if (php && php->current < php->max) {
            float bar_w = 24;
            float hp_ratio = php->current / php->max;
            renderer.draw_rect(ppos->x, ppos->y - 8, bar_w, 3, 0.3f, 0.3f, 0.3f);
            renderer.draw_rect(ppos->x, ppos->y - 8, bar_w * hp_ratio, 3,
                1.0f - hp_ratio, hp_ratio, 0.0f);
        }

        // ---- Draw block highlight (mouse target) ----
        float world_mx = camera.screen_to_world_x(mouse_x);
        float world_my = camera.screen_to_world_y(mouse_y);
        int hbx = (int)(world_mx / BLOCK_SIZE);
        int hby = (int)(world_my / BLOCK_SIZE);
        renderer.draw_rect(hbx * BLOCK_SIZE, hby * BLOCK_SIZE,
            BLOCK_SIZE, BLOCK_SIZE, 1.0f, 1.0f, 1.0f, 0.3f);

        // ---- Draw selected block indicator (top-left corner) ----
        BlockColor sc = get_block_color(selected_block);
        renderer.set_screen_ortho();
        renderer.draw_rect(10, 10, 24, 24, sc.r, sc.g, sc.b);
        renderer.draw_rect(10, 10, 24, 24, 1.0f, 1.0f, 1.0f, 0.4f);

        // ---- Debug overlay ----
        if (show_debug) {
            // Debug background
            renderer.draw_rect(0, 0, 280, 60, 0.0f, 0.0f, 0.0f, 0.7f);

            // Player velocity bars
            renderer.draw_rect(10, 15, std::abs(pvel->x) * 0.1f, 4, 0.98f, 0.75f, 0.14f);
            renderer.draw_rect(10, 22, std::abs(pvel->y) * 0.1f, 4, 0.13f, 0.83f, 0.93f);

            // Player position dots
            float px_norm = std::min(1.0f, std::abs(ppos->x) / 10000.0f);
            float py_norm = std::min(1.0f, std::abs(ppos->y) / 2000.0f);
            renderer.draw_rect(10, 30, px_norm * 200, 3, 0.5f, 0.5f, 0.5f);
            renderer.draw_rect(10, 36, py_norm * 200, 3, 0.5f, 0.5f, 0.5f);
        }

        renderer.present();
        frame_count++;
    }

    std::cout << "Game ended after " << frame_count << " frames." << std::endl;
    renderer.shutdown();
    return 0;
}
