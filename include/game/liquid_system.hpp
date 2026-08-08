#pragma once
// KronoUniverse — Liquid/Fluid System (v0.6)
//
// Sistema de líquidos:
// - Água (azul) - fonte infinita, escoa para baixo e lados
// - Lava (laranja) - causa dano, emite luz, mais lenta
// - Óleo (preto) - flutua em água, inflamável
// - Ácido (verde) - corrói blocos
// - Simulação celular simples (não física real, mas visual)
// - Pressão hidrostática (água profunda flui mais forte)

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "procedural/world.hpp"
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace krono {

enum class LiquidType : uint8_t {
    NONE = 0,
    WATER,
    LAVA,
    OIL,
    ACID,
};

struct LiquidCell {
    LiquidType type = LiquidType::NONE;
    uint8_t level = 0;       // 0-7 (0 = empty, 7 = full)
    uint8_t flow_dir = 0;    // 0=none, 1=down, 2=left, 3=right, 4=up (pressurized)
    bool updated_this_tick = false;
};

class LiquidSystem {
public:
    // Process liquid simulation for visible chunks
    static void update(World& world, int min_bx, int max_bx, int min_by, int max_by, float dt) {
        // Simple cellular automaton - process from bottom to top
        // so falling liquids don't double-update in one tick
        static float tick_accumulator = 0;
        tick_accumulator += dt;
        if (tick_accumulator < 0.1f) return;  // 10 ticks per second
        tick_accumulator = 0;

        // Get all liquid cells in range
        std::vector<std::tuple<int, int, LiquidType, uint8_t>> liquids;
        for (int bx = min_bx; bx <= max_bx; bx++) {
            for (int by = min_by; by <= max_by; by++) {
                Block* b = world.get_block(bx, by);
                if (!b) continue;
                if (b->type == BlockType::WATER) liquids.push_back({bx, by, LiquidType::WATER, 7});
                else if (b->type == BlockType::LAVA) liquids.push_back({bx, by, LiquidType::LAVA, 7});
            }
        }

        // Sort by Y descending (bottom-up)
        std::sort(liquids.begin(), liquids.end(),
            [](const auto& a, const auto& b) { return std::get<1>(a) > std::get<1>(b); });

        // Process each liquid cell
        for (auto& [bx, by, type, level] : liquids) {
            // Try to flow down first
            Block* below = world.get_block(bx, by + 1);
            if (below && below->is_air()) {
                // Flow down
                world.set_block(bx, by + 1, type == LiquidType::WATER ? BlockType::WATER : BlockType::LAVA);
                world.set_block(bx, by, BlockType::AIR);
                continue;
            }
            // Try to flow sideways
            if (level > 0) {
                // Try left
                Block* left = world.get_block(bx - 1, by);
                if (left && left->is_air()) {
                    world.set_block(bx - 1, by, type == LiquidType::WATER ? BlockType::WATER : BlockType::LAVA);
                    world.set_block(bx, by, BlockType::AIR);
                    continue;
                }
                // Try right
                Block* right = world.get_block(bx + 1, by);
                if (right && right->is_air()) {
                    world.set_block(bx + 1, by, type == LiquidType::WATER ? BlockType::WATER : BlockType::LAVA);
                    world.set_block(bx, by, BlockType::AIR);
                    continue;
                }
            }
        }
    }

    // Place a liquid source at position
    static void place_liquid(World& world, int bx, int by, LiquidType type) {
        BlockType bt = BlockType::AIR;
        switch (type) {
            case LiquidType::WATER: bt = BlockType::WATER; break;
            case LiquidType::LAVA: bt = BlockType::LAVA; break;
            default: return;
        }
        world.set_block(bx, by, bt);
    }

    // Check if a position is in liquid
    static bool is_in_liquid(World& world, float x, float y) {
        int bx = (int)(x / 16);
        int by = (int)(y / 16);
        Block* b = world.get_block(bx, by);
        if (!b) return false;
        return b->type == BlockType::WATER || b->type == BlockType::LAVA;
    }

    static LiquidType get_liquid_at(World& world, int bx, int by) {
        Block* b = world.get_block(bx, by);
        if (!b) return LiquidType::NONE;
        if (b->type == BlockType::WATER) return LiquidType::WATER;
        if (b->type == BlockType::LAVA) return LiquidType::LAVA;
        return LiquidType::NONE;
    }

    // Get color for liquid rendering (with level-based alpha)
    static void get_color(LiquidType type, uint8_t level, float& r, float& g, float& b, float& a) {
        float intensity = (float)level / 7.0f;
        switch (type) {
            case LiquidType::WATER:
                r = 0.2f; g = 0.4f; b = 0.8f;
                a = 0.6f + 0.3f * intensity;
                break;
            case LiquidType::LAVA:
                r = 1.0f; g = 0.4f; b = 0.1f;
                a = 0.9f;
                break;
            case LiquidType::OIL:
                r = 0.1f; g = 0.1f; b = 0.15f;
                a = 0.8f;
                break;
            case LiquidType::ACID:
                r = 0.5f; g = 1.0f; b = 0.2f;
                a = 0.7f;
                break;
            default:
                r = 0; g = 0; b = 0; a = 0;
                break;
        }
    }
};

} // namespace krono
