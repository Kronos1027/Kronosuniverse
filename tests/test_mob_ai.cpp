// KronoUniverse — Mob AI Tests (v0.3)

#include "game/mob_ai.hpp"
#include <cassert>
#include <iostream>

using namespace krono;

static int tests_passed = 0;
static int tests_total = 0;
#define TEST(name) tests_total++;
#define ENDTEST() tests_passed++; std::cout << "[OK] " << __func__ << std::endl;

static Entity create_player(Registry& reg, float x, float y) {
    Entity p = reg.create();
    reg.emplace<Position>(p, Position{x, y});
    reg.emplace<Health>(p, Health{100, 100});
    return p;
}

static void test_spawn_zombie() {
    TEST("zombie spawn");
    Registry reg;
    Entity z = MobAISystem::spawn_mob(reg, MobType::ZOMBIE, 100, 100);
    assert(reg.has<MobAI>(z));
    assert(reg.has<Health>(z));
    auto* ai = reg.get<MobAI>(z);
    assert(ai->type == MobType::ZOMBIE);
    assert(!ai->is_passive);
    auto* hp = reg.get<Health>(z);
    assert(hp->max == 40);
    ENDTEST();
}

static void test_spawn_boss_high_hp() {
    TEST("boss has high HP");
    Registry reg;
    Entity boss = MobAISystem::spawn_mob(reg, MobType::BOSS, 0, 0);
    auto* hp = reg.get<Health>(boss);
    auto* ai = reg.get<MobAI>(boss);
    assert(hp->max == 500);
    assert(ai->is_boss);
    assert(ai->xp_reward == 500);
    ENDTEST();
}

static void test_zombie_chases_player() {
    TEST("zombie chases player when in range");
    Registry reg;
    Entity player = create_player(reg, 0, 0);
    Entity zombie = MobAISystem::spawn_mob(reg, MobType::ZOMBIE, 100, 0);
    MobAISystem::update(reg, player, 0.016f, 490.0f);
    auto* ai = reg.get<MobAI>(zombie);
    assert(ai->state == MobState::CHASE);
    auto* vel = reg.get<Velocity>(zombie);
    assert(vel->x < 0);  // moving toward player (negative x)
    ENDTEST();
}

static void test_zombie_idle_when_far() {
    TEST("zombie wanders when player far away");
    Registry reg;
    Entity player = create_player(reg, 0, 0);
    Entity zombie = MobAISystem::spawn_mob(reg, MobType::ZOMBIE, 1000, 0);  // very far
    MobAISystem::update(reg, player, 0.016f, 490.0f);
    auto* ai = reg.get<MobAI>(zombie);
    assert(ai->state == MobState::WANDER);
    ENDTEST();
}

static void test_passive_mob_flees() {
    TEST("passive mob flees from player");
    Registry reg;
    Entity player = create_player(reg, 0, 0);
    Entity deer = MobAISystem::spawn_mob(reg, MobType::DEER, 100, 0);
    MobAISystem::update(reg, player, 0.016f, 490.0f);
    auto* ai = reg.get<MobAI>(deer);
    assert(ai->state == MobState::FLEE);
    auto* vel = reg.get<Velocity>(deer);
    assert(vel->x > 0);  // moving away from player
    ENDTEST();
}

static void test_bat_is_flying() {
    TEST("bat moves in Y direction (flying)");
    Registry reg;
    Entity player = create_player(reg, 0, -200);  // player above
    Entity bat = MobAISystem::spawn_mob(reg, MobType::BAT, 50, 0);
    MobAISystem::update(reg, player, 0.016f, 490.0f);
    auto* vel = reg.get<Velocity>(bat);
    assert(vel->y < 0);  // moving up toward player
    ENDTEST();
}

static void test_skeleton_ranged_attack() {
    TEST("skeleton has ranged weapon");
    Registry reg;
    Entity skel = MobAISystem::spawn_mob(reg, MobType::SKELETON, 0, 0);
    auto* ew = reg.get<EquippedWeapon>(skel);
    assert(ew->stats.type == WeaponType::BOW);
    assert(ew->stats.projectile_speed > 0);  // ranged
    ENDTEST();
}

static void test_mob_dies_when_hp_zero() {
    TEST("mob is marked dead when HP reaches 0");
    Registry reg;
    Entity player = create_player(reg, 0, 0);
    Entity zombie = MobAISystem::spawn_mob(reg, MobType::ZOMBIE, 100, 0);
    auto* hp = reg.get<Health>(zombie);
    hp->current = 0;
    // First update marks the mob as DEAD
    MobAISystem::update(reg, player, 0.016f, 490.0f);
    // Second update cleans up dead mobs (destroys them)
    MobAISystem::update(reg, player, 0.016f, 490.0f);
    // Mob should now be destroyed (no MobAI component)
    auto* ai = reg.get<MobAI>(zombie);
    assert(ai == nullptr);  // cleaned up
    ENDTEST();
}

static void test_mob_freeze_status() {
    TEST("frozen mob can't move");
    Registry reg;
    Entity player = create_player(reg, 0, 0);
    Entity zombie = MobAISystem::spawn_mob(reg, MobType::ZOMBIE, 100, 0);
    auto* sc = reg.get<StatusContainer>(zombie);
    sc->apply(StatusEffect::FREEZE, 2.0f, 0.5f);
    
    MobAISystem::update(reg, player, 0.016f, 490.0f);
    auto* vel = reg.get<Velocity>(zombie);
    // Frozen mob shouldn't move toward player
    assert(vel->x == 0);
    ENDTEST();
}

static void test_different_mob_hp() {
    TEST("different mobs have different HP");
    Registry reg;
    Entity zombie = MobAISystem::spawn_mob(reg, MobType::ZOMBIE, 0, 0);
    Entity slime = MobAISystem::spawn_mob(reg, MobType::SLIME, 100, 0);
    Entity bat = MobAISystem::spawn_mob(reg, MobType::BAT, 200, 0);
    Entity boss = MobAISystem::spawn_mob(reg, MobType::BOSS, 300, 0);
    
    assert(reg.get<Health>(zombie)->max == 40);
    assert(reg.get<Health>(slime)->max == 25);
    assert(reg.get<Health>(bat)->max == 15);
    assert(reg.get<Health>(boss)->max == 500);
    ENDTEST();
}

static void test_mob_colors() {
    TEST("mob colors are different");
    Registry reg;
    float r1,g1,b1, r2,g2,b2;
    MobAISystem::get_mob_color(MobType::ZOMBIE, r1, g1, b1);
    MobAISystem::get_mob_color(MobType::SKELETON, r2, g2, b2);
    // Skeleton should be brighter than zombie
    assert(r2 > r1);
    ENDTEST();
}

int main() {
    std::cout << "=== Mob AI Tests ===" << std::endl;
    test_spawn_zombie();
    test_spawn_boss_high_hp();
    test_zombie_chases_player();
    test_zombie_idle_when_far();
    test_passive_mob_flees();
    test_bat_is_flying();
    test_skeleton_ranged_attack();
    test_mob_dies_when_hp_zero();
    test_mob_freeze_status();
    test_different_mob_hp();
    test_mob_colors();
    
    std::cout << "\n=== Results: " << tests_passed << "/" << tests_total << " ===" << std::endl;
    return tests_passed == tests_total ? 0 : 1;
}
