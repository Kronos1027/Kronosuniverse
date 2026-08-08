#pragma once
// KronoUniverse — Quest System (v0.7)
//
// Sistema de missões:
// - Quests automáticas baseadas em progresso
// - Tipos: kill X mobs, mine X blocks, craft X items, reach X location, survive X time
// - Recompensas: XP, items, recipes desbloqueadas
// - Quest chain (uma leva a outra)
// - Daily quests (resetam a cada dia in-game)
// - Quest tracker na HUD
// - Notificação quando quest completa

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "procedural/world.hpp"
#include <vector>
#include <string>
#include <cstdint>
#include <functional>

namespace krono {

enum class QuestType : uint8_t {
    KILL_MOBS = 0,
    MINE_BLOCKS,
    CRAFT_ITEMS,
    REACH_DISTANCE,
    SURVIVE_TIME,
    COLLECT_ITEMS,
    DEFEAT_BOSS,
    BUILD_STRUCTURE,
};

enum class QuestState : uint8_t {
    INACTIVE = 0,
    ACTIVE,
    COMPLETED,
    FAILED,
    CLAIMED,
};

struct Quest {
    uint32_t id = 0;
    std::string title;
    std::string description;
    QuestType type = QuestType::KILL_MOBS;
    QuestState state = QuestState::INACTIVE;
    int target_amount = 1;
    int current_amount = 0;
    uint16_t target_id = 0;        // mob type or item id or block type
    float target_value = 0;        // distance, time, etc
    float current_value = 0;
    // Rewards
    int xp_reward = 10;
    std::vector<std::pair<uint16_t, int>> item_rewards;
    std::vector<uint16_t> recipe_unlocks;
    // Optional prerequisite
    uint32_t prerequisite_quest = 0;
    // Daily flag
    bool is_daily = false;
    // Time limit (0 = no limit)
    float time_limit = 0;
    float time_elapsed = 0;
};

class QuestSystem {
public:
    std::vector<Quest> quests;
    uint32_t next_quest_id = 1;

    // Quest templates
    void init_default_quests() {
        // Tutorial chain
        add_quest("First Steps", "Move 100 pixels from spawn", QuestType::REACH_DISTANCE, 100, 0, 50);
        add_quest("Timber!", "Mine 5 wood blocks", QuestType::MINE_BLOCKS, 5, (uint16_t)BlockType::WOOD, 100, {{0x0100, 5}});
        add_quest("Stone Age", "Mine 10 stone blocks", QuestType::MINE_BLOCKS, 10, (uint16_t)BlockType::STONE, 150, {{0x0103, 5}});
        add_quest("First Kill", "Defeat 1 hostile mob", QuestType::KILL_MOBS, 1, 0, 200, {{0x0601, 1}});
        add_quest("Hunter", "Defeat 5 hostile mobs", QuestType::KILL_MOBS, 5, 0, 400, {{0x0601, 2}});
        add_quest("Survivor", "Survive 5 minutes (300 sec)", QuestType::SURVIVE_TIME, 300, 0, 500);
        add_quest("Craftsman", "Craft 3 items", QuestType::CRAFT_ITEMS, 3, 0, 300, {{0x0201, 1}});
        add_quest("Well Equipped", "Craft 1 iron weapon", QuestType::CRAFT_ITEMS, 1, 0x0206, 500, {{0x0206, 1}});
        add_quest("Crystal Hunter", "Mine 3 crystals", QuestType::MINE_BLOCKS, 3, (uint16_t)BlockType::CRYSTAL, 1000, {{0x0121, 3}});
        add_quest("Boss Slayer", "Defeat 1 boss", QuestType::DEFEAT_BOSS, 1, 0, 5000, {{0x0801, 1}});
        add_quest("Explorer", "Travel 5000 pixels from spawn", QuestType::REACH_DISTANCE, 5000, 0, 2000);
        add_quest("Master Miner", "Mine 50 blocks total", QuestType::MINE_BLOCKS, 50, 0, 1500);
        add_quest("Warlord", "Defeat 20 mobs total", QuestType::KILL_MOBS, 20, 0, 2500);
    }

    uint32_t add_quest(const std::string& title, const std::string& desc, QuestType type,
                       int target, uint16_t target_id, int xp,
                       std::vector<std::pair<uint16_t, int>> rewards = {}) {
        Quest q;
        q.id = next_quest_id++;
        q.title = title;
        q.description = desc;
        q.type = type;
        q.target_amount = target;
        q.target_id = target_id;
        q.xp_reward = xp;
        q.item_rewards = rewards;
        q.state = QuestState::ACTIVE;
        quests.push_back(q);
        return q.id;
    }

    // Update quest progress
    void on_kill_mob(uint16_t mob_type) {
        for (auto& q : quests) {
            if (q.state != QuestState::ACTIVE) continue;
            if (q.type == QuestType::KILL_MOBS) {
                if (q.target_id == 0 || q.target_id == mob_type) {
                    q.current_amount++;
                }
            } else if (q.type == QuestType::DEFEAT_BOSS && mob_type == 4) {  // 4 = BOSS type
                q.current_amount++;
            }
        }
    }

    void on_mine_block(uint16_t block_type) {
        for (auto& q : quests) {
            if (q.state != QuestState::ACTIVE) continue;
            if (q.type == QuestType::MINE_BLOCKS) {
                if (q.target_id == 0 || q.target_id == block_type) {
                    q.current_amount++;
                }
            }
        }
    }

    void on_craft_item(uint16_t item_id) {
        for (auto& q : quests) {
            if (q.state != QuestState::ACTIVE) continue;
            if (q.type == QuestType::CRAFT_ITEMS) {
                if (q.target_id == 0 || q.target_id == item_id) {
                    q.current_amount++;
                }
            }
        }
    }

    void on_collect_item(uint16_t item_id, int count) {
        for (auto& q : quests) {
            if (q.state != QuestState::ACTIVE) continue;
            if (q.type == QuestType::COLLECT_ITEMS && q.target_id == item_id) {
                q.current_amount += count;
            }
        }
    }

    void update(float dt, float player_distance_from_spawn) {
        for (auto& q : quests) {
            if (q.state != QuestState::ACTIVE) continue;
            // Check time limit
            if (q.time_limit > 0) {
                q.time_elapsed += dt;
                if (q.time_elapsed >= q.time_limit) {
                    q.state = QuestState::FAILED;
                    continue;
                }
            }
            // Update progress for special types
            if (q.type == QuestType::REACH_DISTANCE) {
                q.current_value = std::max(q.current_value, player_distance_from_spawn);
                if (q.current_value >= q.target_amount) {
                    q.state = QuestState::COMPLETED;
                }
            } else if (q.type == QuestType::SURVIVE_TIME) {
                q.current_value += dt;
                q.current_amount = (int)q.current_value;
                if (q.current_value >= q.target_amount) {
                    q.state = QuestState::COMPLETED;
                }
            } else {
                // Counter-based quests
                if (q.current_amount >= q.target_amount) {
                    q.state = QuestState::COMPLETED;
                }
            }
        }
    }

    // Get all completed but unclaimed quests
    std::vector<Quest*> get_completed_unclaimed() {
        std::vector<Quest*> result;
        for (auto& q : quests) {
            if (q.state == QuestState::COMPLETED) {
                result.push_back(&q);
            }
        }
        return result;
    }

    // Claim rewards for a completed quest
    bool claim_quest(uint32_t quest_id, int& out_xp, std::vector<std::pair<uint16_t, int>>& out_items) {
        for (auto& q : quests) {
            if (q.id == quest_id && q.state == QuestState::COMPLETED) {
                out_xp = q.xp_reward;
                out_items = q.item_rewards;
                q.state = QuestState::CLAIMED;
                return true;
            }
        }
        return false;
    }

    // Get progress as string
    std::string get_progress_string(const Quest& q) const {
        switch (q.type) {
            case QuestType::REACH_DISTANCE:
                return std::to_string((int)q.current_value) + "/" + std::to_string(q.target_amount) + " PX";
            case QuestType::SURVIVE_TIME:
                return std::to_string((int)q.current_value) + "/" + std::to_string(q.target_amount) + " SEC";
            default:
                return std::to_string(q.current_amount) + "/" + std::to_string(q.target_amount);
        }
    }

    // Get quest state color
    static void get_state_color(QuestState state, float& r, float& g, float& b) {
        switch (state) {
            case QuestState::ACTIVE: r = 0.9f; g = 0.9f; b = 0.3f; break;       // yellow
            case QuestState::COMPLETED: r = 0.3f; g = 1.0f; b = 0.3f; break;     // green
            case QuestState::CLAIMED: r = 0.5f; g = 0.5f; b = 0.5f; break;       // gray
            case QuestState::FAILED: r = 1.0f; g = 0.3f; b = 0.3f; break;        // red
            default: r = 0.6f; g = 0.6f; b = 0.6f; break;
        }
    }

    int count_active() const {
        int count = 0;
        for (auto& q : quests) {
            if (q.state == QuestState::ACTIVE) count++;
        }
        return count;
    }

    int count_completed_unclaimed() const {
        int count = 0;
        for (auto& q : quests) {
            if (q.state == QuestState::COMPLETED) count++;
        }
        return count;
    }
};

} // namespace krono
