// KronoUniverse — Combat System Tests (v0.3)
// Tests for: weapons, projectiles, status effects, melee, ranged attacks

#include "game/combat_system.hpp"
#include <cassert>
#include <iostream>
#include <cmath>

using namespace krono;

static int tests_passed = 0;
static int tests_total = 0;

#define TEST(name) tests_total++; if (true) { /* test start */
#define ENDTEST() tests_passed++; } std::cout << "[OK] " << __func__ << std::endl;

// Helper: create test entity
static Entity create_test_entity(Registry& reg, float x, float y, bool is_player, float hp = 100) {
    Entity e = reg.create();
    reg.emplace<Position>(e, Position{x, y});
    reg.emplace<Velocity>(e, Velocity{0, 0});
    reg.emplace<Mass>(e, Mass{70});
    reg.emplace<RigidBody>(e, RigidBody{0.0f, 0.5f, false, false});
    reg.emplace<AABBCollider>(e, AABBCollider{24, 32});
    reg.emplace<Health>(e, Health{hp, hp});
    reg.emplace<StatusContainer>(e, StatusContainer{});
    if (is_player) reg.emplace<TagPlayer>(e, TagPlayer{});
    return e;
}

static void test_fist_attack_deals_damage() {
    TEST("fist attack deals damage");
    Registry reg;
    Entity attacker = create_test_entity(reg, 0, 0, true);
    Entity target = create_test_entity(reg, 30, 0, false, 100);
    reg.emplace<EquippedWeapon>(attacker, EquippedWeapon{CombatSystem::make_fist()});
    
    bool success = CombatSystem::attack(reg, attacker, 30, 0);
    assert(success);
    
    auto* thp = reg.get<Health>(target);
    assert(thp->current < 100);
    assert(thp->current >= 95);  // 5 base dmg, no crit
    ENDTEST();
}

static void test_sword_does_more_damage() {
    TEST("sword does more damage than fist");
    Registry reg;
    
    // Fist test
    Entity a1 = create_test_entity(reg, 0, 0, true);
    Entity t1 = create_test_entity(reg, 30, 0, false, 1000);
    reg.emplace<EquippedWeapon>(a1, EquippedWeapon{CombatSystem::make_fist()});
    CombatSystem::attack(reg, a1, 30, 0);
    float fist_dmg = 1000 - reg.get<Health>(t1)->current;
    
    // Sword test
    Entity a2 = create_test_entity(reg, 0, 200, true);
    Entity t2 = create_test_entity(reg, 30, 200, false, 1000);
    reg.emplace<EquippedWeapon>(a2, EquippedWeapon{CombatSystem::make_sword()});
    CombatSystem::attack(reg, a2, 30, 200);
    float sword_dmg = 1000 - reg.get<Health>(t2)->current;
    
    assert(sword_dmg > fist_dmg);
    ENDTEST();
}

static void test_attack_cooldown() {
    TEST("attack cooldown prevents rapid attacks");
    Registry reg;
    Entity attacker = create_test_entity(reg, 0, 0, true);
    Entity target = create_test_entity(reg, 30, 0, false, 1000);
    reg.emplace<EquippedWeapon>(attacker, EquippedWeapon{CombatSystem::make_sword()});
    
    bool first = CombatSystem::attack(reg, attacker, 30, 0);
    bool second = CombatSystem::attack(reg, attacker, 30, 0);
    
    assert(first);
    assert(!second);  // on cooldown
    ENDTEST();
}

static void test_cooldown_recovery() {
    TEST("cooldown recovers over time");
    Registry reg;
    Entity attacker = create_test_entity(reg, 0, 0, true);
    reg.emplace<EquippedWeapon>(attacker, EquippedWeapon{CombatSystem::make_fist()});
    
    CombatSystem::attack(reg, attacker, 30, 0);
    assert(!CombatSystem::attack(reg, attacker, 30, 0));  // still on CD
    
    CombatSystem::update_cooldowns(reg, 1.0f);  // pass 1 sec (fist CD = 0.4)
    
    assert(CombatSystem::attack(reg, attacker, 30, 0));  // CD cleared
    ENDTEST();
}

static void test_bow_spawns_projectile() {
    TEST("bow spawns projectile entity");
    Registry reg;
    Entity attacker = create_test_entity(reg, 0, 0, true);
    reg.emplace<EquippedWeapon>(attacker, EquippedWeapon{CombatSystem::make_bow()});
    
    size_t before = reg.alive();
    CombatSystem::attack(reg, attacker, 100, 0);
    size_t after = reg.alive();
    
    assert(after == before + 1);  // projectile spawned
    ENDTEST();
}

static void test_projectile_lifetime() {
    TEST("projectile expires after lifetime");
    Registry reg;
    Entity attacker = create_test_entity(reg, 0, 0, true);
    reg.emplace<EquippedWeapon>(attacker, EquippedWeapon{CombatSystem::make_bow()});
    
    CombatSystem::attack(reg, attacker, 100, 0);
    size_t after_spawn = reg.alive();
    
    // Bow: range 600, speed 600 → lifetime 1 sec
    CombatSystem::update_projectiles(reg, 1.5f);
    size_t after_expire = reg.alive();
    
    assert(after_expire < after_spawn);  // projectile destroyed
    ENDTEST();
}

static void test_projectile_hits_target() {
    TEST("projectile hits target and deals damage");
    Registry reg;
    Entity attacker = create_test_entity(reg, 0, 0, true);
    Entity target = create_test_entity(reg, 200, 0, false, 1000);
    reg.emplace<EquippedWeapon>(attacker, EquippedWeapon{CombatSystem::make_bow()});

    CombatSystem::attack(reg, attacker, 200, 0);
    // Simulate small steps so projectile doesn't skip past target
    for (int i = 0; i < 40; i++) {
        CombatSystem::update_projectiles(reg, 0.01f);
        CombatSystem::check_projectile_hits(reg);
    }

    auto* thp = reg.get<Health>(target);
    assert(thp->current < 1000);  // took damage
    ENDTEST();
}

static void test_melee_knockback() {
    TEST("melee attack applies knockback");
    Registry reg;
    Entity attacker = create_test_entity(reg, 0, 0, true);
    Entity target = create_test_entity(reg, 30, 0, false, 1000);
    reg.emplace<EquippedWeapon>(attacker, EquippedWeapon{CombatSystem::make_sword()});
    
    CombatSystem::attack(reg, attacker, 30, 0);
    
    auto* tv = reg.get<Velocity>(target);
    assert(tv->x > 0);  // pushed away
    ENDTEST();
}

static void test_fire_weapon_applies_burn() {
    TEST("fire damage applies burn status");
    Registry reg;
    Entity attacker = create_test_entity(reg, 0, 0, true);
    Entity target = create_test_entity(reg, 30, 0, false, 1000);
    auto fire_staff = CombatSystem::make_fire_staff();
    fire_staff.projectile_speed = 0;  // make it melee for test
    reg.emplace<EquippedWeapon>(attacker, EquippedWeapon{fire_staff});
    
    CombatSystem::attack(reg, attacker, 30, 0);
    
    auto* sc = reg.get<StatusContainer>(target);
    assert(sc->has(StatusEffect::BURN));
    ENDTEST();
}

static void test_burn_damage_over_time() {
    TEST("burn deals damage over time");
    Registry reg;
    Entity target = create_test_entity(reg, 0, 0, false, 100);
    reg.emplace<StatusContainer>(target, StatusContainer{});
    auto* sc = reg.get<StatusContainer>(target);
    sc->apply(StatusEffect::BURN, 3.0f, 10.0f);  // 10 dmg per sec for 3 sec
    
    float hp_before = reg.get<Health>(target)->current;
    
    CombatSystem::update_statuses(reg, 1.0f);  // 1 second
    
    float hp_after = reg.get<Health>(target)->current;
    assert(hp_after < hp_before);
    assert(hp_before - hp_after >= 10.0f);  // at least 10 dmg
    ENDTEST();
}

static void test_poison_long_duration() {
    TEST("poison lasts longer than burn");
    Registry reg;
    Entity target = create_test_entity(reg, 0, 0, false, 1000);
    reg.emplace<StatusContainer>(target, StatusContainer{});
    auto* sc = reg.get<StatusContainer>(target);
    sc->apply(StatusEffect::POISON, 5.0f, 5.0f);
    sc->apply(StatusEffect::BURN, 3.0f, 5.0f);
    
    // After 4 seconds, poison should still be active, burn gone
    for (int i = 0; i < 4; i++) CombatSystem::update_statuses(reg, 1.0f);
    
    assert(sc->has(StatusEffect::POISON));
    assert(!sc->has(StatusEffect::BURN));
    ENDTEST();
}

static void test_regen_heals() {
    TEST("regen status heals HP");
    Registry reg;
    Entity target = create_test_entity(reg, 0, 0, false, 100);
    auto* hp = reg.get<Health>(target);
    hp->current = 50;
    reg.emplace<StatusContainer>(target, StatusContainer{});
    auto* sc = reg.get<StatusContainer>(target);
    sc->apply(StatusEffect::REGEN, 5.0f, 5.0f);
    
    CombatSystem::update_statuses(reg, 1.0f);
    
    assert(hp->current > 50);
    ENDTEST();
}

static void test_no_friendly_fire() {
    TEST("no friendly fire between same team");
    Registry reg;
    Entity p1 = create_test_entity(reg, 0, 0, true, 100);
    Entity p2 = create_test_entity(reg, 30, 0, true, 100);
    reg.emplace<EquippedWeapon>(p1, EquippedWeapon{CombatSystem::make_sword()});
    
    CombatSystem::attack(reg, p1, 30, 0);
    
    auto* hp2 = reg.get<Health>(p2);
    assert(hp2->current == 100);  // no damage to friendly
    ENDTEST();
}

static void test_gun_stronger_than_bow() {
    TEST("gun deals more damage than bow");
    Registry reg;
    Entity bow_att = create_test_entity(reg, 0, 0, true);
    Entity gun_att = create_test_entity(reg, 0, 200, true);
    // Place targets far enough that projectiles don't skip past during grace period
    Entity bow_tgt = create_test_entity(reg, 300, 0, false, 1000);
    Entity gun_tgt = create_test_entity(reg, 300, 200, false, 1000);
    reg.emplace<EquippedWeapon>(bow_att, EquippedWeapon{CombatSystem::make_bow()});
    reg.emplace<EquippedWeapon>(gun_att, EquippedWeapon{CombatSystem::make_gun()});

    CombatSystem::attack(reg, bow_att, 300, 0);
    CombatSystem::attack(reg, gun_att, 300, 200);

    // Step in small increments so projectiles don't skip past targets
    for (int i = 0; i < 100; i++) {
        CombatSystem::update_projectiles(reg, 0.01f);
        CombatSystem::check_projectile_hits(reg);
    }

    float bow_dmg = 1000 - reg.get<Health>(bow_tgt)->current;
    float gun_dmg = 1000 - reg.get<Health>(gun_tgt)->current;

    // Gun base damage 35 > bow base 18
    assert(gun_dmg > bow_dmg);
    ENDTEST();
}

int main() {
    std::cout << "=== Combat System Tests ===" << std::endl;
    test_fist_attack_deals_damage();
    test_sword_does_more_damage();
    test_attack_cooldown();
    test_cooldown_recovery();
    test_bow_spawns_projectile();
    test_projectile_lifetime();
    test_projectile_hits_target();
    test_melee_knockback();
    test_fire_weapon_applies_burn();
    test_burn_damage_over_time();
    test_poison_long_duration();
    test_regen_heals();
    test_no_friendly_fire();
    test_gun_stronger_than_bow();
    
    std::cout << "\n=== Results: " << tests_passed << "/" << tests_total << " ===" << std::endl;
    return tests_passed == tests_total ? 0 : 1;
}
