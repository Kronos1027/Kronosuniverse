// Test: Ship Physics (Fase 3, Parte 5.2)
#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "engine/ship_components.hpp"
#include "physics/physics.hpp"
#include "physics/ship_physics.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace krono;

int main() {
    std::cout << "=== Ship Physics Tests (Fase 3) ===" << std::endl;
    const float dt = 1.0f / 60.0f;

    // TEST 1: Ship creation + basic thrust
    {
        std::cout << "\n--- Test 1: Basic thrust ---" << std::endl;
        Registry reg;
        ShipPhysicsSystem sps;

        Entity ship = ShipPhysicsSystem::create_ship(reg, 0, 0);
        auto* ctrl = reg.get<ShipController>(ship);
        auto* vel = reg.get<Velocity>(ship);

        ctrl->throttle = 1.0f; // full forward
        ctrl->fuel_remaining = 1000;

        for (int i = 0; i < 60; i++) sps.update(reg, dt); // 1 second

        float speed = std::sqrt(vel->x * vel->x + vel->y * vel->y);
        std::cout << "  Speed after 1s full throttle: " << speed << " px/s" << std::endl;
        assert(speed > 0);
        std::cout << "  ✓ Ship accelerates" << std::endl;
    }

    // TEST 2: Fuel consumption (consumo = base * throttle² * (1 + carga/capacidade))
    {
        std::cout << "\n--- Test 2: Fuel consumption ---" << std::endl;
        Registry reg;
        ShipPhysicsSystem sps;

        Entity ship = ShipPhysicsSystem::create_ship(reg, 0, 0);
        auto* ctrl = reg.get<ShipController>(ship);

        // Full throttle, no cargo
        ctrl->throttle = 1.0f;
        ctrl->fuel_remaining = 1000;
        ctrl->fuel_consumption_base = 10.0f; // 10/sec at full throttle
        ctrl->cargo_current = 0;
        ctrl->cargo_capacity = 500;

        float fuel_before = ctrl->fuel_remaining;
        sps.update(reg, dt); // 1 frame
        float consumed_no_cargo = fuel_before - ctrl->fuel_remaining;

        // Expected: 10 * 1² * (1 + 0) * dt = 10 * (1/60) = 0.1667
        float expected = 10.0f * 1.0f * 1.0f * dt;
        std::cout << "  No cargo: consumed=" << consumed_no_cargo << " expected=" << expected << std::endl;
        assert(std::abs(consumed_no_cargo - expected) < 0.01f);

        // With full cargo
        ctrl->fuel_remaining = 1000;
        ctrl->cargo_current = 500; // full cargo
        fuel_before = ctrl->fuel_remaining;
        sps.update(reg, dt);
        float consumed_full_cargo = fuel_before - ctrl->fuel_remaining;

        // Expected: 10 * 1² * (1 + 500/500) * dt = 10 * 2 * dt = 0.3333
        expected = 10.0f * 1.0f * 2.0f * dt;
        std::cout << "  Full cargo: consumed=" << consumed_full_cargo << " expected=" << expected << std::endl;
        assert(std::abs(consumed_full_cargo - expected) < 0.01f);
        assert(consumed_full_cargo > consumed_no_cargo); // more fuel with cargo
        std::cout << "  ✓ Full cargo consumes 2x fuel (throttle² * (1+cargo))" << std::endl;
    }

    // TEST 3: Steering (rotation)
    {
        std::cout << "\n--- Test 3: Steering ---" << std::endl;
        Registry reg;
        ShipPhysicsSystem sps;

        Entity ship = ShipPhysicsSystem::create_ship(reg, 0, 0);
        auto* ctrl = reg.get<ShipController>(ship);
        auto* rot = reg.get<Rotation>(ship);

        ctrl->steering = 1.0f; // full right
        ctrl->max_angular_velocity = 2.0f; // 2 rad/s
        ctrl->fuel_remaining = 1000;

        for (int i = 0; i < 60; i++) sps.update(reg, dt); // 1 second

        std::cout << "  Angle after 1s steering right: " << rot->angle << " rad" << std::endl;
        assert(rot->angle > 1.0f); // should have rotated significantly
        std::cout << "  ✓ Ship rotates" << std::endl;
    }

    // TEST 4: Inertia dampener (arcade vs simulation)
    {
        std::cout << "\n--- Test 4: Inertia dampener ---" << std::endl;
        Registry reg;
        ShipPhysicsSystem sps;

        // Arcade ship (dampener on)
        Entity ship_arcade = ShipPhysicsSystem::create_ship(reg, 0, 0);
        auto* ctrl_a = reg.get<ShipController>(ship_arcade);
        auto* vel_a = reg.get<Velocity>(ship_arcade);
        ctrl_a->throttle = 1.0f;
        ctrl_a->fuel_remaining = 1000;
        ctrl_a->inertia_dampener_strength = 3.0f;
        ctrl_a->brake = true;

        for (int i = 0; i < 60; i++) sps.update(reg, dt);
        float arcade_speed = std::sqrt(vel_a->x * vel_a->x + vel_a->y * vel_a->y);

        // Simulation ship (no dampener)
        Entity ship_sim = ShipPhysicsSystem::create_ship(reg, 1000, 0);
        auto* ctrl_s = reg.get<ShipController>(ship_sim);
        auto* vel_s = reg.get<Velocity>(ship_sim);
        ctrl_s->throttle = 1.0f;
        ctrl_s->fuel_remaining = 1000;
        ctrl_s->inertia_dampener_strength = 0;
        ctrl_s->brake = true; // brake but dampener=0 → no effect

        for (int i = 0; i < 60; i++) sps.update(reg, dt);
        float sim_speed = std::sqrt(vel_s->x * vel_s->x + vel_s->y * vel_s->y);

        std::cout << "  Arcade speed (dampener on): " << arcade_speed << std::endl;
        std::cout << "  Sim speed (dampener off): " << sim_speed << std::endl;
        assert(sim_speed > arcade_speed); // sim has more residual velocity
        std::cout << "  ✓ Simulation mode has more speed (no dampening)" << std::endl;
    }

    // TEST 5: Out of fuel
    {
        std::cout << "\n--- Test 5: Out of fuel ---" << std::endl;
        Registry reg;
        ShipPhysicsSystem sps;

        Entity ship = ShipPhysicsSystem::create_ship(reg, 0, 0);
        auto* ctrl = reg.get<ShipController>(ship);
        auto* vel = reg.get<Velocity>(ship);

        ctrl->throttle = 1.0f;
        ctrl->fuel_remaining = 0; // empty!

        float speed_before = std::sqrt(vel->x * vel->x + vel->y * vel->y);
        for (int i = 0; i < 60; i++) sps.update(reg, dt);
        float speed_after = std::sqrt(vel->x * vel->x + vel->y * vel->y);

        std::cout << "  Speed before: " << speed_before << " after: " << speed_after << std::endl;
        assert(std::abs(speed_after - speed_before) < 0.01f); // no change — no fuel
        std::cout << "  ✓ No acceleration without fuel" << std::endl;
    }

    // TEST 6: Atmospheric drag
    {
        std::cout << "\n--- Test 6: Atmospheric drag ---" << std::endl;
        Registry reg;
        ShipPhysicsSystem sps;

        Entity ship = ShipPhysicsSystem::create_ship(reg, 0, 0);
        auto* ctrl = reg.get<ShipController>(ship);
        auto* vel = reg.get<Velocity>(ship);
        auto* atmo = reg.get<AtmosphereEntry>(ship);

        // Give ship high velocity
        vel->y = 500; // fast downward
        ctrl->in_atmosphere = true;
        ctrl->fuel_remaining = 1000;
        atmo->atmosphere_density = 1.0f; // Earth-like
        atmo->drag_coefficient = 0.5f;
        atmo->frontal_area = 64;

        float speed_before = std::sqrt(vel->x * vel->x + vel->y * vel->y);
        for (int i = 0; i < 30; i++) sps.update(reg, dt); // 0.5 second
        float speed_after = std::sqrt(vel->x * vel->x + vel->y * vel->y);

        std::cout << "  Speed before: " << speed_before << " after 0.5s drag: " << speed_after << std::endl;
        assert(speed_after < speed_before); // drag slows down
        std::cout << "  ✓ Atmospheric drag reduces speed" << std::endl;

        // Check heat accumulated
        std::cout << "  Heat accumulated: " << atmo->heat_accumulated << std::endl;
        assert(atmo->heat_accumulated > 0);
        std::cout << "  ✓ Heat from friction accumulated" << std::endl;
    }

    // TEST 7: Heat damage without thermal shield
    {
        std::cout << "\n--- Test 7: Heat damage ---" << std::endl;
        Registry reg;
        ShipPhysicsSystem sps;

        Entity ship = ShipPhysicsSystem::create_ship(reg, 0, 0);
        auto* ctrl = reg.get<ShipController>(ship);
        auto* vel = reg.get<Velocity>(ship);
        auto* atmo = reg.get<AtmosphereEntry>(ship);
        auto* hp = reg.get<Health>(ship);

        // Very fast reentry
        vel->y = 2000; // extremely fast
        ctrl->in_atmosphere = true;
        ctrl->has_thermal_shield = false;
        ctrl->fuel_remaining = 0;
        atmo->atmosphere_density = 1.0f;

        float hp_before = hp->current;
        for (int i = 0; i < 60; i++) sps.update(reg, dt); // 1 second
        float hp_after = hp->current;

        std::cout << "  HP before: " << hp_before << " after 1s reentry: " << hp_after << std::endl;
        std::cout << "  Heat: " << ctrl->heat << std::endl;
        assert(hp_after < hp_before); // took damage
        std::cout << "  ✓ Heat damages hull without thermal shield" << std::endl;
    }

    // TEST 8: Thermal shield prevents heat damage
    {
        std::cout << "\n--- Test 8: Thermal shield ---" << std::endl;
        Registry reg;
        ShipPhysicsSystem sps;

        Entity ship = ShipPhysicsSystem::create_ship(reg, 0, 0);
        auto* ctrl = reg.get<ShipController>(ship);
        auto* vel = reg.get<Velocity>(ship);
        auto* atmo = reg.get<AtmosphereEntry>(ship);
        auto* hp = reg.get<Health>(ship);

        vel->y = 2000;
        ctrl->in_atmosphere = true;
        ctrl->has_thermal_shield = true; // protected!
        ctrl->fuel_remaining = 0;
        atmo->atmosphere_density = 1.0f;

        float hp_before = hp->current;
        for (int i = 0; i < 60; i++) sps.update(reg, dt);
        float hp_after = hp->current;

        std::cout << "  HP with shield: before=" << hp_before << " after=" << hp_after << std::endl;
        assert(hp_after == hp_before); // no damage with shield
        std::cout << "  ✓ Thermal shield prevents heat damage" << std::endl;
    }

    std::cout << "\n=== All Ship Physics tests passed! ✓ ===" << std::endl;
    return 0;
}
