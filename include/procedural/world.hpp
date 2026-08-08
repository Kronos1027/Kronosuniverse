#pragma once
// KronoUniverse — Chunk System (Fase 2, Parte 7.3)
//
// Streaming de terreno em chunks. Cada chunk é uma grade de blocos.
// Chunks são gerados proceduralmente sob demanda e descarregados quando
// fora do raio de renderização (deltas são salvos antes de descarregar).
//
// Arquitetura:
// - World: gerencia todos os chunks ativos, coordenadas de chunk, streaming
// - Chunk: grade CHUNK_W x CHUNK_H de blocos
// - Block: tipo, HP, suporte estrutural
// - ChunkManager: carrega/descarrega chunks conforme jogador se move

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <cstring>
#include <cmath>
#include "FastNoiseLite.h"

namespace krono {

// ---- Block types ----
enum class BlockType : uint16_t {
    AIR = 0,
    DIRT,
    GRASS,
    STONE,
    SAND,
    WOOD,
    LEAVES,
    METAL,
    ICE,
    LAVA,
    WATER,
    BEDROCK,   // indestructible
    CRYSTAL,   // valuable resource
    ANCIENT,   // ancient ruins material
};

// ---- Block flags ----
enum BlockFlags : uint16_t {
    FLAG_SOLID      = 1 << 0,
    FLAG_OPAQUE     = 1 << 1,
    FLAG_LIQUID     = 1 << 2,
    FLAG_ANCHOR     = 1 << 3,  // structural anchor (connected to bedrock/ground)
    FLAG_FLAMMABLE  = 1 << 4,
    FLAG_CONDUCTOR  = 1 << 5,  // conducts electricity (Parte 5.6)
};

// ---- Block instance (one per cell in chunk) ----
struct Block {
    BlockType type = BlockType::AIR;
    uint16_t flags = 0;
    uint8_t hp = 0;          // current HP (0 = destroyed)
    uint8_t max_hp = 0;      // max HP for this material
    uint8_t light_level = 0; // 0-15, for dynamic lighting

    bool is_air() const { return type == BlockType::AIR; }
    bool is_solid() const { return flags & FLAG_SOLID; }
    bool is_liquid() const { return flags & FLAG_LIQUID; }
    bool is_opaque() const { return flags & FLAG_OPAQUE; }
    bool is_anchor() const { return flags & FLAG_ANCHOR; }
};

// ---- Block type properties ----
struct BlockProperties {
    const char* name;
    uint16_t flags;
    uint8_t max_hp;
    uint8_t r, g, b;  // base color (pixel art)
    float load_capacity;  // structural: how much weight it can hold above (Parte 5.3)
    float self_weight;    // structural: its own weight
    float friction;       // surface friction when standing on it (Parte 5.1)
    int mining_tier;      // tool tier required to mine (0 = hand, 1 = stone pick, etc.)
    float energy_value;   // energy extracted when processed (Parte 5.6)
};

// ---- Block properties table ----
inline const BlockProperties& get_block_props(BlockType type) {
    static const BlockProperties props[] = {
        {"Air",      0,                                          0,   0,   0,   0,   0,   0,    0.01f, 0, 0},      // AIR
        {"Dirt",     FLAG_SOLID|FLAG_OPAQUE,                    30,  120,  80,  50,  50,   5,    0.80f, 0, 1},      // DIRT
        {"Grass",    FLAG_SOLID|FLAG_OPAQUE,                    25,  100, 160,  60,  40,   5,    0.75f, 0, 1},      // GRASS
        {"Stone",    FLAG_SOLID|FLAG_OPAQUE,                    80,  130, 130, 140, 200,  15,    0.70f, 1, 2},      // STONE
        {"Sand",     FLAG_SOLID|FLAG_OPAQUE,                    20,  200, 180, 100,  30,   3,    0.85f, 0, 0},      // SAND
        {"Wood",     FLAG_SOLID|FLAG_OPAQUE|FLAG_FLAMMABLE,     50,  140, 100,  60, 150,  10,    0.72f, 0, 3},      // WOOD
        {"Leaves",   FLAG_SOLID|FLAG_FLAMMABLE,                 10,   80, 140,  50,  10,   1,    0.60f, 0, 0},      // LEAVES
        {"Metal",    FLAG_SOLID|FLAG_OPAQUE|FLAG_CONDUCTOR,    150,  180, 180, 200, 400,  30,    0.60f, 2, 5},      // METAL
        {"Ice",      FLAG_SOLID|FLAG_OPAQUE,                    40,  180, 220, 240,  80,   8,    0.05f, 0, 0},      // ICE
        {"Lava",     FLAG_LIQUID,                               10,  220,  80,  20,   0,   0,    0.30f, 3, 10},     // LAVA
        {"Water",    FLAG_LIQUID,                                0,   60, 100, 180,   0,   0,    0.30f, 0, 0},      // WATER
        {"Bedrock",  FLAG_SOLID|FLAG_OPAQUE|FLAG_ANCHOR,       255,   60,  60,  60, 999, 999,    0.80f, 99, 0},     // BEDROCK
        {"Crystal",  FLAG_SOLID|FLAG_OPAQUE,                    60,  150, 200, 255, 300,  20,    0.50f, 1, 20},     // CRYSTAL
        {"Ancient",  FLAG_SOLID|FLAG_OPAQUE|FLAG_CONDUCTOR,    120,  100,  80, 140, 350,  25,    0.65f, 2, 50},     // ANCIENT
    };
    return props[static_cast<int>(type)];
}

// ---- Chunk dimensions ----
constexpr int CHUNK_W = 64;   // blocks wide
constexpr int CHUNK_H = 128;  // blocks tall
constexpr int BLOCK_SIZE = 16; // pixels per block (render)

// ---- Chunk coordinate ----
struct ChunkCoord {
    int32_t x, y;
    bool operator==(const ChunkCoord& o) const { return x == o.x && y == o.y; }
};

struct ChunkCoordHash {
    size_t operator()(const ChunkCoord& c) const {
        return std::hash<int64_t>()((int64_t)c.x * 73856093 ^ (int64_t)c.y * 19349663);
    }
};

// ---- Chunk ----
struct Chunk {
    ChunkCoord coord;
    Block blocks[CHUNK_W * CHUNK_H];
    bool generated = false;
    bool modified = false;  // player has changed blocks (need to save delta)
    bool dirty = true;      // needs render update

    Block& get(int bx, int by) {
        return blocks[by * CHUNK_W + bx];
    }
    const Block& get(int bx, int by) const {
        return blocks[by * CHUNK_W + bx];
    }

    // Convert world block coords to chunk-local coords
    static void world_to_local(int world_bx, int world_by, int& chunk_x, int& chunk_y, int& local_bx, int& local_by) {
        chunk_x = (world_bx >= 0) ? world_bx / CHUNK_W : (world_bx - CHUNK_W + 1) / CHUNK_W;
        chunk_y = (world_by >= 0) ? world_by / CHUNK_H : (world_by - CHUNK_H + 1) / CHUNK_H;
        local_bx = world_bx - chunk_x * CHUNK_W;
        local_by = world_by - chunk_y * CHUNK_H;
    }
};

// ---- Biome types (determined by temperature + humidity from noise) ----
enum class BiomeType : uint8_t {
    OCEAN,
    BEACH,
    PLAINS,
    FOREST,
    DESERT,
    TUNDRA,
    MOUNTAIN,
    VOLCANIC,
    CRYSTAL_CAVE,
};

struct Biome {
    BiomeType type;
    float temperature;  // 0 = frozen, 1 = scorching
    float humidity;     // 0 = dry, 1 = flooded
    const char* name;
};

// ---- World (manages all chunks) ----
class World {
public:
    World(uint32_t seed) : seed_(seed) {
        // Initialize noise generators
        terrain_noise_ = fnlCreateState();
        terrain_noise_.seed = seed;
        terrain_noise_.frequency = 0.008f;
        terrain_noise_.noise_type = FNL_NOISE_PERLIN;
        terrain_noise_.fractal_type = FNL_FRACTAL_FBM;
        terrain_noise_.octaves = 5;

        cave_noise_ = fnlCreateState();
        cave_noise_.seed = seed + 1;
        cave_noise_.frequency = 0.03f;
        cave_noise_.noise_type = FNL_NOISE_PERLIN;
        cave_noise_.fractal_type = FNL_FRACTAL_RIDGED;

        temp_noise_ = fnlCreateState();
        temp_noise_.seed = seed + 2;
        temp_noise_.frequency = 0.002f;
        temp_noise_.noise_type = FNL_NOISE_PERLIN;

        humid_noise_ = fnlCreateState();
        humid_noise_.seed = seed + 3;
        humid_noise_.frequency = 0.003f;
        humid_noise_.noise_type = FNL_NOISE_PERLIN;

        ore_noise_ = fnlCreateState();
        ore_noise_.seed = seed + 4;
        ore_noise_.frequency = 0.05f;
        ore_noise_.noise_type = FNL_NOISE_CELLULAR;
        ore_noise_.cellular_return_type = FNL_CELLULAR_RETURN_TYPE_DISTANCE;
    }

    // Get or load a chunk at chunk coordinates
    Chunk* get_chunk(int cx, int cy) {
        ChunkCoord coord{cx, cy};
        auto it = chunks_.find(coord);
        if (it != chunks_.end()) {
            return &it->second;
        }
        // Generate new chunk
        Chunk& chunk = chunks_[coord];
        chunk.coord = coord;
        generate_chunk(chunk);
        return &chunk;
    }

    // Get a block at world coordinates
    Block* get_block(int world_bx, int world_by) {
        int cx, cy, lx, ly;
        Chunk::world_to_local(world_bx, world_by, cx, cy, lx, ly);
        Chunk* chunk = get_chunk(cx, cy);
        if (!chunk) return nullptr;
        return &chunk->get(lx, ly);
    }

    // Set a block at world coordinates (returns true if successful)
    bool set_block(int world_bx, int world_by, BlockType type) {
        int cx, cy, lx, ly;
        Chunk::world_to_local(world_bx, world_by, cx, cy, lx, ly);
        Chunk* chunk = get_chunk(cx, cy);
        if (!chunk) return false;

        Block& block = chunk->get(lx, ly);
        block.type = type;
        const auto& props = get_block_props(type);
        block.flags = props.flags;
        block.hp = props.max_hp;
        block.max_hp = props.max_hp;
        chunk->modified = true;
        chunk->dirty = true;

        // Mark neighbor chunks dirty if on border
        if (lx == 0) mark_chunk_dirty(cx - 1, cy);
        if (lx == CHUNK_W - 1) mark_chunk_dirty(cx + 1, cy);
        if (ly == 0) mark_chunk_dirty(cx, cy - 1);
        if (ly == CHUNK_H - 1) mark_chunk_dirty(cx, cy + 1);

        return true;
    }

    // Destroy a block (mining/explosion)
    bool destroy_block(int world_bx, int world_by) {
        Block* block = get_block(world_bx, world_by);
        if (!block || block->is_air()) return false;
        block->type = BlockType::AIR;
        block->flags = 0;
        block->hp = 0;

        int cx, cy, lx, ly;
        Chunk::world_to_local(world_bx, world_by, cx, cy, lx, ly);
        if (auto* c = get_chunk(cx, cy)) {
            c->modified = true;
            c->dirty = true;
        }
        return true;
    }

    // Damage a block (partial mining)
    bool damage_block(int world_bx, int world_by, uint8_t damage) {
        Block* block = get_block(world_bx, world_by);
        if (!block || block->is_air()) return false;
        if (block->hp <= damage) {
            return destroy_block(world_bx, world_by);
        }
        block->hp -= damage;

        int cx, cy, lx, ly;
        Chunk::world_to_local(world_bx, world_by, cx, cy, lx, ly);
        if (auto* c = get_chunk(cx, cy)) {
            c->dirty = true;
        }
        return false;
    }

    // Get biome at world coordinates
    Biome get_biome(int world_bx, int world_by) {
        float temp = fnlGetNoise2D(&temp_noise_, (float)world_bx, 0);
        float humid = fnlGetNoise2D(&humid_noise_, 0, (float)world_bx);
        temp = (temp + 1.0f) * 0.5f;  // [0, 1]
        humid = (humid + 1.0f) * 0.5f;

        Biome biome;
        biome.temperature = temp;
        biome.humidity = humid;

        if (temp < 0.2f) {
            biome.type = BiomeType::TUNDRA;
            biome.name = "Tundra";
        } else if (temp > 0.8f && humid < 0.3f) {
            biome.type = BiomeType::DESERT;
            biome.name = "Desert";
        } else if (temp > 0.85f && humid > 0.5f) {
            biome.type = BiomeType::VOLCANIC;
            biome.name = "Volcanic";
        } else if (humid > 0.7f) {
            biome.type = BiomeType::FOREST;
            biome.name = "Forest";
        } else if (humid > 0.4f) {
            biome.type = BiomeType::PLAINS;
            biome.name = "Plains";
        } else {
            biome.type = BiomeType::PLAINS;
            biome.name = "Plains";
        }
        return biome;
    }

    // Unload chunks outside radius (streaming — Parte 7.3)
    void unload_distant(int center_cx, int center_cy, int radius) {
        std::vector<ChunkCoord> to_remove;
        for (auto& [coord, chunk] : chunks_) {
            int dx = coord.x - center_cx;
            int dy = coord.y - center_cy;
            if (dx * dx + dy * dy > radius * radius) {
                // TODO: save delta before unloading
                to_remove.push_back(coord);
            }
        }
        for (auto& coord : to_remove) {
            chunks_.erase(coord);
        }
    }

    size_t loaded_chunks() const { return chunks_.size(); }
    uint32_t seed() const { return seed_; }

    fnl_state& terrain_noise() { return terrain_noise_; }

private:
    uint32_t seed_;
    std::unordered_map<ChunkCoord, Chunk, ChunkCoordHash> chunks_;

    fnl_state terrain_noise_;
    fnl_state cave_noise_;
    fnl_state temp_noise_;
    fnl_state humid_noise_;
    fnl_state ore_noise_;

    void mark_chunk_dirty(int cx, int cy) {
        ChunkCoord coord{cx, cy};
        auto it = chunks_.find(coord);
        if (it != chunks_.end()) {
            it->second.dirty = true;
        }
    }

    // Generate chunk terrain (Parte 7.3 — Camada 2)
    void generate_chunk(Chunk& chunk) {
        int base_x = chunk.coord.x * CHUNK_W;
        int base_y = chunk.coord.y * CHUNK_H;

        Biome biome = get_biome(base_x, 0);

        for (int lx = 0; lx < CHUNK_W; lx++) {
            int world_x = base_x + lx;

            // Surface height from terrain noise
            float surface_noise = fnlGetNoise2D(&terrain_noise_, (float)world_x, 0);
            int surface_height = (int)(surface_noise * 30 + 64); // around y=64 ± 30

            for (int ly = 0; ly < CHUNK_H; ly++) {
                int world_y = base_y + ly;
                Block& block = chunk.get(lx, ly);

                if (world_y < surface_height) {
                    // Above surface — air
                    block.type = BlockType::AIR;
                    block.flags = 0;
                    block.hp = 0;
                } else if (world_y == surface_height) {
                    // Surface layer
                    block.type = (biome.type == BiomeType::DESERT) ? BlockType::SAND : BlockType::GRASS;
                    const auto& props = get_block_props(block.type);
                    block.flags = props.flags;
                    block.hp = props.max_hp;
                    block.max_hp = props.max_hp;
                } else if (world_y < surface_height + 5) {
                    // Just below surface — dirt
                    block.type = BlockType::DIRT;
                    const auto& props = get_block_props(block.type);
                    block.flags = props.flags;
                    block.hp = props.max_hp;
                    block.max_hp = props.max_hp;
                } else if (world_y < CHUNK_H - 5) {
                    // Underground — stone with possible ores/caves
                    float cave_val = fnlGetNoise2D(&cave_noise_, (float)world_x, (float)world_y);
                    if (cave_val > 0.4f) {
                        // Cave — air
                        block.type = BlockType::AIR;
                        block.flags = 0;
                        block.hp = 0;
                    } else {
                        // Check for ore deposits
                        float ore_val = fnlGetNoise2D(&ore_noise_, (float)world_x, (float)world_y);
                        if (ore_val > 0.7f && world_y > surface_height + 20) {
                            // Deep crystal deposit
                            block.type = BlockType::CRYSTAL;
                        } else if (ore_val > 0.6f && world_y > surface_height + 15) {
                            // Metal deposit
                            block.type = BlockType::METAL;
                        } else if (biome.type == BiomeType::VOLCANIC && world_y > surface_height + 10 && ore_val > 0.5f) {
                            // Lava pockets in volcanic biome
                            block.type = BlockType::LAVA;
                        } else {
                            block.type = BlockType::STONE;
                        }
                        const auto& props = get_block_props(block.type);
                        block.flags = props.flags;
                        block.hp = props.max_hp;
                        block.max_hp = props.max_hp;
                    }
                } else {
                    // Bedrock layer (bottom of world)
                    block.type = BlockType::BEDROCK;
                    const auto& props = get_block_props(block.type);
                    block.flags = props.flags;
                    block.hp = props.max_hp;
                    block.max_hp = props.max_hp;
                }
            }
        }

        // Add trees in forest/plains biomes
        if (biome.type == BiomeType::FOREST || biome.type == BiomeType::PLAINS) {
            for (int lx = 2; lx < CHUNK_W - 2; lx++) {
                int world_x = base_x + lx;
                // Pseudo-random tree placement
                float tree_noise = fnlGetNoise2D(&ore_noise_, (float)world_x * 3.7f, 999.0f);
                if (tree_noise > 0.5f) {
                    // Find surface
                    for (int ly = CHUNK_H - 1; ly >= 0; ly--) {
                        Block& b = chunk.get(lx, ly);
                        if (b.type == BlockType::GRASS) {
                            // Plant tree
                            int tree_height = 4 + (int)(tree_noise * 4);
                            for (int h = 1; h <= tree_height; h++) {
                                if (ly - h >= 0) {
                                    Block& trunk = chunk.get(lx, ly - h);
                                    trunk.type = BlockType::WOOD;
                                    const auto& props = get_block_props(BlockType::WOOD);
                                    trunk.flags = props.flags;
                                    trunk.hp = props.max_hp;
                                    trunk.max_hp = props.max_hp;
                                }
                            }
                            // Leaves
                            for (int dx = -2; dx <= 2; dx++) {
                                for (int dy = -2; dy <= 1; dy++) {
                                    int tx = lx + dx;
                                    int ty = ly - tree_height + dy;
                                    if (tx >= 0 && tx < CHUNK_W && ty >= 0 && ty < CHUNK_H) {
                                        Block& leaf = chunk.get(tx, ty);
                                        if (leaf.is_air()) {
                                            leaf.type = BlockType::LEAVES;
                                            const auto& props = get_block_props(BlockType::LEAVES);
                                            leaf.flags = props.flags;
                                            leaf.hp = props.max_hp;
                                            leaf.max_hp = props.max_hp;
                                        }
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }

        // Ice in tundra
        if (biome.type == BiomeType::TUNDRA) {
            for (int lx = 0; lx < CHUNK_W; lx++) {
                for (int ly = 0; ly < CHUNK_H; ly++) {
                    Block& b = chunk.get(lx, ly);
                    if (b.type == BlockType::WATER) {
                        b.type = BlockType::ICE;
                        const auto& props = get_block_props(BlockType::ICE);
                        b.flags = props.flags;
                        b.hp = props.max_hp;
                        b.max_hp = props.max_hp;
                    }
                }
            }
        }

        chunk.generated = true;
        chunk.dirty = true;
    }
};

} // namespace krono
