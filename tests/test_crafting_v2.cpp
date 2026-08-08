// KronoUniverse — Crafting System v2 Tests (v0.3)

#include "game/crafting_system.hpp"
#include <cassert>
#include <iostream>

using namespace krono;

static int tests_passed = 0;
static int tests_total = 0;
#define TEST(name) tests_total++;
#define ENDTEST() tests_passed++; std::cout << "[OK] " << __func__ << std::endl;

static Item make_item(uint16_t id, const char* name, int count = 1) {
    Item i;
    i.id = id;
    i.name = name;
    i.count = count;
    i.stack_size = 99;
    return i;
}

static void test_recipe_count() {
    TEST("recipe database has 40+ recipes");
    auto& recipes = CraftingSystem::all_recipes();
    assert(recipes.size() >= 40);
    ENDTEST();
}

static void test_basic_craft_no_station() {
    TEST("basic recipe crafts without station");
    Inventory inv;
    inv.add(make_item(0x0100, "Wood Log", 5));  // wood log
    auto& recipes = CraftingSystem::all_recipes();
    // Find wood planks recipe
    CraftingRecipe* planks = nullptr;
    for (auto& r : recipes) {
        if (r.result_item_id == 0x0101) { planks = &r; break; }
    }
    assert(planks);
    bool result = CraftingSystem::craft(*planks, inv, CraftingStation::NONE);
    assert(result);
    assert(inv.count_of(0x0101) == 4);  // produces 4 planks
    ENDTEST();
}

static void test_recipe_requires_station() {
    TEST("advanced recipe fails without station");
    Inventory inv;
    inv.add(make_item(0x0101, "Wood Planks", 8));
    inv.add(make_item(0x0100, "Wood Log", 1));
    auto& recipes = CraftingSystem::all_recipes();
    CraftingRecipe* chest = nullptr;
    for (auto& r : recipes) {
        if (r.result_item_id == 0x0104) { chest = &r; break; }
    }
    assert(chest);
    assert(chest->station == CraftingStation::WORKBENCH);
    // Without station
    bool result = CraftingSystem::craft(*chest, inv, CraftingStation::NONE);
    assert(!result);
    // With workbench
    result = CraftingSystem::craft(*chest, inv, CraftingStation::WORKBENCH);
    assert(result);
    ENDTEST();
}

static void test_missing_ingredient_fails() {
    TEST("craft fails without required ingredient");
    Inventory inv;
    // No items added - should fail
    auto& recipes = CraftingSystem::all_recipes();
    CraftingRecipe* planks = nullptr;
    for (auto& r : recipes) {
        if (r.result_item_id == 0x0101) { planks = &r; break; }
    }
    assert(planks);
    bool result = CraftingSystem::craft(*planks, inv, CraftingStation::NONE);
    assert(!result);  // no wood, should fail
    ENDTEST();
}

static void test_recipe_consumes_ingredients() {
    TEST("crafting consumes ingredients");
    Inventory inv;
    inv.add(make_item(0x0100, "Wood Log", 5));
    auto& recipes = CraftingSystem::all_recipes();
    CraftingRecipe* planks = nullptr;
    for (auto& r : recipes) {
        if (r.result_item_id == 0x0101) { planks = &r; break; }
    }
    CraftingSystem::craft(*planks, inv, CraftingStation::NONE);
    assert(inv.count_of(0x0100) == 4);  // consumed 1
    ENDTEST();
}

static void test_smelting_requires_furnace() {
    TEST("smelting requires furnace station");
    Inventory inv;
    inv.add(make_item(0x0110, "Iron Ore", 2));
    inv.add(make_item(0x0103, "Coal", 1));
    auto& recipes = CraftingSystem::all_recipes();
    CraftingRecipe* ingot = nullptr;
    for (auto& r : recipes) {
        if (r.result_item_id == 0x0111) { ingot = &r; break; }
    }
    assert(ingot);
    assert(ingot->station == CraftingStation::FURNACE);
    bool r1 = CraftingSystem::craft(*ingot, inv, CraftingStation::NONE);
    assert(!r1);
    bool r2 = CraftingSystem::craft(*ingot, inv, CraftingStation::FURNACE);
    assert(r2);
    ENDTEST();
}

static void test_get_available_filters_by_station() {
    TEST("get_available returns only craftable recipes");
    Inventory inv;
    inv.add(make_item(0x0100, "Wood Log", 10));
    auto& recipes = CraftingSystem::all_recipes();
    auto available = CraftingSystem::get_available(inv, CraftingStation::NONE);
    assert(available.size() > 0);
    // All returned should be craftable
    for (auto* r : available) {
        assert(CraftingSystem::can_craft(*r, inv, CraftingStation::NONE));
    }
    ENDTEST();
}

static void test_tier_progression() {
    TEST("recipe tiers go from basic to advanced");
    auto& recipes = CraftingSystem::all_recipes();
    bool has_t1 = false, has_t4 = false, has_t6 = false;
    for (auto& r : recipes) {
        if (r.tier == 1) has_t1 = true;
        if (r.tier == 4) has_t4 = true;
        if (r.tier == 6) has_t6 = true;
    }
    assert(has_t1);
    assert(has_t4);
    assert(has_t6);
    ENDTEST();
}

static void test_weapon_recipes() {
    TEST("weapon recipes exist");
    auto& recipes = CraftingSystem::all_recipes();
    bool has_sword = false, has_bow = false, has_gun = false;
    for (auto& r : recipes) {
        if (r.category == "Weapon") {
            if (r.result_name == "Bow") has_bow = true;
        }
        if (r.category == "Tool") {
            if (r.result_name == "Wooden Sword") has_sword = true;
        }
        if (r.category == "Weapon" && r.result_name == "Gun") has_gun = true;
    }
    assert(has_sword);
    assert(has_bow);
    assert(has_gun);
    ENDTEST();
}

static void test_armor_recipes() {
    TEST("armor recipes exist");
    auto& recipes = CraftingSystem::all_recipes();
    bool has_iron_armor = false, has_steel_armor = false;
    for (auto& r : recipes) {
        if (r.category == "Armor") {
            if (r.result_name == "Iron Chestplate") has_iron_armor = true;
            if (r.result_name == "Steel Chestplate") has_steel_armor = true;
        }
    }
    assert(has_iron_armor);
    assert(has_steel_armor);
    ENDTEST();
}

static void test_potion_recipes() {
    TEST("potion recipes exist");
    auto& recipes = CraftingSystem::all_recipes();
    bool has_health = false, has_invis = false;
    for (auto& r : recipes) {
        if (r.category == "Potion") {
            if (r.result_name == "Health Potion") has_health = true;
            if (r.result_name == "Invisibility Potion") has_invis = true;
        }
    }
    assert(has_health);
    assert(has_invis);
    ENDTEST();
}

static void test_tech_recipes_need_high_tech() {
    TEST("tech recipes require HIGH_TECH station");
    auto& recipes = CraftingSystem::all_recipes();
    for (auto& r : recipes) {
        if (r.category == "Tech" || r.result_name == "Gun") {
            assert(r.station == CraftingStation::HIGH_TECH);
        }
    }
    ENDTEST();
}

static void test_magic_recipes_need_altar() {
    TEST("magic recipes require ALTAR station");
    auto& recipes = CraftingSystem::all_recipes();
    for (auto& r : recipes) {
        if (r.category == "Magic") {
            assert(r.station == CraftingStation::ALTAR);
        }
    }
    ENDTEST();
}

static void test_stations_can_be_crafted() {
    TEST("stations themselves can be crafted");
    auto& recipes = CraftingSystem::all_recipes();
    bool has_workbench = false, has_furnace = false, has_anvil = false;
    for (auto& r : recipes) {
        if (r.category == "Station") {
            if (r.result_name == "Workbench") has_workbench = true;
            if (r.result_name == "Furnace") has_furnace = true;
            if (r.result_name == "Anvil") has_anvil = true;
        }
    }
    assert(has_workbench);
    assert(has_furnace);
    assert(has_anvil);
    ENDTEST();
}

static void test_recipe_categories_diverse() {
    TEST("recipe categories are diverse");
    auto& recipes = CraftingSystem::all_recipes();
    bool has_building = false, has_weapon = false, has_armor = false;
    bool has_food = false, has_potion = false, has_magic = false;
    for (auto& r : recipes) {
        if (r.category == "Building") has_building = true;
        if (r.category == "Weapon" || r.category == "Tool") has_weapon = true;
        if (r.category == "Armor") has_armor = true;
        if (r.category == "Food") has_food = true;
        if (r.category == "Potion") has_potion = true;
        if (r.category == "Magic") has_magic = true;
    }
    assert(has_building && has_weapon && has_armor);
    assert(has_food && has_potion && has_magic);
    ENDTEST();
}

int main() {
    std::cout << "=== Crafting System v2 Tests ===" << std::endl;
    test_recipe_count();
    test_basic_craft_no_station();
    test_recipe_requires_station();
    test_missing_ingredient_fails();
    test_recipe_consumes_ingredients();
    test_smelting_requires_furnace();
    test_get_available_filters_by_station();
    test_tier_progression();
    test_weapon_recipes();
    test_armor_recipes();
    test_potion_recipes();
    test_tech_recipes_need_high_tech();
    test_magic_recipes_need_altar();
    test_stations_can_be_crafted();
    test_recipe_categories_diverse();
    
    std::cout << "\n=== Results: " << tests_passed << "/" << tests_total << " ===" << std::endl;
    return tests_passed == tests_total ? 0 : 1;
}
