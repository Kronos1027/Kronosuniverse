#pragma once
// KronoUniverse — Weather System (v0.3)
//
// Clima dinâmico com:
// - Chuva (afeta movimento, sons)
// - Neve (acumula no chão)
// - Tempestades (raios + trovões)
// - Neblina (visibilidade reduzida)
// - Ventania (empurra entidades leves)
// - Transições suaves entre climas
// - Bioma afeta clima padrão (deserto: sem chuva, tundra: neve)

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "procedural/world.hpp"
#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace krono {

enum class WeatherType : uint8_t {
    CLEAR = 0,
    CLOUDY,
    RAIN,
    SNOW,
    STORM,
    FOG,
    SANDSTORM,
};

struct WeatherParticle {
    float x, y;
    float vx, vy;
    float life;
    float size;
    float r, g, b, a;
};

class WeatherSystem {
public:
    WeatherType current = WeatherType::CLEAR;
    WeatherType target = WeatherType::CLEAR;
    float transition = 1.0f;  // 0 = target, 1 = current
    float intensity = 0.5f;   // 0-1 strength
    float wind_x = 0;         // wind direction/speed
    float lightning_timer = 0;
    float lightning_flash = 0;
    float fog_density = 0;
    std::vector<WeatherParticle> particles;

    // Decide weather based on biome + randomness
    void decide_for_biome(BiomeType biome) {
        float r = (float)rand() / RAND_MAX;
        switch (biome) {
            case BiomeType::DESERT:
                target = (r < 0.7f) ? WeatherType::CLEAR : WeatherType::SANDSTORM;
                break;
            case BiomeType::TUNDRA:
                target = (r < 0.4f) ? WeatherType::SNOW : (r < 0.7f ? WeatherType::CLEAR : WeatherType::CLOUDY);
                break;
            case BiomeType::VOLCANIC:
                target = (r < 0.6f) ? WeatherType::CLEAR : WeatherType::FOG;
                break;
            case BiomeType::OCEAN:
            case BiomeType::BEACH:
                target = (r < 0.4f) ? WeatherType::CLEAR : (r < 0.7f ? WeatherType::RAIN : WeatherType::STORM);
                break;
            case BiomeType::FOREST:
            case BiomeType::PLAINS:
            default:
                target = (r < 0.4f) ? WeatherType::CLEAR : (r < 0.65f ? WeatherType::CLOUDY : (r < 0.85f ? WeatherType::RAIN : WeatherType::STORM));
                break;
        }
        if (target != current) transition = 0;
    }

    void update(float dt, float cam_x, float cam_y, float view_w, float view_h) {
        // Transition toward target
        if (transition < 1.0f) {
            transition += dt / 10.0f; // 10s transition
            if (transition >= 1.0f) {
                current = target;
                transition = 1.0f;
            }
        }

        // Wind dynamics
        if (current == WeatherType::STORM || target == WeatherType::STORM) {
            wind_x += ((float)(rand()%100-50)/50.0f) * 30 * dt;
            wind_x = std::max(-200.0f, std::min(200.0f, wind_x));
        } else if (current == WeatherType::SANDSTORM || target == WeatherType::SANDSTORM) {
            wind_x = -150; // blows from east
        } else {
            wind_x *= 0.95f;
        }

        // Spawn particles
        int max_particles = 0;
        float spawn_rate = 0;
        float pr=1, pg=1, pb=1, pa=0.7f;
        float psize = 2;
        float pvy = 0, pvx = 0;

        switch (current) {
            case WeatherType::RAIN:
                max_particles = 300;
                spawn_rate = 200 * intensity;
                pr = 0.5f; pg = 0.7f; pb = 1.0f; pa = 0.6f;
                psize = 1.5f;
                pvy = 400;
                pvx = wind_x * 0.3f;
                break;
            case WeatherType::SNOW:
                max_particles = 200;
                spawn_rate = 80 * intensity;
                pr = 1; pg = 1; pb = 1; pa = 0.85f;
                psize = 2.5f;
                pvy = 60;
                pvx = wind_x * 0.5f;
                break;
            case WeatherType::STORM:
                max_particles = 400;
                spawn_rate = 250 * intensity;
                pr = 0.4f; pg = 0.6f; pb = 1.0f; pa = 0.7f;
                psize = 1.8f;
                pvy = 500;
                pvx = wind_x * 0.4f;
                // Lightning
                lightning_timer -= dt;
                if (lightning_timer <= 0) {
                    lightning_flash = 1.0f;
                    lightning_timer = 3.0f + (float)(rand()%50)/10.0f;
                }
                if (lightning_flash > 0) lightning_flash -= dt * 3;
                break;
            case WeatherType::SANDSTORM:
                max_particles = 500;
                spawn_rate = 300 * intensity;
                pr = 0.85f; pg = 0.7f; pb = 0.4f; pa = 0.5f;
                psize = 1.5f;
                pvy = 30;
                pvx = wind_x;
                break;
            case WeatherType::FOG:
                fog_density = std::min(0.6f, fog_density + dt * 0.05f);
                max_particles = 0;
                break;
            default:
                fog_density = std::max(0.0f, fog_density - dt * 0.05f);
                max_particles = 0;
                break;
        }

        // Spawn new particles
        if ((int)particles.size() < max_particles) {
            int to_spawn = (int)(spawn_rate * dt);
            for (int i = 0; i < to_spawn && (int)particles.size() < max_particles; i++) {
                WeatherParticle p;
                p.x = cam_x + (float)(rand() % (int)view_w);
                p.y = cam_y - 20;
                p.vx = pvx + ((float)(rand()%100-50)/50.0f) * 20;
                p.vy = pvy + ((float)(rand()%100)/100.0f) * 50;
                p.life = view_h / std::max(1.0f, pvy) + 1.0f;
                p.size = psize + ((float)(rand()%100)/100.0f) * 0.5f;
                p.r = pr; p.g = pg; p.b = pb; p.a = pa;
                particles.push_back(p);
            }
        }

        // Update particles
        for (auto& p : particles) {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.life -= dt;
        }
        particles.erase(std::remove_if(particles.begin(), particles.end(),
            [&](const WeatherParticle& p) {
                return p.life <= 0 || p.y > cam_y + view_h + 20 || p.x < cam_x - 50 || p.x > cam_x + view_w + 50;
            }), particles.end());
    }

    // Apply wind force to light entities
    void apply_wind(Registry& reg, float dt) {
        if (std::abs(wind_x) < 30) return;
        reg.each<Velocity, Mass>([&](auto ent, Velocity& v, Mass& m) {
            if (m.value < 5) {
                v.x += wind_x * dt * 0.5f / m.value;
            }
        });
    }

    // Render weather particles
    template<typename Renderer>
    void render(Renderer& r) {
        for (auto& p : particles) {
            r.draw_rect(p.x, p.y, p.size, p.size, p.r, p.g, p.b, p.a);
        }
    }

    // Returns true if a lightning flash is happening (for screen tint)
    float get_lightning_intensity() const {
        return std::max(0.0f, lightning_flash);
    }

    float get_fog_density() const {
        return fog_density;
    }

    bool is_stormy() const {
        return current == WeatherType::STORM || current == WeatherType::SANDSTORM;
    }
};

} // namespace krono
