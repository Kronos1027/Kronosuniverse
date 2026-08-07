// Test: War, Crime, Politics & God System (Fase 8 + 9)
#include "game/war_politics_god.hpp"
#include <iostream>
#include <cassert>

using namespace krono;

int main() {
    std::cout << "=== War, Crime, Politics & God Tests (Fase 8+9) ===" << std::endl;

    // TEST 1: Crime system
    {
        std::cout << "\n--- Test 1: Crime ---" << std::endl;
        CrimeRecord record;
        Crime c{CrimeType::MURDER, 1, 2, 5, 10, 500, 1000, false, false};
        record.commit(c);
        assert(record.crimes.size() == 1);
        assert(record.total_bounty == 500);
        std::cout << "  Crime: MURDER, bounty=500, total=" << record.total_bounty << std::endl;

        Crime c2{CrimeType::THEFT, 1, 3, 5, 3, 100, 1001, false, false};
        record.commit(c2);
        assert(record.total_bounty == 600);
        assert(record.unresolved_count() == 2);
        std::cout << "  After theft: bounty=600, unresolved=2" << std::endl;
        std::cout << "  ✓ Crime system works" << std::endl;
    }

    // TEST 2: Declare war
    {
        std::cout << "\n--- Test 2: War declaration ---" << std::endl;
        WarSystem wars;
        wars.declare_war(1, 2, 1000, 800, 0);
        assert(wars.active_wars() == 1);
        assert(wars.at_war(1, 2));
        assert(wars.at_war(2, 1));
        std::cout << "  War: faction 1 vs 2, active=" << wars.active_wars() << std::endl;

        // Can't declare same war twice
        wars.declare_war(1, 2, 500, 400, 1);
        assert(wars.active_wars() == 1);
        std::cout << "  Duplicate war rejected" << std::endl;
        std::cout << "  ✓ War declaration works" << std::endl;
    }

    // TEST 3: War simulation + resolution
    {
        std::cout << "\n--- Test 3: War simulation ---" << std::endl;
        WarSystem wars;
        wars.declare_war(1, 2, 100, 50, 0);

        // Simulate many ticks
        for (int i = 0; i < 200; i++) wars.simulate_tick();

        // Defender should lose (weaker)
        assert(wars.active_wars() == 0); // war ended
        std::cout << "  After 200 ticks: war ended" << std::endl;
        std::cout << "  ✓ War resolves when one side defeated" << std::endl;
    }

    // TEST 4: Peace negotiation
    {
        std::cout << "\n--- Test 4: Peace ---" << std::endl;
        WarSystem wars;
        wars.declare_war(1, 2, 1000, 1000, 0);
        assert(wars.at_war(1, 2));
        wars.negotiate_peace(1, 2);
        assert(!wars.at_war(1, 2));
        std::cout << "  Peace negotiated, war ended" << std::endl;
        std::cout << "  ✓ Peace negotiation works" << std::endl;
    }

    // TEST 5: Political actions
    {
        std::cout << "\n--- Test 5: Politics ---" << std::endl;
        FactionData f1{1, "Kingdom A", "", 200, 100, 100};
        f1.relations[2] = 0;
        FactionData f2{2, "Kingdom B", "", 100, 200, 100};
        f2.relations[1] = 0;
        WarSystem wars;
        PoliticalSystem politics;

        // Diplomatic gift: +10
        float change = politics.perform_action(PoliticalAction::DIPLOMATIC_GIFT, f1, f2, wars, 0);
        assert(change == 10);
        assert(f1.relations[2] == 10);
        std::cout << "  Gift: relations +10 → " << (int)f1.relations[2] << std::endl;

        // Trade agreement: +15, shares tech
        f1.tech_level = 3;
        f2.tech_level = 7;
        politics.perform_action(PoliticalAction::TRADE_AGREEMENT, f1, f2, wars, 0);
        assert(f1.tech_level == 7); // f1 gets f2's tech
        std::cout << "  Trade: f1 tech 3→" << f1.tech_level << " (shared from f2)" << std::endl;

        // Declare war: -100
        politics.perform_action(PoliticalAction::DECLARE_WAR, f1, f2, wars, 0);
        assert(wars.at_war(1, 2));
        assert(f1.relations[2] <= 0); // negative after war
        std::cout << "  War declared: relations=" << (int)f1.relations[2] << std::endl;

        // Sue for peace: +5, ends war
        politics.perform_action(PoliticalAction::SUE_FOR_PEACE, f1, f2, wars, 0);
        assert(!wars.at_war(1, 2));
        std::cout << "  Peace: war ended" << std::endl;
        std::cout << "  ✓ Political system works" << std::endl;
    }

    // TEST 6: Annex territory
    {
        std::cout << "\n--- Test 6: Annex ---" << std::endl;
        FactionData f1{1, "Empire", "", 200, 50, 50};
        f1.territory_count = 5;
        FactionData f2{2, "City State", "", 100, 200, 50};
        f2.territory_count = 2;
        WarSystem wars;
        PoliticalSystem politics;

        politics.perform_action(PoliticalAction::ANNEX_TERRITORY, f1, f2, wars, 0);
        assert(f1.territory_count == 6);
        assert(f2.territory_count == 1);
        std::cout << "  Empire: 5→" << f1.territory_count << " City: 2→" << f2.territory_count << std::endl;
        std::cout << "  ✓ Annexation works" << std::endl;
    }

    // TEST 7: God awakening + challenge requirements
    {
        std::cout << "\n--- Test 7: God challenge ---" << std::endl;
        GodSystem god;
        assert(!god.god_awakened);
        assert(god.god_alive);

        // Player doesn't meet requirements yet
        assert(!god.can_challenge(50, 10, 1, 1, 5, false));
        std::cout << "  Player too weak to challenge God" << std::endl;

        // Meet all requirements
        god.god_awakened = true;
        god.requirements.has_god_key = true;
        assert(god.can_challenge(100, 50, 5, 3, 20, true));
        std::cout << "  Player meets all requirements → can challenge" << std::endl;
        std::cout << "  ✓ God challenge gating works" << std::endl;
    }

    // TEST 8: God combat + ascension
    {
        std::cout << "\n--- Test 8: God combat ---" << std::endl;
        GodSystem god;
        god.god_awakened = true;
        god.requirements.has_god_key = true;
        god.god_defense = 100;

        // God attacks
        float god_dmg = god.god_attack();
        assert(god_dmg == 5000);
        std::cout << "  God attack: " << god_dmg << " damage" << std::endl;

        // Player damages God
        float actual = god.damage_god(200); // 200 raw - 100 def = 100 actual
        assert(actual == 100);
        std::cout << "  Player hits God: 200 raw - 100 def = " << actual << " actual" << std::endl;
        assert(god.god_hp == 100000 - 100);

        // Kill God
        while (god.god_alive) {
            god.damage_god(100000); // massive damage
        }
        assert(!god.god_alive);
        assert(god.god_hp == 0);
        std::cout << "  God defeated! HP=0" << std::endl;

        // Can ascend
        assert(god.can_ascend());
        std::cout << "  ✓ Player can ascend to godhood" << std::endl;
    }

    std::cout << "\n=== All War/Crime/Politics/God tests passed! ✓ ===" << std::endl;
    return 0;
}
