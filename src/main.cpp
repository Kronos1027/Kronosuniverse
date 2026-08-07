// KronoUniverse — Main entry point
// Fase 0: Fundação técnica — ECS + GameLoop + Physics + Render básico
//
// Este main cria uma janela SDL2, inicializa o ECS, cria algumas entidades
// de teste (caixas caindo com física), e roda o game loop.
// Quando uma caixa cai e bate no "chão" (caixa estática), ela quica.

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "engine/gameloop.hpp"
#include "physics/physics.hpp"
#include "render/renderer.hpp"
#include <iostream>
#include <cstdlib>

using namespace krono;

int main() {
    std::cout << "=== KronoUniverse — Fase 0 (Fundação técnica) ===" << std::endl;

    // Init renderer
    const int WINDOW_W = 1280;
    const int WINDOW_H = 720;

    Renderer renderer;
    if (!renderer.init(WINDOW_W, WINDOW_H, "KronoUniverse — Fase 0")) {
        std::cerr << "Failed to init renderer: " << SDL_GetError() << std::endl;
        return 1;
    }
    renderer.set_ortho(WINDOW_W, WINDOW_H);
    std::cout << "Renderer OK (" << WINDOW_W << "x" << WINDOW_H << ")" << std::endl;

    // Init ECS + Physics
    Registry reg;
    PhysicsSystem physics;
    physics.gravity_y = 500.0f; // pixels per second² (game scale)
    physics.linear_damping = 0.02f;

    // Create floor (static)
    Entity floor = reg.create();
    reg.emplace<Position>(floor, Position{WINDOW_W / 2.0f - 400, (float)WINDOW_H - 40});
    reg.emplace<AABBCollider>(floor, AABBCollider{800, 40});
    reg.emplace<RigidBody>(floor, RigidBody{0.2f, 1.0f, true, false}); // static
    reg.emplace<Mass>(floor, Mass{1000000});

    // Create some falling boxes
    struct BoxInfo { Entity e; float r, g, b; };
    std::vector<BoxInfo> boxes;
    for (int i = 0; i < 5; i++) {
        Entity e = reg.create();
        float x = 200 + i * 180;
        reg.emplace<Position>(e, Position{x, 50});
        reg.emplace<Velocity>(e, Velocity{(float)(rand() % 100 - 50), 0});
        reg.emplace<Mass>(e, Mass{1.0f + (rand() % 30) / 10.0f});
        reg.emplace<AABBCollider>(e, AABBCollider{48, 48});
        reg.emplace<RigidBody>(e, RigidBody{0.6f, 0.5f, false, false});
        float r = 0.1f + (rand() % 100) / 200.0f;
        float g = 0.5f + (rand() % 100) / 300.0f;
        float b = 0.8f + (rand() % 100) / 500.0f;
        boxes.push_back({e, r, g, b});
    }

    std::cout << "ECS initialized: " << reg.alive() << " entities alive" << std::endl;
    std::cout << "Physics: gravity=" << physics.gravity_y << " damping=" << physics.linear_damping << std::endl;

    // Game loop
    int frame_count = 0;
    GameLoop loop(
        [&](double dt) {
            // Update
            renderer.poll_events();
            physics.update(reg, (float)dt);
        },
        [&](double alpha) {
            // Render
            renderer.clear();
            renderer.set_ortho(WINDOW_W, WINDOW_H);

            // Draw floor (teal)
            auto* fp = reg.get<Position>(floor);
            renderer.draw_rect(fp->x, fp->y, 800, 40, 0.13f, 0.83f, 0.93f, 0.3f);

            // Draw boxes
            for (auto& box : boxes) {
                auto* pos = reg.get<Position>(box.e);
                auto* vel = reg.get<Velocity>(box.e);
                if (!pos) continue;
                // Interpolate position for smooth rendering
                float rx = pos->x + vel->x * (float)alpha;
                float ry = pos->y + vel->y * (float)alpha;
                renderer.draw_rect(rx, ry, 48, 48, box.r, box.g, box.b);

                // Draw velocity vector (debug)
                renderer.draw_rect(rx + 24, ry + 24, vel->x * 0.05f, vel->y * 0.05f,
                    1.0f, 0.75f, 0.14f, 0.5f);
            }

            renderer.present();
            frame_count++;
        }
    );

    // Run in a separate thread, check for quit
    std::thread game_thread([&]() { loop.run(); });

    // Wait for quit
    while (!renderer.should_quit()) {
        SDL_Delay(100);
    }
    loop.stop();
    game_thread.join();

    std::cout << "Game ended after " << frame_count << " frames" << std::endl;
    std::cout << "=== Fase 0 complete ===" << std::endl;
    return 0;
}
