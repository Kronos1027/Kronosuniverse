// KronoUniverse — Wildlife & Item Drops Tests (v0.5)

#include "game/wildlife_system.hpp"
#include "game/item_drops.hpp"
#include <cassert>
#include <iostream>

using namespace krono;

static int tests_passed = 0;
static int tests_total = 0;
#define TEST(name) tests_total++;
#define ENDTEST() tests_passed++; std::cout << "[OK] " << __func__ << std::endl;

// ---- Plant tests ----
static void test_plant_spawn_for_forest() {
    TEST("plants spawn in forest biome");
    PlantSystem ps;
    World world(42);
    ps.spawn_for_chunk(world, 0, BiomeType::FOREST, 42);
    assert(ps.plants.size() > 0);
    ENDTEST();
}

static void test_plant_spawn_desert_has_cactus() {
    TEST("desert biome can spawn cactus");
    PlantSystem ps;
    World world(42);
    // Spawn many chunks to find at least one cactus
    for (int i = 0; i < 20; i++) {
        ps.spawn_for_chunk(world, i, BiomeType::DESERT, 42 + i);
    }
    bool has_cactus = false;
    for (auto& p : ps.plants) {
        if (p.type == PlantType::CACTUS) has_cactus = true;
    }
    assert(has_cactus);
    ENDTEST();
}

static void test_plant_growth_over_time() {
    TEST("plants grow over time");
    PlantSystem ps;
    World world(42);
    ps.spawn_for_chunk(world, 0, BiomeType::FOREST, 42);
    if (ps.plants.empty()) { ENDTEST(); return; }
    float initial = ps.plants[0].growth;
    ps.update(100.0f);  // 100 seconds
    assert(ps.plants[0].growth > initial);
    ENDTEST();
}

static void test_plant_harvest() {
    TEST("plant can be harvested");
    PlantSystem ps;
    Plant p;
    p.type = PlantType::BUSH_BERRY;
    p.x = 100; p.y = 100;
    p.growth = 1.0f;
    p.harvestable = true;
    p.harvest_timer = 0;
    p.yield_item_id = 0x0125;
    p.yield_count = 2;
    ps.plants.push_back(p);

    uint16_t item; uint8_t count;
    bool result = ps.try_harvest(105, 100, item, count);
    assert(result);
    assert(item == 0x0125);
    assert(count == 2);
    ENDTEST();
}

static void test_plant_harvest_cooldown() {
    TEST("plant has harvest cooldown");
    PlantSystem ps;
    Plant p;
    p.type = PlantType::BUSH_BERRY;
    p.x = 100; p.y = 100;
    p.growth = 1.0f;
    p.harvestable = true;
    p.harvest_timer = 0;
    p.yield_item_id = 0x0125;
    p.yield_count = 2;
    ps.plants.push_back(p);

    uint16_t item; uint8_t count;
    bool first = ps.try_harvest(105, 100, item, count);
    assert(first);
    // Try again immediately - should fail
    bool second = ps.try_harvest(105, 100, item, count);
    assert(!second);
    ENDTEST();
}

static void test_plant_cull_far() {
    TEST("plants far from player are culled");
    PlantSystem ps;
    for (int i = 0; i < 10; i++) {
        Plant p;
        p.x = (float)(i * 1000);
        p.y = 0;
        ps.plants.push_back(p);
    }
    ps.cull_far(5000, 1500);
    // Only plants within 1500 of x=5000 should remain
    for (auto& p : ps.plants) {
        assert(std::abs(p.x - 5000) <= 1500);
    }
    ENDTEST();
}

static void test_plant_colors_differ() {
    TEST("different plants have different colors");
    float r1, g1, b1, r2, g2, b2;
    PlantSystem::get_color(PlantType::FLOWER_RED, r1, g1, b1);
    PlantSystem::get_color(PlantType::FLOWER_BLUE, r2, g2, b2);
    assert(r1 > r2);  // red has more red
    assert(b2 > b1);  // blue has more blue
    ENDTEST();
}

static void test_glowing_plants() {
    TEST("mushroom glow and crystal flower are glowing");
    assert(PlantSystem::is_glowing(PlantType::MUSHROOM_GLOW));
    assert(PlantSystem::is_glowing(PlantType::CRYSTAL_FLOWER));
    assert(!PlantSystem::is_glowing(PlantType::GRASS_TUFT));
    ENDTEST();
}

// ---- Wildlife tests ----
static void test_wildlife_spawn_forest() {
    TEST("wildlife spawns in forest");
    WildlifeSystem ws;
    World world(42);
    ws.spawn_for_biome(world, BiomeType::FOREST, 100, 100, 42);
    assert(ws.animals.size() > 0);
    ENDTEST();
}

static void test_wildlife_update_movement() {
    TEST("wildlife moves over time");
    WildlifeSystem ws;
    World world(42);
    ws.spawn_for_biome(world, BiomeType::FOREST, 100, 100, 42);
    if (ws.animals.empty()) { ENDTEST(); return; }
    float initial_x = ws.animals[0].x;
    ws.update(5.0f, 0.5f, 100, 100);  // 5 sec at noon
    // Some movement should have happened
    bool any_moved = false;
    for (auto& w : ws.animals) {
        if (w.x != initial_x) any_moved = true;
    }
    assert(any_moved);
    ENDTEST();
}

static void test_wildlife_nocturnal_firefly() {
    TEST("firefly is nocturnal");
    WildlifeSystem ws;
    World world(42);
    // Force spawn a firefly
    Wildlife f;
    f.type = WildlifeType::FIREFLY;
    f.x = 100; f.y = 100;
    f.is_noturnal = true;
    f.active = true;
    ws.animals.push_back(f);

    // During day - should be inactive
    ws.update(0.1f, 0.5f, 100, 100);  // noon
    assert(!ws.animals[0].active);

    // During night - should be active
    ws.update(0.1f, 0.0f, 100, 100);  // midnight
    assert(ws.animals[0].active);
    ENDTEST();
}

static void test_wildlife_cull_far() {
    TEST("wildlife far from player is culled");
    WildlifeSystem ws;
    for (int i = 0; i < 10; i++) {
        Wildlife w;
        w.x = (float)(i * 5000);
        w.y = 0;
        ws.animals.push_back(w);
    }
    ws.update(0.1f, 0.5f, 0, 0);
    assert(ws.animals.size() < 10);
    ENDTEST();
}

static void test_wildlife_colors_differ() {
    TEST("wildlife colors differ");
    float r1, g1, b1, r2, g2, b2;
    WildlifeSystem::get_color(WildlifeType::BIRD, r1, g1, b1);
    WildlifeSystem::get_color(WildlifeType::FOX, r2, g2, b2);
    // Fox should be more red/orange
    assert(r2 > r1);
    ENDTEST();
}

// ---- Item drop tests ----
static void test_block_to_item_drop_dirt() {
    TEST("dirt block drops dirt item");
    auto [item, count] = block_to_item_drop(BlockType::DIRT);
    assert(item == 0x0102);
    assert(count == 1);
    ENDTEST();
}

static void test_block_to_item_drop_crystal() {
    TEST("crystal block drops crystal item");
    auto [item, count] = block_to_item_drop(BlockType::CRYSTAL);
    assert(item == 0x0121);
    assert(count == 1);
    ENDTEST();
}

static void test_block_to_item_drop_bedrock_no_drop() {
    TEST("bedrock drops nothing");
    auto [item, count] = block_to_item_drop(BlockType::BEDROCK);
    assert(item == 0);
    assert(count == 0);
    ENDTEST();
}

static void test_item_drop_spawn() {
    TEST("item drop spawns as entity");
    Registry reg;
    Entity e = ItemDropSystem::spawn_drop(reg, 100, 100, 0x0102, 1);
    assert(reg.has<ItemDrop>(e));
    assert(reg.has<Position>(e));
    auto* drop = reg.get<ItemDrop>(e);
    assert(drop->item_id == 0x0102);
    assert(drop->count == 1);
    ENDTEST();
}

static void test_item_drop_lifetime() {
    TEST("item drop expires after lifetime");
    Registry reg;
    World world(42);
    Entity player = reg.create();
    reg.emplace<Position>(player, Position{1000, 1000});  // far away
    Entity drop = ItemDropSystem::spawn_drop(reg, 100, 100, 0x0102, 1);
    auto* d = reg.get<ItemDrop>(drop);
    d->lifetime = 1.0f;  // short lifetime
    ItemDropSystem::update(reg, player, 2.0f, world);  // 2 sec
    // Drop should be destroyed
    assert(!reg.has<ItemDrop>(drop));
    ENDTEST();
}

static void test_item_drop_attracts_to_player() {
    TEST("item drop attracts to player when close");
    Registry reg;
    World world(42);
    Entity player = reg.create();
    reg.emplace<Position>(player, Position{60, 60});  // very close
    Entity drop = ItemDropSystem::spawn_drop(reg, 70, 70, 0x0102, 1);
    // Multiple updates to ensure collection
    for (int i = 0; i < 60; i++) {
        if (!reg.has<ItemDrop>(drop)) break;
        ItemDropSystem::update(reg, player, 0.1f, world);
    }
    // Drop should be collected
    assert(!reg.has<ItemDrop>(drop));
    ENDTEST();
}

static void test_player_inventory_add() {
    TEST("player inventory adds items");
    PlayerInventory pinv;
    bool result = pinv.add_item(0x0102, 5);
    assert(result);
    assert(pinv.count_item(0x0102) == 5);
    ENDTEST();
}

static void test_player_inventory_stack() {
    TEST("player inventory stacks items");
    PlayerInventory pinv;
    pinv.add_item(0x0102, 50);
    pinv.add_item(0x0102, 50);
    // First stack maxes at 99, remaining 1 in another slot
    // Total count = 100
    assert(pinv.count_item(0x0102) == 100);
    assert(pinv.hotbar[0] == 0x0102);
    assert(pinv.hotbar_count[0] == 99);
    assert(pinv.hotbar[1] == 0x0102);
    assert(pinv.hotbar_count[1] == 1);
    ENDTEST();
}

static void test_player_inventory_remove() {
    TEST("player inventory removes items");
    PlayerInventory pinv;
    pinv.add_item(0x0102, 10);
    bool result = pinv.remove_item(0x0102, 3);
    assert(result);
    assert(pinv.count_item(0x0102) == 7);
    ENDTEST();
}

static void test_player_inventory_remove_all() {
    TEST("player inventory removes all items");
    PlayerInventory pinv;
    pinv.add_item(0x0102, 5);
    bool result = pinv.remove_item(0x0102, 5);
    assert(result);
    assert(pinv.count_item(0x0102) == 0);
    ENDTEST();
}

static void test_item_names() {
    TEST("item names are correct");
    assert(std::string(item_name(0x0100)) == "Wood Log");
    assert(std::string(item_name(0x0103)) == "Stone");
    assert(std::string(item_name(0x0121)) == "Crystal");
    assert(std::string(item_name(0x0601)) == "Health Potion");
    ENDTEST();
}

static void test_item_colors() {
    TEST("item colors are distinct");
    float r1, g1, b1, r2, g2, b2;
    item_color(0x0601, r1, g1, b1);  // Health potion
    item_color(0x0602, r2, g2, b2);  // Mana potion
    assert(r1 > r2);  // health more red
    assert(b2 > b1);  // mana more blue
    ENDTEST();
}

int main() {
    std::cout << "=== Wildlife & Item Drops Tests (v0.5) ===" << std::endl;
    test_plant_spawn_for_forest();
    test_plant_spawn_desert_has_cactus();
    test_plant_growth_over_time();
    test_plant_harvest();
    test_plant_harvest_cooldown();
    test_plant_cull_far();
    test_plant_colors_differ();
    test_glowing_plants();
    test_wildlife_spawn_forest();
    test_wildlife_update_movement();
    test_wildlife_nocturnal_firefly();
    test_wildlife_cull_far();
    test_wildlife_colors_differ();
    test_block_to_item_drop_dirt();
    test_block_to_item_drop_crystal();
    test_block_to_item_drop_bedrock_no_drop();
    test_item_drop_spawn();
    test_item_drop_lifetime();
    test_item_drop_attracts_to_player();
    test_player_inventory_add();
    test_player_inventory_stack();
    test_player_inventory_remove();
    test_player_inventory_remove_all();
    test_item_names();
    test_item_colors();
    std::cout << "\n=== Results: " << tests_passed << "/" << tests_total << " ===" << std::endl;
    return tests_passed == tests_total ? 0 : 1;
}
