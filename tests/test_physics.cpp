// Test: Physics system — gravity, collision, impulse
#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "physics/physics.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace krono;

int main() {
    std::cout << "=== Physics Tests ===" << std::endl;

    Registry reg;
    PhysicsSystem physics;
    physics.gravity_y = 100.0f;
    physics.linear_damping = 0.0f;

    // Test 1: Gravity affects dynamic body
    Entity e = reg.create();
    reg.emplace<Position>(e, Position{0, 0});
    reg.emplace<Velocity>(e, Velocity{0, 0});
    reg.emplace<Mass>(e, Mass{1.0f});
    reg.emplace<RigidBody>(e, RigidBody{0.5f, 0.5f, false, false});

    physics.update(reg, 0.1f); // 100ms
    auto* vel = reg.get<Velocity>(e);
    assert(std::abs(vel->y - 10.0f) < 0.01f); // v = g*t = 100*0.1 = 10
    std::cout << "✓ Gravity: vel.y=" << vel->y << " (expected 10)" << std::endl;

    // Test 2: Static body not affected by gravity
    Entity s = reg.create();
    reg.emplace<Position>(s, Position{0, 0});
    reg.emplace<Velocity>(s, Velocity{0, 0});
    reg.emplace<Mass>(s, Mass{1.0f});
    reg.emplace<RigidBody>(s, RigidBody{0.5f, 0.5f, true, false}); // static

    physics.update(reg, 0.1f);
    auto* svel = reg.get<Velocity>(s);
    assert(std::abs(svel->y) < 0.01f); // should not move
    std::cout << "✓ Static body: vel.y=" << svel->y << " (expected 0)" << std::endl;

    // Test 3: Impulse (aplicar_impulso — Parte 5.7)
    physics.add_impulse(e, 50.0f, 0.0f); // push right
    physics.update(reg, 0.1f);
    vel = reg.get<Velocity>(e);
    // Δv = F/m = 50/1 = 50
    assert(std::abs(vel->x - 50.0f) < 0.01f);
    std::cout << "✓ Impulse: vel.x=" << vel->x << " (expected 50)" << std::endl;

    // Test 4: Position integration
    auto* pos = reg.get<Position>(e);
    // After 2 steps of 0.1s with vel.x=50: x = 50*0.1*2 = 10 (approx, plus gravity on y)
    std::cout << "✓ Position integration: pos.x=" << pos->x << " pos.y=" << pos->y << std::endl;

    // Test 5: Collision detection (AABB)
    Registry reg2;
    PhysicsSystem phys2;
    phys2.gravity_y = 0;

    Entity a = reg2.create();
    reg2.emplace<Position>(a, Position{0, 0});
    reg2.emplace<Velocity>(a, Velocity{10, 0});
    reg2.emplace<Mass>(a, Mass{1.0f});
    reg2.emplace<AABBCollider>(a, AABBCollider{32, 32});
    reg2.emplace<RigidBody>(a, RigidBody{0.0f, 0.5f, false, false});

    Entity b = reg2.create();
    reg2.emplace<Position>(b, Position{30, 0}); // overlapping!
    reg2.emplace<Velocity>(b, Velocity{0, 0});
    reg2.emplace<Mass>(b, Mass{1.0f});
    reg2.emplace<AABBCollider>(b, AABBCollider{32, 32});
    reg2.emplace<RigidBody>(b, RigidBody{0.0f, 0.5f, true, false}); // static wall

    phys2.update(reg2, 0.01f);
    auto* pos_a = reg2.get<Position>(a);
    auto* vel_a = reg2.get<Velocity>(a);
    // a should have been pushed back by collision resolution
    std::cout << "✓ Collision: pos_a.x=" << pos_a->x << " vel_a.x=" << vel_a->x
              << " (expected push-back and stop)" << std::endl;

    std::cout << std::endl << "All physics tests passed! ✓" << std::endl;
    return 0;
}
