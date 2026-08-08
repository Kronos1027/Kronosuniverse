#pragma once
// KronoUniverse — Wildlife & Foliage System (v0.5)
//
// Adiciona vida ao mundo:
// - Plantas que crescem (grama, flores, cogumelos, arbustos)
// - Árvores frutíferas (geram frutas periodicamente)
// - Animais passivos (pássaros, peixes, insetos)
// - Insetos voadores (vaga-lumes à noite)
// - Sistema de crescimento baseado em tempo
// - Sistema de spawn natural (animais aparecem em biomas apropriados)

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "procedural/world.hpp"
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace krono {

// ---- Plant types ----
enum class PlantType : uint8_t {
    GRASS_TUFT = 0,
    FLOWER_RED,
    FLOWER_YELLOW,
    FLOWER_BLUE,
    MUSHROOM,
    BUSH_BERRY,
    MUSHROOM_GLOW,
    CACTUS,
    CRYSTAL_FLOWER,
    DEAD_BUSH,
};

struct Plant {
    PlantType type = PlantType::GRASS_TUFT;
    float x = 0, y = 0;
    float growth = 1.0f;       // 0-1
    float growth_rate = 0.01f; // per second
    bool harvestable = false;
    float harvest_timer = 0;   // time until next harvest
    uint16_t yield_item_id = 0;
    uint8_t yield_count = 1;
};

// ---- Wildlife types ----
enum class WildlifeType : uint8_t {
    BIRD = 0,        // flies in air, lands on trees
    FISH,            // swims in water
    BUTTERFLY,       // flies near flowers
    FIREFLY,         // glows at night
    BEE,             // pollinates flowers
    RABBIT_WILD,     // hops on ground (different from mob rabbit)
    FOX,             // small predator
};

struct Wildlife {
    WildlifeType type = WildlifeType::BIRD;
    float x = 0, y = 0;
    float vx = 0, vy = 0;
    float wander_timer = 0;
    float wander_target_x = 0, wander_target_y = 0;
    float speed = 50;
    bool active = true;
    bool resting = false;
    float rest_timer = 0;
    uint16_t home_x = 0, home_y = 0;  // spawn point they return to
    float glow_phase = 0;  // for fireflies
    bool is_noturnal = false;
};

// ---- Plant manager ----
class PlantSystem {
public:
    std::vector<Plant> plants;
    static constexpr int MAX_PLANTS = 500;

    // Spawn plants based on biome when chunk loads
    void spawn_for_chunk(World& world, int chunk_x, BiomeType biome, uint32_t seed) {
        int base_x = chunk_x * CHUNK_W;
        // Find surface for each column
        for (int lx = 0; lx < CHUNK_W; lx += 2) {  // every 2 blocks
            int wx = base_x + lx;
            // Find surface
            int surface_y = -1;
            for (int y = 0; y < CHUNK_H; y++) {
                Block* b = world.get_block(wx, y);
                if (b && b->is_solid() && b->type != BlockType::AIR) {
                    surface_y = y;
                    break;
                }
            }
            if (surface_y < 0) continue;

            Block* surface_block = world.get_block(wx, surface_y);
            if (!surface_block) continue;

            // Pseudo-random based on position + seed
            uint32_t r = hash_pos(wx, surface_y, seed);
            float chance = (r % 1000) / 1000.0f;

            PlantType ptype = PlantType::GRASS_TUFT;
            float growth_rate = 0.005f;
            uint16_t yield = 0;
            uint8_t yield_count = 0;

            switch (biome) {
                case BiomeType::FOREST:
                    if (chance < 0.3f) { ptype = PlantType::GRASS_TUFT; growth_rate = 0.01f; }
                    else if (chance < 0.5f) { ptype = PlantType::FLOWER_RED; yield = 0x0125; yield_count = 1; }
                    else if (chance < 0.65f) { ptype = PlantType::FLOWER_YELLOW; yield = 0x0125; yield_count = 1; }
                    else if (chance < 0.75f) { ptype = PlantType::BUSH_BERRY; yield = 0x0125; yield_count = 2; growth_rate = 0.003f; }
                    else if (chance < 0.82f) { ptype = PlantType::MUSHROOM; yield = 0x0127; yield_count = 1; }
                    else continue;
                    break;
                case BiomeType::PLAINS:
                    if (chance < 0.5f) { ptype = PlantType::GRASS_TUFT; growth_rate = 0.012f; }
                    else if (chance < 0.7f) { ptype = PlantType::FLOWER_BLUE; yield = 0x0125; yield_count = 1; }
                    else if (chance < 0.8f) { ptype = PlantType::FLOWER_YELLOW; yield = 0x0125; yield_count = 1; }
                    else if (chance < 0.85f) { ptype = PlantType::BUSH_BERRY; yield = 0x0125; yield_count = 2; growth_rate = 0.003f; }
                    else continue;
                    break;
                case BiomeType::DESERT:
                    if (chance < 0.1f) { ptype = PlantType::CACTUS; growth_rate = 0.001f; }
                    else if (chance < 0.15f) { ptype = PlantType::DEAD_BUSH; yield = 0x0100; yield_count = 1; }
                    else continue;
                    break;
                case BiomeType::TUNDRA:
                    if (chance < 0.05f) { ptype = PlantType::DEAD_BUSH; yield = 0x0100; yield_count = 1; }
                    else continue;
                    break;
                case BiomeType::VOLCANIC:
                    if (chance < 0.05f) { ptype = PlantType::MUSHROOM_GLOW; yield = 0x0127; yield_count = 1; }
                    else continue;
                    break;
                case BiomeType::CRYSTAL_CAVE:
                    if (chance < 0.1f) { ptype = PlantType::CRYSTAL_FLOWER; yield = 0x0121; yield_count = 1; growth_rate = 0.002f; }
                    else continue;
                    break;
                default:
                    continue;
            }

            if ((int)plants.size() >= MAX_PLANTS) break;
            Plant p;
            p.type = ptype;
            p.x = wx * 16.0f + 8;
            p.y = (surface_y - 1) * 16.0f;
            p.growth = 0.3f + (r % 70) / 100.0f;
            p.growth_rate = growth_rate;
            p.yield_item_id = yield;
            p.yield_count = yield_count;
            p.harvestable = (yield != 0);
            p.harvest_timer = 5.0f + (r % 100) / 10.0f;
            plants.push_back(p);
        }
    }

    void update(float dt) {
        for (auto& p : plants) {
            if (p.growth < 1.0f) {
                p.growth += p.growth_rate * dt;
                if (p.growth > 1.0f) p.growth = 1.0f;
            }
            if (p.harvestable && p.harvest_timer > 0) {
                p.harvest_timer -= dt;
            }
        }
        // Remove plants too far away (cleanup)
        // (caller should handle based on player position)
    }

    // Try to harvest a plant near position
    bool try_harvest(float x, float y, uint16_t& out_item, uint8_t& out_count) {
        for (auto& p : plants) {
            float dx = p.x - x;
            float dy = p.y - y;
            if (dx*dx + dy*dy < 400) {  // within 20px
                if (p.harvestable && p.harvest_timer <= 0 && p.growth >= 0.5f) {
                    out_item = p.yield_item_id;
                    out_count = p.yield_count;
                    p.harvest_timer = 30.0f;  // 30 sec to regrow
                    p.growth = 0.3f;
                    return true;
                }
            }
        }
        return false;
    }

    void cull_far(float player_x, float max_dist = 1500) {
        plants.erase(std::remove_if(plants.begin(), plants.end(),
            [&](const Plant& p) {
                return std::abs(p.x - player_x) > max_dist;
            }), plants.end());
    }

    // Get plant color for rendering
    static void get_color(PlantType t, float& r, float& g, float& b) {
        switch (t) {
            case PlantType::GRASS_TUFT:    r=0.4f; g=0.7f; b=0.2f; break;
            case PlantType::FLOWER_RED:    r=0.9f; g=0.2f; b=0.2f; break;
            case PlantType::FLOWER_YELLOW: r=1.0f; g=0.9f; b=0.2f; break;
            case PlantType::FLOWER_BLUE:   r=0.3f; g=0.5f; b=0.9f; break;
            case PlantType::MUSHROOM:      r=0.7f; g=0.5f; b=0.3f; break;
            case PlantType::BUSH_BERRY:    r=0.2f; g=0.5f; b=0.2f; break;
            case PlantType::MUSHROOM_GLOW: r=0.4f; g=0.9f; b=0.6f; break;
            case PlantType::CACTUS:        r=0.3f; g=0.5f; b=0.2f; break;
            case PlantType::CRYSTAL_FLOWER:r=0.6f; g=0.8f; b=1.0f; break;
            case PlantType::DEAD_BUSH:     r=0.5f; g=0.4f; b=0.2f; break;
            default: r=0.5f; g=0.5f; b=0.5f; break;
        }
    }

    // Get plant size (width, height in pixels)
    static void get_size(PlantType t, float& w, float& h) {
        switch (t) {
            case PlantType::GRASS_TUFT:    w=8;  h=6;  break;
            case PlantType::FLOWER_RED:    w=6;  h=10; break;
            case PlantType::FLOWER_YELLOW: w=6;  h=10; break;
            case PlantType::FLOWER_BLUE:   w=6;  h=10; break;
            case PlantType::MUSHROOM:      w=6;  h=8;  break;
            case PlantType::BUSH_BERRY:    w=14; h=10; break;
            case PlantType::MUSHROOM_GLOW: w=6;  h=8;  break;
            case PlantType::CACTUS:        w=10; h=20; break;
            case PlantType::CRYSTAL_FLOWER:w=8;  h=12; break;
            case PlantType::DEAD_BUSH:     w=8;  h=6;  break;
            default: w=8; h=8; break;
        }
    }

    static bool is_glowing(PlantType t) {
        return t == PlantType::MUSHROOM_GLOW || t == PlantType::CRYSTAL_FLOWER;
    }

    static uint32_t hash_pos(int x, int y, uint32_t seed) {
        uint32_t h = seed;
        h ^= (uint32_t)x * 2654435761u;
        h ^= (uint32_t)y * 40503u;
        h ^= h >> 16;
        h *= 0x85ebca6bu;
        h ^= h >> 13;
        return h;
    }
};

// ---- Wildlife manager ----
class WildlifeSystem {
public:
    std::vector<Wildlife> animals;
    static constexpr int MAX_WILDLIFE = 30;

    void spawn_for_biome(World& world, BiomeType biome, float center_x, float center_y, uint32_t seed) {
        if ((int)animals.size() >= MAX_WILDLIFE) return;
        int count_to_spawn = 1 + (rand() % 3);
        for (int i = 0; i < count_to_spawn && (int)animals.size() < MAX_WILDLIFE; i++) {
            WildlifeType wtype;
            float r = (float)rand() / RAND_MAX;
            bool is_nocturnal = false;
            float speed = 50;

            switch (biome) {
                case BiomeType::FOREST:
                    if (r < 0.4f) { wtype = WildlifeType::BIRD; speed = 80; }
                    else if (r < 0.6f) { wtype = WildlifeType::BUTTERFLY; speed = 40; }
                    else if (r < 0.8f) { wtype = WildlifeType::RABBIT_WILD; speed = 70; }
                    else if (r < 0.95f) { wtype = WildlifeType::FOX; speed = 90; }
                    else { wtype = WildlifeType::BEE; speed = 60; }
                    break;
                case BiomeType::PLAINS:
                    if (r < 0.5f) { wtype = WildlifeType::BIRD; speed = 80; }
                    else if (r < 0.7f) { wtype = WildlifeType::BUTTERFLY; speed = 40; }
                    else if (r < 0.9f) { wtype = WildlifeType::RABBIT_WILD; speed = 70; }
                    else { wtype = WildlifeType::BEE; speed = 60; }
                    break;
                case BiomeType::OCEAN:
                case BiomeType::BEACH:
                    if (r < 0.7f) { wtype = WildlifeType::FISH; speed = 50; }
                    else { wtype = WildlifeType::BIRD; speed = 80; }
                    break;
                case BiomeType::DESERT:
                    if (r < 0.6f) { wtype = WildlifeType::RABBIT_WILD; speed = 80; }
                    else { wtype = WildlifeType::BIRD; speed = 90; }
                    break;
                case BiomeType::TUNDRA:
                    if (r < 0.5f) { wtype = WildlifeType::FOX; speed = 80; }
                    else { wtype = WildlifeType::BIRD; speed = 70; }
                    break;
                default:
                    if (r < 0.5f) { wtype = WildlifeType::FIREFLY; speed = 30; is_nocturnal = true; }
                    else { wtype = WildlifeType::BIRD; speed = 60; }
                    break;
            }

            Wildlife w;
            w.type = wtype;
            w.x = center_x + ((float)(rand()%400) - 200);
            w.y = center_y + ((float)(rand()%200) - 100);
            w.home_x = (uint16_t)(w.x / 16);
            w.home_y = (uint16_t)(w.y / 16);
            w.speed = speed;
            w.wander_timer = (float)(rand()%100)/10.0f;
            w.is_noturnal = is_nocturnal;
            w.glow_phase = (float)(rand()%628)/100.0f;
            animals.push_back(w);
        }
    }

    void update(float dt, float time_of_day, float player_x, float player_y) {
        float daylight = std::sin(time_of_day * M_PI);
        daylight = std::max(0.0f, daylight);
        bool is_night = daylight < 0.3f;

        for (auto& w : animals) {
            // Skip nocturnal animals during day, diurnal at night
            if (w.is_noturnal && !is_night) { w.active = false; continue; }
            if (!w.is_noturnal && is_night && w.type != WildlifeType::FIREFLY) { w.active = false; continue; }
            w.active = true;

            w.glow_phase += dt * 3;

            // Resting behavior
            if (w.resting) {
                w.rest_timer -= dt;
                if (w.rest_timer <= 0) {
                    w.resting = false;
                    w.wander_timer = 0;
                }
                continue;
            }

            // Wander AI
            w.wander_timer -= dt;
            if (w.wander_timer <= 0) {
                // Pick new wander target
                float range = 100;
                w.wander_target_x = w.x + ((float)(rand()%200) - 100);
                if (w.type == WildlifeType::BIRD || w.type == WildlifeType::BUTTERFLY ||
                    w.type == WildlifeType::BEE || w.type == WildlifeType::FIREFLY) {
                    w.wander_target_y = w.y + ((float)(rand()%100) - 50);
                } else {
                    // Ground animals - keep on ground
                    w.wander_target_y = w.y;
                }
                w.wander_timer = 2.0f + (float)(rand()%30)/10.0f;
                // Sometimes rest
                if (rand() % 100 < 20) {
                    w.resting = true;
                    w.rest_timer = 1.0f + (float)(rand()%30)/10.0f;
                }
            }

            // Move toward target
            float dx = w.wander_target_x - w.x;
            float dy = w.wander_target_y - w.y;
            float dist = std::sqrt(dx*dx + dy*dy);
            if (dist > 2) {
                w.vx = (dx/dist) * w.speed;
                w.vy = (dy/dist) * w.speed;
                w.x += w.vx * dt;
                w.y += w.vy * dt;
            } else {
                w.vx = 0; w.vy = 0;
            }

            // Birds flap (slight Y oscillation)
            if (w.type == WildlifeType::BIRD) {
                w.y += sin(w.glow_phase * 2) * 0.5f;
            }
            // Fireflies glow
            if (w.type == WildlifeType::FIREFLY) {
                w.y += sin(w.glow_phase) * 0.3f;
                w.x += cos(w.glow_phase * 0.7f) * 0.3f;
            }
        }

        // Remove wildlife too far from player
        animals.erase(std::remove_if(animals.begin(), animals.end(),
            [&](const Wildlife& w) {
                float dx = w.x - player_x;
                float dy = w.y - player_y;
                return (dx*dx + dy*dy) > 2500*2500;  // > 2500px away
            }), animals.end());
    }

    // Get wildlife color
    static void get_color(WildlifeType t, float& r, float& g, float& b) {
        switch (t) {
            case WildlifeType::BIRD:       r=0.4f; g=0.5f; b=0.7f; break;
            case WildlifeType::FISH:       r=0.5f; g=0.7f; b=0.9f; break;
            case WildlifeType::BUTTERFLY:  r=0.9f; g=0.5f; b=0.7f; break;
            case WildlifeType::FIREFLY:    r=1.0f; g=0.9f; b=0.3f; break;
            case WildlifeType::BEE:        r=0.9f; g=0.7f; b=0.1f; break;
            case WildlifeType::RABBIT_WILD:r=0.7f; g=0.6f; b=0.5f; break;
            case WildlifeType::FOX:        r=0.8f; g=0.4f; b=0.2f; break;
            default: r=0.5f; g=0.5f; b=0.5f; break;
        }
    }

    static void get_size(WildlifeType t, float& w, float& h) {
        switch (t) {
            case WildlifeType::BIRD:       w=8;  h=6;  break;
            case WildlifeType::FISH:       w=10; h=4;  break;
            case WildlifeType::BUTTERFLY:  w=8;  h=6;  break;
            case WildlifeType::FIREFLY:    w=3;  h=3;  break;
            case WildlifeType::BEE:        w=4;  h=3;  break;
            case WildlifeType::RABBIT_WILD:w=10; h=8;  break;
            case WildlifeType::FOX:        w=14; h=8;  break;
            default: w=6; h=6; break;
        }
    }

    static bool is_glowing(WildlifeType t) {
        return t == WildlifeType::FIREFLY;
    }
};

} // namespace krono
