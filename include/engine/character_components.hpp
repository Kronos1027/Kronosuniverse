#pragma once
// KronoUniverse — Character Components (Fase 1, Parte 5.1)
// Componentes específicos de personagem: movimento, espécie, inventário, estados.

#include "engine/ecs.hpp"
#include <cstdint>
#include <string>

namespace krono {

// ---- Movement State Machine ----
enum class MoveState : uint8_t {
    IDLE,
    WALKING,
    RUNNING,
    JUMPING,
    FALLING,
    SWIMMING,
    FLYING,
    CLIMBING,    // magnetic boots on metal surfaces
    CROUCHING,
};

struct CharacterController {
    // Input state (set by InputSystem, read by MovementSystem)
    bool input_left = false;
    bool input_right = false;
    bool input_up = false;       // jump / swim up / fly up
    bool input_down = false;     // crouch / swim down / fly down
    bool input_run = false;      // hold to run
    bool input_jump_pressed = false;  // edge-triggered (pressed this frame)

    // Configuration
    float walk_speed = 200.0f;       // pixels/sec
    float run_speed = 350.0f;        // pixels/sec
    float jump_height = 80.0f;       // target jump height in pixels (v = sqrt(2*g*h))
    float swim_speed = 100.0f;       // pixels/sec in water
    float fly_speed = 250.0f;        // pixels/sec when flying
    float crouch_speed = 80.0f;      // pixels/sec when crouching
    float acceleration = 2000.0f;    // how fast to reach target speed
    float deceleration = 3000.0f;    // how fast to stop when no input
    float air_control = 0.3f;        // movement control while airborne (0-1)

    // State
    MoveState state = MoveState::IDLE;
    MoveState prev_state = MoveState::IDLE;
    bool grounded = false;           // touching ground?
    bool was_grounded = false;       // was grounded last frame?
    bool in_water = false;           // submerged in liquid?
    bool is_flying = false;          // flying via equipment?
    bool on_metal_surface = false;   // for magnetic boots?
    int jump_count = 0;              // for double-jump (if species allows)
    int max_jumps = 1;               // species-dependent
    float coyote_time = 0.0f;        // grace period after leaving ground
    float jump_buffer_time = 0.0f;   // grace period for buffered jump input

    // Facing
    bool facing_right = true;
};

// ---- Species (Parte 6 — GDD v0.1 seção 3) ----
struct Species {
    std::string name = "Human";

    // Physical traits (affect physics directly — Parte 5.1)
    float base_mass = 70.0f;              // kg (affects fall speed, impulse response)
    float mass_multiplier = 1.0f;         // species modifier (heavy species = slower)
    float fall_damage_multiplier = 1.0f;  // species modifier (resilient species = less damage)
    float fall_damage_threshold = 400.0f; // safe fall velocity (pixels/sec) — above this = damage
    float swim_friction_modifier = 1.0f;  // aquatic species swim faster (lower friction)
    float jump_multiplier = 1.0f;         // species jump height modifier
    float run_multiplier = 1.0f;          // species run speed modifier

    // Traits
    bool can_fly = false;                 // species with natural flight
    bool can_breathe_underwater = false;  // aquatic species
    bool can_climb_walls = false;         // insectoid species
    int extra_jumps = 0;                  // species with multiple jumps (winged)
};

// ---- Inventory Weight (Parte 6 — GDD v0.2 seção 1.2) ----
// Weight from inventory directly affects movement physics.
struct InventoryWeight {
    float current_weight = 0.0f;    // sum of all carried items' weights
    float max_weight = 100.0f;      // carry capacity (species + equipment dependent)
    float encumbrance_ratio = 0.0f; // current/max (0 = empty, 1+ = overencumbered)

    // Encumbrance effects on physics:
    // - speed_multiplier = 1.0 - (encumbrance_ratio * 0.5)  → max 50% slow at full load
    // - jump_multiplier = 1.0 - (encumbrance_ratio * 0.7)  → max 70% jump reduction at full load
    // - acceleration_multiplier = 1.0 - (encumbrance_ratio * 0.6) → slower to get moving
};

// ---- Terrain Surface (for friction) ----
enum class SurfaceType : uint8_t {
    DIRT,
    GRASS,
    STONE,
    METAL,
    ICE,        // very low friction — sliding
    MUD,        // high friction — slows down
    WOOD,
    SAND,
    GLASS,
    LAVA,       // damages + low friction
    WATER,      // swimming
    DEEP_WATER, // swimming, can't stand
    AIR,        // no surface (flying/falling)
};

struct Surface {
    SurfaceType type = SurfaceType::DIRT;
    float friction = 0.8f;        // 0 = ice (slide), 1 = sticky
    float damage_per_sec = 0.0f;  // lava damages, others 0
};

// ---- Fall Damage Tracker ----
struct FallDamageTracker {
    float max_fall_velocity = 0.0f;  // track peak downward velocity during fall
    bool was_falling = false;        // was in falling state last frame
};

// ---- Animation State (sprite layers) ----
struct AnimationState {
    // Current animation
    enum Anim : uint8_t {
        ANIM_IDLE,
        ANIM_WALK,
        ANIM_RUN,
        ANIM_JUMP,
        ANIM_FALL,
        ANIM_SWIM,
        ANIM_FLY,
        ANIM_CLIMB,
        ANIM_CROUCH,
        ANIM_HURT,
        ANIM_DEATH,
    };
    Anim current = ANIM_IDLE;
    Anim prev = ANIM_IDLE;
    float frame_time = 0.0f;       // accumulated time for current frame
    int frame_index = 0;           // current frame in animation
    int frame_count = 4;           // total frames in current animation
    float frame_duration = 0.15f;  // seconds per frame
    bool facing_right = true;

    // Sprite layer configuration (Parte 5.1 — "animação em camadas de sprite")
    // Layers are drawn bottom-to-top: body, arms, legs, head, equipment
    bool has_body_layer = true;
    bool has_arms_layer = true;
    bool has_legs_layer = true;
    bool has_head_layer = true;
    bool has_equipment_layer = false;
    uint32_t body_sprite_id = 0;
    uint32_t arms_sprite_id = 0;
    uint32_t legs_sprite_id = 0;
    uint32_t head_sprite_id = 0;
    uint32_t equipment_sprite_id = 0;
};

// ---- Planet Gravity (per-entity override for different planets) ----
struct PlanetGravity {
    float gravity = 9.81f;       // m/s² (Earth default)
    float atmosphere_height = 1000.0f;  // above this = vacuum
    float atmosphere_density = 1.0f;    // at surface
    bool has_breathable_atmosphere = true;
};

// ---- Liquid Volume (for swimming) ----
struct LiquidVolume {
    float density = 1.0f;       // water = 1.0, lava = 2.5 (denser → more buoyancy)
    float viscosity = 0.5f;     // water = 0.5, lava = 0.9 (higher = more drag)
    float damage_per_sec = 0.0f; // lava damages
    bool is_liquid = true;
};

// ---- Equipment (flying, magnetic boots, etc.) ----
struct Equipment {
    bool has_flight_pack = false;
    bool has_magnetic_boots = false;
    bool has_thermal_shield = false;  // for atmospheric reentry (Parte 5.2)
    float flight_energy_cost = 5.0f;  // energy per second of flight
    float flight_thrust = 600.0f;    // vertical thrust when flying
};

} // namespace krono
