#pragma once
// KronoUniverse — Mob AI System (v0.3)
//
// Hostile creatures with different AI behaviors:
// - Zombie: slow, melee, swarms player
// - Slime: bounces, splits when killed
// - Skeleton: ranged bow, runs from melee
// - Bat: fast, swarms, flying
// - Boss: high HP, multiple attacks, drops loot
// - Wildlife (passive): flees from player

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "engine/character_components.hpp"
#include "game/combat_system.hpp"
#include <cmath>
#include <vector>

namespace krono {

enum class MobType : uint8_t {
    ZOMBIE = 0,
    SLIME,
    SKELETON,
    BAT,
    BOSS,
    DEER,    // passive wildlife
    RABBIT,  // passive wildlife
};

enum class MobState : uint8_t {
    IDLE = 0,
    WANDER,
    CHASE,
    ATTACK,
    FLEE,
    DEAD,
};

struct MobAI {
    MobType type = MobType::ZOMBIE;
    MobState state = MobState::IDLE;
    float detect_range = 200;
    float attack_range = 40;
    float attack_cooldown = 1.0f;
    float attack_timer = 0;
    float wander_target_x = 0;
    float wander_target_y = 0;
    float wander_timer = 0;
    float flee_timer = 0;
    float speed = 60;
    int xp_reward = 10;
    bool is_passive = false;
    bool is_boss = false;
};

struct MobLoot {
    struct Drop { uint16_t item_id; float chance; uint8_t min; uint8_t max; };
    std::vector<Drop> drops;
};

class MobAISystem {
public:
    static Entity spawn_mob(Registry& reg, MobType type, float x, float y) {
        Entity mob = reg.create();
        reg.emplace<Position>(mob, Position{x, y});
        reg.emplace<Velocity>(mob, Velocity{0, 0});
        reg.emplace<AABBCollider>(mob, AABBCollider{24, 32});
        reg.emplace<RigidBody>(mob, RigidBody{0.0f, 0.5f, false, false});
        reg.emplace<StatusContainer>(mob, StatusContainer{});

        MobAI ai;
        ai.type = type;

        switch (type) {
            case MobType::ZOMBIE:
                ai.detect_range = 220;
                ai.attack_range = 32;
                ai.attack_cooldown = 1.0f;
                ai.speed = 50;
                ai.xp_reward = 10;
                reg.emplace<Health>(mob, Health{40, 40});
                reg.emplace<Mass>(mob, Mass{60});
                reg.emplace<EquippedWeapon>(mob, EquippedWeapon{CombatSystem::make_fist(), 0, false, 0, 0.3f});
                break;
            case MobType::SLIME:
                ai.detect_range = 180;
                ai.attack_range = 28;
                ai.attack_cooldown = 1.2f;
                ai.speed = 80;
                ai.xp_reward = 8;
                reg.emplace<Health>(mob, Health{25, 25});
                reg.emplace<Mass>(mob, Mass{20});
                reg.emplace<AABBCollider>(mob, AABBCollider{20, 20});
                reg.emplace<EquippedWeapon>(mob, EquippedWeapon{CombatSystem::make_fist(), 0, false, 0, 0.3f});
                break;
            case MobType::SKELETON:
                ai.detect_range = 300;
                ai.attack_range = 250;
                ai.attack_cooldown = 1.5f;
                ai.speed = 70;
                ai.xp_reward = 15;
                reg.emplace<Health>(mob, Health{30, 30});
                reg.emplace<Mass>(mob, Mass{40});
                reg.emplace<EquippedWeapon>(mob, EquippedWeapon{CombatSystem::make_bow(), 0, false, 0, 0.4f});
                break;
            case MobType::BAT:
                ai.detect_range = 250;
                ai.attack_range = 24;
                ai.attack_cooldown = 0.6f;
                ai.speed = 130;
                ai.xp_reward = 12;
                reg.emplace<Health>(mob, Health{15, 15});
                reg.emplace<Mass>(mob, Mass{5});
                reg.emplace<AABBCollider>(mob, AABBCollider{16, 16});
                reg.emplace<EquippedWeapon>(mob, EquippedWeapon{CombatSystem::make_fist(), 0, false, 0, 0.2f});
                break;
            case MobType::BOSS:
                ai.detect_range = 500;
                ai.attack_range = 80;
                ai.attack_cooldown = 0.8f;
                ai.speed = 90;
                ai.xp_reward = 500;
                ai.is_boss = true;
                reg.emplace<Health>(mob, Health{500, 500});
                reg.emplace<Mass>(mob, Mass{500});
                reg.emplace<AABBCollider>(mob, AABBCollider{64, 80});
                reg.emplace<EquippedWeapon>(mob, EquippedWeapon{CombatSystem::make_sword(), 0, false, 0, 0.5f});
                break;
            case MobType::DEER:
                ai.detect_range = 200;
                ai.attack_range = 0;
                ai.attack_cooldown = 0;
                ai.speed = 100;
                ai.xp_reward = 5;
                ai.is_passive = true;
                reg.emplace<Health>(mob, Health{20, 20});
                reg.emplace<Mass>(mob, Mass{80});
                reg.emplace<AABBCollider>(mob, AABBCollider{32, 32});
                break;
            case MobType::RABBIT:
                ai.detect_range = 150;
                ai.attack_range = 0;
                ai.attack_cooldown = 0;
                ai.speed = 140;
                ai.xp_reward = 2;
                ai.is_passive = true;
                reg.emplace<Health>(mob, Health{8, 8});
                reg.emplace<Mass>(mob, Mass{3});
                reg.emplace<AABBCollider>(mob, AABBCollider{12, 12});
                break;
        }
        reg.emplace<MobAI>(mob, std::move(ai));
        reg.emplace<TagNPC>(mob, TagNPC{});
        return mob;
    }

    // Update mob AI — chase/attack player
    static void update(Registry& reg, Entity player, float dt, float gravity) {
        auto* ppos = reg.get<Position>(player);
        auto* php = reg.get<Health>(player);
        if (!ppos || !php || php->current <= 0) return;

        reg.each<Position, Velocity, MobAI, Health>([&](auto mob_ent, Position& pos, Velocity& vel, MobAI& ai, Health& hp) {
            if (hp.current <= 0) {
                ai.state = MobState::DEAD;
                return;
            }
            if (ai.attack_timer > 0) ai.attack_timer -= dt;
            if (ai.wander_timer > 0) ai.wander_timer -= dt;

            float dx = ppos->x - pos.x;
            float dy = ppos->y - pos.y;
            float dist = std::sqrt(dx*dx + dy*dy);

            // Update status effects that affect movement
            auto* sc = reg.get<StatusContainer>(mob_ent);
            if (sc) {
                if (sc->has(StatusEffect::FREEZE)) return; // can't move
                if (sc->has(StatusEffect::STUN)) return;
            }

            if (dist <= ai.detect_range && !ai.is_passive) {
                ai.state = MobState::CHASE;
                if (dist <= ai.attack_range) {
                    ai.state = MobState::ATTACK;
                    if (ai.attack_timer <= 0) {
                        auto* ew = reg.get<EquippedWeapon>(mob_ent);
                        if (ew) {
                            CombatSystem::attack(reg, mob_ent, ppos->x, ppos->y);
                            ai.attack_timer = ai.attack_cooldown;
                        }
                    }
                    // Stop moving when in attack range (melee)
                    if (ai.attack_range < 50) {
                        vel.x = 0;
                    }
                } else {
                    // Move toward player
                    if (dist > 0.001f) {
                        vel.x = (dx/dist) * ai.speed;
                        // For flying mobs (bat), move in Y too
                        if (ai.type == MobType::BAT) {
                            vel.y = (dy/dist) * ai.speed * 0.7f;
                        } else {
                            // Ground mob — try to jump if obstacle ahead or player above
                            auto* ctrl = reg.get<CharacterController>(mob_ent);
                            if (ctrl && ctrl->grounded && dy < -10) {
                                vel.y = -std::sqrt(2 * gravity * 60);
                            }
                        }
                    }
                }
            } else if (ai.is_passive) {
                // Flee from player
                if (dist <= ai.detect_range) {
                    ai.state = MobState::FLEE;
                    if (dist > 0.001f) {
                        vel.x = -(dx/dist) * ai.speed;
                        // Random jump
                        if ((rand() % 100) < 2) {
                            auto* ctrl = reg.get<CharacterController>(mob_ent);
                            if (ctrl && ctrl->grounded) {
                                vel.y = -std::sqrt(2 * gravity * 60);
                            }
                        }
                    }
                } else {
                    ai.state = MobState::WANDER;
                    if (ai.wander_timer <= 0) {
                        ai.wander_target_x = pos.x + (float)(rand() % 200 - 100);
                        ai.wander_timer = 2.0f + (float)(rand() % 30) / 10.0f;
                    }
                    float wdx = ai.wander_target_x - pos.x;
                    if (std::abs(wdx) > 5) {
                        vel.x = (wdx > 0 ? 1 : -1) * ai.speed * 0.4f;
                    } else {
                        vel.x = 0;
                    }
                }
            } else {
                // Idle / wander
                ai.state = MobState::WANDER;
                if (ai.wander_timer <= 0) {
                    ai.wander_target_x = pos.x + (float)(rand() % 200 - 100);
                    ai.wander_timer = 3.0f + (float)(rand() % 50) / 10.0f;
                }
                float wdx = ai.wander_target_x - pos.x;
                if (std::abs(wdx) > 5) {
                    vel.x = (wdx > 0 ? 1 : -1) * ai.speed * 0.3f;
                } else {
                    vel.x = 0;
                }
            }

            // Apply gravity for ground mobs
            if (ai.type != MobType::BAT) {
                vel.y += gravity * dt;
            }
        });

        // Clean up dead mobs
        std::vector<Entity> dead;
        reg.each<MobAI, Health>([&](auto ent, MobAI& ai, Health& hp) {
            if (hp.current <= 0 && ai.state == MobState::DEAD) {
                dead.push_back(ent);
            }
        });
        for (auto e : dead) reg.destroy(e);
    }

    // Get color for mob rendering
    static void get_mob_color(MobType type, float& r, float& g, float& b) {
        switch (type) {
            case MobType::ZOMBIE:   r=0.4f; g=0.55f; b=0.35f; break;
            case MobType::SLIME:    r=0.4f; g=0.8f; b=0.5f; break;
            case MobType::SKELETON: r=0.85f; g=0.85f; b=0.78f; break;
            case MobType::BAT:      r=0.2f; g=0.15f; b=0.25f; break;
            case MobType::BOSS:     r=0.7f; g=0.15f; b=0.15f; break;
            case MobType::DEER:     r=0.55f; g=0.4f; b=0.25f; break;
            case MobType::RABBIT:   r=0.85f; g=0.78f; b=0.7f; break;
            default: r=0.5f; g=0.5f; b=0.5f; break;
        }
    }
};

} // namespace krono
