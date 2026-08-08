#pragma once
// KronoUniverse — Energy/Redstone System (v0.6)
//
// Sistema de circuitos de energia:
// - Fontes de energia (geradores, painéis solares, baterias)
// - Condutores (fios que transmitem energia)
// - Consumidores (lâmpadas, portões, máquinas)
// - Portas lógicas (AND, OR, NOT)
// - Pressão de energia (0-15 níveis como redstone)
// - Atualização em cascata (energia se propaga)

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "procedural/world.hpp"
#include <vector>
#include <queue>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace krono {

enum class PowerElementType : uint8_t {
    NONE = 0,
    SOURCE,      // generates power (e.g. solar panel, generator)
    WIRE,        // conducts power
    LAMP,        // lights up when powered
    DOOR,        // opens when powered
    PRESSURE_PLATE,  // emits power when entity on top
    LEVER,       // manual power source
    BUTTON,      // temporary power source
    NOT_GATE,    // inverts signal
    SENSOR,      // detects entities
};

struct PowerElement {
    PowerElementType type = PowerElementType::WIRE;
    int x = 0, y = 0;           // block position
    uint8_t power_level = 0;     // 0-15
    uint8_t output_power = 0;    // power it provides to neighbors
    bool is_source = false;
    bool active = false;         // for visual state
    float cooldown = 0;          // for buttons
};

class PowerSystem {
public:
    std::vector<PowerElement> elements;

    PowerElement* find(int x, int y) {
        for (auto& e : elements) {
            if (e.x == x && e.y == y) return &e;
        }
        return nullptr;
    }

    void add(const PowerElement& e) {
        if (!find(e.x, e.y)) {
            elements.push_back(e);
        }
    }

    void remove(int x, int y) {
        elements.erase(std::remove_if(elements.begin(), elements.end(),
            [x, y](const PowerElement& e) { return e.x == x && e.y == y; }),
            elements.end());
    }

    // Update power propagation using BFS
    void update(float dt, World& world, Entity player) {
        // First, reset all non-source power levels
        for (auto& e : elements) {
            if (!e.is_source) {
                e.power_level = 0;
                e.output_power = 0;
            }
            // Update sources
            if (e.type == PowerElementType::SOURCE) {
                e.power_level = 15;
                e.output_power = 15;
            } else if (e.type == PowerElementType::LEVER) {
                e.output_power = e.active ? 15 : 0;
            } else if (e.type == PowerElementType::BUTTON) {
                if (e.cooldown > 0) {
                    e.output_power = 15;
                    e.cooldown -= dt;
                    if (e.cooldown < 0) e.cooldown = 0;
                } else {
                    e.output_power = 0;
                }
            }
        }

        // BFS propagation from sources
        std::queue<std::tuple<int, int, uint8_t>> to_process;
        for (auto& e : elements) {
            if (e.output_power > 0) {
                to_process.push({e.x, e.y, e.output_power});
            }
        }

        while (!to_process.empty()) {
            auto [x, y, power] = to_process.front();
            to_process.pop();
            if (power <= 1) continue;

            // Check 4 neighbors
            int neighbors[4][2] = {{x+1, y}, {x-1, y}, {x, y+1}, {x, y-1}};
            for (auto& [nx, ny] : neighbors) {
                PowerElement* n = find(nx, ny);
                if (!n) continue;
                if (n->is_source) continue;
                uint8_t new_power = power - 1;
                if (n->power_level < new_power) {
                    n->power_level = new_power;
                    n->output_power = new_power;
                    n->active = (new_power > 0);
                    to_process.push({nx, ny, new_power});
                }
            }
        }

        // Update active states
        for (auto& e : elements) {
            e.active = (e.power_level > 0 || e.output_power > 0);
        }
    }

    // Press a button (temporary activation)
    void press_button(int x, int y) {
        PowerElement* e = find(x, y);
        if (e && e->type == PowerElementType::BUTTON) {
            e->cooldown = 1.0f;  // 1 second
        }
    }

    // Toggle a lever
    void toggle_lever(int x, int y) {
        PowerElement* e = find(x, y);
        if (e && e->type == PowerElementType::LEVER) {
            e->active = !e->active;
        }
    }

    // Get lamp brightness (0-1)
    float get_lamp_brightness(int x, int y) const {
        for (auto& e : elements) {
            if (e.x == x && e.y == y && e.type == PowerElementType::LAMP) {
                return (float)e.power_level / 15.0f;
            }
        }
        return 0;
    }

    // Count active elements
    int count_active() const {
        int count = 0;
        for (auto& e : elements) {
            if (e.active) count++;
        }
        return count;
    }

    // Get color for power element rendering
    static void get_color(PowerElementType type, bool active, float& r, float& g, float& b) {
        switch (type) {
            case PowerElementType::WIRE:
                r = active ? 1.0f : 0.3f;
                g = active ? 0.2f : 0.2f;
                b = active ? 0.2f : 0.2f;
                break;
            case PowerElementType::SOURCE:
                r = 0.8f; g = 0.8f; b = 0.2f;  // yellow
                break;
            case PowerElementType::LAMP:
                r = active ? 1.0f : 0.4f;
                g = active ? 0.9f : 0.4f;
                b = active ? 0.5f : 0.4f;
                break;
            case PowerElementType::LEVER:
                r = 0.5f; g = 0.4f; b = 0.3f;
                break;
            case PowerElementType::BUTTON:
                r = 0.6f; g = 0.3f; b = 0.3f;
                break;
            case PowerElementType::DOOR:
                r = active ? 0.3f : 0.6f;
                g = active ? 0.6f : 0.4f;
                b = 0.3f;
                break;
            case PowerElementType::NOT_GATE:
                r = 0.4f; g = 0.4f; b = 0.4f;
                break;
            case PowerElementType::PRESSURE_PLATE:
                r = 0.6f; g = 0.6f; b = 0.6f;
                break;
            default:
                r = 0.5f; g = 0.5f; b = 0.5f;
                break;
        }
    }
};

} // namespace krono
