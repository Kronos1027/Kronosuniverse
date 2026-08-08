#pragma once
// KronoUniverse — Inventory & Item Drop System (v0.5)
//
// Sistema funcional de drops:
// - Cada bloco minado dá um item específico
// - Itens caem no chão como entidades físicas
// - Player coleta automaticamente ao passar por cima
// - Inventário visual com slots e contadores
// - Item drops têm física própria (gravidade, bounce)

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "procedural/world.hpp"
#include "game/inventory.hpp"
#include "game/combat_system.hpp"
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace krono {

// ---- Item Drop (entity in world) ----
struct ItemDrop {
    uint16_t item_id = 0;
    std::string item_name;
    int count = 1;
    float lifetime = 60.0f;  // despawn after 60 sec
    float elapsed = 0;
    float bob_phase = 0;     // floating animation
    bool collected = false;
    uint32_t attracts_to = 0;  // entity ID to attract to (player)
};

// ---- Maps block types to item IDs ----
// BlockType -> {item_id, count}
inline std::pair<uint16_t, uint8_t> block_to_item_drop(BlockType bt) {
    switch (bt) {
        case BlockType::DIRT:    return {0x0102, 1};   // Dirt block
        case BlockType::GRASS:   return {0x0102, 1};   // Dirt (grass drops as dirt)
        case BlockType::STONE:   return {0x0103, 1};   // Stone
        case BlockType::SAND:    return {0x0116, 1};   // Sand
        case BlockType::WOOD:    return {0x0100, 1};   // Wood log
        case BlockType::LEAVES:  return {0x0119, 1};   // Stick (sometimes sapling)
        case BlockType::METAL:   return {0x0110, 1};   // Iron ore
        case BlockType::ICE:     return {0x0115, 1};   // Ice
        case BlockType::LAVA:    return {0x0134, 1};   // Fire essence
        case BlockType::WATER:   return {0, 0};
        case BlockType::BEDROCK: return {0, 0};        // No drop
        case BlockType::CRYSTAL: return {0x0121, 1};   // Crystal
        case BlockType::ANCIENT: return {0x0122, 1};   // Ancient relic
        default: return {0, 0};
    }
}

// ---- Get item name from ID ----
inline const char* item_name(uint16_t id) {
    switch (id) {
        case 0x0100: return "Wood Log";
        case 0x0101: return "Wood Plank";
        case 0x0102: return "Dirt";
        case 0x0103: return "Stone";
        case 0x0104: return "Chest";
        case 0x0105: return "Door";
        case 0x0106: return "Ladder";
        case 0x0110: return "Iron Ore";
        case 0x0111: return "Iron Ingot";
        case 0x0113: return "Copper Ingot";
        case 0x0114: return "Steel Ingot";
        case 0x0115: return "Glass";
        case 0x0116: return "Sand";
        case 0x0117: return "Brick";
        case 0x0118: return "Clay";
        case 0x0119: return "Stick";
        case 0x0120: return "Leather";
        case 0x0121: return "Crystal";
        case 0x0122: return "Ancient Relic";
        case 0x0123: return "Wheat";
        case 0x0124: return "Raw Meat";
        case 0x0125: return "Apple";
        case 0x0126: return "Carrot";
        case 0x0127: return "Herb";
        case 0x0128: return "Bottle";
        case 0x0129: return "Mana Herb";
        case 0x0130: return "Strength Root";
        case 0x0131: return "Speed Leaf";
        case 0x0132: return "Invisibility Dust";
        case 0x0133: return "Antidote Plant";
        case 0x0134: return "Fire Essence";
        case 0x0135: return "Ice Shard";
        case 0x0136: return "Lightning Crystal";
        case 0x0137: return "Soul Gem";
        case 0x0201: return "Wooden Sword";
        case 0x0202: return "Wooden Pickaxe";
        case 0x0203: return "Wooden Axe";
        case 0x0204: return "Stone Sword";
        case 0x0205: return "Stone Pickaxe";
        case 0x0206: return "Iron Sword";
        case 0x0207: return "Iron Pickaxe";
        case 0x0208: return "Steel Sword";
        case 0x0209: return "Steel Pickaxe";
        case 0x0301: return "Bow";
        case 0x0302: return "Arrow";
        case 0x0303: return "Iron Arrow";
        case 0x0304: return "Gun";
        case 0x0305: return "Bullet";
        case 0x0306: return "Dagger";
        case 0x0307: return "War Hammer";
        case 0x0308: return "Spear";
        case 0x0401: return "Leather Cap";
        case 0x0402: return "Leather Tunic";
        case 0x0403: return "Iron Helmet";
        case 0x0404: return "Iron Chestplate";
        case 0x0405: return "Steel Helmet";
        case 0x0406: return "Steel Chestplate";
        case 0x0407: return "Crystal Crown";
        case 0x0408: return "Ancient Robe";
        case 0x0501: return "Bread";
        case 0x0502: return "Cooked Meat";
        case 0x0503: return "Apple Pie";
        case 0x0504: return "Stew";
        case 0x0601: return "Health Potion";
        case 0x0602: return "Mana Potion";
        case 0x0603: return "Strength Potion";
        case 0x0604: return "Speed Potion";
        case 0x0605: return "Invisibility Potion";
        case 0x0606: return "Antidote";
        case 0x0701: return "Energy Cell";
        case 0x0702: return "Jetpack";
        case 0x0703: return "Shield Generator";
        case 0x0704: return "Hover Boots";
        case 0x0705: return "Mining Drill";
        case 0x0706: return "Teleporter Beacon";
        case 0x0801: return "Fire Staff";
        case 0x0802: return "Ice Staff";
        case 0x0803: return "Lightning Staff";
        case 0x0804: return "Summoning Wand";
        case 0x0805: return "Fireball Scroll";
        case 0x0806: return "Heal Scroll";
        case 0x0901: return "Workbench";
        case 0x0902: return "Furnace";
        case 0x0903: return "Anvil";
        case 0x0904: return "Alchemy Table";
        case 0x0905: return "Altar";
        case 0x0906: return "High Tech Bench";
        default: return "Unknown";
    }
}

// ---- Get item color for rendering ----
inline void item_color(uint16_t id, float& r, float& g, float& b) {
    uint8_t cat = (id >> 8) & 0xFF;
    switch (cat) {
        case 0x01: r=0.5f; g=0.4f; b=0.3f; break;  // Materials - brown
        case 0x02: r=0.6f; g=0.6f; b=0.7f; break;  // Tools - gray
        case 0x03: r=0.8f; g=0.5f; b=0.3f; break;  // Weapons - orange-brown
        case 0x04: r=0.5f; g=0.6f; b=0.8f; break;  // Armor - blue-gray
        case 0x05: r=0.9f; g=0.6f; b=0.3f; break;  // Food - warm
        case 0x06: r=0.8f; g=0.3f; b=0.5f; break;  // Potions - pink
        case 0x07: r=0.3f; g=0.8f; b=1.0f; break;  // Tech - cyan
        case 0x08: r=0.7f; g=0.4f; b=1.0f; break;  // Magic - purple
        case 0x09: r=0.5f; g=0.5f; b=0.5f; break;  // Stations - gray
        default:   r=0.7f; g=0.7f; b=0.7f; break;
    }
    // Specific overrides
    switch (id) {
        case 0x0121: r=0.6f; g=0.8f; b=1.0f; break;  // Crystal
        case 0x0122: r=0.5f; g=0.4f; b=0.7f; break;  // Ancient
        case 0x0134: r=1.0f; g=0.5f; b=0.1f; break;  // Fire essence
        case 0x0135: r=0.5f; g=0.9f; b=1.0f; break;  // Ice shard
        case 0x0601: r=0.9f; g=0.2f; b=0.2f; break;  // Health potion
        case 0x0602: r=0.2f; g=0.3f; b=0.9f; break;  // Mana potion
        case 0x0801: r=1.0f; g=0.5f; b=0.2f; break;  // Fire staff
        case 0x0802: r=0.5f; g=0.9f; b=1.0f; break;  // Ice staff
    }
}

// ---- Item Drop System ----
class ItemDropSystem {
public:
    // Spawn a drop at position with given item
    static Entity spawn_drop(Registry& reg, float x, float y, uint16_t item_id, int count = 1,
                              float initial_vx = 0, float initial_vy = -100) {
        Entity e = reg.create();
        reg.emplace<Position>(e, Position{x, y});
        reg.emplace<Velocity>(e, Velocity{initial_vx, initial_vy});
        reg.emplace<Mass>(e, Mass{0.1f});
        reg.emplace<RigidBody>(e, RigidBody{0.3f, 0.5f, false, false});
        reg.emplace<AABBCollider>(e, AABBCollider{10, 10});

        ItemDrop drop;
        drop.item_id = item_id;
        drop.item_name = item_name(item_id);
        drop.count = count;
        drop.bob_phase = (float)(rand()%628) / 100.0f;
        reg.emplace<ItemDrop>(e, std::move(drop));
        return e;
    }

    // Spawn drop with random scatter (for mining)
    static void spawn_drop_scatter(Registry& reg, float x, float y, uint16_t item_id, int count = 1) {
        float vx = ((float)(rand()%100) - 50) * 2;
        float vy = -100 - (float)(rand()%100);
        spawn_drop(reg, x, y, item_id, count, vx, vy);
    }

    // Update drops: physics, lifetime, attraction to player
    // Returns list of (item_id, count) that were collected this frame
    static std::vector<std::pair<uint16_t, int>> update(Registry& reg, Entity player, float dt, World& world) {
        auto* ppos = reg.get<Position>(player);
        std::vector<Entity> to_destroy;
        std::vector<std::pair<uint16_t, int>> collected;

        reg.each<Position, Velocity, ItemDrop>([&](auto ent, Position& pos, Velocity& vel, ItemDrop& drop) {
            drop.elapsed += dt;
            drop.bob_phase += dt * 3;

            // Apply gravity
            vel.y += 500 * dt;
            vel.x *= 0.95f;

            // Move
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;

            // Simple ground collision
            int bx = (int)(pos.x / 16);
            int by = (int)((pos.y + 5) / 16);  // check just below
            Block* b = world.get_block(bx, by);
            if (b && b->is_solid() && vel.y > 0) {
                pos.y = by * 16 - 5;
                vel.y = -vel.y * 0.3f;  // bounce
                if (std::abs(vel.y) < 30) vel.y = 0;
                vel.x *= 0.7f;
            }

            // Attract to player if close
            if (ppos) {
                float dx = ppos->x - pos.x;
                float dy = ppos->y - pos.y;
                float dist2 = dx*dx + dy*dy;
                if (dist2 < 6400) {  // within 80px
                    float dist = std::sqrt(dist2);
                    if (dist > 0.1f) {
                        float attract_speed = 300 * (1 - dist/80);
                        vel.x = (dx/dist) * attract_speed;
                        vel.y = (dy/dist) * attract_speed;
                    }
                }
                // Auto-collect when very close
                if (dist2 < 400) {  // within 20px
                    drop.collected = true;
                    collected.push_back({drop.item_id, drop.count});
                    to_destroy.push_back(ent);
                }
            }

            // Despawn after lifetime
            if (drop.elapsed > drop.lifetime) {
                to_destroy.push_back(ent);
            }
        });

        // Destroy collected/expired drops
        for (auto e : to_destroy) {
            reg.destroy(e);
        }
        return collected;
    }

    // Get all drops that were collected this frame (caller adds to inventory)
    static std::vector<std::pair<uint16_t, int>> get_collected(Registry& reg) {
        std::vector<std::pair<uint16_t, int>> result;
        // This would need to be called before destroy - let's use a different approach
        // Actually since we destroy in update, we need a callback system
        return result;
    }
};

// ---- Player Inventory Component ----
struct PlayerInventory {
    Inventory inv;
    // Hotbar: first 8 slots are quick-access
    uint16_t hotbar[8] = {0};
    int hotbar_count[8] = {0};
    int selected_slot = 0;

    bool add_item(uint16_t id, int count = 1) {
        // First try to stack in hotbar
        for (int i = 0; i < 8; i++) {
            if (hotbar[i] == id && hotbar_count[i] < 99) {
                int space = 99 - hotbar_count[i];
                int to_add = std::min(space, count);
                hotbar_count[i] += to_add;
                count -= to_add;
                if (count <= 0) return true;
            }
        }
        // Find empty hotbar slot
        for (int i = 0; i < 8; i++) {
            if (hotbar[i] == 0) {
                hotbar[i] = id;
                hotbar_count[i] = std::min(99, count);
                count -= hotbar_count[i];
                if (count <= 0) return true;
            }
        }
        // Add to main inventory
        Item item;
        item.id = id;
        item.name = item_name(id);
        item.count = count;
        item.stack_size = 99;
        return inv.add(item);
    }

    int count_item(uint16_t id) const {
        int total = 0;
        for (int i = 0; i < 8; i++) {
            if (hotbar[i] == id) total += hotbar_count[i];
        }
        return total + inv.count_of(id);
    }

    bool remove_item(uint16_t id, int count = 1) {
        // First from hotbar
        for (int i = 0; i < 8 && count > 0; i++) {
            if (hotbar[i] == id && hotbar_count[i] > 0) {
                int to_remove = std::min(hotbar_count[i], count);
                hotbar_count[i] -= to_remove;
                count -= to_remove;
                if (hotbar_count[i] == 0) hotbar[i] = 0;
            }
        }
        // Then from main inventory
        if (count > 0) {
            return inv.remove(id, count);
        }
        return true;
    }
};

} // namespace krono
