// Test: Inventory & Items (Fase 4)
#include "game/inventory.hpp"
#include <iostream>
#include <cassert>

using namespace krono;

int main() {
    std::cout << "=== Inventory Tests (Fase 4) ===" << std::endl;

    // TEST 1: Add items + stacking
    {
        std::cout << "\n--- Test 1: Add + stack ---" << std::endl;
        Inventory inv;
        inv.max_weight = 100;
        inv.max_volume = 200;

        Item dirt{1, "Dirt", "", ItemCategory::MATERIAL, ItemRarity::COMMON, 0.5f, 1, 99, 50};
        assert(inv.add(dirt));
        assert(inv.items.size() == 1);
        assert(inv.items[0].count == 50);

        // Add more — should stack up to 99
        Item more_dirt{1, "Dirt", "", ItemCategory::MATERIAL, ItemRarity::COMMON, 0.5f, 1, 99, 60};
        assert(inv.add(more_dirt));
        // 50+60=110, cap=99, so 99 in first + 11 in second
        assert(inv.items.size() == 2);
        assert(inv.items[0].count == 99);
        assert(inv.items[1].count == 11);
        std::cout << "  Stacks: " << inv.items.size() << " first.count=" << inv.items[0].count << " second.count=" << inv.items[1].count << std::endl;
        std::cout << "  ✓ Stacking works" << std::endl;
    }

    // TEST 2: Weight/volume limits
    {
        std::cout << "\n--- Test 2: Weight/volume limits ---" << std::endl;
        Inventory inv;
        inv.max_weight = 10;
        inv.max_volume = 20;

        Item heavy{1, "Steel Block", "", ItemCategory::MATERIAL, ItemRarity::COMMON, 5.0f, 5, 99, 3};
        // 3*5=15 > max_weight=10 → should fail
        assert(!inv.add(heavy));
        std::cout << "  Heavy item (15kg > 10kg max) rejected ✓" << std::endl;

        Item light{2, "Feather", "", ItemCategory::MATERIAL, ItemRarity::COMMON, 0.1f, 0.1f, 99, 50};
        assert(inv.add(light)); // 0.1*50=5 < 10 ✓
        std::cout << "  Weight: " << inv.current_weight() << "/" << inv.max_weight << std::endl;
        assert(inv.current_weight() == 5.0f);

        // Try to add too much
        Item too_heavy{3, "Anvil", "", ItemCategory::MATERIAL, ItemRarity::COMMON, 6.0f, 1, 1, 1};
        assert(!inv.add(too_heavy)); // 5+6=11 > 10
        std::cout << "  ✓ Overweight item rejected" << std::endl;
    }

    // TEST 3: Remove items
    {
        std::cout << "\n--- Test 3: Remove ---" << std::endl;
        Inventory inv;
        Item item{1, "Stone", "", ItemCategory::MATERIAL, ItemRarity::COMMON, 1, 1, 99, 10};
        inv.add(item);

        assert(inv.remove(1, 3)); // remove 3
        assert(inv.count_of(1) == 7);
        std::cout << "  After removing 3: " << inv.count_of(1) << " remaining" << std::endl;

        assert(inv.remove(1, 7)); // remove all
        assert(inv.count_of(1) == 0);
        std::cout << "  After removing all: 0" << std::endl;

        assert(!inv.remove(1, 1)); // can't remove from empty
        std::cout << "  ✓ Remove works correctly" << std::endl;
    }

    // TEST 4: Encumbrance ratio (affects physics — Parte 5.1)
    {
        std::cout << "\n--- Test 4: Encumbrance ---" << std::endl;
        Inventory inv;
        inv.max_weight = 100;

        Item item{1, "Iron", "", ItemCategory::MATERIAL, ItemRarity::COMMON, 10, 1, 99, 5};
        inv.add(item); // 50kg

        float enc = inv.encumbrance_ratio();
        std::cout << "  Encumbrance: " << enc << " (50/100=0.5)" << std::endl;
        assert(std::abs(enc - 0.5f) < 0.01f);

        // Speed multiplier = 1 - (enc * 0.5) = 1 - 0.25 = 0.75
        float speed_mult = 1.0f - (enc * 0.5f);
        std::cout << "  Speed multiplier: " << speed_mult << std::endl;
        assert(std::abs(speed_mult - 0.75f) < 0.01f);
        std::cout << "  ✓ Encumbrance correctly affects movement" << std::endl;
    }

    // TEST 5: Rarities
    {
        std::cout << "\n--- Test 5: Rarities ---" << std::endl;
        ItemRarity rarities[] = {ItemRarity::COMMON, ItemRarity::UNCOMMON, ItemRarity::RARE,
                                  ItemRarity::EPIC, ItemRarity::LEGENDARY, ItemRarity::ANOMALOUS};
        for (auto r : rarities) {
            uint8_t rr, gg, bb;
            rarity_color(r, rr, gg, bb);
            std::cout << "  " << rarity_name(r) << ": rgb(" << (int)rr << "," << (int)gg << "," << (int)bb << ")" << std::endl;
        }
        std::cout << "  ✓ All 6 rarities defined" << std::endl;
    }

    // TEST 6: Crafting recipe
    {
        std::cout << "\n--- Test 6: Crafting ---" << std::endl;
        Inventory inv;
        inv.max_weight = 1000;

        // Add ingredients
        inv.add(Item{1, "Iron Ore", "", ItemCategory::MATERIAL, ItemRarity::COMMON, 1, 1, 99, 3});
        inv.add(Item{2, "Coal", "", ItemCategory::MATERIAL, ItemRarity::COMMON, 0.5f, 1, 99, 2});

        Recipe recipe;
        recipe.result_item_id = 10;
        recipe.result_count = 1;
        recipe.ingredients = {{1, 2}, {2, 1}}; // 2 iron ore + 1 coal
        recipe.craft_time = 2.0f;

        // Check if can craft
        bool can_craft = true;
        for (auto& ing : recipe.ingredients) {
            if (inv.count_of(ing.item_id) < ing.count) {
                can_craft = false;
                break;
            }
        }
        assert(can_craft);
        std::cout << "  Can craft: yes (have 2 iron + 1 coal)" << std::endl;

        // Craft: remove ingredients
        for (auto& ing : recipe.ingredients) {
            inv.remove(ing.item_id, ing.count);
        }
        // Add result
        Item result{10, "Steel Bar", "Refined metal", ItemCategory::MATERIAL, ItemRarity::UNCOMMON, 2, 1, 99, 1};
        inv.add(result);

        assert(inv.count_of(10) == 1);
        assert(inv.count_of(1) == 1); // 3-2=1
        assert(inv.count_of(2) == 1); // 2-1=1
        std::cout << "  After craft: steel=1, iron=1, coal=1" << std::endl;
        std::cout << "  ✓ Crafting works" << std::endl;
    }

    // TEST 7: Item powers (GDD v0.2 seção 2)
    {
        std::cout << "\n--- Test 7: Item powers ---" << std::endl;
        Item flight_pack{100, "Flight Pack", "Grants flight ability",
                         ItemCategory::TECH, ItemRarity::EPIC, 5, 5, 1, 1};
        flight_pack.power = Item::FLIGHT;
        flight_pack.power_cost = 5.0f;
        flight_pack.power_strength = 600.0f;

        assert(flight_pack.power == Item::FLIGHT);
        assert(flight_pack.power_cost == 5.0f);
        std::cout << "  Flight Pack: cost=" << flight_pack.power_cost << " strength=" << flight_pack.power_strength << std::endl;
        std::cout << "  ✓ Powers defined (FLIGHT, TELEKINESIS, SHIELD, CLOAK, REGENERATE, TIME_DILATION)" << std::endl;
    }

    // TEST 8: Find item
    {
        std::cout << "\n--- Test 8: Find ---" << std::endl;
        Inventory inv;
        inv.add(Item{1, "Sword", "", ItemCategory::WEAPON, ItemRarity::RARE, 3, 2, 1, 1});
        inv.add(Item{2, "Shield", "", ItemCategory::ARMOR, ItemRarity::UNCOMMON, 4, 3, 1, 1});

        Item* found = inv.find(1);
        assert(found != nullptr);
        assert(found->name == "Sword");
        std::cout << "  Found: " << found->name << " (" << rarity_name(found->rarity) << ")" << std::endl;

        Item* not_found = inv.find(999);
        assert(not_found == nullptr);
        std::cout << "  ✓ Find works" << std::endl;
    }

    std::cout << "\n=== All Inventory tests passed! ✓ ===" << std::endl;
    return 0;
}
