#pragma once
// KronoUniverse — Structural Integrity System (Fase 2, Parte 5.3)
//
// Grafo de suporte estrutural: cada bloco colocado é um nó.
// Arestas conectam blocos adjacentes ao "chão" (bedrock/âncora).
// BFS periódica: blocos sem caminho até âncora entram em queda livre.
//
// Carga por material: metal aguenta mais peso que madeira.
// Destruição em combate recalcula grafo — "destruir pilar central" surge naturalmente.

#include "engine/ecs.hpp"
#include "procedural/world.hpp"
#include <queue>
#include <set>
#include <vector>

namespace krono {

class StructuralIntegritySystem {
public:
    // Recalculate structural support for a region around the modified block.
    // Called after block placement/destruction (not every frame).
    //
    // Algorithm (Parte 5.3):
    // 1. Find all anchor blocks (bedrock) in the region
    // 2. BFS from each anchor — mark all reachable blocks as "supported"
    // 3. Any solid block NOT marked as "supported" enters "falling" state
    // 4. Falling blocks get FLAG_ANCHOR cleared + are converted to physics entities
    void check_region(World& world, int center_x, int center_y, int radius = 16) {
        int min_x = center_x - radius;
        int max_x = center_x + radius;
        int min_y = center_y - radius;
        int max_y = center_y + radius;

        // Collect all solid blocks in region + find anchors
        struct BlockRef {
            int x, y;
            bool supported;
            float accumulated_weight;
        };

        std::vector<BlockRef> blocks;
        std::queue<std::pair<int,int>> bfs_queue;

        // Phase 1: collect blocks + seed BFS with anchors
        for (int bx = min_x; bx <= max_x; bx++) {
            for (int by = min_y; by <= max_y; by++) {
                Block* block = world.get_block(bx, by);
                if (!block || !block->is_solid()) continue;

                blocks.push_back({bx, by, false, 0.0f});

                if (block->is_anchor()) {
                    // Bedrock = anchor — seed BFS
                    blocks.back().supported = true;
                    bfs_queue.push({bx, by});
                }
            }
        }

        // Create lookup map: (x,y) → index in blocks vector
        std::set<std::pair<int,int>> block_set;
        for (auto& b : blocks) {
            block_set.insert({b.x, b.y});
        }

        auto is_supported = [&](int x, int y) -> bool {
            for (auto& b : blocks) {
                if (b.x == x && b.y == y) return b.supported;
            }
            return false;
        };

        auto set_supported = [&](int x, int y) {
            for (auto& b : blocks) {
                if (b.x == x && b.y == y) {
                    b.supported = true;
                    return;
                }
            }
        };

        // Phase 2: BFS from anchors through adjacent solid blocks
        while (!bfs_queue.empty()) {
            auto [cx, cy] = bfs_queue.front();
            bfs_queue.pop();

            // Check 4 neighbors (up, down, left, right)
            int neighbors[4][2] = {{cx, cy-1}, {cx, cy+1}, {cx-1, cy}, {cx+1, cy}};
            for (auto& [nx, ny] : neighbors) {
                if (nx < min_x || nx > max_x || ny < min_y || ny > max_y) continue;

                Block* nb = world.get_block(nx, ny);
                if (!nb || !nb->is_solid()) continue;

                if (!is_supported(nx, ny)) {
                    set_supported(nx, ny);
                    bfs_queue.push({nx, ny});
                }
            }
        }

        // Phase 3: Mark unsupported blocks as falling
        int fallen_count = 0;
        for (auto& b : blocks) {
            if (!b.supported) {
                // This block has no path to an anchor — it should fall!
                // For now, just mark it (in full game, convert to physics entity)
                Block* block = world.get_block(b.x, b.y);
                if (block) {
                    // Visual indicator: reduce HP to show cracking (Parte 5.3: "rachadura")
                    block->hp = block->hp / 2;
                    fallen_count++;
                }
            }
        }

        if (fallen_count > 0) {
            // In full game: spawn falling block entities with physics
            // For now: just mark them
        }
    }

    // Check load capacity: can a block at (x,y) support the weight above it?
    // (Parte 5.3: "cada tipo de bloco tem capacidade_de_carga e peso_próprio")
    bool can_support_weight(World& world, int x, int y, float additional_weight) {
        Block* block = world.get_block(x, y);
        if (!block || !block->is_solid()) return false;

        const auto& props = get_block_props(block->type);

        // Calculate total weight above this block (column above)
        float total_weight = props.self_weight;
        for (int above = y - 1; above >= 0; above--) {
            Block* upper = world.get_block(x, above);
            if (!upper || upper->is_air()) break;
            if (upper->is_solid()) {
                const auto& upper_props = get_block_props(upper->type);
                total_weight += upper_props.self_weight;
            }
        }

        return (total_weight + additional_weight) <= props.load_capacity;
    }

    // Apply explosion damage (Parte 5.4: "força de impulso física aplicada a blocos próximos")
    void apply_explosion(World& world, int center_x, int center_y, int radius, uint8_t damage) {
        for (int dx = -radius; dx <= radius; dx++) {
            for (int dy = -radius; dy <= radius; dy++) {
                float dist = std::sqrt(dx * dx + dy * dy);
                if (dist > radius) continue;

                int bx = center_x + dx;
                int by = center_y + dy;

                // Damage falloff (Parte 5.4: dano_base * (1 - distância/raio_máximo))
                float falloff = 1.0f - (dist / radius);
                uint8_t scaled_damage = (uint8_t)(damage * falloff);

                Block* block = world.get_block(bx, by);
                if (!block || block->is_air()) continue;

                // Bedrock is indestructible
                if (block->type == BlockType::BEDROCK) continue;

                if (block->hp <= scaled_damage) {
                    world.destroy_block(bx, by);
                } else {
                    block->hp -= scaled_damage;
                }
            }
        }

        // Recalculate structural integrity in affected area
        check_region(world, center_x, center_y, radius + 8);
    }
};

} // namespace krono
