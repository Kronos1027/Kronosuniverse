// KronoUniverse — Survival System Tests (v0.5)

#include "game/survival_system.hpp"
#include <cassert>
#include <iostream>

using namespace krono;

static int tests_passed = 0;
static int tests_total = 0;
#define TEST(name) tests_total++;
#define ENDTEST() tests_passed++; std::cout << "[OK] " << __func__ << std::endl;

static Entity create_test_entity(Registry& reg) {
    Entity e = reg.create();
    reg.emplace<Health>(e, Health{100, 100});
    reg.emplace<SurvivalStats>(e, SurvivalStats{});
    return e;
}

static void test_hunger_decreases_over_time() {
    TEST("hunger decreases over time");
    Registry reg;
    Entity e = create_test_entity(reg);
    auto* s = reg.get<SurvivalStats>(e);
    float initial = s->hunger;
    SurvivalSystem::update(reg, 10.0f, 20.0f, false, false, false);
    assert(s->hunger < initial);
    ENDTEST();
}

static void test_thirst_decreases_faster_than_hunger() {
    TEST("thirst decreases faster than hunger");
    Registry reg;
    Entity e = create_test_entity(reg);
    auto* s = reg.get<SurvivalStats>(e);
    float initial_hunger = s->hunger;
    float initial_thirst = s->thirst;
    SurvivalSystem::update(reg, 10.0f, 20.0f, false, false, false);
    float hunger_lost = initial_hunger - s->hunger;
    float thirst_lost = initial_thirst - s->thirst;
    assert(thirst_lost > hunger_lost);
    ENDTEST();
}

static void test_starving_deals_damage() {
    TEST("starving deals damage over time");
    Registry reg;
    Entity e = create_test_entity(reg);
    auto* s = reg.get<SurvivalStats>(e);
    auto* hp = reg.get<Health>(e);
    s->hunger = 0;
    float initial_hp = hp->current;
    SurvivalSystem::update(reg, 5.0f, 20.0f, false, false, false);
    assert(hp->current < initial_hp);
    ENDTEST();
}

static void test_dehydration_deals_more_damage() {
    TEST("dehydration deals more damage than starvation");
    Registry reg;
    Entity e1 = create_test_entity(reg);
    Entity e2 = create_test_entity(reg);
    // Get pointers AFTER all entities created (sparse set may reallocate)
    auto* s1 = reg.get<SurvivalStats>(e1);
    auto* hp1 = reg.get<Health>(e1);
    auto* s2 = reg.get<SurvivalStats>(e2);
    auto* hp2 = reg.get<Health>(e2);
    s1->hunger = 0;
    s1->thirst = 100;
    s2->hunger = 100;
    s2->thirst = 0;

    SurvivalSystem::update(reg, 5.0f, 20.0f, false, false, false);
    float starvation_dmg = 100 - hp1->current;
    float dehydration_dmg = 100 - hp2->current;
    assert(dehydration_dmg > starvation_dmg);
    ENDTEST();
}

static void test_stamina_decreases_when_running() {
    TEST("stamina decreases when running");
    Registry reg;
    Entity e = create_test_entity(reg);
    auto* s = reg.get<SurvivalStats>(e);
    float initial = s->stamina;
    SurvivalSystem::update(reg, 5.0f, 20.0f, false, true, true);
    assert(s->stamina < initial);
    ENDTEST();
}

static void test_stamina_regenerates_when_idle() {
    TEST("stamina regenerates when idle");
    Registry reg;
    Entity e = create_test_entity(reg);
    auto* s = reg.get<SurvivalStats>(e);
    s->stamina = 50;
    SurvivalSystem::update(reg, 5.0f, 20.0f, false, false, false);
    assert(s->stamina > 50);
    ENDTEST();
}

static void test_exhausted_reduces_speed() {
    TEST("exhausted player moves slower");
    SurvivalStats s;
    s.stamina = 0;
    s.is_exhausted = true;  // manually set since we're not running update
    float speed_mult = SurvivalSystem::get_speed_multiplier(s);
    assert(speed_mult < 0.5f);
    ENDTEST();
}

static void test_eat_restores_hunger() {
    TEST("eating restores hunger");
    SurvivalStats s;
    s.hunger = 50;
    SurvivalSystem::eat(s, 30);
    assert(s.hunger == 80);
    ENDTEST();
}

static void test_eat_caps_at_100() {
    TEST("hunger caps at 100");
    SurvivalStats s;
    s.hunger = 90;
    SurvivalSystem::eat(s, 30);
    assert(s.hunger == 100);
    ENDTEST();
}

static void test_drink_restores_thirst() {
    TEST("drinking restores thirst");
    SurvivalStats s;
    s.thirst = 50;
    SurvivalSystem::drink(s, 30);
    assert(s.thirst == 80);
    ENDTEST();
}

static void test_cold_temperature_causes_hypothermia() {
    TEST("cold temperature causes hypothermia");
    Registry reg;
    Entity e = create_test_entity(reg);
    auto* s = reg.get<SurvivalStats>(e);
    SurvivalSystem::update(reg, 60.0f, -10.0f, false, false, false);  // cold env
    assert(s->is_hypothermic);
    ENDTEST();
}

static void test_hot_temperature_causes_hyperthermia() {
    TEST("hot temperature causes hyperthermia");
    Registry reg;
    Entity e = create_test_entity(reg);
    auto* s = reg.get<SurvivalStats>(e);
    SurvivalSystem::update(reg, 60.0f, 50.0f, false, false, false);  // hot env
    assert(s->is_hyperthermic);
    ENDTEST();
}

static void test_dark_cave_reduces_sanity() {
    TEST("dark cave reduces sanity");
    Registry reg;
    Entity e = create_test_entity(reg);
    auto* s = reg.get<SurvivalStats>(e);
    float initial = s->sanity;
    SurvivalSystem::update(reg, 30.0f, 20.0f, true, false, false);  // in cave
    assert(s->sanity < initial);
    ENDTEST();
}

static void test_daylight_restores_sanity() {
    TEST("daylight restores sanity");
    Registry reg;
    Entity e = create_test_entity(reg);
    auto* s = reg.get<SurvivalStats>(e);
    s->sanity = 50;
    SurvivalSystem::update(reg, 30.0f, 20.0f, false, false, false);  // not in cave
    assert(s->sanity > 50);
    ENDTEST();
}

static void test_sleep_restores_stamina() {
    TEST("sleeping restores stamina");
    SurvivalStats s;
    s.stamina = 20;
    SurvivalSystem::sleep(s, 5.0f);
    assert(s.stamina == s.max_stamina);
    ENDTEST();
}

static void test_can_perform_action_when_enough_stamina() {
    TEST("can perform action when enough stamina");
    SurvivalStats s;
    s.stamina = 50;
    assert(SurvivalSystem::can_perform_action(s, 30));
    assert(!SurvivalSystem::can_perform_action(s, 60));
    ENDTEST();
}

static void test_consume_stamina() {
    TEST("consume stamina reduces stamina");
    SurvivalStats s;
    s.stamina = 50;
    SurvivalSystem::consume_stamina(s, 20);
    assert(s.stamina == 30);
    ENDTEST();
}

static void test_jump_cost_more_than_run() {
    TEST("jump costs more stamina than running");
    SurvivalStats s;
    assert(s.stamina_jump_cost > s.stamina_run_cost);
    ENDTEST();
}

static void test_hp_doesnt_go_negative() {
    TEST("HP doesn't go negative");
    Registry reg;
    Entity e = create_test_entity(reg);
    auto* s = reg.get<SurvivalStats>(e);
    auto* hp = reg.get<Health>(e);
    s->hunger = 0;
    s->thirst = 0;
    SurvivalSystem::update(reg, 1000.0f, 20.0f, false, false, false);  // very long
    assert(hp->current >= 0);
    ENDTEST();
}

int main() {
    std::cout << "=== Survival System Tests ===" << std::endl;
    test_hunger_decreases_over_time();
    test_thirst_decreases_faster_than_hunger();
    test_starving_deals_damage();
    test_dehydration_deals_more_damage();
    test_stamina_decreases_when_running();
    test_stamina_regenerates_when_idle();
    test_exhausted_reduces_speed();
    test_eat_restores_hunger();
    test_eat_caps_at_100();
    test_drink_restores_thirst();
    test_cold_temperature_causes_hypothermia();
    test_hot_temperature_causes_hyperthermia();
    test_dark_cave_reduces_sanity();
    test_daylight_restores_sanity();
    test_sleep_restores_stamina();
    test_can_perform_action_when_enough_stamina();
    test_consume_stamina();
    test_jump_cost_more_than_run();
    test_hp_doesnt_go_negative();
    std::cout << "\n=== Results: " << tests_passed << "/" << tests_total << " ===" << std::endl;
    return tests_passed == tests_total ? 0 : 1;
}
