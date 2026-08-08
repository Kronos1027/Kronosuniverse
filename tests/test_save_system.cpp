// KronoUniverse — Save System Tests (v0.3)

#include "game/save_system.hpp"
#include <cassert>
#include <iostream>
#include <cstdio>

using namespace krono;

static int tests_passed = 0;
static int tests_total = 0;
#define TEST(name) tests_total++;
#define ENDTEST() tests_passed++; std::cout << "[OK] " << __func__ << std::endl;

static void test_open_close() {
    TEST("open and close database");
    SaveSystem ss;
    assert(ss.open("/tmp/test_krono_save.db"));
    ss.close();
    std::remove("/tmp/test_krono_save.db");
    ENDTEST();
}

static void test_save_load_player() {
    TEST("save and load player data");
    SaveSystem ss;
    assert(ss.open("/tmp/test_krono_save2.db"));
    SaveData data;
    data.player_x = 1234.5f;
    data.player_y = 678.9f;
    data.player_health = 75.5f;
    data.player_max_health = 150;
    data.player_level = 7;
    data.player_xp = 234.5f;
    data.time_of_day = 0.3f;
    data.world_seed = 99999;
    assert(ss.save_player(data));
    SaveData loaded;
    assert(ss.load_player(loaded));
    assert(loaded.player_x == data.player_x);
    assert(loaded.player_y == data.player_y);
    assert(loaded.player_health == data.player_health);
    assert(loaded.player_level == data.player_level);
    assert(loaded.world_seed == data.world_seed);
    ss.close();
    std::remove("/tmp/test_krono_save2.db");
    ENDTEST();
}

static void test_save_load_stat() {
    TEST("save and load stats");
    SaveSystem ss;
    assert(ss.open("/tmp/test_krono_save3.db"));
    assert(ss.save_stat("kills", 42));
    assert(ss.save_stat("deaths", 7));
    assert(ss.load_stat("kills") == 42);
    assert(ss.load_stat("deaths") == 7);
    assert(ss.load_stat("nonexistent", 99) == 99);  // default
    ss.close();
    std::remove("/tmp/test_krono_save3.db");
    ENDTEST();
}

static void test_block_delta() {
    TEST("save and load block deltas");
    SaveSystem ss;
    assert(ss.open("/tmp/test_krono_save4.db"));
    ss.save_block_delta(100, 200, 3, 80);
    ss.save_block_delta(-50, 75, 5, 100);
    ss.save_block_delta(100, 200, 7, 60);  // overwrite
    auto deltas = ss.load_block_deltas();
    assert(deltas.size() == 2);
    ss.close();
    std::remove("/tmp/test_krono_save4.db");
    ENDTEST();
}

static void test_recipe_discovery() {
    TEST("recipe discovery tracking");
    SaveSystem ss;
    assert(ss.open("/tmp/test_krono_save5.db"));
    assert(!ss.is_recipe_discovered(0x0801));
    ss.mark_recipe_discovered(0x0801);
    assert(ss.is_recipe_discovered(0x0801));
    assert(!ss.is_recipe_discovered(0x0802));
    ss.mark_recipe_discovered(0x0802);
    ss.mark_recipe_discovered(0x0803);
    assert(ss.count_discovered_recipes() == 3);
    ss.close();
    std::remove("/tmp/test_krono_save5.db");
    ENDTEST();
}

static void test_persistence_across_reopen() {
    TEST("data persists across reopen");
    SaveSystem ss;
    assert(ss.open("/tmp/test_krono_save6.db"));
    ss.save_stat("test_stat", 12345);
    ss.close();
    SaveSystem ss2;
    assert(ss2.open("/tmp/test_krono_save6.db"));
    assert(ss2.load_stat("test_stat") == 12345);
    ss2.close();
    std::remove("/tmp/test_krono_save6.db");
    ENDTEST();
}

int main() {
    std::cout << "=== Save System Tests ===" << std::endl;
    test_open_close();
    test_save_load_player();
    test_save_load_stat();
    test_block_delta();
    test_recipe_discovery();
    test_persistence_across_reopen();
    std::cout << "\n=== Results: " << tests_passed << "/" << tests_total << " ===" << std::endl;
    return tests_passed == tests_total ? 0 : 1;
}
