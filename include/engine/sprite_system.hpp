#pragma once
// KronoUniverse — Procedural Pixel Art Generator + Particles + Lighting
// Gera sprites, partículas e iluminação proceduralmente (sem arquivos externos).

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <string>
#include "render/renderer.hpp"
#include "procedural/world.hpp"

namespace krono {

class SpriteGenerator {
public:
    // Generate a character sprite sheet (4 directions x 4 animation frames)
    // Returns a SDL_Texture that can be used for rendering
    // Each frame is 32x48 pixels, arranged in a grid
    static SDL_Texture* generate_character_sheet(SDL_Renderer* sdl_renderer) {
        // We'll use SDL_Surface for pixel manipulation
        const int FRAME_W = 32;
        const int FRAME_H = 48;
        const int COLS = 4; // 4 animation frames per direction
        const int ROWS = 4; // 4 directions: down, left, right, up
        
        SDL_Surface* surface = SDL_CreateRGBSurface(0, FRAME_W * COLS, FRAME_H * ROWS, 32,
            0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        
        Uint32 transparent = SDL_MapRGBA(surface->format, 0, 0, 0, 0);
        SDL_FillRect(surface, NULL, transparent);
        
        Uint32 skin = SDL_MapRGBA(surface->format, 220, 180, 140, 255);
        Uint32 hair = SDL_MapRGBA(surface->format, 80, 50, 30, 255);
        Uint32 shirt = SDL_MapRGBA(surface->format, 60, 120, 200, 255);
        Uint32 pants = SDL_MapRGBA(surface->format, 50, 50, 60, 255);
        Uint32 eyes = SDL_MapRGBA(surface->format, 30, 30, 40, 255);
        Uint32 outline = SDL_MapRGBA(surface->format, 20, 20, 30, 255);
        
        for (int row = 0; row < ROWS; row++) {
            for (int col = 0; col < COLS; col++) {
                int ox = col * FRAME_W;
                int oy = row * FRAME_H;
                
                // Animation offset (walking cycle)
                int leg_offset = 0;
                if (col == 1) leg_offset = 2;
                else if (col == 3) leg_offset = -2;
                
                // Head (8x8 at top center)
                fill_rect(surface, ox+12, oy+4, 8, 8, skin);
                // Hair (top of head)
                fill_rect(surface, ox+11, oy+3, 10, 3, hair);
                fill_rect(surface, ox+11, oy+4, 1, 4, hair);
                fill_rect(surface, ox+20, oy+4, 1, 4, hair);
                
                // Eyes (varies by direction)
                if (row == 0) { // down
                    fill_rect(surface, ox+13, oy+7, 2, 2, eyes);
                    fill_rect(surface, ox+17, oy+7, 2, 2, eyes);
                } else if (row == 1) { // left
                    fill_rect(surface, ox+13, oy+7, 2, 2, eyes);
                } else if (row == 2) { // right
                    fill_rect(surface, ox+17, oy+7, 2, 2, eyes);
                }
                // up: no eyes visible
                
                // Body/torso (12x14)
                fill_rect(surface, ox+10, oy+14, 12, 14, shirt);
                // Arms (3x14 on each side)
                fill_rect(surface, ox+7, oy+14, 3, 12, skin);
                fill_rect(surface, ox+22, oy+14, 3, 12, skin);
                
                // Legs (5x16 each, with walk animation offset)
                fill_rect(surface, ox+10, oy+28+leg_offset, 5, 16, pants);
                fill_rect(surface, ox+17, oy+28-leg_offset, 5, 16, pants);
                
                // Feet (shoes)
                fill_rect(surface, ox+10, oy+44+leg_offset, 5, 2, outline);
                fill_rect(surface, ox+17, oy+44-leg_offset, 5, 2, outline);
            }
        }
        
        SDL_Texture* tex = SDL_CreateTextureFromSurface(sdl_renderer, surface);
        SDL_FreeSurface(surface);
        return tex;
    }
    
    // Generate a simple particle texture (soft circle)
    static SDL_Texture* generate_particle(SDL_Renderer* sdl_renderer, int r, int g, int b) {
        const int SIZE = 16;
        SDL_Surface* surface = SDL_CreateRGBSurface(0, SIZE, SIZE, 32,
            0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
        
        Uint32 transparent = SDL_MapRGBA(surface->format, 0, 0, 0, 0);
        SDL_FillRect(surface, NULL, transparent);
        
        int cx = SIZE/2, cy = SIZE/2;
        Uint32 color = SDL_MapRGBA(surface->format, r, g, b, 255);
        
        for (int y = 0; y < SIZE; y++) {
            for (int x = 0; x < SIZE; x++) {
                float dx = x - cx, dy = y - cy;
                float dist = sqrt(dx*dx + dy*dy);
                if (dist <= SIZE/2) {
                    float alpha = 1.0f - (dist / (SIZE/2));
                    Uint8 a = (Uint8)(alpha * 255);
                    Uint32 c = SDL_MapRGBA(surface->format, r, g, b, a);
                    put_pixel(surface, x, y, c);
                }
            }
        }
        
        SDL_Texture* tex = SDL_CreateTextureFromSurface(sdl_renderer, surface);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_FreeSurface(surface);
        return tex;
    }

private:
    static void fill_rect(SDL_Surface* surf, int x, int y, int w, int h, Uint32 color) {
        SDL_Rect r = {x, y, w, h};
        SDL_FillRect(surf, &r, color);
    }
    
    static void put_pixel(SDL_Surface* surf, int x, int y, Uint32 color) {
        if (x < 0 || x >= surf->w || y < 0 || y >= surf->h) return;
        Uint32* pixels = (Uint32*)surf->pixels;
        pixels[y * surf->w + x] = color;
    }
};

// ---- Particle System ----
struct Particle {
    float x, y;
    float vx, vy;
    float life;
    float max_life;
    float r, g, b;
    float size;
};

class ParticleSystem {
public:
    std::vector<Particle> particles;
    
    void emit(float x, float y, int count, float r, float g, float b,
              float speed_min = 50, float speed_max = 200,
              float life_min = 0.3f, float life_max = 0.8f,
              float size_min = 2, float size_max = 5) {
        for (int i = 0; i < count; i++) {
            float angle = ((float)rand() / RAND_MAX) * 2 * M_PI;
            float speed = speed_min + ((float)rand() / RAND_MAX) * (speed_max - speed_min);
            float life = life_min + ((float)rand() / RAND_MAX) * (life_max - life_min);
            float size = size_min + ((float)rand() / RAND_MAX) * (size_max - size_min);
            
            particles.push_back({
                x, y,
                cos(angle) * speed, sin(angle) * speed,
                life, life,
                r, g, b,
                size
            });
        }
    }
    
    // Emit dust particles (when mining)
    void emit_dust(float x, float y, float block_r, float block_g, float block_b) {
        emit(x + 8, y + 8, 8, block_r * 0.8f, block_g * 0.8f, block_b * 0.8f,
             30, 100, 0.3f, 0.6f, 1, 3);
    }
    
    // Emit sparks (when hitting metal)
    void emit_sparks(float x, float y) {
        emit(x, y, 6, 1.0f, 0.8f, 0.2f,
             80, 200, 0.2f, 0.5f, 1, 2);
    }
    
    // Emit landing dust
    void emit_land(float x, float y) {
        for (int i = 0; i < 6; i++) {
            float angle = M_PI + ((float)rand() / RAND_MAX) * M_PI; // upward spread
            float speed = 30 + ((float)rand() / RAND_MAX) * 60;
            particles.push_back({
                x + (float)(rand() % 20 - 10), y + 20,
                cos(angle) * speed, sin(angle) * speed,
                0.4f, 0.4f,
                0.6f, 0.5f, 0.4f,
                2 + (float)(rand() % 3)
            });
        }
    }
    
    void update(float dt) {
        for (auto& p : particles) {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.vy += 200 * dt; // gravity on particles
            p.vx *= 0.95f;
            p.life -= dt;
        }
        // Remove dead particles
        particles.erase(
            std::remove_if(particles.begin(), particles.end(),
                [](const Particle& p) { return p.life <= 0; }),
            particles.end()
        );
    }
    
    void render(Renderer& renderer) {
        for (auto& p : particles) {
            float alpha = p.life / p.max_life;
            renderer.draw_rect(p.x, p.y, p.size, p.size, p.r, p.g, p.b, alpha);
        }
    }
    
    size_t count() const { return particles.size(); }
};

// ---- Lighting System (simple darkness in caves) ----
class LightingSystem {
public:
    // Calculate light level at a position (0 = pitch black, 1 = full bright)
    static float get_light_level(World& world, int bx, int by, float time_of_day) {
        // Day/night cycle (0 = midnight, 0.5 = noon)
        float daylight = sin(time_of_day * M_PI); // 0 at midnight, 1 at noon
        daylight = std::max(0.0f, daylight);
        
        // Count solid blocks above (sky occlusion)
        int occlusion = 0;
        for (int y = by - 1; y >= 0; y--) {
            Block* b = world.get_block(bx, y);
            if (b && b->is_solid()) {
                occlusion++;
                if (occlusion > 10) break;
            }
        }
        
        float sky_light = 1.0f - (occlusion / 10.0f);
        sky_light = std::max(0.0f, sky_light);
        
        // Underground = dark
        if (occlusion >= 10) {
            // Check for light sources (lava, torches)
            float light = 0.0f;
            for (int dx = -5; dx <= 5; dx++) {
                for (int dy = -5; dy <= 5; dy++) {
                    Block* b = world.get_block(bx + dx, by + dy);
                    if (b) {
                        if (b->type == BlockType::LAVA) {
                            float d = sqrt(dx*dx + dy*dy);
                            light = std::max(light, 0.8f * (1.0f - d/5.0f));
                        } else if (b->type == BlockType::CRYSTAL) {
                            float d = sqrt(dx*dx + dy*dy);
                            light = std::max(light, 0.6f * (1.0f - d/5.0f));
                        }
                    }
                }
            }
            return light;
        }
        
        return sky_light * daylight;
    }
    
    // Apply darkness overlay to the screen
    static void render_darkness(Renderer& renderer, int screen_w, int screen_h, float time_of_day) {
        float daylight = sin(time_of_day * M_PI);
        daylight = std::max(0.0f, daylight);
        if (daylight < 0.3f) {
            float darkness = 1.0f - (daylight / 0.3f);
            renderer.set_screen_ortho();
            renderer.draw_rect(0, 0, screen_w, screen_h, 0.0f, 0.0f, 0.1f, darkness * 0.5f);
        }
    }
};

} // namespace krono
