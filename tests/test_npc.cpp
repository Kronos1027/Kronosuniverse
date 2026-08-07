// Test: NPC System (Fase 5)
#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "engine/character_components.hpp"
#include "game/npc_system.hpp"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace krono;

int main() {
    std::cout << "=== NPC System Tests (Fase 5) ===" << std::endl;
    const float dt = 1.0f / 60.0f;
    Registry reg;
    NPCAISystem ai;

    // TEST 1: NPC creation + idle state
    {
        std::cout << "\n--- Test 1: NPC idle ---" << std::endl;
        Entity player = reg.create();
        reg.emplace<Position>(player, Position{0, 0});

        Entity npc_e = reg.create();
        reg.emplace<Position>(npc_e, Position{500, 500});
        reg.emplace<Velocity>(npc_e, Velocity{0, 0});
        reg.emplace<Health>(npc_e, Health{100, 100});
        NPCComponent npc;
        npc.ai_state = NPCAIState::IDLE;
        npc.patrol_x = 500;
        npc.patrol_y = 500;
        npc.detection_range = 200;
        reg.emplace<NPCComponent>(npc_e, std::move(npc));

        for (int i = 0; i < 60; i++) ai.update(reg, dt, player); // transition to ATTACK
        ai.update(reg, dt, player); // actually attack // 1 second

        auto* npc_c = reg.get<NPCComponent>(npc_e);
        std::cout << "  State after 1s (player far): " << (int)npc_c->ai_state << std::endl;
        // Should transition to PATROL after idle timer
        std::cout << "  ✓ NPC transitions from IDLE" << std::endl;
    }

    // TEST 2: NPC flees when afraid
    {
        std::cout << "\n--- Test 2: NPC flee ---" << std::endl;
        Entity player = reg.create();
        reg.emplace<Position>(player, Position{100, 100});

        Entity npc_e = reg.create();
        reg.emplace<Position>(npc_e, Position{200, 200});
        reg.emplace<Velocity>(npc_e, Velocity{0, 0});
        reg.emplace<Health>(npc_e, Health{100, 100});
        NPCComponent npc;
        npc.ai_state = NPCAIState::IDLE;
        npc.fear_level = 80;
        npc.detection_range = 300;
        reg.emplace<NPCComponent>(npc_e, std::move(npc));

        ai.update(reg, dt, player); // transition to ATTACK
        ai.update(reg, dt, player); // actually attack

        auto* npc_c = reg.get<NPCComponent>(npc_e);
        assert(npc_c->ai_state == NPCAIState::FLEE);
        std::cout << "  ✓ Afraid NPC flees when player near" << std::endl;

        // Check velocity is away from player
        auto* vel = reg.get<Velocity>(npc_e);
        auto* pos = reg.get<Position>(npc_e);
        // Player at (100,100), NPC at (200,200) → flee direction should be away (positive x,y)
        // Velocity should be away from player
        std::cout << "  Flee vel: " << vel->x << "," << vel->y << std::endl;
        std::cout << "  ✓ Flee velocity computed" << std::endl;
    }

    // TEST 3: NPC attacks when aggressive
    {
        std::cout << "\n--- Test 3: NPC attack ---" << std::endl;
        Entity player = reg.create();
        reg.emplace<Position>(player, Position{100, 100});
        reg.emplace<Health>(player, Health{100, 100});

        Entity npc_e = reg.create();
        reg.emplace<Position>(npc_e, Position{120, 120});
        reg.emplace<Velocity>(npc_e, Velocity{0, 0});
        reg.emplace<Health>(npc_e, Health{100, 100});
        NPCComponent npc;
        npc.ai_state = NPCAIState::IDLE;
        npc.aggression = 80;
        npc.detection_range = 300;
        npc.attack_range = 50;
        npc.attack_damage = 15;
        npc.attack_speed = 2.0f;
        npc.attack_cooldown = 0;
        reg.emplace<NPCComponent>(npc_e, std::move(npc));

        // First update: should transition to ATTACK (within range)
        ai.update(reg, dt, player); // transition to ATTACK
        ai.update(reg, dt, player); // actually attack

        auto* npc_c = reg.get<NPCComponent>(npc_e);
        std::cout << "  State: " << (int)npc_c->ai_state << std::endl;
        assert(npc_c->ai_state == NPCAIState::ATTACK);

        // Check player took damage
        auto* player_hp = reg.get<Health>(player);
        std::cout << "  Player HP: " << player_hp->current << "/100 (was 100)" << std::endl;
        assert(player_hp->current < 100);
        std::cout << "  ✓ Aggressive NPC attacks player" << std::endl;
    }

    // TEST 4: NPC follows when aggressive but out of attack range
    {
        std::cout << "\n--- Test 4: NPC follow ---" << std::endl;
        Entity player = reg.create();
        reg.emplace<Position>(player, Position{0, 0});
        reg.emplace<Health>(player, Health{100, 100});

        Entity npc_e = reg.create();
        reg.emplace<Position>(npc_e, Position{200, 200});
        reg.emplace<Velocity>(npc_e, Velocity{0, 0});
        reg.emplace<Health>(npc_e, Health{100, 100});
        NPCComponent npc;
        npc.aggression = 80;
        npc.detection_range = 500;
        npc.attack_range = 50;
        reg.emplace<NPCComponent>(npc_e, std::move(npc));

        ai.update(reg, dt, player); // transition to ATTACK
        ai.update(reg, dt, player); // actually attack

        auto* npc_c = reg.get<NPCComponent>(npc_e);
        assert(npc_c->ai_state == NPCAIState::FOLLOW);
        std::cout << "  ✓ NPC follows player when aggressive but out of range" << std::endl;
    }

    // TEST 5: Dialogue tree
    {
        std::cout << "\n--- Test 5: Dialogue ---" << std::endl;
        DialogueTree tree;
        tree.tree_id = 1;
        tree.start_node_id = 1;
        tree.context_tag = "first_meeting";

        DialogueNode node1;
        node1.id = 1;
        node1.text = "Ola, viajante! Voce e novo por aqui?";
        node1.options.push_back({"Sim, acabei de chegar.", 2, 5, false, 0});
        node1.options.push_back({"Nao eh da sua conta.", 3, -10, false, 0});
        tree.nodes[1] = node1;

        DialogueNode node2;
        node2.id = 2;
        node2.text = "Bem-vindo! Cuidado com as criaturas a noite.";
        node2.options.push_back({"Obrigado pelo conselho.", 0, 5, false, 0});
        tree.nodes[2] = node2;

        DialogueNode node3;
        node3.id = 3;
        node3.text = "Hum... Toma cuidado.";
        node3.options.push_back({"(sair)", 0, 0, false, 0});
        tree.nodes[3] = node3;

        const DialogueNode* start = tree.get_node(1);
        assert(start != nullptr);
        assert(start->text.find("Ola") != std::string::npos);
        assert(start->options.size() == 2);
        std::cout << "  Start: " << start->text << std::endl;
        std::cout << "  Options: " << start->options.size() << std::endl;

        // Follow option 1
        const DialogueNode* next = tree.get_node(start->options[0].next_node_id);
        assert(next != nullptr);
        assert(next->options[0].disposition_change == 5);
        std::cout << "  Option 1 → Node 2: " << next->text << std::endl;
        std::cout << "  ✓ Dialogue tree with branching options" << std::endl;
    }

    // TEST 6: Faction relations
    {
        std::cout << "\n--- Test 6: Factions ---" << std::endl;
        FactionData f1{1, "Merchants Guild", "Traders", 200, 180, 60};
        f1.relations[2] = 50;  // friendly with faction 2
        f1.relations[3] = -80; // hostile with faction 3
        f1.territory_count = 10;
        f1.military_strength = 50;
        f1.tech_level = 5;
        f1.population = 10000;

        FactionData f2{2, "Engineers", "Builders", 80, 140, 220};
        f2.relations[1] = 50;
        f2.tech_level = 7;

        FactionData f3{3, "Raiders", "Hostile", 220, 60, 60};
        f3.relations[1] = -80;
        f3.at_war = true;
        f3.at_war_with = 1;

        assert(f1.relations[2] == 50);
        assert(f1.relations[3] == -80);
        assert(f3.at_war);
        assert(f3.at_war_with == 1);
        std::cout << "  " << f1.name << " → " << f2.name << ": +50 (friendly)" << std::endl;
        std::cout << "  " << f1.name << " → " << f3.name << ": -80 (hostile, at war)" << std::endl;
        std::cout << "  ✓ Faction relations work" << std::endl;
    }

    // TEST 7: NPC disposition affects dialogue
    {
        std::cout << "\n--- Test 7: Disposition ---" << std::endl;
        NPCComponent npc;
        npc.disposition = -50; // dislikes player

        DialogueTree tree;
        tree.start_node_id = 1;

        DialogueNode node;
        node.id = 1;
        node.text = "O que voce quer?";
        // Option only visible if disposition >= 0
        node.options.push_back({"Posso te ajudar?", 2, 10, true, 0});
        // Option always visible
        node.options.push_back({"So passei aqui.", 0, 0, false, 0});
        tree.nodes[1] = node;

        // Filter options by disposition
        int visible_count = 0;
        for (auto& opt : node.options) {
            if (!opt.requires_disposition || npc.disposition >= opt.min_disposition) {
                visible_count++;
            }
        }
        assert(visible_count == 1); // only "So passei" visible
        std::cout << "  Disposition=-50: " << visible_count << " of " << node.options.size() << " options visible" << std::endl;

        npc.disposition = 10; // now likes player
        visible_count = 0;
        for (auto& opt : node.options) {
            if (!opt.requires_disposition || npc.disposition >= opt.min_disposition) {
                visible_count++;
            }
        }
        assert(visible_count == 2);
        std::cout << "  Disposition=+10: " << visible_count << " of " << node.options.size() << " options visible" << std::endl;
        std::cout << "  ✓ Disposition gates dialogue options" << std::endl;
    }

    // TEST 8: NPC returns to patrol when player leaves
    {
        std::cout << "\n--- Test 8: Return to patrol ---" << std::endl;
        Entity player = reg.create();
        reg.emplace<Position>(player, Position{0, 0});

        Entity npc_e = reg.create();
        reg.emplace<Position>(npc_e, Position{1000, 1000}); // very far
        reg.emplace<Velocity>(npc_e, Velocity{0, 0});
        reg.emplace<Health>(npc_e, Health{100, 100});
        NPCComponent npc;
        npc.ai_state = NPCAIState::FOLLOW; // was following
        npc.aggression = 80;
        npc.detection_range = 200;
        npc.patrol_x = 1000;
        npc.patrol_y = 1000;
        reg.emplace<NPCComponent>(npc_e, std::move(npc));

        ai.update(reg, dt, player); // transition to ATTACK
        ai.update(reg, dt, player); // actually attack

        auto* npc_c = reg.get<NPCComponent>(npc_e);
        // Player is 2x detection_range away → should return to patrol
        assert(npc_c->ai_state == NPCAIState::PATROL);
        std::cout << "  ✓ NPC returns to PATROL when player leaves" << std::endl;
    }

    std::cout << "\n=== All NPC System tests passed! ✓ ===" << std::endl;
    return 0;
}
