#pragma once
// KronoUniverse — Core Components
// Define os componentes básicos do ECS usados pelos sistemas.

#include "engine/ecs.hpp"
#include <cmath>

namespace krono {

// ---- Transform / Position ----
struct Position {
    float x = 0, y = 0;
};

struct Velocity {
    float x = 0, y = 0;
};

struct Mass {
    float value = 1.0f;
};

struct Rotation {
    float angle = 0.0f; // radians
};

// ---- Physics body ----
struct RigidBody {
    float restitution = 0.3f;   // bounciness (0 = no bounce, 1 = perfect)
    float friction = 0.8f;      // surface friction coefficient
    bool is_static = false;     // static bodies don't move
    bool is_kinematic = false;  // kinematic: moved externally, not by physics
};

// ---- AABB Collider ----
struct AABBCollider {
    float width = 32, height = 32;
    float offset_x = 0, offset_y = 0;
};

// ---- Sprite (rendering) ----
struct Sprite {
    uint32_t texture_id = 0;
    float u = 0, v = 0, w = 1, h = 1; // UV coords in texture atlas
    int pixel_w = 32, pixel_h = 32;
};

// ---- Health ----
struct Health {
    float current = 100;
    float max = 100;
};

// ---- Tags ----
struct TagPlayer {};
struct TagNPC {};
struct TagShip {};
struct TagProjectile {};
struct TagStructure {};

// ---- Ship module (Parte 5.2) ----
struct ShipModule {
    enum Type { ENGINE, HULL, WEAPON, SHIELD, SENSOR, REACTOR };
    Type type = HULL;
    float mass = 1.0f;
    float offset_x = 0, offset_y = 0; // relative to ship center
    float thrust = 0;  // for engines
    float health = 100;
    float max_health = 100;
};

// ---- Energy (Parte 5.6) ----
struct EnergyProducer {
    float output = 0; // per tick
};

struct EnergyConsumer {
    float input = 0; // per tick
};

// ---- Faction (Parte 6) ----
struct Faction {
    uint32_t id = 0;
    std::string name;
};

} // namespace krono
