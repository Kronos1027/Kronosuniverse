// Test: Universe Generation (Fase 6, Parte 7.1-7.2)
#include "procedural/universe.hpp"
#include <iostream>
#include <cassert>

using namespace krono;

int main() {
    std::cout << "=== Universe Generation Tests (Fase 6) ===" << std::endl;

    // TEST 1: Deterministic planet generation
    {
        std::cout << "\n--- Test 1: Deterministic ---" << std::endl;
        Universe u1(42);
        Universe u2(42);
        PlanetCoord coord{1, 2, 3, 0, 1};
        PlanetParams p1 = u1.generate_planet(coord);
        PlanetParams p2 = u2.generate_planet(coord);
        assert(p1.type == p2.type);
        assert(p1.gravity == p2.gravity);
        assert(p1.name == p2.name);
        std::cout << "  Planet: " << p1.name << " type=" << (int)p1.type
                  << " g=" << p1.gravity << " temp=" << p1.temperature << "C" << std::endl;
        std::cout << "  ✓ Same seed + coords = same planet" << std::endl;
    }

    // TEST 2: Different coordinates = different planets
    {
        std::cout << "\n--- Test 2: Different planets ---" << std::endl;
        Universe u(42);
        PlanetParams p1 = u.generate_planet({1, 0, 0, 0, 0});
        PlanetParams p2 = u.generate_planet({2, 0, 0, 0, 0});
        assert(p1.name != p2.name || p1.type != p2.type);
        std::cout << "  Planet 1: " << p1.name << " type=" << (int)p1.type << std::endl;
        std::cout << "  Planet 2: " << p2.name << " type=" << (int)p2.type << std::endl;
        std::cout << "  ✓ Different coords = different planets" << std::endl;
    }

    // TEST 3: Planet parameters are valid
    {
        std::cout << "\n--- Test 3: Valid params ---" << std::endl;
        Universe u(42);
        for (int i = 0; i < 10; i++) {
            PlanetParams p = u.generate_planet({i, 0, 0, 0, 0});
            assert(p.gravity > 0 && p.gravity < 30);
            assert(p.atmosphere_density >= 0 && p.atmosphere_density <= 1);
            assert(p.radius > 500 && p.radius < 25000);
            assert(p.tech_level >= 1 && p.tech_level <= 10);
            assert(p.population >= 0);
            assert(!p.name.empty());
        }
        std::cout << "  10 planets validated: gravity, atmosphere, radius, tech, pop, name" << std::endl;
        std::cout << "  ✓ All params in valid ranges" << std::endl;
    }

    // TEST 4: System/planet counts
    {
        std::cout << "\n--- Test 4: System counts ---" << std::endl;
        Universe u(42);
        uint32_t systems = u.count_systems(1, 0, 0);
        assert(systems >= 1 && systems <= 12);
        uint32_t planets = u.count_planets(1, 0, 0, 0);
        assert(planets >= 1 && planets <= 8);
        std::cout << "  Systems in sector (1,0,0): " << systems << std::endl;
        std::cout << "  Planets in system 0: " << planets << std::endl;
        std::cout << "  ✓ Counts deterministic and valid" << std::endl;
    }

    // TEST 5: Faction simulation (Camada 1)
    {
        std::cout << "\n--- Test 5: Faction simulation ---" << std::endl;
        Universe u(42);
        FactionData f{1, "Test Faction", "Test", 100, 200, 100};
        f.tech_level = 5;
        f.population = 1000000;
        u.add_faction(f);

        assert(u.faction_count() == 1);
        uint64_t tick_before = u.sim_tick();
        u.simulate_tick();
        assert(u.sim_tick() == tick_before + 1);
        std::cout << "  Tick: " << tick_before << " → " << u.sim_tick() << std::endl;

        FactionData* ff = u.get_faction(1);
        assert(ff != nullptr);
        std::cout << "  Population: " << ff->population << " (should grow)" << std::endl;
        std::cout << "  ✓ Simulation tick runs" << std::endl;
    }

    // TEST 6: Planet types
    {
        std::cout << "\n--- Test 6: Planet types ---" << std::endl;
        Universe u(42);
        bool types_seen[8] = {};
        for (int i = 0; i < 100; i++) {
            PlanetParams p = u.generate_planet({i, 0, 0, 0, 0});
            types_seen[(int)p.type] = true;
        }
        int unique_types = 0;
        for (int i = 0; i < 8; i++) if (types_seen[i]) unique_types++;
        std::cout << "  Unique types in 100 planets: " << unique_types << "/8" << std::endl;
        assert(unique_types >= 5); // should see at least 5 of 8 types
        std::cout << "  ✓ Planet type variety" << std::endl;
    }

    // TEST 7: Volcanic planets have high temperature
    {
        std::cout << "\n--- Test 7: Volcanic temperature ---" << std::endl;
        Universe u(42);
        bool found_volcanic = false;
        for (int i = 0; i < 50; i++) {
            PlanetParams p = u.generate_planet({i*7, 0, 0, 0, 0});
            if (p.type == PlanetType::VOLCANIC) {
                assert(p.temperature > 100);
                found_volcanic = true;
                std::cout << "  Volcanic: " << p.name << " temp=" << p.temperature << "C" << std::endl;
            }
        }
        assert(found_volcanic);
        std::cout << "  ✓ Volcanic planets are hot" << std::endl;
    }

    // TEST 8: Name generation
    {
        std::cout << "\n--- Test 8: Name generation ---" << std::endl;
        Universe u(42);
        for (int i = 0; i < 5; i++) {
            PlanetParams p = u.generate_planet({i*13, 0, 0, 0, 0});
            std::cout << "  " << p.name << std::endl;
            assert(!p.name.empty());
            assert(isupper(p.name[0])); // starts with uppercase
        }
        std::cout << "  ✓ Names are procedural and capitalized" << std::endl;
    }

    std::cout << "\n=== All Universe Generation tests passed! ✓ ===" << std::endl;
    return 0;
}
