#pragma once
// KronoUniverse — Inventory & Items (Fase 4, GDD v0.2 seção 1)
// Inventário quase infinito com raridades, peso/volume, crafting.

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace krono {

enum class ItemRarity : uint8_t {
    COMMON,     // gray
    UNCOMMON,   // green
    RARE,       // blue
    EPIC,       // purple
    LEGENDARY,  // orange
    ANOMALOUS,  // teal (anomalous items — GDD v0.2 seção 2)
};

enum class ItemCategory : uint8_t {
    MATERIAL,    // raw resources
    TOOL,        // pickaxe, scanner
    WEAPON,      // guns, melee
    ARMOR,       // protective gear
    CONSUMABLE,  // food, medicine
    TECH,        // tech modules, circuits
    BIOLOGICAL,  // seeds, specimens
    ANOMALOUS,   // reality-bending items
    KEY_ITEM,    // quest/progression items (can't drop)
};

struct Item {
    uint32_t id = 0;
    std::string name;
    std::string description;
    ItemCategory category = ItemCategory::MATERIAL;
    ItemRarity rarity = ItemRarity::COMMON;
    float weight = 0.1f;       // kg (affects physics — Parte 5.1 encumbrance)
    float volume = 1.0f;       // slots (affects inventory capacity)
    int stack_size = 99;       // max per stack
    int count = 1;             // current stack count
    uint32_t value = 1;        // base trade value
    uint8_t r = 130, g = 130, b = 130; // color for pixel art

    // Effects (when equipped/used)
    float damage_bonus = 0;
    float defense_bonus = 0;
    float speed_bonus = 0;
    float jump_bonus = 0;
    float energy_output = 0;   // for tech items (Parte 5.6)
    float heal_amount = 0;     // for consumables

    // Tech/bio/anomalous powers (GDD v0.2 seção 2)
    enum PowerType : uint8_t { NONE, FLIGHT, TELEKINESIS, SHIELD, CLOAK, REGENERATE, TIME_DILATION };
    PowerType power = NONE;
    float power_cost = 0;      // energy per second when active
    float power_strength = 0;  // magnitude of effect
};

struct Inventory {
    std::vector<Item> items;
    float max_weight = 100.0f;
    float max_volume = 200.0f;

    float current_weight() const {
        float total = 0;
        for (auto& item : items) total += item.weight * item.count;
        return total;
    }

    float current_volume() const {
        float total = 0;
        for (auto& item : items) total += item.volume * item.count;
        return total;
    }

    float encumbrance_ratio() const {
        return (max_weight > 0) ? current_weight() / max_weight : 0;
    }

    bool can_add(const Item& item) const {
        return current_weight() + item.weight * item.count <= max_weight &&
               current_volume() + item.volume * item.count <= max_volume;
    }

    bool add(Item item) {
        // Try to stack with existing
        for (auto& existing : items) {
            if (existing.id == item.id && existing.count < existing.stack_size) {
                int space = existing.stack_size - existing.count;
                int to_add = std::min(space, item.count);
                existing.count += to_add;
                item.count -= to_add;
                if (item.count <= 0) return true;
            }
        }
        // Add new stack
        if (can_add(item)) {
            items.push_back(item);
            return true;
        }
        return false;
    }

    bool remove(uint32_t item_id, int count = 1) {
        for (auto& item : items) {
            if (item.id == item_id) {
                if (item.count >= count) {
                    item.count -= count;
                    if (item.count <= 0) {
                        // Remove empty stack
                        items.erase(items.begin() + (&item - &items[0]));
                    }
                    return true;
                }
            }
        }
        return false;
    }

    Item* find(uint32_t item_id) {
        for (auto& item : items) {
            if (item.id == item_id) return &item;
        }
        return nullptr;
    }

    int count_of(uint32_t item_id) const {
        int total = 0;
        for (auto& item : items) {
            if (item.id == item_id) total += item.count;
        }
        return total;
    }
};

// ---- Crafting Recipe ----
struct Recipe {
    uint32_t result_item_id;
    int result_count = 1;
    struct Ingredient { uint32_t item_id; int count; };
    std::vector<Ingredient> ingredients;
    float craft_time = 1.0f;  // seconds
    bool requires_workbench = false;
};

inline const char* rarity_name(ItemRarity r) {
    switch (r) {
        case ItemRarity::COMMON: return "Common";
        case ItemRarity::UNCOMMON: return "Uncommon";
        case ItemRarity::RARE: return "Rare";
        case ItemRarity::EPIC: return "Epic";
        case ItemRarity::LEGENDARY: return "Legendary";
        case ItemRarity::ANOMALOUS: return "Anomalous";
        default: return "Unknown";
    }
}

inline void rarity_color(ItemRarity r, uint8_t& rr, uint8_t& gg, uint8_t& bb) {
    switch (r) {
        case ItemRarity::COMMON:    rr=160; gg=160; bb=160; break;
        case ItemRarity::UNCOMMON:  rr=80;  gg=200; bb=80;  break;
        case ItemRarity::RARE:      rr=80;  gg=140; bb=220; break;
        case ItemRarity::EPIC:      rr=180; gg=80;  bb=200; break;
        case ItemRarity::LEGENDARY: rr=240; gg=160; bb=40;  break;
        case ItemRarity::ANOMALOUS: rr=40;  gg=220; bb=200; break;
    }
}

} // namespace krono
