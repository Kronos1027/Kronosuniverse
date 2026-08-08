#pragma once
// KronoUniverse — War, Crime & Politics (Fase 8, GDD v0.1 sec 7-8; v0.2 sec 5)
// + Endgame: God System (Fase 9, GDD v0.2 sec 6)

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "game/npc_system.hpp"
#include "game/powers.hpp"
#include "procedural/universe.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>

namespace krono {

// ---- Crime System ----
enum class CrimeType : uint8_t {
    THEFT, ASSAULT, MURDER, TRESPASSING, CONTRABAND, TREASON, VANDALISM
};

struct Crime {
    CrimeType type;
    uint32_t perpetrator_id;   // entity or player
    uint32_t victim_id;        // entity or faction
    uint32_t faction_id;       // where crime happened
    float severity;            // 1-10 (affects bounty)
    float bounty;              // credits for capture
    uint64_t timestamp;
    bool reported = false;     // witnesses?
    bool resolved = false;     // caught/pardoned?
};

struct CrimeRecord {
    std::vector<Crime> crimes;
    float total_bounty = 0;

    void commit(Crime c) {
        crimes.push_back(c);
        total_bounty += c.bounty;
    }

    int crime_count(CrimeType type) const {
        int n = 0;
        for (auto& c : crimes) if (c.type == type && !c.resolved) n++;
        return n;
    }

    int unresolved_count() const {
        int n = 0;
        for (auto& c : crimes) if (!c.resolved) n++;
        return n;
    }
};

// ---- War System ----
struct War {
    uint32_t attacker_faction;
    uint32_t defender_faction;
    uint64_t start_tick;
    float attacker_strength;
    float defender_strength;
    bool active = true;
    bool ceasefire = false;

    // War resolution
    enum Outcome { ONGOING, ATTACKER_WINS, DEFENDER_WINS, PEACE_TREATY };
    Outcome resolve() {
        if (!active) return PEACE_TREATY;
        if (ceasefire) return PEACE_TREATY;
        if (attacker_strength <= 0) return DEFENDER_WINS;
        if (defender_strength <= 0) return ATTACKER_WINS;
        return ONGOING;
    }
};

struct WarSystem {
    std::vector<War> wars;

    void declare_war(uint32_t attacker, uint32_t defender, float atk_str, float def_str, uint64_t tick) {
        // Check if already at war
        for (auto& w : wars) {
            if (w.active && ((w.attacker_faction == attacker && w.defender_faction == defender) ||
                            (w.attacker_faction == defender && w.defender_faction == attacker))) {
                return; // already at war
            }
        }
        wars.push_back({attacker, defender, tick, atk_str, def_str, true, false});
    }

    void negotiate_peace(uint32_t f1, uint32_t f2) {
        for (auto& w : wars) {
            if (w.active && ((w.attacker_faction == f1 && w.defender_faction == f2) ||
                            (w.attacker_faction == f2 && w.defender_faction == f1))) {
                w.ceasefire = true;
                w.active = false;
            }
        }
    }

    // Simulate one tick of war (Parte 7.2 — Camada 1)
    void simulate_tick() {
        for (auto& w : wars) {
            if (!w.active || w.ceasefire) continue;
            // Both sides lose strength
            float damage_to_def = w.attacker_strength * 0.01f;
            float damage_to_atk = w.defender_strength * 0.01f;
            w.defender_strength -= damage_to_def;
            w.attacker_strength -= damage_to_atk;
            // Check resolution
            if (w.resolve() != War::ONGOING) {
                w.active = false;
            }
        }
    }

    int active_wars() const {
        int n = 0;
        for (auto& w : wars) if (w.active && !w.ceasefire) n++;
        return n;
    }

    bool at_war(uint32_t f1, uint32_t f2) const {
        for (auto& w : wars) {
            if (w.active && !w.ceasefire && ((w.attacker_faction == f1 && w.defender_faction == f2) ||
                                              (w.attacker_faction == f2 && w.defender_faction == f1))) {
                return true;
            }
        }
        return false;
    }
};

// ---- Political System ----
enum class PoliticalAction : uint8_t {
    DIPLOMATIC_GIFT,     // improve relations
    TRADE_AGREEMENT,     // mutual benefit
    MILITARY_ALLIANCE,   // defend each other
    EMBARGO,             // stop trade
    DECLARE_WAR,         // start war
    SUE_FOR_PEACE,       // end war
    ANNEX_TERRITORY,     // conquer
    DECLARE_INDEPENDENCE // break from larger faction
};

struct PoliticalSystem {
    // Perform a political action between factions
    float perform_action(PoliticalAction action, FactionData& actor, FactionData& target,
                         WarSystem& wars, uint64_t tick) {
        float relation_change = 0;
        switch (action) {
            case PoliticalAction::DIPLOMATIC_GIFT:
                relation_change = +10;
                break;
            case PoliticalAction::TRADE_AGREEMENT:
                relation_change = +15;
                actor.tech_level = std::max(actor.tech_level, target.tech_level);
                target.tech_level = std::max(actor.tech_level, target.tech_level);
                break;
            case PoliticalAction::MILITARY_ALLIANCE:
                relation_change = +25;
                break;
            case PoliticalAction::EMBARGO:
                relation_change = -20;
                break;
            case PoliticalAction::DECLARE_WAR:
                relation_change = -100;
                wars.declare_war(actor.id, target.id, actor.military_strength,
                                target.military_strength, tick);
                actor.at_war = true;
                actor.at_war_with = target.id;
                target.at_war = true;
                target.at_war_with = actor.id;
                break;
            case PoliticalAction::SUE_FOR_PEACE:
                relation_change = +5;
                wars.negotiate_peace(actor.id, target.id);
                actor.at_war = false;
                target.at_war = false;
                break;
            case PoliticalAction::ANNEX_TERRITORY:
                relation_change = -50;
                actor.territory_count += 1;
                target.territory_count = (target.territory_count > 0) ? target.territory_count - 1 : 0;
                break;
            case PoliticalAction::DECLARE_INDEPENDENCE:
                relation_change = -30;
                break;
        }

        // Update relations
        actor.relations[target.id] = std::clamp(actor.relations[target.id] + (int8_t)relation_change, -100, 100);
        target.relations[actor.id] = std::clamp(target.relations[actor.id] + (int8_t)relation_change, -100, 100);

        return relation_change;
    }
};

// ---- God System / Endgame (Fase 9, GDD v0.2 sec 6) ----
struct GodSystem {
    // The "God" of the universe — final boss
    float god_hp = 100000;
    float god_max_hp = 100000;
    float god_power = 5000;       // attack power
    float god_defense = 1000;     // damage reduction
    bool god_alive = true;
    bool god_awakened = false;    // player must trigger awakening

    // Ascension requirements (player must meet ALL to challenge God)
    struct Requirements {
        float min_level = 100;
        float min_tech_unlocked = 50;   // tech items researched
        float min_anomalous_powers = 5; // anomalous powers acquired
        float min_faction_support = 3;  // factions allied with
        float min_planets_visited = 20;
        bool has_god_key = false;       // special key item
    } requirements;

    // Check if player can challenge God
    bool can_challenge(float player_level, int tech_count, int anomalous_count,
                       int allies, int planets, bool has_key) const {
        return player_level >= requirements.min_level &&
               tech_count >= requirements.min_tech_unlocked &&
               anomalous_count >= requirements.min_anomalous_powers &&
               allies >= requirements.min_faction_support &&
               planets >= requirements.min_planets_visited &&
               has_key == requirements.has_god_key;
    }

    // God attacks player
    float god_attack() {
        if (!god_alive || !god_awakened) return 0;
        return god_power;
    }

    // Player damages God
    float damage_god(float raw_damage) {
        if (!god_alive || !god_awakened) return 0;
        float actual = std::max(0.0f, raw_damage - god_defense);
        god_hp -= actual;
        if (god_hp <= 0) {
            god_hp = 0;
            god_alive = false;
        }
        return actual;
    }

    // Check ascension (player becomes new God)
    bool can_ascend() const {
        return !god_alive; // God must be defeated
    }
};

} // namespace krono
