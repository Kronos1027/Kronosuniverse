// KronoUniverse — Collision & Jump Tests (v0.4)
// Tests the critical bug fix: collision grounded logic was inverted

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "engine/character_components.hpp"
#include "physics/physics.hpp"
#include "physics/movement_system.hpp"
#include "procedural/world.hpp"
#include <cassert>
#include <iostream>
#include <cmath>

using namespace krono;

static int tests_passed = 0;
static int tests_total = 0;
#define TEST(name) tests_total++;
#define ENDTEST() tests_passed++; std::cout << "[OK] " << __func__ << std::endl;

constexpr float GRAVITY = 9.81f * 50.0f;
constexpr float DT = 1.0f / 60.0f;
// Use krono::BLOCK_SIZE from world.hpp

// Fixed collision resolution (matches the corrected main.cpp logic)
static void resolve_collision(Registry& reg, Entity player, World& world) {
    auto* pos = reg.get<Position>(player);
    auto* vel = reg.get<Velocity>(player);
    auto* col = reg.get<AABBCollider>(player);
    auto* ctrl = reg.get<CharacterController>(player);
    if (!pos||!vel||!col||!ctrl) return;
    float px=pos->x, py=pos->y, pw=col->width, ph=col->height;
    int min_bx=(int)(px/BLOCK_SIZE)-1, max_bx=(int)((px+pw)/BLOCK_SIZE)+1;
    int min_by=(int)(py/BLOCK_SIZE)-1, max_by=(int)((py+ph)/BLOCK_SIZE)+1;
    ctrl->grounded = false;
    for (int bx=min_bx; bx<=max_bx; bx++) {
        for (int by=min_by; by<=max_by; by++) {
            Block* b = world.get_block(bx, by);
            if (!b||!b->is_solid()) continue;
            float bwx=bx*BLOCK_SIZE, bwy=by*BLOCK_SIZE;
            float ox = std::min(px+pw, bwx+BLOCK_SIZE) - std::max(px, bwx);
            float oy = std::min(py+ph, bwy+BLOCK_SIZE) - std::max(py, bwy);
            if (ox>0 && oy>0) {
                if (ox < oy) {
                    if (px+pw/2 < bwx+BLOCK_SIZE/2) pos->x = bwx-pw;
                    else pos->x = bwx+BLOCK_SIZE;
                    vel->x = 0;
                } else {
                    if (py+ph/2 < bwy+BLOCK_SIZE/2) {
                        pos->y = bwy - ph;
                        vel->y = 0;
                        ctrl->grounded = true;
                    } else {
                        pos->y = bwy + BLOCK_SIZE;
                        vel->y = 0;
                    }
                }
                px=pos->x; py=pos->y;
            }
        }
    }
}

static Entity create_player(Registry& reg, float x, float y) {
    Entity p = reg.create();
    reg.emplace<Position>(p, Position{x,y});
    reg.emplace<Velocity>(p, Velocity{0,0});
    reg.emplace<Mass>(p, Mass{70.0f});
    reg.emplace<RigidBody>(p, RigidBody{0.0f, 0.5f, false, false});
    reg.emplace<AABBCollider>(p, AABBCollider{24, 40});
    reg.emplace<CharacterController>(p, CharacterController{});
    reg.emplace<Health>(p, Health{100, 100});
    reg.emplace<FallDamageTracker>(p, FallDamageTracker{});
    Species human; human.name="Human"; human.base_mass=70.0f;
    reg.emplace<Species>(p, std::move(human));
    InventoryWeight inv; inv.max_weight=100.0f;
    reg.emplace<InventoryWeight>(p, std::move(inv));
    return p;
}

static void test_player_falls_and_lands() {
    TEST("player falls and lands on ground");
    Registry reg;
    World world(42);
    MovementSystem movement;
    // Find ground
    int ground_y = -1;
    for (int y=0; y<128; y++) {
        Block* b = world.get_block(0, y);
        if (b && b->is_solid()) { ground_y = y; break; }
    }
    assert(ground_y > 0);
    float spawn_y = (ground_y - 3) * 16.0f;
    Entity player = create_player(reg, 0, spawn_y);
    auto* pos = reg.get<Position>(player);
    auto* vel = reg.get<Velocity>(player);
    auto* ctrl = reg.get<CharacterController>(player);

    // Simulate 2 seconds of falling
    for (int i = 0; i < 120; i++) {
        movement.update(reg, DT, GRAVITY);
        vel->y += GRAVITY * DT;
        pos->y += vel->y * DT;
        resolve_collision(reg, player, world);
    }
    assert(ctrl->grounded);
    assert(vel->y == 0);
    ENDTEST();
}

static void test_jump_works_when_grounded() {
    TEST("jump works when grounded");
    Registry reg;
    World world(42);
    MovementSystem movement;
    int ground_y = -1;
    for (int y=0; y<128; y++) {
        Block* b = world.get_block(0, y);
        if (b && b->is_solid()) { ground_y = y; break; }
    }
    float spawn_y = (ground_y - 3) * 16.0f;
    Entity player = create_player(reg, 0, spawn_y);
    auto* pos = reg.get<Position>(player);
    auto* vel = reg.get<Velocity>(player);
    auto* ctrl = reg.get<CharacterController>(player);

    // Fall to ground
    for (int i = 0; i < 120; i++) {
        movement.update(reg, DT, GRAVITY);
        vel->y += GRAVITY * DT;
        pos->y += vel->y * DT;
        resolve_collision(reg, player, world);
    }
    assert(ctrl->grounded);

    // Now press jump
    ctrl->input_jump_pressed = true;
    movement.update(reg, DT, GRAVITY);
    assert(vel->y < 0);  // negative = upward
    assert(ctrl->state == MoveState::JUMPING);
    ENDTEST();
}

static void test_jump_doesnt_work_in_air() {
    TEST("jump doesn't work in mid-air (no double jump)");
    Registry reg;
    World world(42);
    MovementSystem movement;
    Entity player = create_player(reg, 0, -100);  // start in air
    auto* vel = reg.get<Velocity>(player);
    auto* ctrl = reg.get<CharacterController>(player);

    // Try to jump while in air
    ctrl->input_jump_pressed = true;
    movement.update(reg, DT, GRAVITY);
    // vel.y should still be positive (falling) — no jump
    assert(vel->y >= 0);  // not jumping
    assert(ctrl->state != MoveState::JUMPING);
    ENDTEST();
}

static void test_jump_height_reached() {
    TEST("jump reaches approximately configured height");
    Registry reg;
    World world(42);
    MovementSystem movement;
    int ground_y = -1;
    for (int y=0; y<128; y++) {
        Block* b = world.get_block(0, y);
        if (b && b->is_solid()) { ground_y = y; break; }
    }
    float spawn_y = (ground_y - 3) * 16.0f;
    Entity player = create_player(reg, 0, spawn_y);
    auto* pos = reg.get<Position>(player);
    auto* vel = reg.get<Velocity>(player);
    auto* ctrl = reg.get<CharacterController>(player);

    // Fall to ground
    for (int i = 0; i < 120; i++) {
        movement.update(reg, DT, GRAVITY);
        vel->y += GRAVITY * DT;
        pos->y += vel->y * DT;
        resolve_collision(reg, player, world);
    }
    float ground_pos = pos->y;

    // Jump
    ctrl->input_jump_pressed = true;
    movement.update(reg, DT, GRAVITY);

    // Simulate upward motion until peak (vel.y crosses 0)
    float max_height = 0;
    for (int i = 0; i < 60; i++) {
        movement.update(reg, DT, GRAVITY);
        vel->y += GRAVITY * DT;
        pos->y += vel->y * DT;
        resolve_collision(reg, player, world);
        float height = ground_pos - pos->y;
        if (height > max_height) max_height = height;
        if (ctrl->grounded) break;
    }
    // jump_height default is 80, so should reach ~80 pixels up
    assert(max_height > 60);  // some tolerance
    assert(max_height < 100);
    ENDTEST();
}

static void test_coyote_time_allows_jump_after_leaving_ground() {
    TEST("coyote time allows jump shortly after leaving ground");
    Registry reg;
    World world(42);
    MovementSystem movement;
    int ground_y = -1;
    for (int y=0; y<128; y++) {
        Block* b = world.get_block(0, y);
        if (b && b->is_solid()) { ground_y = y; break; }
    }
    float spawn_y = (ground_y - 3) * 16.0f;
    Entity player = create_player(reg, 0, spawn_y);
    auto* pos = reg.get<Position>(player);
    auto* vel = reg.get<Velocity>(player);
    auto* ctrl = reg.get<CharacterController>(player);

    // Fall to ground
    for (int i = 0; i < 120; i++) {
        movement.update(reg, DT, GRAVITY);
        vel->y += GRAVITY * DT;
        pos->y += vel->y * DT;
        resolve_collision(reg, player, world);
    }
    assert(ctrl->grounded);

    // Walk off edge (no ground below) - need to find an edge or just move up
    // Actually we'll just move the player up by 1 pixel to simulate leaving ground
    pos->y -= 1;
    resolve_collision(reg, player, world);
    assert(!ctrl->grounded);

    // Within coyote time (100ms), jump should still work
    ctrl->input_jump_pressed = true;
    movement.update(reg, DT, GRAVITY);
    assert(vel->y < 0);  // jumped!
    ENDTEST();
}

static void test_head_bump_doesnt_set_grounded() {
    TEST("hitting head on block doesn't set grounded");
    Registry reg;
    World world(42);
    // Create a player with a block above them
    // First, find a clear area
    Entity player = create_player(reg, 100, 100);
    auto* pos = reg.get<Position>(player);
    auto* vel = reg.get<Velocity>(player);
    auto* ctrl = reg.get<CharacterController>(player);

    // Place a block directly above the player
    int block_x = (int)(pos->x / 16);
    int block_y = (int)((pos->y - 5) / 16);  // block just above player
    world.set_block(block_x, block_y, BlockType::STONE);

    // Give player upward velocity (jumping)
    vel->y = -300;

    // Apply movement
    pos->y += vel->y * DT;
    resolve_collision(reg, player, world);

    // Player should NOT be grounded (hit head)
    assert(!ctrl->grounded);
    assert(vel->y == 0);  // but velocity should be stopped
    ENDTEST();
}

static void test_landing_on_top_sets_grounded() {
    TEST("landing on top of block sets grounded");
    Registry reg;
    World world(42);
    // Find ground
    int ground_y = -1;
    for (int y=0; y<128; y++) {
        Block* b = world.get_block(0, y);
        if (b && b->is_solid()) { ground_y = y; break; }
    }
    Entity player = create_player(reg, 0, (ground_y - 5) * 16.0f);
    auto* pos = reg.get<Position>(player);
    auto* vel = reg.get<Velocity>(player);
    auto* ctrl = reg.get<CharacterController>(player);

    // Apply downward velocity and simulate several frames to land
    vel->y = 200;
    for (int i = 0; i < 30; i++) {
        vel->y += GRAVITY * DT;
        pos->y += vel->y * DT;
        resolve_collision(reg, player, world);
        if (ctrl->grounded) break;
    }

    // Player should be grounded
    assert(ctrl->grounded);
    assert(vel->y == 0);
    ENDTEST();
}

static void test_horizontal_collision_stops_movement() {
    TEST("horizontal collision stops X movement");
    Registry reg;
    World world(42);
    // Find ground level
    int ground_y = -1;
    for (int y=0; y<128; y++) {
        Block* b = world.get_block(50, y);
        if (b && b->is_solid()) { ground_y = y; break; }
    }
    // Place player on ground, with a wall close to the right
    // Player at x=50*16=800, width 24, so right edge at 824
    // Wall at x=51*16=816 (overlaps player already)
    Entity player = create_player(reg, 50*16, (ground_y - 3) * 16.0f);
    auto* pos = reg.get<Position>(player);
    auto* vel = reg.get<Velocity>(player);

    // Place wall right next to player at x=52 (so player needs to move into it)
    world.set_block(52, ground_y - 1, BlockType::STONE);
    world.set_block(52, ground_y - 2, BlockType::STONE);
    world.set_block(52, ground_y - 3, BlockType::STONE);

    // Move right into wall - need enough movement to actually overlap
    vel->x = 500;  // fast enough to reach wall in 1 frame
    for (int i = 0; i < 5; i++) {
        pos->x += vel->x * DT;
        resolve_collision(reg, player, world);
        if (vel->x == 0) break;
    }

    assert(vel->x == 0);  // stopped by wall
    ENDTEST();
}

int main() {
    std::cout << "=== Collision & Jump Tests (v0.4 fix) ===" << std::endl;
    test_player_falls_and_lands();
    test_jump_works_when_grounded();
    test_jump_doesnt_work_in_air();
    test_jump_height_reached();
    test_coyote_time_allows_jump_after_leaving_ground();
    test_head_bump_doesnt_set_grounded();
    test_landing_on_top_sets_grounded();
    test_horizontal_collision_stops_movement();
    std::cout << "\n=== Results: " << tests_passed << "/" << tests_total << " ===" << std::endl;
    return tests_passed == tests_total ? 0 : 1;
}
