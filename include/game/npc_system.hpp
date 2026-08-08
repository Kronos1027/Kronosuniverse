#pragma once
// KronoUniverse — NPC System (Fase 5, GDD v0.1 seção 7-8)
// NPCs com IA básica, sistema de diálogo por template, facções.

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "engine/character_components.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace krono {

// ---- NPC AI States ----
enum class NPCAIState : uint8_t {
    IDLE,
    PATROL,
    FOLLOW,
    FLEE,
    ATTACK,
    TALK,
    TRADE,
    WORK,
    SLEEP,
};

// ---- NPC Component ----
struct NPCComponent {
    NPCAIState ai_state = NPCAIState::IDLE;
    NPCAIState prev_state = NPCAIState::IDLE;

    // Patrol
    float patrol_x = 0, patrol_y = 0;
    float patrol_radius = 200;
    float patrol_speed = 80;
    float patrol_timer = 0;
    float patrol_target_x = 0, patrol_target_y = 0;

    // Fear/aggression
    float fear_level = 0;        // 0-100 (high = flees)
    float aggression = 0;        // 0-100 (high = attacks)
    float detection_range = 300; // how far they can see
    float attack_range = 50;
    float attack_damage = 10;
    float attack_cooldown = 0;
    float attack_speed = 1.0f;   // attacks per second

    // Dialogue
    uint32_t dialogue_id = 0;
    bool has_talked = false;     // has player talked to this NPC before?

    // Faction
    uint32_t faction_id = 0;
    int8_t disposition = 0;      // -100 (hate) to 100 (love) toward player

    // Trade
    bool can_trade = false;
    uint32_t shop_id = 0;
};

// ---- Dialogue System ----
struct DialogueNode {
    uint32_t id;
    std::string text;            // what NPC says
    struct Option {
        std::string text;        // player's response
        uint32_t next_node_id;   // 0 = end conversation
        int disposition_change = 0;  // affects NPC disposition
        bool requires_disposition = false;
        int min_disposition = 0;     // only shows if disposition >= this
    };
    std::vector<Option> options;
};

struct DialogueTree {
    uint32_t tree_id;
    std::unordered_map<uint32_t, DialogueNode> nodes;
    uint32_t start_node_id;

    // Contextual responses based on world state (Parte 7.2 — "linha do tempo")
    std::string context_tag;  // e.g., "war_active", "peace", "first_meeting"

    const DialogueNode* get_node(uint32_t id) const {
        auto it = nodes.find(id);
        return (it != nodes.end()) ? &it->second : nullptr;
    }
};

// ---- Faction ----
struct FactionData {
    uint32_t id;
    std::string name;
    std::string description;
    uint8_t r, g, b;  // faction color

    // Relations with other factions (-100 to 100)
    std::unordered_map<uint32_t, int8_t> relations;

    // Faction stats
    uint32_t territory_count = 0;
    uint32_t military_strength = 0;
    uint32_t tech_level = 1;      // 1-10
    uint32_t population = 0;

    // Status
    bool at_war = false;
    uint32_t at_war_with = 0;
};

// ---- NPC AI System ----
class NPCAISystem {
public:
    void update(Registry& reg, float dt, Entity player_entity) {
        auto* player_pos = reg.get<Position>(player_entity);
        if (!player_pos) return;

        reg.each<NPCComponent, Position, Velocity, Health>(
            [&](Entity e, NPCComponent& npc, Position& pos, Velocity& vel, Health& hp) {
                if (hp.current <= 0) return;

                float dx = player_pos->x - pos.x;
                float dy = player_pos->y - pos.y;
                float dist = std::sqrt(dx * dx + dy * dy);

                switch (npc.ai_state) {
                    case NPCAIState::IDLE:
                        handle_idle(npc, pos, vel, dt, dist);
                        break;
                    case NPCAIState::PATROL:
                        handle_patrol(npc, pos, vel, dt);
                        break;
                    case NPCAIState::FLEE:
                        handle_flee(npc, pos, vel, dt, dx, dy, dist);
                        break;
                    case NPCAIState::ATTACK:
                        handle_attack(npc, pos, vel, dt, dx, dy, dist, player_entity, reg);
                        break;
                    case NPCAIState::FOLLOW:
                        handle_follow(npc, pos, vel, dt, dx, dy, dist);
                        break;
                    default:
                        break;
                }

                // State transitions based on player proximity
                if (npc.fear_level > 50 && dist < npc.detection_range) {
                    npc.ai_state = NPCAIState::FLEE;
                } else if (npc.aggression > 50 && dist < npc.detection_range) {
                    if (dist < npc.attack_range) {
                        npc.ai_state = NPCAIState::ATTACK;
                    } else {
                        npc.ai_state = NPCAIState::FOLLOW;
                    }
                } else if (dist > npc.detection_range * 2 && npc.ai_state != NPCAIState::PATROL) {
                    npc.ai_state = NPCAIState::PATROL;
                    npc.patrol_x = pos.x;
                    npc.patrol_y = pos.y;
                }
            }
        );
    }

private:
    void handle_idle(NPCComponent& npc, Position& pos, Velocity& vel, float dt, float player_dist) {
        vel.x *= 0.9f;
        vel.y *= 0.9f;
        // Random chance to start patrolling
        npc.patrol_timer -= dt;
        if (npc.patrol_timer <= 0) {
            npc.ai_state = NPCAIState::PATROL;
            npc.patrol_timer = 5.0f + (rand() % 1000) / 100.0f;
        }
    }

    void handle_patrol(NPCComponent& npc, Position& pos, Velocity& vel, float dt) {
        // Move toward patrol target
        float dx = npc.patrol_target_x - pos.x;
        float dy = npc.patrol_target_y - pos.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < 10) {
            // Reached target, pick new one
            float angle = (rand() % 360) * 3.14159f / 180.0f;
            npc.patrol_target_x = npc.patrol_x + std::cos(angle) * npc.patrol_radius;
            npc.patrol_target_y = npc.patrol_y + std::sin(angle) * npc.patrol_radius;
        } else {
            vel.x = (dx / dist) * npc.patrol_speed;
            vel.y = (dy / dist) * npc.patrol_speed;
        }
    }

    void handle_flee(NPCComponent& npc, Position& pos, Velocity& vel, float dt, float dx, float dy, float dist) {
        if (dist > 0) {
            vel.x = -(dx / dist) * npc.patrol_speed * 1.5f; // run away
            vel.y = -(dy / dist) * npc.patrol_speed * 1.5f;
        }
    }

    void handle_follow(NPCComponent& npc, Position& pos, Velocity& vel, float dt, float dx, float dy, float dist) {
        if (dist > 0) {
            vel.x = (dx / dist) * npc.patrol_speed * 1.2f; // chase
            vel.y = (dy / dist) * npc.patrol_speed * 1.2f;
        }
    }

    void handle_attack(NPCComponent& npc, Position& pos, Velocity& vel, float dt, float dx, float dy,
                       float dist, Entity target, Registry& reg) {
        // Stop moving when in attack range
        vel.x *= 0.5f;
        vel.y *= 0.5f;

        // Attack cooldown
        npc.attack_cooldown -= dt;
        if (npc.attack_cooldown <= 0) {
            // Attack!
            auto* target_hp = reg.get<Health>(target);
            if (target_hp) {
                target_hp->current -= npc.attack_damage;
            }
            npc.attack_cooldown = 1.0f / npc.attack_speed;
        }
    }
};

} // namespace krono
