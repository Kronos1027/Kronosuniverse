#pragma once
// KronoUniverse — Ship Components (Fase 3, Parte 5.2)
// Sistema de naves modulares com física de corpo rígido completa.

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include <vector>
#include <cmath>

namespace krono {

// ---- Ship Module (individual piece of a ship) ----
struct ShipModuleComp {
    enum Type : uint8_t {
        ENGINE, HULL, WEAPON, SHIELD, SENSOR, REACTOR, CARGO, COCKPIT
    };
    Type type = HULL;
    float mass = 1.0f;           // kg (affects center of mass)
    float offset_x = 0, offset_y = 0;  // relative to ship center
    float width = 32, height = 32;     // size for collision
    float health = 100;
    float max_health = 100;
    float thrust = 0;            // for engines: force output
    float thrust_direction = 0;  // angle in radians (0 = right, -PI/2 = up)
    bool is_active = true;       // can be disabled by damage
    bool is_destroyed = false;
    uint8_t r = 130, g = 130, b = 140; // color for rendering
};

// ---- Ship Controller (input + config) ----
struct ShipController {
    // Input
    float throttle = 0;          // -1 to 1 (forward/reverse)
    float steering = 0;          // -1 to 1 (left/right rotation)
    bool brake = false;          // inertia dampener active
    bool fire = false;           // fire weapons

    // Config
    float max_angular_velocity = 2.0f;  // rad/s
    float inertia_dampener_strength = 3.0f; // damping coefficient (0 = simulation mode, 3 = arcade)
    float fuel_capacity = 1000.0f;
    float fuel_remaining = 1000.0f;
    float fuel_consumption_base = 0.5f;  // per second at full throttle
    float cargo_capacity = 500.0f;
    float cargo_current = 0.0f;

    // State
    float angular_velocity = 0;
    float heat = 0;              // atmospheric reentry heat (0-100, >50 = damage)
    bool in_atmosphere = false;
    bool has_thermal_shield = false;
    bool is_landed = false;
};

// ---- Hull Segment (per-segment HP for partial destruction) ----
struct HullSegment {
    enum Position : uint8_t {
        FRONT, BACK, LEFT, RIGHT, TOP, BOTTOM, CENTER
    };
    Position position = CENTER;
    float health = 200;
    float max_health = 200;
    float armor = 0;  // damage reduction (0-1)
};

// ---- Atmospheric Entry Tracker ----
struct AtmosphereEntry {
    float velocity = 0;          // current speed (for drag calculation)
    float altitude = 0;          // distance from planet surface
    float atmosphere_density = 0; // current density at this altitude
    float drag_coefficient = 0.5f; // ship's drag coefficient (hull shape)
    float frontal_area = 64;     // pixels² (for drag calc)
    float heat_accumulated = 0;  // total heat from reentry
};

} // namespace krono
