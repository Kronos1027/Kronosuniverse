// KronoUniverse — Quest System Tests (v0.7)

#include "game/quest_system.hpp"
#include "procedural/world.hpp"
#include <cassert>
#include <iostream>

using namespace krono;

static int tests_passed = 0;
static int tests_total = 0;
#define TEST(name) tests_total++;
#define ENDTEST() tests_passed++; std::cout << "[OK] " << __func__ << std::endl;

static void test_init_default_quests() {
    TEST("init_default_quests creates quests");
    QuestSystem qs;
    qs.init_default_quests();
    assert(qs.quests.size() > 5);
    ENDTEST();
}

static void test_quest_starts_active() {
    TEST("quest starts in ACTIVE state");
    QuestSystem qs;
    qs.add_quest("Test", "Desc", QuestType::KILL_MOBS, 1, 0, 10);
    assert(qs.quests[0].state == QuestState::ACTIVE);
    ENDTEST();
}

static void test_kill_mob_progress() {
    TEST("kill mob increments quest progress");
    QuestSystem qs;
    qs.add_quest("Kill", "Kill 3 mobs", QuestType::KILL_MOBS, 3, 0, 10);
    qs.on_kill_mob(1);
    qs.on_kill_mob(1);
    assert(qs.quests[0].current_amount == 2);
    assert(qs.quests[0].state == QuestState::ACTIVE);
    ENDTEST();
}

static void test_kill_mob_completes_quest() {
    TEST("killing enough mobs completes quest");
    QuestSystem qs;
    qs.add_quest("Kill", "Kill 2 mobs", QuestType::KILL_MOBS, 2, 0, 10);
    qs.on_kill_mob(1);
    qs.on_kill_mob(1);
    qs.update(0.1f, 0);
    assert(qs.quests[0].state == QuestState::COMPLETED);
    ENDTEST();
}

static void test_mine_block_progress() {
    TEST("mine block increments quest");
    QuestSystem qs;
    qs.add_quest("Mine", "Mine 5 stone", QuestType::MINE_BLOCKS, 5, (uint16_t)BlockType::STONE, 10);
    qs.on_mine_block((uint16_t)BlockType::STONE);
    qs.on_mine_block((uint16_t)BlockType::DIRT);  // shouldn't count
    qs.on_mine_block((uint16_t)BlockType::STONE);
    assert(qs.quests[0].current_amount == 2);
    ENDTEST();
}

static void test_craft_item_progress() {
    TEST("craft item increments quest");
    QuestSystem qs;
    qs.add_quest("Craft", "Craft 1 sword", QuestType::CRAFT_ITEMS, 1, 0x0201, 10);
    qs.on_craft_item(0x0201);
    qs.update(0.1f, 0);
    assert(qs.quests[0].state == QuestState::COMPLETED);
    ENDTEST();
}

static void test_reach_distance_progress() {
    TEST("reach distance quest progresses");
    QuestSystem qs;
    qs.add_quest("Travel", "Go 100 pixels", QuestType::REACH_DISTANCE, 100, 0, 10);
    qs.update(0.1f, 50);
    assert(qs.quests[0].current_value == 50);
    assert(qs.quests[0].state == QuestState::ACTIVE);
    qs.update(0.1f, 150);
    assert(qs.quests[0].state == QuestState::COMPLETED);
    ENDTEST();
}

static void test_survive_time_progress() {
    TEST("survive time quest progresses");
    QuestSystem qs;
    qs.add_quest("Survive", "Survive 10 sec", QuestType::SURVIVE_TIME, 10, 0, 10);
    qs.update(5.0f, 0);
    assert(qs.quests[0].state == QuestState::ACTIVE);
    qs.update(6.0f, 0);
    assert(qs.quests[0].state == QuestState::COMPLETED);
    ENDTEST();
}

static void test_collect_items_progress() {
    TEST("collect items increments quest");
    QuestSystem qs;
    qs.add_quest("Collect", "Collect 5 wood", QuestType::COLLECT_ITEMS, 5, 0x0100, 10);
    qs.on_collect_item(0x0100, 3);
    qs.on_collect_item(0x0100, 2);
    qs.update(0.1f, 0);
    assert(qs.quests[0].state == QuestState::COMPLETED);
    ENDTEST();
}

static void test_claim_quest_gives_rewards() {
    TEST("claim quest gives XP and items");
    QuestSystem qs;
    qs.add_quest("Test", "Desc", QuestType::KILL_MOBS, 1, 0, 100, {{0x0601, 2}});
    qs.on_kill_mob(1);
    qs.update(0.1f, 0);
    int xp = 0;
    std::vector<std::pair<uint16_t, int>> items;
    bool claimed = qs.claim_quest(qs.quests[0].id, xp, items);
    assert(claimed);
    assert(xp == 100);
    assert(items.size() == 1);
    assert(items[0].first == 0x0601);
    assert(items[0].second == 2);
    assert(qs.quests[0].state == QuestState::CLAIMED);
    ENDTEST();
}

static void test_cant_claim_uncompleted_quest() {
    TEST("can't claim uncompleted quest");
    QuestSystem qs;
    qs.add_quest("Test", "Desc", QuestType::KILL_MOBS, 5, 0, 100);
    int xp = 0;
    std::vector<std::pair<uint16_t, int>> items;
    bool claimed = qs.claim_quest(qs.quests[0].id, xp, items);
    assert(!claimed);
    ENDTEST();
}

static void test_cant_claim_twice() {
    TEST("can't claim quest twice");
    QuestSystem qs;
    qs.add_quest("Test", "Desc", QuestType::KILL_MOBS, 1, 0, 100);
    qs.on_kill_mob(1);
    qs.update(0.1f, 0);
    int xp = 0;
    std::vector<std::pair<uint16_t, int>> items;
    bool first = qs.claim_quest(qs.quests[0].id, xp, items);
    bool second = qs.claim_quest(qs.quests[0].id, xp, items);
    assert(first);
    assert(!second);
    ENDTEST();
}

static void test_get_completed_unclaimed() {
    TEST("get_completed_unclaimed returns only completed");
    QuestSystem qs;
    qs.add_quest("Q1", "Desc", QuestType::KILL_MOBS, 1, 0, 100);
    qs.add_quest("Q2", "Desc", QuestType::KILL_MOBS, 5, 0, 100);
    qs.on_kill_mob(1);
    qs.update(0.1f, 0);
    auto completed = qs.get_completed_unclaimed();
    assert(completed.size() == 1);
    assert(completed[0]->title == "Q1");
    ENDTEST();
}

static void test_count_active() {
    TEST("count_active returns active quests");
    QuestSystem qs;
    qs.add_quest("Q1", "Desc", QuestType::KILL_MOBS, 1, 0, 100);
    qs.add_quest("Q2", "Desc", QuestType::KILL_MOBS, 5, 0, 100);
    qs.add_quest("Q3", "Desc", QuestType::KILL_MOBS, 10, 0, 100);
    qs.on_kill_mob(1);
    qs.update(0.1f, 0);
    assert(qs.count_active() == 2);
    ENDTEST();
}

static void test_count_completed_unclaimed() {
    TEST("count_completed_unclaimed returns count");
    QuestSystem qs;
    qs.add_quest("Q1", "Desc", QuestType::KILL_MOBS, 1, 0, 100);
    qs.add_quest("Q2", "Desc", QuestType::KILL_MOBS, 1, 0, 100);
    qs.on_kill_mob(1);
    qs.on_kill_mob(1);
    qs.update(0.1f, 0);
    assert(qs.count_completed_unclaimed() == 2);
    ENDTEST();
}

static void test_progress_string() {
    TEST("progress string formats correctly");
    QuestSystem qs;
    qs.add_quest("Q", "Desc", QuestType::KILL_MOBS, 5, 0, 10);
    qs.on_kill_mob(1);
    qs.on_kill_mob(1);
    std::string progress = qs.get_progress_string(qs.quests[0]);
    assert(progress == "2/5");
    ENDTEST();
}

static void test_state_color_differs() {
    TEST("quest state colors differ");
    float ar, ag, ab, cr, cg, cb;
    QuestSystem::get_state_color(QuestState::ACTIVE, ar, ag, ab);
    QuestSystem::get_state_color(QuestState::COMPLETED, cr, cg, cb);
    assert(cr != ar || cg != ag || cb != ab);
    ENDTEST();
}

static void test_target_id_filters_kills() {
    TEST("target_id filters which mobs count");
    QuestSystem qs;
    // Quest to kill mob type 2 (skeleton)
    qs.add_quest("Kill Skeletons", "Desc", QuestType::KILL_MOBS, 3, 2, 10);
    qs.on_kill_mob(1);  // zombie - shouldn't count
    qs.on_kill_mob(2);  // skeleton - counts
    qs.on_kill_mob(2);  // skeleton - counts
    assert(qs.quests[0].current_amount == 2);
    ENDTEST();
}

int main() {
    std::cout << "=== Quest System Tests ===" << std::endl;
    test_init_default_quests();
    test_quest_starts_active();
    test_kill_mob_progress();
    test_kill_mob_completes_quest();
    test_mine_block_progress();
    test_craft_item_progress();
    test_reach_distance_progress();
    test_survive_time_progress();
    test_collect_items_progress();
    test_claim_quest_gives_rewards();
    test_cant_claim_uncompleted_quest();
    test_cant_claim_twice();
    test_get_completed_unclaimed();
    test_count_active();
    test_count_completed_unclaimed();
    test_progress_string();
    test_state_color_differs();
    test_target_id_filters_kills();
    std::cout << "\n=== Results: " << tests_passed << "/" << tests_total << " ===" << std::endl;
    return tests_passed == tests_total ? 0 : 1;
}
