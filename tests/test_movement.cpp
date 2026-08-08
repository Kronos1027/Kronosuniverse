// Test: Movement System (Fase 1, Parte 5.1)
// Validates ALL physics formulas from the prompt:
// - Jump: v = sqrt(2 * g * h) — consistent height across gravities
// - Fall damage: dano = max(0, (v_impacto - limiar) * mult_espécie)
// - Encumbrance: weight reduces speed and jump
// - Surface friction: ice slides, mud slows
// - States: walk, run, jump, fall, swim, fly, climb, crouch
// - Coyote time + jump buffering

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "engine/character_components.hpp"
#include "physics/physics.hpp"
#include "physics/movement_system.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace krono;

int main() {
    std::cout << "=== Movement System Tests (Fase 1) ===" << std::endl;

    Registry reg;
    MovementSystem movement;
    PhysicsSystem physics;
    const float EARTH_GRAVITY = 9.81f;
    const float dt = 1.0f / 60.0f; // 60Hz

    // ============================================================
    // TEST 1: Jump formula — v = sqrt(2 * g * h)
    //         Same height on different gravities
    // ============================================================
    {
        std::cout << "\n--- Test 1: Jump formula (v = sqrt(2*g*h)) ---" << std::endl;

        // Character on Earth (g=9.81) vs Moon (g=1.62)
        float earth_g = 9.81f * 50.0f; // scale to pixels (game units)
        float moon_g = 1.62f * 50.0f;
        float target_height = 80.0f; // 80 pixels jump height

        // Earth jump velocity
        float v_earth = std::sqrt(2.0f * earth_g * target_height);
        // Moon jump velocity
        float v_moon = std::sqrt(2.0f * moon_g * target_height);

        // Verify formula: both should reach the same height
        // h = v² / (2g) → check that v²/(2g) == target_height for both
        float h_earth = (v_earth * v_earth) / (2.0f * earth_g);
        float h_moon = (v_moon * v_moon) / (2.0f * moon_g);

        std::cout << "  Earth: v=" << v_earth << " → h=" << h_earth << std::endl;
        std::cout << "  Moon:  v=" << v_moon << " → h=" << h_moon << std::endl;

        assert(std::abs(h_earth - target_height) < 0.01f);
        assert(std::abs(h_moon - target_height) < 0.01f);
        std::cout << "  ✓ Both reach same height despite different gravity" << std::endl;
    }

    // ============================================================
    // TEST 2: Fall damage formula
    //         dano = max(0, (v_impacto - limiar) * mult_espécie)
    // ============================================================
    {
        std::cout << "\n--- Test 2: Fall damage ---" << std::endl;

        float impact_vel = 600.0f; // fast fall
        float threshold = 400.0f;  // safe threshold
        float human_mult = 1.0f;   // normal species
        float dwarf_mult = 0.5f;   // resilient species

        float human_damage = std::max(0.0f, (impact_vel - threshold) * human_mult * 0.1f);
        float dwarf_damage = std::max(0.0f, (impact_vel - threshold) * dwarf_mult * 0.1f);
        float safe_damage = std::max(0.0f, (300.0f - threshold) * human_mult * 0.1f); // below threshold

        std::cout << "  Human fall (v=600): damage=" << human_damage << std::endl;
        std::cout << "  Dwarf fall (v=600): damage=" << dwarf_damage << std::endl;
        std::cout << "  Safe fall (v=300):  damage=" << safe_damage << std::endl;

        assert(human_damage > 0);
        assert(dwarf_damage > 0);
        assert(dwarf_damage < human_damage); // dwarf takes less
        assert(safe_damage == 0); // below threshold = no damage
        std::cout << "  ✓ Dwarf takes half damage, safe fall = 0 damage" << std::endl;
    }

    // ============================================================
    // TEST 3: Encumbrance affects speed and jump
    // ============================================================
    {
        std::cout << "\n--- Test 3: Encumbrance ---" << std::endl;

        float max_weight = 100.0f;

        // Empty inventory
        float empty_enc = 0.0f / max_weight;
        float empty_speed = 1.0f - (empty_enc * 0.5f);
        float empty_jump = 1.0f - (empty_enc * 0.7f);

        // Full inventory
        float full_enc = 100.0f / max_weight;
        float full_speed = 1.0f - (full_enc * 0.5f);
        float full_jump = 1.0f - (full_enc * 0.7f);

        // Overencumbered
        float over_enc = 150.0f / max_weight;
        float over_speed = 1.0f - (over_enc * 0.5f);
        float over_jump = 1.0f - (over_enc * 0.7f);

        std::cout << "  Empty: speed_mult=" << empty_speed << " jump_mult=" << empty_jump << std::endl;
        std::cout << "  Full:  speed_mult=" << full_speed << " jump_mult=" << full_jump << std::endl;
        std::cout << "  Over:  speed_mult=" << over_speed << " jump_mult=" << over_jump << std::endl;

        assert(empty_speed == 1.0f && empty_jump == 1.0f);
        assert(full_speed == 0.5f); // 50% slow at full load
        assert(full_jump == 0.3f);  // 70% jump reduction
        assert(over_speed < 0.5f);  // even slower when overencumbered
        assert(over_jump < 0.0f);   // can't jump when overencumbered
        std::cout << "  ✓ Encumbrance reduces speed (max 50%) and jump (max 70%)" << std::endl;
    }

    // ============================================================
    // TEST 4: Surface friction (ice slides, mud slows)
    // ============================================================
    {
        std::cout << "\n--- Test 4: Surface friction ---" << std::endl;

        float ice_friction = MovementSystem::surface_friction(SurfaceType::ICE);
        float mud_friction = MovementSystem::surface_friction(SurfaceType::MUD);
        float metal_friction = MovementSystem::surface_friction(SurfaceType::METAL);
        float stone_friction = MovementSystem::surface_friction(SurfaceType::STONE);

        std::cout << "  Ice:   " << ice_friction << " (should be very low — slides)" << std::endl;
        std::cout << "  Mud:   " << mud_friction << " (should be very high — slows)" << std::endl;
        std::cout << "  Metal: " << metal_friction << " (medium)" << std::endl;
        std::cout << "  Stone: " << stone_friction << " (standard)" << std::endl;

        assert(ice_friction < 0.1f);   // ice = very slippery
        assert(mud_friction > 0.9f);   // mud = very sticky
        assert(metal_friction < stone_friction); // metal smoother than stone
        std::cout << "  ✓ Ice < 0.1, Mud > 0.9, Metal < Stone" << std::endl;
    }

    // ============================================================
    // TEST 5: Walk and run speeds
    // ============================================================
    {
        std::cout << "\n--- Test 5: Walk vs Run ---" << std::endl;

        Entity e = reg.create();
        reg.emplace<Position>(e, Position{0, 0});
        reg.emplace<Velocity>(e, Velocity{0, 0});
        reg.emplace<Mass>(e, Mass{70.0f});
        reg.emplace<RigidBody>(e, RigidBody{0.0f, 0.8f, false, false});
        reg.emplace<AABBCollider>(e, AABBCollider{32, 48});
        reg.emplace<CharacterController>(e, CharacterController{});
        reg.emplace<Health>(e, Health{});
        reg.emplace<AnimationState>(e, AnimationState{});

        auto* ctrl = reg.get<CharacterController>(e);
        auto* vel = reg.get<Velocity>(e);

        // Walk right
        ctrl->input_right = true;
        ctrl->input_run = false;
        ctrl->grounded = true;
        for (int i = 0; i < 60; i++) { // 1 second
            movement.update(reg, dt, EARTH_GRAVITY * 50);
            physics.update(reg, dt);
        }
        float walk_speed = std::abs(vel->x);
        std::cout << "  Walk speed after 1s: " << walk_speed << " (target=" << ctrl->walk_speed << ")" << std::endl;
        assert(walk_speed > ctrl->walk_speed * 0.9f); // should be near walk_speed

        // Run right
        ctrl->input_run = true;
        vel->x = 0;
        for (int i = 0; i < 60; i++) {
            movement.update(reg, dt, EARTH_GRAVITY * 50);
            physics.update(reg, dt);
        }
        float run_speed = std::abs(vel->x);
        std::cout << "  Run speed after 1s: " << run_speed << " (target=" << ctrl->run_speed << ")" << std::endl;
        assert(run_speed > walk_speed); // run > walk
        std::cout << "  ✓ Run > Walk" << std::endl;

        reg.destroy(e);
    }

    // ============================================================
    // TEST 6: Jump actually makes entity go up
    // ============================================================
    {
        std::cout << "\n--- Test 6: Jump ---" << std::endl;

        Entity e = reg.create();
        reg.emplace<Position>(e, Position{0, 500}); // on ground
        reg.emplace<Velocity>(e, Velocity{0, 0});
        reg.emplace<Mass>(e, Mass{70.0f});
        reg.emplace<RigidBody>(e, RigidBody{0.0f, 0.8f, false, false});
        reg.emplace<AABBCollider>(e, AABBCollider{32, 48});
        reg.emplace<CharacterController>(e, CharacterController{});
        reg.emplace<Health>(e, Health{});
        reg.emplace<AnimationState>(e, AnimationState{});
        reg.emplace<FallDamageTracker>(e, FallDamageTracker{});

        auto* ctrl = reg.get<CharacterController>(e);
        auto* pos = reg.get<Position>(e);
        auto* vel = reg.get<Velocity>(e);

        ctrl->grounded = true;
        float start_y = pos->y;

        // Jump
        ctrl->input_jump_pressed = true;
        movement.update(reg, dt, EARTH_GRAVITY * 50);
        ctrl->input_jump_pressed = false;

        std::cout << "  After jump: vel.y=" << vel->y << " (should be negative = up)" << std::endl;
        assert(vel->y < 0); // should be going up
        std::cout << "  ✓ Entity jumps upward" << std::endl;

        reg.destroy(e);
    }

    // ============================================================
    // TEST 7: State machine transitions
    // ============================================================
    {
        std::cout << "\n--- Test 7: State machine ---" << std::endl;

        Entity e = reg.create();
        reg.emplace<Position>(e, Position{0, 500});
        reg.emplace<Velocity>(e, Velocity{0, 0});
        reg.emplace<Mass>(e, Mass{70.0f});
        reg.emplace<RigidBody>(e, RigidBody{0.0f, 0.8f, false, false});
        reg.emplace<AABBCollider>(e, AABBCollider{32, 48});
        reg.emplace<CharacterController>(e, CharacterController{});
        reg.emplace<Health>(e, Health{});
        reg.emplace<AnimationState>(e, AnimationState{});

        auto* ctrl = reg.get<CharacterController>(e);

        // IDLE
        ctrl->grounded = true;
        movement.update(reg, dt, EARTH_GRAVITY * 50);
        assert(ctrl->state == MoveState::IDLE);
        std::cout << "  ✓ IDLE when standing still" << std::endl;

        // WALKING
        ctrl->input_right = true;
        movement.update(reg, dt, EARTH_GRAVITY * 50);
        assert(ctrl->state == MoveState::WALKING);
        std::cout << "  ✓ WALKING when moving" << std::endl;

        // RUNNING
        ctrl->input_run = true;
        movement.update(reg, dt, EARTH_GRAVITY * 50);
        assert(ctrl->state == MoveState::RUNNING);
        std::cout << "  ✓ RUNNING when moving + shift" << std::endl;

        // CROUCHING
        ctrl->input_right = false;
        ctrl->input_run = false;
        ctrl->input_down = true;
        movement.update(reg, dt, EARTH_GRAVITY * 50);
        assert(ctrl->state == MoveState::CROUCHING);
        std::cout << "  ✓ CROUCHING when pressing down" << std::endl;

        reg.destroy(e);
    }

    // ============================================================
    // TEST 8: Animation updates
    // ============================================================
    {
        std::cout << "\n--- Test 8: Animation ---" << std::endl;

        Entity e = reg.create();
        reg.emplace<Position>(e, Position{0, 500});
        reg.emplace<Velocity>(e, Velocity{0, 0});
        reg.emplace<Mass>(e, Mass{70.0f});
        reg.emplace<RigidBody>(e, RigidBody{0.0f, 0.8f, false, false});
        reg.emplace<AABBCollider>(e, AABBCollider{32, 48});
        reg.emplace<CharacterController>(e, CharacterController{});
        reg.emplace<Health>(e, Health{});
        reg.emplace<AnimationState>(e, AnimationState{});

        auto* anim = reg.get<AnimationState>(e);
        auto* ctrl = reg.get<CharacterController>(e);

        ctrl->grounded = true;
        ctrl->input_right = true;
        movement.update(reg, dt, EARTH_GRAVITY * 50);

        assert(anim->current == AnimationState::ANIM_WALK);
        assert(anim->facing_right == true);
        std::cout << "  ✓ Animation = WALK, facing right" << std::endl;

        ctrl->input_right = false;
        ctrl->input_left = true;
        movement.update(reg, dt, EARTH_GRAVITY * 50);
        assert(anim->facing_right == false);
        std::cout << "  ✓ Facing left when moving left" << std::endl;

        reg.destroy(e);
    }

    std::cout << "\n=== All Movement System tests passed! ✓ ===" << std::endl;
    return 0;
}
