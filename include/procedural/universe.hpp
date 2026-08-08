#pragma once
// KronoUniverse — Universe Generation (Fase 6, Parte 7.1-7.2)
// Camada 0: Seed matemática (hash determinístico)
// Camada 1: Simulação abstrata de facções em ticks

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <cmath>
#include "FastNoiseLite.h"
#include "game/npc_system.hpp"

namespace krono {

// ---- Universe coordinates ----
struct GalaxyCoord { int32_t x, y, z; };
struct SystemCoord { int32_t galaxy_x, galaxy_y, galaxy_z; int32_t system_index; };
struct PlanetCoord { int32_t galaxy_x, galaxy_y, galaxy_z; int32_t system_index; int32_t planet_index; };

// ---- Hash function for deterministic generation (Parte 7.1) ----
inline uint64_t hash_seed(uint32_t seed, int32_t a, int32_t b = 0, int32_t c = 0, int32_t d = 0) {
    uint64_t h = seed;
    h = h * 6364136223846793005ULL + a + 1442695040888963407ULL;
    h = h * 6364136223846793005ULL + b + 1442695040888963407ULL;
    h = h * 6364136223846793005ULL + c + 1442695040888963407ULL;
    h = h * 6364136223846793005ULL + d + 1442695040888963407ULL;
    return h;
}

// Pseudo-random float [0, 1) from seed
inline float rand_float(uint64_t seed) {
    return (seed >> 11) * (1.0f / 9007199254740992.0f);
}

inline float rand_range(uint64_t seed, float min, float max) {
    return min + rand_float(seed) * (max - min);
}

// ---- Planet parameters (generated from seed — Parte 7.1) ----
enum class PlanetType : uint8_t {
    TERRESTRIAL,   // earth-like
    GAS_GIANT,     // jupiter-like
    ICE_WORLD,     // frozen
    DESERT_WORLD,  // barren
    VOLCANIC,      // lava
    OCEAN_WORLD,   // water world
    CRYSTAL_WORLD, // exotic
    ARRRIFICIAL,   // built by ancient civ
};

struct PlanetParams {
    PlanetCoord coord;
    PlanetType type;
    float gravity;              // m/s²
    float atmosphere_density;   // 0-1
    bool breathable;            // has oxygen?
    float temperature;          // average °C
    float radius;               // km
    uint32_t seed;              // planet-specific seed for terrain gen
    std::string name;

    // Civilization
    uint32_t dominant_faction_id;
    uint32_t tech_level;        // 1-10
    uint32_t population;
    bool at_war;

    // Resources
    float crystal_abundance;
    float metal_abundance;
    float energy_abundance;
};

// ---- Universe ----
class Universe {
public:
    Universe(uint32_t seed) : seed_(seed) {
        noise_ = fnlCreateState();
        noise_.seed = seed;
        noise_.frequency = 0.01f;
        noise_.noise_type = FNL_NOISE_PERLIN;
    }

    // Camada 0: Generate planet parameters from coordinates (Parte 7.1)
    PlanetParams generate_planet(const PlanetCoord& coord) {
        uint64_t h = hash_seed(seed_, coord.galaxy_x, coord.galaxy_y, coord.galaxy_z,
                               coord.system_index * 100 + coord.planet_index);

        PlanetParams p;
        p.coord = coord;
        p.seed = (uint32_t)(h & 0xFFFFFFFF);

        // Planet type (deterministic from hash)
        int type_roll = (int)(rand_float(h) * 8);
        p.type = static_cast<PlanetType>(type_roll);

        // Gravity (0.1 to 3.0 g)
        p.gravity = rand_range(h >> 8, 0.1f, 3.0f) * 9.81f;

        // Atmosphere
        float atmo_roll = rand_float(h >> 16);
        p.atmosphere_density = atmo_roll;
        p.breathable = (atmo_roll > 0.3f && atmo_roll < 0.9f && p.type != PlanetType::VOLCANIC);

        // Temperature
        switch (p.type) {
            case PlanetType::ICE_WORLD:    p.temperature = rand_range(h >> 24, -150, -30); break;
            case PlanetType::VOLCANIC:     p.temperature = rand_range(h >> 24, 200, 800); break;
            case PlanetType::DESERT_WORLD: p.temperature = rand_range(h >> 24, 40, 90); break;
            case PlanetType::GAS_GIANT:    p.temperature = rand_range(h >> 24, -100, 100); break;
            default:                       p.temperature = rand_range(h >> 24, -20, 40); break;
        }

        // Radius
        p.radius = rand_range(h >> 32, 1000, 20000);

        // Name (procedural)
        p.name = generate_name(h);

        // Civilization
        p.dominant_faction_id = (uint32_t)(rand_float(h >> 40) * 100);
        p.tech_level = 1 + (uint32_t)(rand_float(h >> 48) * 10);
        p.population = (uint32_t)(rand_float(h >> 56) * 10000000);
        p.at_war = rand_float(h >> 60) > 0.7f;

        // Resources
        p.crystal_abundance = rand_float(h >> 4);
        p.metal_abundance = rand_float(h >> 12);
        p.energy_abundance = rand_float(h >> 20);

        return p;
    }

    // Count systems in a galaxy region
    uint32_t count_systems(int32_t gx, int32_t gy, int32_t gz) {
        uint64_t h = hash_seed(seed_, gx, gy, gz);
        return 1 + (uint32_t)(rand_float(h) * 12); // 1-12 systems per galaxy sector
    }

    // Count planets in a system
    uint32_t count_planets(int32_t gx, int32_t gy, int32_t gz, int32_t sys_idx) {
        uint64_t h = hash_seed(seed_, gx, gy, gz, sys_idx * 777);
        return 1 + (uint32_t)(rand_float(h) * 8); // 1-8 planets
    }

    // Camada 1: Simulate faction politics in background (Parte 7.2)
    void simulate_tick() {
        // Abstract simulation: update faction relations, trigger wars, etc.
        // Runs on a timer (not every frame — e.g., 1 tick per 5 minutes real time)
        sim_tick_++;

        // Random events
        for (auto& [id, faction] : factions_) {
            // Tech progression
            if (rand_float(hash_seed(seed_, sim_tick_, id)) > 0.95f) {
                faction.tech_level = std::min(10u, faction.tech_level + 1);
            }
            // Population growth
            faction.population = (uint32_t)(faction.population * 1.001f);
        }
    }

    // Register a faction
    void add_faction(const FactionData& faction) {
        factions_[faction.id] = faction;
    }

    FactionData* get_faction(uint32_t id) {
        auto it = factions_.find(id);
        return (it != factions_.end()) ? &it->second : nullptr;
    }

    uint32_t seed() const { return seed_; }
    uint64_t sim_tick() const { return sim_tick_; }
    size_t faction_count() const { return factions_.size(); }

private:
    uint32_t seed_;
    uint64_t sim_tick_ = 0;
    fnl_state noise_;
    std::unordered_map<uint32_t, FactionData> factions_;

    // Procedural name generator
    std::string generate_name(uint64_t h) {
        static const char* syllables[] = {
            "ka", "ron", "zel", "thar", "nox", "vir", "dra", "lun",
            "sol", "aer", "mor", "kri", "ven", "osh", "tau", "rho",
            "nyx", "ere", "ion", "qua", "xor", "phy", "gae", "hel"
        };
        std::string name;
        int parts = 2 + (int)(rand_float(h) * 3); // 2-4 syllables
        for (int i = 0; i < parts; i++) {
            uint64_t sh = hash_seed((uint32_t)h, i);
            name += syllables[(int)(rand_float(sh) * 24)];
        }
        // Capitalize first letter
        if (!name.empty()) name[0] = toupper(name[0]);
        return name;
    }
};

} // namespace krono
