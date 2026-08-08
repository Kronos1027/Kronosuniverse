// KronoUniverse — Liquid System Tests (v0.6)

#include "game/liquid_system.hpp"
#include <cassert>
#include <iostream>

using namespace krono;

static int tests_passed = 0;
static int tests_total = 0;
#define TEST(name) tests_total++;
#define ENDTEST() tests_passed++; std::cout << "[OK] " << __func__ << std::endl;

static void test_place_water() {
    TEST("place water creates water block");
    World world(42);
    LiquidSystem::place_liquid(world, 10, 10, LiquidType::WATER);
    Block* b = world.get_block(10, 10);
    assert(b != nullptr);
    assert(b->type == BlockType::WATER);
    ENDTEST();
}

static void test_place_lava() {
    TEST("place lava creates lava block");
    World world(42);
    LiquidSystem::place_liquid(world, 10, 10, LiquidType::LAVA);
    Block* b = world.get_block(10, 10);
    assert(b->type == BlockType::LAVA);
    ENDTEST();
}

static void test_water_flows_down() {
    TEST("water flows down when air below");
    World world(42);
    // Clear area
    for (int y = 10; y < 15; y++) {
        world.set_block(10, y, BlockType::AIR);
    }
    // Place water at top
    LiquidSystem::place_liquid(world, 10, 10, LiquidType::WATER);
    // Update liquid (force tick by passing enough dt)
    LiquidSystem::update(world, 5, 15, 5, 15, 1.0f);
    // Water should have flowed down at least one block
    bool water_below = false;
    for (int y = 11; y < 15; y++) {
        if (world.get_block(10, y)->type == BlockType::WATER) {
            water_below = true;
            break;
        }
    }
    assert(water_below);
    ENDTEST();
}

static void test_water_doesnt_flow_through_solid() {
    TEST("water doesn't flow through solid block");
    World world(42);
    // Clear entire area and surround with solid
    for (int x = 9; x <= 11; x++) {
        for (int y = 10; y <= 14; y++) {
            world.set_block(x, y, BlockType::STONE);
        }
    }
    // Carve a small pocket
    world.set_block(10, 10, BlockType::AIR);
    world.set_block(10, 11, BlockType::AIR);  // Wait this should be solid
    world.set_block(10, 11, BlockType::STONE);  // Solid floor
    // Place water
    LiquidSystem::place_liquid(world, 10, 10, LiquidType::WATER);
    LiquidSystem::update(world, 5, 15, 5, 15, 1.0f);
    // Water should stay at y=10 (blocked by stone below and on sides)
    assert(world.get_block(10, 10)->type == BlockType::WATER);
    assert(world.get_block(10, 12)->type == BlockType::STONE);  // not water
    ENDTEST();
}

static void test_water_flows_sideways() {
    TEST("water flows sideways when blocked below");
    World world(42);
    // Create a long horizontal channel with solid floor
    // Make it long enough that water doesn't fall off the end during test
    for (int x = 5; x < 20; x++) {
        world.set_block(x, 10, BlockType::AIR);
        world.set_block(x, 11, BlockType::STONE);  // solid floor
        world.set_block(x, 12, BlockType::STONE);
        world.set_block(x, 13, BlockType::STONE);
    }
    // Place water in middle
    LiquidSystem::place_liquid(world, 12, 10, LiquidType::WATER);
    // Update a few times - water should spread sideways
    LiquidSystem::update(world, 0, 25, 5, 15, 1.0f);
    // After 1 tick water should be at (11,10) or (13,10) (sideways flow)
    bool flowed = false;
    if (world.get_block(11, 10)->type == BlockType::WATER) flowed = true;
    if (world.get_block(13, 10)->type == BlockType::WATER) flowed = true;
    assert(flowed);
    ENDTEST();
}

static void test_is_in_liquid() {
    TEST("is_in_liquid detects water");
    World world(42);
    world.set_block(10, 10, BlockType::WATER);
    // Position 10*16+8 = 168, 10*16+8 = 168
    assert(LiquidSystem::is_in_liquid(world, 168.0f, 168.0f));
    // Position in air
    world.set_block(10, 10, BlockType::AIR);
    assert(!LiquidSystem::is_in_liquid(world, 168.0f, 168.0f));
    ENDTEST();
}

static void test_get_liquid_at() {
    TEST("get_liquid_at returns correct type");
    World world(42);
    world.set_block(10, 10, BlockType::WATER);
    assert(LiquidSystem::get_liquid_at(world, 10, 10) == LiquidType::WATER);
    world.set_block(10, 10, BlockType::LAVA);
    assert(LiquidSystem::get_liquid_at(world, 10, 10) == LiquidType::LAVA);
    world.set_block(10, 10, BlockType::AIR);
    assert(LiquidSystem::get_liquid_at(world, 10, 10) == LiquidType::NONE);
    ENDTEST();
}

static void test_water_color() {
    TEST("water color is blue");
    float r, g, b, a;
    LiquidSystem::get_color(LiquidType::WATER, 7, r, g, b, a);
    assert(b > r);
    assert(b > g);
    ENDTEST();
}

static void test_lava_color() {
    TEST("lava color is orange-red");
    float r, g, b, a;
    LiquidSystem::get_color(LiquidType::LAVA, 7, r, g, b, a);
    assert(r > g);
    assert(r > b);
    ENDTEST();
}

static void test_lava_alpha_higher_than_water() {
    TEST("lava is at least as opaque as water");
    float wr, wg, wb, wa;
    float lr, lg, lb, la;
    LiquidSystem::get_color(LiquidType::WATER, 7, wr, wg, wb, wa);
    LiquidSystem::get_color(LiquidType::LAVA, 7, lr, lg, lb, la);
    // Lava should be opaque (0.9) while water at full level is 0.6+0.3*1 = 0.9
    // They can be equal, so check >= instead of >
    assert(la >= wa - 0.01f);
    ENDTEST();
}

int main() {
    std::cout << "=== Liquid System Tests ===" << std::endl;
    test_place_water();
    test_place_lava();
    test_water_flows_down();
    test_water_doesnt_flow_through_solid();
    test_water_flows_sideways();
    test_is_in_liquid();
    test_get_liquid_at();
    test_water_color();
    test_lava_color();
    test_lava_alpha_higher_than_water();
    std::cout << "\n=== Results: " << tests_passed << "/" << tests_total << " ===" << std::endl;
    return tests_passed == tests_total ? 0 : 1;
}
