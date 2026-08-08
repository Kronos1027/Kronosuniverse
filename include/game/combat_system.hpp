#pragma once
// KronoUniverse — Combat System (v0.3)
//
// Sistema de combate completo:
// - 5 tipos de armas (Sword, Bow, Gun, Magic Staff, Fist)
// - Projéteis com física própria (arrow, bullet, magic bolt)
// - Knockback, dano crítico, status effects (poison, burn, freeze, stun)
// - Cooldown e ammo system
// - Hitboxes e hurtboxes
// - Death/respawn

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "engine/character_components.hpp"
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace krono {

// ---- Weapon types ----
enum class WeaponType : uint8_t {
    FIST = 0,
    SWORD,
    BOW,
    GUN,
    STAFF,
};

enum class DamageType : uint8_t {
    PHYSICAL = 0,
    PIERCING,
    FIRE,
    ICE,
    POISON,
    SHOCK,
    HOLY,
};

// ---- Status effects (applied to entity) ----
enum class StatusEffect : uint8_t {
    NONE = 0,
    POISON,
    BURN,
    FREEZE,
    STUN,
    BLEED,
    REGEN,
    HASTE,
    SLOW,
};

struct ActiveStatus {
    StatusEffect type = StatusEffect::NONE;
    float duration = 0;       // total duration
    float elapsed = 0;        // time since applied
    float tick_interval = 1.0f;
    float tick_accumulator = 0;
    float magnitude = 0;      // dmg per tick / speed modifier
    uint32_t source_entity = 0;
};

// ---- Weapon stats ----
struct WeaponStats {
    WeaponType type = WeaponType::SWORD;
    DamageType damage_type = DamageType::PHYSICAL;
    float base_damage = 10;
    float crit_chance = 0.1f;     // 0-1
    float crit_multiplier = 2.0f;
    float knockback = 50;         // force applied to target
    float range = 40;             // melee reach / projectile range
    float cooldown = 0.4f;        // seconds between attacks
    float projectile_speed = 0;   // 0 = melee
    int ammo_per_shot = 0;        // 0 = no ammo needed
    int stamina_cost = 5;
};

// ---- Projectile component ----
struct Projectile {
    float damage = 10;
    DamageType damage_type = DamageType::PHYSICAL;
    float lifetime = 2.0f;
    float elapsed = 0;
    float knockback = 30;
    bool from_player = true;
    bool pierce = false;
    int pierce_count = 0;
    int max_pierce = 0;
    uint32_t owner = 0;
    StatusEffect applies_status = StatusEffect::NONE;
    float status_duration = 0;
    float status_magnitude = 0;
};

// ---- Equipped weapon component ----
struct EquippedWeapon {
    WeaponStats stats;
    float cooldown_timer = 0;
    bool attacking = false;
    float attack_anim_time = 0;
    float attack_anim_duration = 0.2f;
};

// ---- Status container ----
struct StatusContainer {
    std::vector<ActiveStatus> effects;

    void apply(StatusEffect type, float duration, float magnitude, uint32_t source = 0) {
        // Refresh if already exists
        for (auto& s : effects) {
            if (s.type == type) {
                s.duration = duration;
                s.elapsed = 0;
                s.magnitude = magnitude;
                s.source_entity = source;
                return;
            }
        }
        effects.push_back({type, duration, 0, 1.0f, 0, magnitude, source});
    }

    bool has(StatusEffect type) const {
        for (auto& s : effects) if (s.type == type) return true;
        return false;
    }

    void clear() { effects.clear(); }
};

// ---- Combat System ----
class CombatSystem {
public:
    // Update status effects on entity
    static void update_statuses(Registry& reg, float dt) {
        reg.each<Health, StatusContainer>([&](auto entity, Health& hp, StatusContainer& sc) {
            for (auto it = sc.effects.begin(); it != sc.effects.end(); ) {
                auto& s = *it;
                s.elapsed += dt;
                s.tick_accumulator += dt;

                // Apply periodic effects
                while (s.tick_accumulator >= s.tick_interval) {
                    s.tick_accumulator -= s.tick_interval;
                    switch (s.type) {
                        case StatusEffect::POISON:
                        case StatusEffect::BURN:
                        case StatusEffect::BLEED:
                            hp.current -= s.magnitude;
                            break;
                        case StatusEffect::REGEN:
                            hp.current = std::min(hp.max, hp.current + s.magnitude);
                            break;
                        default: break;
                    }
                }

                // Movement/speed modifiers handled by MovementSystem reading has(FREEZE)/has(SLOW)/has(HASTE)

                if (s.elapsed >= s.duration) {
                    it = sc.effects.erase(it);
                } else {
                    ++it;
                }
            }

            // Death check
            if (hp.current <= 0) {
                hp.current = 0;
                // Mark as dead (death handling can be done by other system)
            }
        });
    }

    // Update cooldowns
    static void update_cooldowns(Registry& reg, float dt) {
        reg.each<EquippedWeapon>([&](auto entity, EquippedWeapon& w) {
            if (w.cooldown_timer > 0) w.cooldown_timer -= dt;
            if (w.attacking) {
                w.attack_anim_time += dt;
                if (w.attack_anim_time >= w.attack_anim_duration) {
                    w.attacking = false;
                    w.attack_anim_time = 0;
                }
            }
        });
    }

    // Update projectiles
    static void update_projectiles(Registry& reg, float dt) {
        std::vector<Entity> to_destroy;
        reg.each<Position, Velocity, Projectile>([&](auto entity, Position& pos, Velocity& vel, Projectile& proj) {
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
            proj.elapsed += dt;
            if (proj.elapsed >= proj.lifetime) {
                to_destroy.push_back(entity);
            }
        });
        for (auto e : to_destroy) reg.destroy(e);
    }

    // Attack with equipped weapon
    static bool attack(Registry& reg, Entity attacker, float aim_x, float aim_y) {
        auto* ew = reg.get<EquippedWeapon>(attacker);
        auto* pos = reg.get<Position>(attacker);
        if (!ew || !pos) return false;
        if (ew->cooldown_timer > 0) return false;

        const auto& stats = ew->stats;
        ew->cooldown_timer = stats.cooldown;
        ew->attacking = true;
        ew->attack_anim_time = 0;

        // Ranged: spawn projectile
        if (stats.projectile_speed > 0) {
            Entity proj = reg.create();
            float dx = aim_x - pos->x;
            float dy = aim_y - pos->y;
            float len = std::sqrt(dx*dx + dy*dy);
            if (len < 0.001f) { dx = 1; dy = 0; len = 1; }
            dx /= len; dy /= len;

            // Spawn slightly in front
            float spawn_x = pos->x + dx * 20;
            float spawn_y = pos->y + dy * 20;

            reg.emplace<Position>(proj, Position{spawn_x, spawn_y});
            reg.emplace<Velocity>(proj, Velocity{dx * stats.projectile_speed, dy * stats.projectile_speed});
            reg.emplace<Mass>(proj, Mass{0.1f});
            reg.emplace<RigidBody>(proj, RigidBody{0.0f, 0.0f, false, true});
            reg.emplace<AABBCollider>(proj, AABBCollider{6, 6});
            reg.emplace<TagProjectile>(proj, TagProjectile{});

            // Check crit
            float dmg = stats.base_damage;
            bool is_crit = ((float)rand() / RAND_MAX) < stats.crit_chance;
            if (is_crit) dmg *= stats.crit_multiplier;

            Projectile proj_comp;
            proj_comp.damage = dmg;
            proj_comp.damage_type = stats.damage_type;
            proj_comp.lifetime = stats.range / stats.projectile_speed;
            proj_comp.knockback = stats.knockback;
            proj_comp.from_player = reg.has<TagPlayer>(attacker);
            proj_comp.owner = (uint32_t)attacker;
            reg.emplace<Projectile>(proj, std::move(proj_comp));
        }
        // Melee: instant hit detection on nearby enemies
        else {
            float dmg = stats.base_damage;
            bool is_crit = ((float)rand() / RAND_MAX) < stats.crit_chance;
            if (is_crit) dmg *= stats.crit_multiplier;
            apply_melee_damage(reg, attacker, *pos, dmg, stats);
        }

        return true;
    }

    // Apply melee damage to all enemies in range
    static void apply_melee_damage(Registry& reg, Entity attacker, const Position& pos, float dmg, const WeaponStats& stats) {
        bool attacker_is_player = reg.has<TagPlayer>(attacker);
        auto* attacker_col = reg.get<AABBCollider>(attacker);
        float ax = pos.x + (attacker_col ? attacker_col->width/2 : 0);
        float ay = pos.y + (attacker_col ? attacker_col->height/2 : 0);

        reg.each<Position, AABBCollider, Health>([&](auto target, Position& tp, AABBCollider& tc, Health& th) {
            if (target == attacker) return;
            // Don't hit same team
            bool target_is_player = reg.has<TagPlayer>(target);
            if (attacker_is_player == target_is_player) return;

            float dx = (tp.x + tc.width/2) - ax;
            float dy = (tp.y + tc.height/2) - ay;
            float dist = std::sqrt(dx*dx + dy*dy);
            if (dist > stats.range) return;

            // Apply damage
            th.current -= dmg;

            // Knockback
            auto* tv = reg.get<Velocity>(target);
            if (tv && dist > 0.001f) {
                tv->x += (dx/dist) * stats.knockback;
                tv->y += (dy/dist) * stats.knockback * 0.5f;
            }

            // Apply status effect
            if (stats.damage_type == DamageType::FIRE) {
                StatusContainer* sc = reg.get<StatusContainer>(target);
                if (!sc) {
                    reg.emplace<StatusContainer>(target, StatusContainer{});
                    sc = reg.get<StatusContainer>(target);
                }
                sc->apply(StatusEffect::BURN, 3.0f, 4.0f, (uint32_t)attacker);
            } else if (stats.damage_type == DamageType::ICE) {
                StatusContainer* sc = reg.get<StatusContainer>(target);
                if (!sc) {
                    reg.emplace<StatusContainer>(target, StatusContainer{});
                    sc = reg.get<StatusContainer>(target);
                }
                sc->apply(StatusEffect::FREEZE, 2.0f, 0.5f, (uint32_t)attacker);
            } else if (stats.damage_type == DamageType::POISON) {
                StatusContainer* sc = reg.get<StatusContainer>(target);
                if (!sc) {
                    reg.emplace<StatusContainer>(target, StatusContainer{});
                    sc = reg.get<StatusContainer>(target);
                }
                sc->apply(StatusEffect::POISON, 5.0f, 3.0f, (uint32_t)attacker);
            }
        });
    }

    // Check projectile hits
    static void check_projectile_hits(Registry& reg) {
        std::vector<Entity> to_destroy;
        reg.each<Position, AABBCollider, Projectile>([&](auto proj_ent, Position& ppos, AABBCollider& pcol, Projectile& proj) {
            if (proj.elapsed < 0.05f) return; // grace period
            reg.each<Position, AABBCollider, Health>([&](auto target, Position& tp, AABBCollider& tc, Health& th) {
                if (target == proj.owner) return;
                if (to_destroy.size() > 0 && std::find(to_destroy.begin(), to_destroy.end(), proj_ent) != to_destroy.end()) return;
                bool target_is_player = reg.has<TagPlayer>(target);
                if (proj.from_player == target_is_player) return; // same team

                // AABB overlap
                if (ppos.x < tp.x + tc.width && ppos.x + pcol.width > tp.x &&
                    ppos.y < tp.y + tc.height && ppos.y + pcol.height > tp.y) {
                    th.current -= proj.damage;
                    auto* tv = reg.get<Velocity>(target);
                    if (tv) {
                        auto* pv = reg.get<Velocity>(proj_ent);
                        if (pv) {
                            float len = std::sqrt(pv->x*pv->x + pv->y*pv->y);
                            if (len > 0.001f) {
                                tv->x += (pv->x/len) * proj.knockback;
                                tv->y += (pv->y/len) * proj.knockback * 0.5f;
                            }
                        }
                    }
                    // Apply status
                    if (proj.applies_status != StatusEffect::NONE) {
                        StatusContainer* sc = reg.get<StatusContainer>(target);
                        if (!sc) {
                            reg.emplace<StatusContainer>(target, StatusContainer{});
                            sc = reg.get<StatusContainer>(target);
                        }
                        sc->apply(proj.applies_status, proj.status_duration, proj.status_magnitude, proj.owner);
                    }
                    if (!proj.pierce || proj.pierce_count >= proj.max_pierce) {
                        to_destroy.push_back(proj_ent);
                    } else {
                        proj.pierce_count++;
                    }
                }
            });
        });
        for (auto e : to_destroy) reg.destroy(e);
    }

    // ---- Weapon presets ----
    static WeaponStats make_fist() {
        return {WeaponType::FIST, DamageType::PHYSICAL, 5, 0.05f, 1.5f, 30, 30, 0.4f, 0, 0, 2};
    }
    static WeaponStats make_sword() {
        return {WeaponType::SWORD, DamageType::PHYSICAL, 20, 0.15f, 2.0f, 80, 50, 0.5f, 0, 0, 8};
    }
    static WeaponStats make_bow() {
        return {WeaponType::BOW, DamageType::PIERCING, 18, 0.2f, 2.0f, 30, 600, 0.7f, 600, 1, 5};
    }
    static WeaponStats make_gun() {
        return {WeaponType::GUN, DamageType::PIERCING, 35, 0.1f, 1.5f, 100, 1200, 0.3f, 1200, 1, 3};
    }
    static WeaponStats make_fire_staff() {
        return {WeaponType::STAFF, DamageType::FIRE, 25, 0.25f, 2.5f, 20, 400, 0.6f, 400, 0, 10};
    }
    static WeaponStats make_ice_staff() {
        return {WeaponType::STAFF, DamageType::ICE, 18, 0.2f, 2.0f, 10, 400, 0.6f, 400, 0, 10};
    }
    static WeaponStats make_poison_dagger() {
        return {WeaponType::SWORD, DamageType::POISON, 12, 0.3f, 2.0f, 40, 35, 0.3f, 0, 0, 4};
    }
};

} // namespace krono
