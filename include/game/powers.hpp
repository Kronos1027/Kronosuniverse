#pragma once
// KronoUniverse — Powers & Social Stats (Fase 7, GDD v0.2 seções 2 & 4)
// 3 origens de poderes + stats sociais (fome, reputação, respeito, fé, ódio, medo)

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include <string>

namespace krono {

// ---- Power Origins (GDD v0.2 seção 2) ----
enum class PowerOrigin : uint8_t {
    TECHNOLOGICAL,  // tech items — flight pack, energy weapons
    BIOLOGICAL,     // mutations — regeneration, speed boost
    ANOMALOUS,      // reality-bending — telekinesis, time dilation
};

// ---- Active Power ----
struct Power {
    PowerOrigin origin;
    enum Type : uint8_t {
        FLIGHT, TELEKINESIS, SHIELD, CLOAK, REGENERATE, TIME_DILATION,
        ENERGY_BLAST, SUPER_SPEED, MAGNETIC_FIELD, PHASE_SHIFT
    };
    Type type;
    float energy_cost_per_sec = 5.0f;
    float strength = 1.0f;       // magnitude
    float cooldown = 0;
    float cooldown_remaining = 0;
    bool is_active = false;
    bool is_unlocked = true;
};

// ---- Powers Component (entity can have multiple powers) ----
struct Powers {
    std::vector<Power> powers;

    Power* find(Power::Type type) {
        for (auto& p : powers) if (p.type == type) return &p;
        return nullptr;
    }

    void toggle(Power::Type type) {
        auto* p = find(type);
        if (p && p->is_unlocked && p->cooldown_remaining <= 0) {
            p->is_active = !p->is_active;
        }
    }
};

// ---- Social Stats (GDD v0.2 seção 4) ----
struct SocialStats {
    // Survival
    float hunger = 0;          // 0 = full, 100 = starving (damage)
    float thirst = 0;          // 0 = hydrated, 100 = dehydrated
    float fatigue = 0;         // 0 = rested, 100 = exhausted

    // Social reputation (per faction — stored as map elsewhere)
    float reputation = 0;      // global reputation modifier
    float respect = 0;         // 0-100 (high = NPCs defer to you)
    float faith = 0;           // 0-100 (high = religious NPCs help you)
    float hatred = 0;          // 0-100 (high = NPCs hate you)
    float fear = 0;            // 0-100 (high = NPCs fear you)

    // Notoriety
    float notoriety = 0;       // 0-100 (how well known you are)

    // Moral alignment (-100 to 100)
    float morality = 0;        // -100 = evil, 100 = good

    // Update stats over time
    void update(float dt) {
        hunger += 0.5f * dt;    // gets hungry
        thirst += 0.7f * dt;    // gets thirsty faster
        fatigue += 0.3f * dt;   // gets tired

        // Clamp
        if (hunger > 100) hunger = 100;
        if (thirst > 100) thirst = 100;
        if (fatigue > 100) fatigue = 100;
    }

    // Apply stat effects to health
    float get_health_drain() const {
        float drain = 0;
        if (hunger > 80) drain += (hunger - 80) * 0.1f;  // starving
        if (thirst > 80) drain += (thirst - 80) * 0.15f; // dehydrated (worse)
        if (fatigue > 90) drain += (fatigue - 90) * 0.2f; // exhausted
        return drain;
    }

    // Movement modifiers from stats
    float get_speed_modifier() const {
        float mod = 1.0f;
        if (hunger > 50) mod -= 0.2f;     // hungry = slower
        if (thirst > 50) mod -= 0.2f;     // thirsty = slower
        if (fatigue > 70) mod -= 0.3f;    // exhausted = much slower
        return std::max(0.3f, mod);       // min 30% speed
    }

    float get_jump_modifier() const {
        float mod = 1.0f;
        if (fatigue > 60) mod -= 0.4f;    // can't jump well when tired
        return std::max(0.3f, mod);
    }
};

// ---- Power System ----
class PowerSystem {
public:
    void update(Registry& reg, float dt) {
        reg.each<Powers, Health>([&](Entity e, Powers& powers, Health& hp) {
            for (auto& power : powers.powers) {
                if (!power.is_active) {
                    if (power.cooldown_remaining > 0) {
                        power.cooldown_remaining -= dt;
                    }
                    continue;
                }

                // Apply power effects
                switch (power.type) {
                    case Power::FLIGHT:
                        apply_flight(reg, e, power, dt);
                        break;
                    case Power::REGENERATE:
                        hp.current = std::min(hp.max, hp.current + power.strength * 10 * dt);
                        break;
                    case Power::SHIELD:
                        // Reduces incoming damage (handled in damage system)
                        break;
                    case Power::CLOAK:
                        // Reduces NPC detection (handled in NPC system)
                        break;
                    case Power::TELEKINESIS:
                        // Applies force to targeted objects (Parte 5.7)
                        break;
                    case Power::TIME_DILATION:
                        // Slows down world simulation (handled in game loop)
                        break;
                    default:
                        break;
                }

                // Energy cost
                // (would drain from EnergyProducer/EnergyConsumer component)
            }
        });

        // Update social stats
        reg.each<SocialStats>([&](Entity e, SocialStats& stats) {
            stats.update(dt);

            // Apply health drain from starvation
            float drain = stats.get_health_drain();
            if (drain > 0) {
                auto* hp = reg.get<Health>(e);
                if (hp) {
                    hp->current -= drain * dt;
                    if (hp->current < 0) hp->current = 0;
                }
            }
        });
    }

private:
    void apply_flight(Registry& reg, Entity e, Power& power, float dt) {
        auto* vel = reg.get<Velocity>(e);
        if (!vel) return;
        // Counteract gravity — apply upward force
        // (Parte 5.7: voo assistido aplica força vertical contínua)
        // Full implementation in MovementSystem handles the actual flight
        // Here we just mark that flight is active
    }

    // Apply telekinetic force to target (Parte 5.7 — same função central aplicar_impulso)
    void apply_telekinesis(Registry& reg, Entity target, float force_x, float force_y) {
        auto* physics = reg.get<RigidBody>(target);
        if (!physics || physics->is_static) return;
        auto* vel = reg.get<Velocity>(target);
        auto* mass = reg.get<Mass>(target);
        if (!vel || !mass) return;
        // Δv = F/m (impulse)
        vel->x += force_x / mass->value;
        vel->y += force_y / mass->value;
    }
};

} // namespace krono
