#pragma once
// KronoUniverse — Crafting System v2 (v0.3)
//
// Crafting grid-based:
// - 3x3 crafting grid
// - Shapeless recipes (just ingredients)
// - Shaped recipes (specific pattern)
// - Tool required (need pickaxe to craft advanced items)
// - Recipe discovery (must find blueprint or learn)
// - Crafting stations (workbench, furnace, anvil, altar)
// - 40+ recipes organized by tier

#include "game/inventory.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>

namespace krono {

enum class CraftingStation : uint8_t {
    NONE = 0,        // hand-craft only (basic)
    WORKBENCH,       // basic tools, blocks
    FURNACE,         // smelting ore → ingot
    ANVIL,           // advanced weapons/armor
    ALTAR,           // magical items
    ALCHEMY_TABLE,   // potions
    HIGH_TECH,       // futuristic
};

struct CraftingRecipe {
    uint16_t result_item_id = 0;
    std::string result_name;
    uint8_t result_count = 1;
    struct Ingredient { uint16_t item_id; uint8_t count; };
    std::vector<Ingredient> ingredients;
    CraftingStation station = CraftingStation::NONE;
    bool requires_discovery = false;
    bool discovered = true;
    uint8_t tier = 1;  // for sorting
    std::string category = "Misc";
};

class CraftingSystem {
public:
    static std::vector<CraftingRecipe>& all_recipes() {
        static std::vector<CraftingRecipe> recipes;
        if (recipes.empty()) {
            init_recipes(recipes);
        }
        return recipes;
    }

    static bool can_craft(const CraftingRecipe& recipe, const Inventory& inv, CraftingStation available) {
        // Check station
        if (recipe.station != CraftingStation::NONE && recipe.station != available) return false;
        // Check discovery
        if (recipe.requires_discovery && !recipe.discovered) return false;
        // Check ingredients
        for (auto& ing : recipe.ingredients) {
            if ((uint16_t)inv.count_of(ing.item_id) < ing.count) return false;
        }
        return true;
    }

    static bool craft(CraftingRecipe& recipe, Inventory& inv, CraftingStation available) {
        if (!can_craft(recipe, inv, available)) return false;
        // Consume ingredients
        for (auto& ing : recipe.ingredients) {
            inv.remove(ing.item_id, ing.count);
        }
        // Add result
        Item result;
        result.id = recipe.result_item_id;
        result.name = recipe.result_name;
        result.count = recipe.result_count;
        inv.add(result);
        return true;
    }

    static std::vector<CraftingRecipe*> get_available(const Inventory& inv, CraftingStation station) {
        std::vector<CraftingRecipe*> result;
        for (auto& r : all_recipes()) {
            if (can_craft(r, inv, station)) {
                result.push_back(&r);
            }
        }
        return result;
    }

private:
    static void init_recipes(std::vector<CraftingRecipe>& recipes) {
        // Item IDs follow pattern: 0xTTCC where TT=type, CC=specific
        // Materials: 0x01xx, Tools: 0x02xx, Weapons: 0x03xx, Armor: 0x04xx
        // Food: 0x05xx, Potions: 0x06xx, Tech: 0x07xx, Magic: 0x08xx

        CraftingRecipe r;

        // ---- Basic blocks (hand-craft) ----
        r = {0x0101, "Wood Planks", 4, {{0x0100, 1}}, CraftingStation::NONE, false, true, 1, "Building"};
        recipes.push_back(r);
        r = {0x0102, "Torch", 4, {{0x0100, 1}, {0x0103, 1}}, CraftingStation::NONE, false, true, 1, "Building"};
        recipes.push_back(r);
        r = {0x0104, "Chest", 1, {{0x0101, 8}, {0x0100, 1}}, CraftingStation::WORKBENCH, false, true, 1, "Building"};
        recipes.push_back(r);
        r = {0x0105, "Door", 1, {{0x0101, 6}}, CraftingStation::WORKBENCH, false, true, 1, "Building"};
        recipes.push_back(r);
        r = {0x0106, "Ladder", 2, {{0x0101, 1}}, CraftingStation::NONE, false, true, 1, "Building"};
        recipes.push_back(r);

        // ---- Smelting (Furnace) ----
        r = {0x0111, "Iron Ingot", 1, {{0x0110, 2}, {0x0103, 1}}, CraftingStation::FURNACE, false, true, 2, "Material"};
        recipes.push_back(r);
        r = {0x0112, "Gold Ingot", 1, {{0x0110, 1}, {0x0113, 1}, {0x0103, 1}}, CraftingStation::FURNACE, false, true, 2, "Material"};
        recipes.push_back(r);
        r = {0x0113, "Copper Ingot", 1, {{0x0114, 2}, {0x0103, 1}}, CraftingStation::FURNACE, false, true, 2, "Material"};
        recipes.push_back(r);
        r = {0x0114, "Steel Ingot", 1, {{0x0111, 2}, {0x0103, 1}}, CraftingStation::FURNACE, false, true, 3, "Material"};
        recipes.push_back(r);
        r = {0x0115, "Glass", 1, {{0x0116, 1}, {0x0103, 1}}, CraftingStation::FURNACE, false, true, 2, "Material"};
        recipes.push_back(r);
        r = {0x0117, "Brick", 1, {{0x0118, 1}, {0x0103, 1}}, CraftingStation::FURNACE, false, true, 2, "Building"};
        recipes.push_back(r);

        // ---- Tools (Workbench) ----
        r = {0x0201, "Wooden Sword", 1, {{0x0101, 6}}, CraftingStation::WORKBENCH, false, true, 1, "Tool"};
        recipes.push_back(r);
        r = {0x0202, "Wooden Pickaxe", 1, {{0x0101, 4}, {0x0100, 2}}, CraftingStation::WORKBENCH, false, true, 1, "Tool"};
        recipes.push_back(r);
        r = {0x0203, "Wooden Axe", 1, {{0x0101, 4}, {0x0100, 2}}, CraftingStation::WORKBENCH, false, true, 1, "Tool"};
        recipes.push_back(r);
        r = {0x0204, "Stone Sword", 1, {{0x0101, 2}, {0x0103, 3}}, CraftingStation::WORKBENCH, false, true, 2, "Tool"};
        recipes.push_back(r);
        r = {0x0205, "Stone Pickaxe", 1, {{0x0101, 2}, {0x0103, 3}}, CraftingStation::WORKBENCH, false, true, 2, "Tool"};
        recipes.push_back(r);
        r = {0x0206, "Iron Sword", 1, {{0x0101, 1}, {0x0111, 2}}, CraftingStation::ANVIL, false, true, 3, "Tool"};
        recipes.push_back(r);
        r = {0x0207, "Iron Pickaxe", 1, {{0x0101, 2}, {0x0111, 3}}, CraftingStation::ANVIL, false, true, 3, "Tool"};
        recipes.push_back(r);
        r = {0x0208, "Steel Sword", 1, {{0x0114, 2}, {0x0100, 1}}, CraftingStation::ANVIL, false, true, 4, "Tool"};
        recipes.push_back(r);
        r = {0x0209, "Steel Pickaxe", 1, {{0x0114, 3}}, CraftingStation::ANVIL, false, true, 4, "Tool"};
        recipes.push_back(r);

        // ---- Weapons (Anvil) ----
        r = {0x0301, "Bow", 1, {{0x0100, 3}, {0x0101, 2}, {0x0119, 1}}, CraftingStation::WORKBENCH, false, true, 2, "Weapon"};
        recipes.push_back(r);
        r = {0x0302, "Arrow", 8, {{0x0101, 1}, {0x0119, 1}, {0x0103, 1}}, CraftingStation::WORKBENCH, false, true, 2, "Ammo"};
        recipes.push_back(r);
        r = {0x0303, "Iron Arrow", 8, {{0x0101, 1}, {0x0119, 1}, {0x0111, 1}}, CraftingStation::ANVIL, false, true, 3, "Ammo"};
        recipes.push_back(r);
        r = {0x0304, "Gun", 1, {{0x0111, 4}, {0x0114, 1}, {0x0103, 2}}, CraftingStation::HIGH_TECH, true, true, 5, "Weapon"};
        recipes.push_back(r);
        r = {0x0305, "Bullet", 8, {{0x0111, 1}, {0x0113, 1}, {0x0103, 1}}, CraftingStation::HIGH_TECH, true, true, 5, "Ammo"};
        recipes.push_back(r);
        r = {0x0306, "Dagger", 1, {{0x0111, 1}, {0x0100, 1}}, CraftingStation::ANVIL, false, true, 3, "Weapon"};
        recipes.push_back(r);
        r = {0x0307, "War Hammer", 1, {{0x0114, 5}, {0x0100, 2}}, CraftingStation::ANVIL, false, true, 4, "Weapon"};
        recipes.push_back(r);
        r = {0x0308, "Spear", 1, {{0x0100, 4}, {0x0111, 1}}, CraftingStation::ANVIL, false, true, 3, "Weapon"};
        recipes.push_back(r);

        // ---- Armor (Anvil) ----
        r = {0x0401, "Leather Cap", 1, {{0x0120, 2}}, CraftingStation::WORKBENCH, false, true, 2, "Armor"};
        recipes.push_back(r);
        r = {0x0402, "Leather Tunic", 1, {{0x0120, 4}}, CraftingStation::WORKBENCH, false, true, 2, "Armor"};
        recipes.push_back(r);
        r = {0x0403, "Iron Helmet", 1, {{0x0111, 2}}, CraftingStation::ANVIL, false, true, 3, "Armor"};
        recipes.push_back(r);
        r = {0x0404, "Iron Chestplate", 1, {{0x0111, 5}}, CraftingStation::ANVIL, false, true, 3, "Armor"};
        recipes.push_back(r);
        r = {0x0405, "Steel Helmet", 1, {{0x0114, 2}}, CraftingStation::ANVIL, false, true, 4, "Armor"};
        recipes.push_back(r);
        r = {0x0406, "Steel Chestplate", 1, {{0x0114, 5}}, CraftingStation::ANVIL, false, true, 4, "Armor"};
        recipes.push_back(r);
        r = {0x0407, "Crystal Crown", 1, {{0x0121, 3}, {0x0114, 1}}, CraftingStation::ALTAR, true, true, 5, "Armor"};
        recipes.push_back(r);
        r = {0x0408, "Ancient Robe", 1, {{0x0122, 4}, {0x0121, 1}}, CraftingStation::ALTAR, true, true, 5, "Armor"};
        recipes.push_back(r);

        // ---- Food (Workbench / Hand) ----
        r = {0x0501, "Bread", 1, {{0x0123, 3}}, CraftingStation::NONE, false, true, 1, "Food"};
        recipes.push_back(r);
        r = {0x0502, "Cooked Meat", 1, {{0x0124, 1}, {0x0103, 1}}, CraftingStation::FURNACE, false, true, 1, "Food"};
        recipes.push_back(r);
        r = {0x0503, "Apple Pie", 1, {{0x0125, 2}, {0x0123, 1}}, CraftingStation::WORKBENCH, false, true, 2, "Food"};
        recipes.push_back(r);
        r = {0x0504, "Stew", 1, {{0x0124, 1}, {0x0126, 2}, {0x0123, 1}}, CraftingStation::WORKBENCH, false, true, 2, "Food"};
        recipes.push_back(r);

        // ---- Potions (Alchemy Table) ----
        r = {0x0601, "Health Potion", 1, {{0x0127, 1}, {0x0128, 1}}, CraftingStation::ALCHEMY_TABLE, false, true, 2, "Potion"};
        recipes.push_back(r);
        r = {0x0602, "Mana Potion", 1, {{0x0129, 1}, {0x0128, 1}}, CraftingStation::ALCHEMY_TABLE, false, true, 2, "Potion"};
        recipes.push_back(r);
        r = {0x0603, "Strength Potion", 1, {{0x0127, 1}, {0x0130, 1}}, CraftingStation::ALCHEMY_TABLE, false, true, 3, "Potion"};
        recipes.push_back(r);
        r = {0x0604, "Speed Potion", 1, {{0x0129, 1}, {0x0131, 1}}, CraftingStation::ALCHEMY_TABLE, false, true, 3, "Potion"};
        recipes.push_back(r);
        r = {0x0605, "Invisibility Potion", 1, {{0x0127, 1}, {0x0129, 1}, {0x0132, 1}}, CraftingStation::ALCHEMY_TABLE, true, true, 4, "Potion"};
        recipes.push_back(r);
        r = {0x0606, "Antidote", 1, {{0x0133, 2}, {0x0128, 1}}, CraftingStation::ALCHEMY_TABLE, false, true, 2, "Potion"};
        recipes.push_back(r);

        // ---- Magic (Altar) ----
        r = {0x0801, "Fire Staff", 1, {{0x0100, 2}, {0x0121, 1}, {0x0134, 1}}, CraftingStation::ALTAR, true, true, 4, "Magic"};
        recipes.push_back(r);
        r = {0x0802, "Ice Staff", 1, {{0x0100, 2}, {0x0121, 1}, {0x0135, 1}}, CraftingStation::ALTAR, true, true, 4, "Magic"};
        recipes.push_back(r);
        r = {0x0803, "Lightning Staff", 1, {{0x0100, 2}, {0x0121, 2}, {0x0136, 1}}, CraftingStation::ALTAR, true, true, 5, "Magic"};
        recipes.push_back(r);
        r = {0x0804, "Summoning Wand", 1, {{0x0100, 1}, {0x0122, 2}, {0x0137, 1}}, CraftingStation::ALTAR, true, true, 5, "Magic"};
        recipes.push_back(r);
        r = {0x0805, "Magic Scroll: Fireball", 1, {{0x0128, 1}, {0x0134, 1}}, CraftingStation::ALTAR, true, true, 3, "Magic"};
        recipes.push_back(r);
        r = {0x0806, "Magic Scroll: Heal", 1, {{0x0128, 1}, {0x0127, 1}}, CraftingStation::ALTAR, true, true, 3, "Magic"};
        recipes.push_back(r);

        // ---- Tech (High Tech) ----
        r = {0x0701, "Energy Cell", 1, {{0x0114, 1}, {0x0121, 1}}, CraftingStation::HIGH_TECH, true, true, 5, "Tech"};
        recipes.push_back(r);
        r = {0x0702, "Jetpack", 1, {{0x0114, 4}, {0x0121, 2}, {0x0701, 2}}, CraftingStation::HIGH_TECH, true, true, 6, "Tech"};
        recipes.push_back(r);
        r = {0x0703, "Shield Generator", 1, {{0x0114, 3}, {0x0121, 3}, {0x0701, 1}}, CraftingStation::HIGH_TECH, true, true, 6, "Tech"};
        recipes.push_back(r);
        r = {0x0704, "Hover Boots", 1, {{0x0114, 2}, {0x0121, 1}, {0x0701, 1}}, CraftingStation::HIGH_TECH, true, true, 5, "Tech"};
        recipes.push_back(r);
        r = {0x0705, "Mining Drill", 1, {{0x0114, 3}, {0x0121, 1}, {0x0701, 2}}, CraftingStation::HIGH_TECH, true, true, 6, "Tool"};
        recipes.push_back(r);
        r = {0x0706, "Teleporter Beacon", 1, {{0x0122, 2}, {0x0121, 3}, {0x0701, 3}}, CraftingStation::HIGH_TECH, true, true, 7, "Tech"};
        recipes.push_back(r);

        // ---- Stations (made from each other) ----
        r = {0x0901, "Workbench", 1, {{0x0101, 4}, {0x0100, 2}}, CraftingStation::NONE, false, true, 1, "Station"};
        recipes.push_back(r);
        r = {0x0902, "Furnace", 1, {{0x0103, 8}}, CraftingStation::WORKBENCH, false, true, 1, "Station"};
        recipes.push_back(r);
        r = {0x0903, "Anvil", 1, {{0x0111, 3}, {0x0100, 1}}, CraftingStation::FURNACE, false, true, 2, "Station"};
        recipes.push_back(r);
        r = {0x0904, "Alchemy Table", 1, {{0x0101, 4}, {0x0121, 1}}, CraftingStation::WORKBENCH, true, true, 3, "Station"};
        recipes.push_back(r);
        r = {0x0905, "Altar", 1, {{0x0122, 4}, {0x0121, 2}}, CraftingStation::WORKBENCH, true, true, 4, "Station"};
        recipes.push_back(r);
        r = {0x0906, "High Tech Bench", 1, {{0x0114, 4}, {0x0121, 4}, {0x0122, 1}}, CraftingStation::ANVIL, true, true, 5, "Station"};
        recipes.push_back(r);
    }
};

} // namespace krono
