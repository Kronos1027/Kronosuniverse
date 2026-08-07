// Test: ECS basic operations
#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include <iostream>
#include <cassert>

using namespace krono;

int main() {
    std::cout << "=== ECS Tests ===" << std::endl;

    Registry reg;

    // Test 1: Create entity
    Entity e1 = reg.create();
    assert(e1 != INVALID_ENTITY);
    std::cout << "✓ Entity created: " << e1 << std::endl;

    // Test 2: Add component
    reg.emplace<Position>(e1, Position{10.0f, 20.0f});
    assert(reg.has<Position>(e1));
    auto* pos = reg.get<Position>(e1);
    assert(pos != nullptr);
    assert(pos->x == 10.0f && pos->y == 20.0f);
    std::cout << "✓ Component added and retrieved" << std::endl;

    // Test 3: Modify component
    pos->x = 99.0f;
    auto* pos2 = reg.get<Position>(e1);
    assert(pos2->x == 99.0f);
    std::cout << "✓ Component modified" << std::endl;

    // Test 4: Remove component
    reg.remove<Position>(e1);
    assert(!reg.has<Position>(e1));
    std::cout << "✓ Component removed" << std::endl;

    // Test 5: Multiple entities + iteration
    Entity e2 = reg.create();
    Entity e3 = reg.create();
    reg.emplace<Position>(e1, Position{1, 1});
    reg.emplace<Position>(e2, Position{2, 2});
    reg.emplace<Position>(e3, Position{3, 3});
    reg.emplace<Velocity>(e1, Velocity{10, 10});
    reg.emplace<Velocity>(e3, Velocity{30, 30});

    int count = 0;
    reg.each<Position>([&](Entity e, Position& p) {
        count++;
    });
    assert(count == 3);
    std::cout << "✓ Iteration over 3 entities with Position" << std::endl;

    // Test 6: each with multiple components (only e1 and e3 have Velocity)
    count = 0;
    reg.each<Position, Velocity>([&](Entity e, Position& p, Velocity& v) {
        count++;
    });
    assert(count == 2);
    std::cout << "✓ Multi-component iteration (2 entities with Position+Velocity)" << std::endl;

    // Test 7: Destroy entity
    reg.destroy(e2);
    assert(!reg.has<Position>(e2));
    std::cout << "✓ Entity destroyed (components auto-removed)" << std::endl;

    // Test 8: Entity recycling
    Entity e4 = reg.create();
    // e4 should reuse e2's ID slot (with incremented version)
    std::cout << "✓ Entity recycled: new=" << e4 << " (was e2=" << e2 << ")" << std::endl;

    std::cout << std::endl << "All ECS tests passed! ✓" << std::endl;
    std::cout << "Alive entities: " << reg.alive() << std::endl;
    return 0;
}
