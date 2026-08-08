#pragma once
// KronoUniverse — Lighting System (v0.3)
//
// Iluminação dinâmica 2D:
// - Fontes de luz pontuais (tochas, lava, cristais, magia)
// - Light map em grid (16x16 px por célula)
// - Mistura com luz ambiente (dia/noite)
// - Sombras suaves (não raycasting — usa occlusion aprox)
// - Cores de luz (laranja para fogo, azul para gelo, etc.)
// - Performance: max 32 luzes ativas simultâneas

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "procedural/world.hpp"
#include <SDL2/SDL_opengl.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace krono {

struct LightSource {
    enum Type : uint8_t {
        TORCH = 0,        // warm flickering
        LAVA_GLOW,        // orange-red, constant
        CRYSTAL_GLOW,     // cyan, constant
        MAGIC_FIRE,       // bright orange, sparks
        MAGIC_ICE,        // bright cyan
        PLAYER_GLOW,      // subtle white
        EXPLOSION,        // brief intense flash
        STAR,             // small twinkle
        MOONLIGHT,        // global subtle
    };
    Type type = TORCH;
    float x = 0, y = 0;
    float radius = 80;       // in pixels
    float intensity = 1.0f;  // 0-1
    float r = 1, g = 1, b = 1;
    float flicker = 0;       // 0 = stable, 1 = full flicker
    float lifetime = -1;     // -1 = infinite
    float elapsed = 0;
};

class LightingSystem {
public:
    static constexpr int LIGHT_GRID_SIZE = 16; // pixels per light cell
    static constexpr int MAX_LIGHTS = 64;

    std::vector<LightSource> lights;
    float ambient_light = 0.6f; // 0=dark, 1=bright
    float ambient_r = 1.0f, ambient_g = 0.95f, ambient_b = 0.85f;

    void add_light(LightSource::Type type, float x, float y, float radius, float intensity = 1.0f) {
        if ((int)lights.size() >= MAX_LIGHTS) {
            // Try to remove oldest temporary light first
            bool removed = false;
            for (auto it = lights.begin(); it != lights.end(); ++it) {
                if (it->lifetime > 0) {
                    lights.erase(it);
                    removed = true;
                    break;
                }
            }
            // If no temporary lights, remove the oldest permanent one
            if (!removed) {
                lights.erase(lights.begin());
            }
        }
        LightSource l;
        l.type = type;
        l.x = x; l.y = y;
        l.radius = radius;
        l.intensity = intensity;
        switch (type) {
            case LightSource::TORCH:
                l.r = 1.0f; l.g = 0.7f; l.b = 0.3f;
                l.flicker = 0.15f;
                break;
            case LightSource::LAVA_GLOW:
                l.r = 1.0f; l.g = 0.4f; l.b = 0.1f;
                l.flicker = 0.1f;
                break;
            case LightSource::CRYSTAL_GLOW:
                l.r = 0.5f; l.g = 0.85f; l.b = 1.0f;
                l.flicker = 0.05f;
                break;
            case LightSource::MAGIC_FIRE:
                l.r = 1.0f; l.g = 0.6f; l.b = 0.2f;
                l.flicker = 0.2f;
                l.lifetime = 2.0f;
                break;
            case LightSource::MAGIC_ICE:
                l.r = 0.6f; l.g = 0.9f; l.b = 1.0f;
                l.flicker = 0.15f;
                l.lifetime = 2.0f;
                break;
            case LightSource::PLAYER_GLOW:
                l.r = 1.0f; l.g = 1.0f; l.b = 0.9f;
                l.flicker = 0;
                l.intensity = 0.5f;
                break;
            case LightSource::EXPLOSION:
                l.r = 1.0f; l.g = 0.8f; l.b = 0.4f;
                l.flicker = 0.3f;
                l.lifetime = 0.5f;
                break;
            case LightSource::STAR:
                l.r = 1.0f; l.g = 1.0f; l.b = 1.0f;
                l.flicker = 0.5f;
                l.radius = 30;
                break;
            case LightSource::MOONLIGHT:
                l.r = 0.6f; l.g = 0.7f; l.b = 1.0f;
                l.intensity = 0.4f;
                l.radius = 500;
                break;
        }
        lights.push_back(l);
    }

    void update(float dt, float player_x, float player_y, float time_of_day) {
        // Ambient based on day/night
        float daylight = std::sin(time_of_day * M_PI);
        daylight = std::max(0.15f, daylight);
        ambient_light = daylight;
        // Night has cooler tint
        float night_factor = 1.0f - daylight;
        ambient_r = 1.0f - night_factor * 0.3f;
        ambient_g = 0.95f - night_factor * 0.15f;
        ambient_b = 0.85f + night_factor * 0.2f;

        // Add player glow if dark
        bool has_player_glow = false;
        for (auto& l : lights) {
            if (l.type == LightSource::PLAYER_GLOW) {
                l.x = player_x;
                l.y = player_y;
                has_player_glow = true;
                break;
            }
        }
        if (!has_player_glow && ambient_light < 0.4f) {
            add_light(LightSource::PLAYER_GLOW, player_x, player_y, 120, 0.4f);
        }

        // Update light lifetimes
        for (auto it = lights.begin(); it != lights.end(); ) {
            it->elapsed += dt;
            if (it->lifetime > 0 && it->elapsed >= it->lifetime) {
                it = lights.erase(it);
            } else {
                ++it;
            }
        }

        // Random flicker
        for (auto& l : lights) {
            if (l.flicker > 0) {
                l.intensity = std::max(0.4f, 1.0f - ((float)rand()/RAND_MAX) * l.flicker);
            }
        }
    }

    // Get lighting at a position — used by renderer to tint blocks
    // Returns {r, g, b} light multiplier (each 0-1+)
    struct LightResult { float r, g, b; };
    LightResult get_light_at(float x, float y) const {
        float r = ambient_r * ambient_light;
        float g = ambient_g * ambient_light;
        float b = ambient_b * ambient_light;

        for (auto& l : lights) {
            float dx = x - l.x;
            float dy = y - l.y;
            float dist2 = dx*dx + dy*dy;
            float r2 = l.radius * l.radius;
            if (dist2 > r2) continue;
            float falloff = 1.0f - std::sqrt(dist2) / l.radius;
            falloff *= falloff; // quadratic falloff
            falloff *= l.intensity;
            r += l.r * falloff;
            g += l.g * falloff;
            b += l.b * falloff;
        }
        return {r, g, b};
    }

    // Render light glows as additive quads (after main render)
    template<typename Renderer>
    void render_lights(Renderer& r) {
        // Use additive blending for glows
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        for (auto& l : lights) {
            // Draw concentric circles (quads approximating radial gradient)
            int steps = 4;
            for (int i = steps; i > 0; i--) {
                float radius = l.radius * (float)i / steps;
                float alpha = l.intensity * 0.15f * (1.0f - (float)(i-1) / steps);
                r.draw_rect(l.x - radius, l.y - radius, radius*2, radius*2,
                            l.r, l.g, l.b, alpha);
            }
        }
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    // Render fog overlay
    template<typename Renderer>
    void render_fog(Renderer& r, float cam_x, float cam_y, float w, float h, float density) {
        if (density < 0.05f) return;
        r.draw_rect(cam_x, cam_y, w, h, 0.7f, 0.7f, 0.75f, density * 0.5f);
    }

    void clear() { lights.clear(); }
};

} // namespace krono
