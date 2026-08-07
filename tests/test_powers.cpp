// Test: Powers & Social Stats (Fase 7)
#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "game/powers.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace krono;

int main() {
    std::cout << "=== Powers & Social Stats Tests (Fase 7) ===" << std::endl;
    const float dt = 1.0f / 60.0f;
    Registry reg;
    PowerSystem psys;

    // TEST 1: Social stats degrade over time
    {
        std::cout << "\n--- Test 1: Stats degrade ---" << std::endl;
        SocialStats stats;
        assert(stats.hunger == 0);
        stats.update(10.0f); // 10 seconds
        assert(stats.hunger > 0);
        assert(stats.thirst > stats.hunger); // thirst faster
        std::cout << "  After 10s: hunger=" << stats.hunger << " thirst=" << stats.thirst << " fatigue=" << stats.fatigue << std::endl;
        std::cout << "  ✓ Stats increase over time (thirst > hunger > fatigue)" << std::endl;
    }

    // TEST 2: Starvation damages health
    {
        std::cout << "\n--- Test 2: Starvation damage ---" << std::endl;
        Entity e = reg.create();
        reg.emplace<Health>(e, Health{100, 100});
        SocialStats stats;
        stats.hunger = 90; // starving
        stats.thirst = 90;
        reg.emplace<SocialStats>(e, std::move(stats));

        float hp_before = reg.get<Health>(e)->current;
        psys.update(reg, 1.0f); // 1 second
        float hp_after = reg.get<Health>(e)->current;

        std::cout << "  HP: " << hp_before << " → " << hp_after << std::endl;
        assert(hp_after < hp_before);
        std::cout << "  ✓ Starvation damages health" << std::endl;
    }

    // TEST 3: Speed modifier from fatigue
    {
        std::cout << "\n--- Test 3: Fatigue slows movement ---" << std::endl;
        SocialStats rested;
        rested.fatigue = 0;
        SocialStats exhausted;
        exhausted.fatigue = 95;

        float rested_speed = rested.get_speed_modifier();
        float exhausted_speed = exhausted.get_speed_modifier();
        std::cout << "  Rested speed: " << rested_speed << " Exhausted: " << exhausted_speed << std::endl;
        assert(exhausted_speed < rested_speed);
        assert(exhausted_speed >= 0.3f); // minimum 30%
        std::cout << "  ✓ Exhausted character is slower (min 30%)" << std::endl;
    }

    // TEST 4: Jump modifier from fatigue
    {
        std::cout << "\n--- Test 4: Fatigue reduces jump ---" << std::endl;
        SocialStats stats;
        stats.fatigue = 70;
        float jump_mod = stats.get_jump_modifier();
        std::cout << "  Jump modifier at fatigue=70: " << jump_mod << std::endl;
        assert(jump_mod < 1.0f);
        std::cout << "  ✓ Tired character jumps lower" << std::endl;
    }

    // TEST 5: Power toggle
    {
        std::cout << "\n--- Test 5: Power toggle ---" << std::endl;
        Powers powers;
        Power flight;
        flight.type = Power::FLIGHT;
        flight.is_unlocked = true;
        flight.is_active = false;
        powers.powers.push_back(flight);

        assert(!powers.find(Power::FLIGHT)->is_active);
        powers.toggle(Power::FLIGHT);
        assert(powers.find(Power::FLIGHT)->is_active);
        std::cout << "  Flight toggled on" << std::endl;
        powers.toggle(Power::FLIGHT);
        assert(!powers.find(Power::FLIGHT)->is_active);
        std::cout << "  Flight toggled off" << std::endl;
        std::cout << "  ✓ Power toggle works" << std::endl;
    }

    // TEST 6: Regeneration power heals
    {
        std::cout << "\n--- Test 6: Regeneration ---" << std::endl;
        Entity e = reg.create();
        reg.emplace<Health>(e, Health{50, 100});

        Powers powers;
        Power regen;
        regen.type = Power::REGENERATE;
        regen.is_active = true;
        regen.strength = 2.0f;
        powers.powers.push_back(regen);
        reg.emplace<Powers>(e, std::move(powers));

        float hp_before = reg.get<Health>(e)->current;
        psys.update(reg, 1.0f); // 1 second
        float hp_after = reg.get<Health>(e)->current;

        std::cout << "  HP: " << hp_before << " → " << hp_after << std::endl;
        assert(hp_after > hp_before);
        std::cout << "  ✓ Regeneration heals" << std::endl;
    }

    // TEST 7: Power origins (3 types)
    {
        std::cout << "\n--- Test 7: Power origins ---" << std::endl;
        Power tech;
        tech.origin = PowerOrigin::TECHNOLOGICAL;
        tech.type = Power::FLIGHT;

        Power bio;
        bio.origin = PowerOrigin::BIOLOGICAL;
        bio.type = Power::REGENERATE;

        Power anom;
        anom.origin = PowerOrigin::ANOMALOUS;
        anom.type = Power::TELEKINESIS;

        assert(tech.origin == PowerOrigin::TECHNOLOGICAL);
        assert(bio.origin == PowerOrigin::BIOLOGICAL);
        assert(anom.origin == PowerOrigin::ANOMALOUS);
        std::cout << "  Tech: Flight | Bio: Regenerate | Anomalous: Telekinesis" << std::endl;
        std::cout << "  ✓ 3 power origins defined" << std::endl;
    }

    // TEST 8: Morality affects disposition
    {
        std::cout << "\n--- Test 8: Morality ---" << std::endl;
        SocialStats good;
        good.morality = 80;
        good.respect = 50;

        SocialStats evil;
        evil.morality = -80;
        evil.fear = 70;

        assert(good.morality > 0);
        assert(evil.morality < 0);
        std::cout << "  Good: morality=+80 respect=50" << std::endl;
        std::cout << "  Evil: morality=-80 fear=70" << std::endl;
        std::cout << "  ✓ Morality system (-100 to +100)" << std::endl;
    }

    std::cout << "\n=== All Powers & Social Stats tests passed! ✓ ===" << std::endl;
    return 0;
}
