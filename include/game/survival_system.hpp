#pragma once
// KronoUniverse — Survival System (v0.5)
//
// Sistema de sobrevivência:
// - Fome (hunger): diminui ao longo do tempo, drena HP quando 0
// - Sede (thirst): diminui mais rápido, drena HP quando 0
// - Stamina: gasta ao correr/pular/atacar, regenera ao descansar
// - Temperatura: hypotermia em biomas frios, heat stroke em quentes
// - Sanidade (sanity): diminui em cavernas escuras, ao matar, etc
// - Doenças (disease): comidas estragadas, água suja, veneno
// - Sono (sleep): precisa dormir ou ter penalidades

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "engine/character_components.hpp"
#include <cstdint>
#include <cmath>

namespace krono {

struct SurvivalStats {
    float hunger = 100.0f;      // 0-100, 0 = starving
    float thirst = 100.0f;      // 0-100, 0 = dehydrated
    float stamina = 100.0f;     // 0-100, 0 = exhausted
    float max_stamina = 100.0f;
    float temperature = 37.0f;  // body temp in Celsius (35-39 safe range)
    float sanity = 100.0f;      // 0-100, 0 = insane
    float sleep = 100.0f;       // 0-100, 0 = sleep deprived

    // Decay rates per second
    float hunger_decay = 0.05f;     // 100 -> 0 in ~33 minutes
    float thirst_decay = 0.1f;      // 100 -> 0 in ~17 minutes
    float stamina_regen = 5.0f;     // 100 -> 0 in 20 sec of running
    float stamina_run_cost = 3.0f;  // per second of running
    float stamina_jump_cost = 8.0f; // per jump
    float stamina_attack_cost = 5.0f;
    float sleep_decay = 0.02f;      // 100 -> 0 in ~83 minutes

    // Status flags
    bool is_starving = false;
    bool is_dehydrated = false;
    bool is_exhausted = false;
    bool is_hypothermic = false;
    bool is_hyperthermic = false;
    bool is_insane = false;
    bool is_sleep_deprived = false;
};

class SurvivalSystem {
public:
    static void update(Registry& reg, float dt, float ambient_temp, bool in_dark_cave, bool is_moving, bool is_running) {
        reg.each<SurvivalStats, Health>([&](auto ent, SurvivalStats& s, Health& hp) {
            // Decay hunger and thirst
            s.hunger = std::max(0.0f, s.hunger - s.hunger_decay * dt);
            s.thirst = std::max(0.0f, s.thirst - s.thirst_decay * dt);
            s.sleep = std::max(0.0f, s.sleep - s.sleep_decay * dt);

            // Stamina
            if (is_running && is_moving) {
                s.stamina = std::max(0.0f, s.stamina - s.stamina_run_cost * dt);
            } else {
                s.stamina = std::min(s.max_stamina, s.stamina + s.stamina_regen * dt);
            }

            // Body temperature - drift toward ambient
            float target_temp = ambient_temp;
            float temp_diff = target_temp - s.temperature;
            s.temperature += temp_diff * 0.01f * dt;
            // Cold biomes reduce body temp
            if (ambient_temp < 0) s.temperature -= 0.5f * dt;
            if (ambient_temp > 40) s.temperature += 0.5f * dt;
            s.temperature = std::max(30.0f, std::min(45.0f, s.temperature));

            // Sanity - decreases in dark caves
            if (in_dark_cave) {
                s.sanity = std::max(0.0f, s.sanity - 0.5f * dt);
            } else {
                s.sanity = std::min(100.0f, s.sanity + 0.1f * dt);
            }

            // Update status flags
            s.is_starving = (s.hunger <= 0);
            s.is_dehydrated = (s.thirst <= 0);
            s.is_exhausted = (s.stamina <= 0);
            s.is_hypothermic = (s.temperature < 35.0f);
            s.is_hyperthermic = (s.temperature > 39.0f);
            s.is_insane = (s.sanity <= 0);
            s.is_sleep_deprived = (s.sleep <= 0);

            // Apply damage when starving/dehydrated
            if (s.is_starving) {
                hp.current -= 1.0f * dt;
            }
            if (s.is_dehydrated) {
                hp.current -= 2.0f * dt;  // dehydration is worse
            }
            if (s.is_hypothermic) {
                hp.current -= 3.0f * dt;
            }
            if (s.is_hyperthermic) {
                hp.current -= 3.0f * dt;
            }

            // Prevent HP going below 0
            if (hp.current < 0) hp.current = 0;
        });
    }

    // Eat food to restore hunger
    static void eat(SurvivalStats& s, float hunger_amount, float sanity_bonus = 0) {
        s.hunger = std::min(100.0f, s.hunger + hunger_amount);
        s.sanity = std::min(100.0f, s.sanity + sanity_bonus);
    }

    // Drink to restore thirst
    static void drink(SurvivalStats& s, float thirst_amount) {
        s.thirst = std::min(100.0f, s.thirst + thirst_amount);
    }

    // Sleep to restore sleep stat
    static void sleep(SurvivalStats& s, float duration) {
        s.sleep = std::min(100.0f, s.sleep + duration * 10);
        s.stamina = s.max_stamina;
    }

    // Get movement speed multiplier based on stats
    static float get_speed_multiplier(const SurvivalStats& s) {
        float mult = 1.0f;
        if (s.is_exhausted) mult *= 0.3f;  // very slow when exhausted
        else if (s.stamina < 30) mult *= 0.6f;
        if (s.is_hypothermic) mult *= 0.5f;
        if (s.is_hyperthermic) mult *= 0.7f;
        if (s.is_starving) mult *= 0.7f;
        if (s.is_dehydrated) mult *= 0.5f;
        if (s.is_sleep_deprived) mult *= 0.6f;
        if (s.is_insane) mult *= 0.8f;
        return mult;
    }

    // Get jump height multiplier
    static float get_jump_multiplier(const SurvivalStats& s) {
        float mult = 1.0f;
        if (s.is_exhausted) mult *= 0.4f;
        if (s.is_hypothermic) mult *= 0.6f;
        if (s.is_dehydrated) mult *= 0.7f;
        return mult;
    }

    // Check if player can perform action (jump, attack)
    static bool can_perform_action(const SurvivalStats& s, float stamina_cost) {
        return s.stamina >= stamina_cost;
    }

    // Consume stamina for an action
    static void consume_stamina(SurvivalStats& s, float cost) {
        s.stamina = std::max(0.0f, s.stamina - cost);
    }
};

} // namespace krono
