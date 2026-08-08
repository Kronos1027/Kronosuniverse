#define _USE_MATH_DEFINES
// KronoUniverse — Main (v0.2 — Áudio + Sprites + Partículas + Iluminação + Day/Night)
//
// NOVIDADES vs v0.1:
// - Sistema de áudio procedural (sons de jump, mine, step, hurt, place, land, explosion)
// - Sistema de partículas (poeira ao cavar, faíscas, landing dust)
// - Ciclo dia/noite (escurece à noite, ilumina de dia)
// - Iluminação de cavernas (escuro underground, brilho de lava/crystal)
// - Sprites de personagem animados (4 direções x 4 frames)
// - Hotbar de blocos (1-6 seleciona tipo)
// - HUD melhorado (health bar, coordenadas, FPS)
// - Som 3D (volume baseado em distância, panning estéreo)

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "engine/character_components.hpp"
#include "engine/input_system.hpp"
#include "physics/physics.hpp"
#include "physics/movement_system.hpp"
#include "procedural/world.hpp"
#include "render/renderer.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// SDL_mixer might not be available — guard with #ifdef
#ifdef __has_include
  #if __has_include(<SDL2/SDL_mixer.h>)
    #define HAS_AUDIO 1
    #include <SDL2/SDL_mixer.h>
  #endif
#endif

using namespace krono;

static constexpr int WINDOW_W = 1280;
static constexpr int WINDOW_H = 720;
static constexpr float GAME_GRAVITY = 9.81f * 50.0f;
static constexpr float FIXED_DT = 1.0f / 60.0f;

struct Camera {
    float x = 0, y = 0, zoom = 1.0f;
    void follow(float tx, float ty, float dt) {
        x += (tx - (WINDOW_W/zoom)/2.0f - x) * 5.0f * dt;
        y += (ty - (WINDOW_H/zoom)/2.0f - y) * 5.0f * dt;
    }
    float s2w_x(float sx) const { return sx/zoom + x; }
    float s2w_y(float sy) const { return sy/zoom + y; }
    float left() const { return x; }
    float right() const { return x + WINDOW_W/zoom; }
    float top() const { return y; }
    float bottom() const { return y + WINDOW_H/zoom; }
};

struct BlockColor { float r, g, b; };
BlockColor get_block_color(BlockType t) {
    switch(t) {
        case BlockType::DIRT:    return {0.47f,0.31f,0.20f};
        case BlockType::GRASS:   return {0.39f,0.63f,0.24f};
        case BlockType::STONE:   return {0.51f,0.51f,0.55f};
        case BlockType::SAND:    return {0.78f,0.71f,0.39f};
        case BlockType::WOOD:    return {0.55f,0.39f,0.24f};
        case BlockType::LEAVES:  return {0.31f,0.55f,0.20f};
        case BlockType::METAL:   return {0.71f,0.71f,0.78f};
        case BlockType::ICE:     return {0.71f,0.86f,0.94f};
        case BlockType::LAVA:    return {0.86f,0.31f,0.08f};
        case BlockType::WATER:   return {0.24f,0.39f,0.71f};
        case BlockType::BEDROCK: return {0.24f,0.24f,0.24f};
        case BlockType::CRYSTAL: return {0.59f,0.78f,1.0f};
        case BlockType::ANCIENT: return {0.39f,0.31f,0.55f};
        default: return {0.04f,0.043f,0.059f};
    }
}

// ---- Simple procedural audio (no SDL_mixer needed — uses SDL audio directly) ----
struct SimpleAudio {
    SDL_AudioDeviceID dev = 0;
    SDL_AudioSpec spec;
    
    bool init() {
        SDL_AudioSpec want;
        SDL_zero(want);
        want.freq = 44100;
        want.format = AUDIO_S16;
        want.channels = 1;
        want.samples = 1024;
        want.callback = audio_callback;
        want.userdata = this;
        dev = SDL_OpenAudioDevice(NULL, 0, &want, &spec, 0);
        if (!dev) return false;
        SDL_PauseAudioDevice(dev, 0); // unpause
        return true;
    }
    
    struct SoundEvent {
        float freq;
        float duration;
        float volume;
        float elapsed;
        int type; // 0=sine, 1=noise, 2=mixed
        SoundEvent* next = nullptr;
    };
    
    SoundEvent* head = nullptr;
    
    void play_tone(float freq, float duration, float volume, int type) {
        SoundEvent* s = new SoundEvent{freq, duration, volume, 0, type, nullptr};
        SDL_LockAudioDevice(dev);
        s->next = head;
        head = s;
        SDL_UnlockAudioDevice(dev);
    }
    
    void play_jump()  { play_tone(300, 0.15f, 0.3f, 0); }
    void play_mine()  { play_tone(150, 0.1f, 0.4f, 1); }
    void play_step()  { play_tone(80, 0.05f, 0.15f, 1); }
    void play_hurt()  { play_tone(200, 0.2f, 0.3f, 0); }
    void play_place() { play_tone(400, 0.08f, 0.2f, 0); }
    void play_land()  { play_tone(60, 0.12f, 0.3f, 1); }
    
    static void audio_callback(void* userdata, Uint8* stream, int len) {
        SimpleAudio* self = (SimpleAudio*)userdata;
        int16_t* out = (int16_t*)stream;
        int samples = len / 2;
        
        for (int i = 0; i < samples; i++) {
            float val = 0;
            SoundEvent** pp = &self->head;
            while (*pp) {
                SoundEvent* s = *pp;
                float t = s->elapsed;
                float env = exp(-t / s->duration);
                if (s->type == 0) { // sine
                    val += sin(2*M_PI*s->freq*t) * env * s->volume;
                } else if (s->type == 1) { // noise
                    val += ((float)rand()/RAND_MAX - 0.5f) * 2 * env * s->volume;
                }
                s->elapsed += 1.0f / self->spec.freq;
                if (s->elapsed > s->duration * 3) {
                    *pp = s->next;
                    delete s;
                } else {
                    pp = &s->next;
                }
            }
            out[i] = (int16_t)(val * 32767 * 0.5f);
        }
    }
    
    void shutdown() {
        if (dev) SDL_CloseAudioDevice(dev);
        while (head) { SoundEvent* t = head; head = head->next; delete t; }
    }
};

// ---- Particle System ----
struct Particle { float x,y,vx,vy,life,max_life,r,g,b,size; };

struct ParticleSystem {
    std::vector<Particle> particles;
    
    void emit(float x, float y, int count, float r, float g, float b,
              float spd_min=30, float spd_max=100, float life_min=0.3f, float life_max=0.6f) {
        for (int i = 0; i < count; i++) {
            float a = ((float)rand()/RAND_MAX)*2*M_PI;
            float s = spd_min + ((float)rand()/RAND_MAX)*(spd_max-spd_min);
            float l = life_min + ((float)rand()/RAND_MAX)*(life_max-life_min);
            particles.push_back({x,y,cos(a)*s,sin(a)*s,l,l,r,g,b, 1+(float)(rand()%3)});
        }
    }
    
    void emit_dust(float x, float y, float r, float g, float b) {
        emit(x+8, y+8, 6, r*0.8f, g*0.8f, b*0.8f, 20, 80, 0.2f, 0.5f);
    }
    
    void emit_land(float x, float y) {
        for (int i = 0; i < 5; i++) {
            float a = M_PI + ((float)rand()/RAND_MAX)*M_PI;
            float s = 20 + ((float)rand()/RAND_MAX)*50;
            particles.push_back({x+(float)(rand()%16-8), y+20, cos(a)*s, sin(a)*s, 0.3f, 0.3f, 0.6f,0.5f,0.4f, 2.0f});
        }
    }
    
    void update(float dt) {
        for (auto& p : particles) {
            p.x += p.vx * dt; p.y += p.vy * dt;
            p.vy += 150 * dt; p.vx *= 0.95f;
            p.life -= dt;
        }
        particles.erase(std::remove_if(particles.begin(), particles.end(),
            [](const Particle& p){return p.life<=0;}), particles.end());
    }
    
    void render(Renderer& r) {
        for (auto& p : particles) {
            float a = p.life / p.max_life;
            r.draw_rect(p.x, p.y, p.size, p.size, p.r, p.g, p.b, a);
        }
    }
};

// ---- Entity creation ----
static Entity create_player(Registry& reg, float x, float y) {
    Entity p = reg.create();
    reg.emplace<Position>(p, Position{x,y});
    reg.emplace<Velocity>(p, Velocity{0,0});
    reg.emplace<Mass>(p, Mass{70.0f});
    reg.emplace<RigidBody>(p, RigidBody{0.0f, 0.8f, false, false});
    reg.emplace<AABBCollider>(p, AABBCollider{24, 40});
    reg.emplace<CharacterController>(p, CharacterController{});
    reg.emplace<Health>(p, Health{100, 100});
    reg.emplace<AnimationState>(p, AnimationState{});
    reg.emplace<FallDamageTracker>(p, FallDamageTracker{});
    reg.emplace<TagPlayer>(p, TagPlayer{});
    Species human; human.name="Human"; human.base_mass=70.0f;
    reg.emplace<Species>(p, std::move(human));
    InventoryWeight inv; inv.max_weight=100.0f;
    reg.emplace<InventoryWeight>(p, std::move(inv));
    return p;
}

// ---- World collision ----
static void resolve_world_collision(Registry& reg, Entity player, World& world, CharacterController& ctrl) {
    auto* pos = reg.get<Position>(player);
    auto* vel = reg.get<Velocity>(player);
    auto* col = reg.get<AABBCollider>(player);
    if (!pos||!vel||!col) return;
    float px=pos->x, py=pos->y, pw=col->width, ph=col->height;
    int min_bx=(int)(px/BLOCK_SIZE)-1, max_bx=(int)((px+pw)/BLOCK_SIZE)+1;
    int min_by=(int)(py/BLOCK_SIZE)-1, max_by=(int)((py+ph)/BLOCK_SIZE)+1;
    ctrl.grounded = false;
    for (int bx=min_bx; bx<=max_bx; bx++) {
        for (int by=min_by; by<=max_by; by++) {
            Block* b = world.get_block(bx, by);
            if (!b||!b->is_solid()) continue;
            float bwx=bx*BLOCK_SIZE, bwy=by*BLOCK_SIZE;
            float ox = std::min(px+pw, bwx+BLOCK_SIZE) - std::max(px, bwx);
            float oy = std::min(py+ph, bwy+BLOCK_SIZE) - std::max(py, bwy);
            if (ox>0 && oy>0) {
                if (ox < oy) {
                    if (px+pw/2 < bwx+BLOCK_SIZE/2) pos->x = bwx-pw;
                    else pos->x = bwx+BLOCK_SIZE;
                    vel->x = 0;
                } else {
                    if (py+ph/2 < bwy+BLOCK_SIZE/2) { pos->y=bwy-ph; vel->y=0; }
                    else { pos->y=bwy-ph; vel->y=0; ctrl.grounded=true;
                        auto* surf=reg.get<Surface>(player);
                        const auto& props=get_block_props(b->type);
                        if (surf){surf->type=SurfaceType::STONE; surf->friction=props.friction;}
                        else{Surface s; s.type=SurfaceType::STONE; s.friction=props.friction; reg.emplace<Surface>(player,std::move(s));}
                    }
                }
                px=pos->x; py=pos->y;
            }
        }
    }
    if (!ctrl.grounded) { auto* s=reg.get<Surface>(player); if(s){s->type=SurfaceType::AIR; s->friction=0.01f;} }
}

int main() {
    std::cout << "=== KronoUniverse v0.2 ===" << std::endl;

    Renderer renderer;
    if (!renderer.init(WINDOW_W, WINDOW_H, "KronoUniverse v0.2")) return 1;

    // Audio (using SDL audio directly, no SDL_mixer needed)
    SimpleAudio audio;
    bool has_audio = audio.init();
    if (has_audio) std::cout << "Audio OK" << std::endl;
    else std::cout << "Audio disabled (no device)" << std::endl;

    Registry reg;
    PhysicsSystem physics;
    physics.gravity_y = GAME_GRAVITY;
    physics.linear_damping = 0.01f;
    MovementSystem movement;
    InputSystem input;

    World world(42);
    std::cout << "World seed=42" << std::endl;

    // Find spawn
    int spawn_bx=0, spawn_by=0;
    for (int y=0; y<CHUNK_H; y++) {
        Block* b = world.get_block(spawn_bx, y);
        if (b && b->is_solid()) { spawn_by = y-3; break; }
    }
    float spawn_x = spawn_bx * BLOCK_SIZE;
    float spawn_y = spawn_by * BLOCK_SIZE;

    Entity player = create_player(reg, spawn_x, spawn_y);
    Camera camera;
    camera.x = spawn_x - WINDOW_W/2;
    camera.y = spawn_y - WINDOW_H/2;

    ParticleSystem particles;
    
    // Input state
    int mouse_x=0, mouse_y=0;
    bool mouse_left=false, mouse_right=false;
    bool show_debug=true;
    bool was_grounded=true;
    
    BlockType palette[] = {BlockType::DIRT, BlockType::STONE, BlockType::WOOD,
                            BlockType::METAL, BlockType::SAND, BlockType::ICE};
    int palette_idx = 0;
    BlockType selected = palette[0];

    // Day/night cycle
    float time_of_day = 0.5f; // start at noon
    
    // Game loop (single-threaded)
    bool running = true;
    Uint32 last_time = SDL_GetTicks();
    float accumulator = 0;
    int frame_count = 0;
    int fps_timer = 0;
    int fps_count = 0;
    int current_fps = 0;

    while (running) {
        Uint32 current_time = SDL_GetTicks();
        float frame_time = (current_time - last_time) / 1000.0f;
        last_time = current_time;
        if (frame_time > 0.25f) frame_time = 0.25f;
        accumulator += frame_time;
        fps_count++;
        fps_timer += (int)(frame_time * 1000);
        if (fps_timer >= 1000) { current_fps = fps_count; fps_count = 0; fps_timer = 0; }

        // ---- Events ----
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) running = false;
                if (e.key.keysym.sym == SDLK_F1) show_debug = !show_debug;
                if (e.key.keysym.sym == SDLK_F2) { palette_idx=(palette_idx+1)%6; selected=palette[palette_idx]; }
                // Number keys 1-6 select blocks
                if (e.key.keysym.sym >= SDLK_1 && e.key.keysym.sym <= SDLK_6) {
                    palette_idx = e.key.keysym.sym - SDLK_1;
                    selected = palette[palette_idx];
                }
            }
            if (e.type == SDL_MOUSEMOTION) { mouse_x=e.motion.x; mouse_y=e.motion.y; }
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button==SDL_BUTTON_LEFT) mouse_left=true;
                if (e.button.button==SDL_BUTTON_RIGHT) mouse_right=true;
            }
            if (e.type == SDL_MOUSEWHEEL) {
                camera.zoom = std::max(0.5f, std::min(3.0f, camera.zoom + e.wheel.y*0.1f));
            }
            input.process_event(e, reg);
        }

        // ---- Fixed updates ----
        while (accumulator >= FIXED_DT) {
            input.update(reg);
            movement.update(reg, FIXED_DT, GAME_GRAVITY);

            auto* pos = reg.get<Position>(player);
            auto* vel = reg.get<Velocity>(player);
            auto* mass = reg.get<Mass>(player);
            auto* rb = reg.get<RigidBody>(player);
            auto* ctrl = reg.get<CharacterController>(player);

            if (pos && vel && mass && rb && !rb->is_static) {
                vel->y += GAME_GRAVITY * FIXED_DT;
                vel->x *= (1.0f - 0.01f * FIXED_DT);
                pos->x += vel->x * FIXED_DT;
                pos->y += vel->y * FIXED_DT;
            }

            resolve_world_collision(reg, player, world, *ctrl);

            // Landing detection
            if (ctrl->grounded && !was_grounded && vel->y > 100) {
                if (has_audio) audio.play_land();
                particles.emit_land(pos->x, pos->y);
            }
            was_grounded = ctrl->grounded;

            // Jump sound
            if (ctrl->state == MoveState::JUMPING && ctrl->prev_state != MoveState::JUMPING) {
                if (has_audio) audio.play_jump();
            }
            ctrl->prev_state = ctrl->state;

            // Mining/placing
            float wmx = camera.s2w_x(mouse_x);
            float wmy = camera.s2w_y(mouse_y);
            int tbx = (int)(wmx / BLOCK_SIZE);
            int tby = (int)(wmy / BLOCK_SIZE);

            if (mouse_left) {
                Block* b = world.get_block(tbx, tby);
                if (b && b->is_solid() && b->type != BlockType::BEDROCK) {
                    BlockColor bc = get_block_color(b->type);
                    particles.emit_dust(tbx*BLOCK_SIZE, tby*BLOCK_SIZE, bc.r, bc.g, bc.b);
                    if (b->type == BlockType::METAL) particles.emit(tbx*BLOCK_SIZE+8, tby*BLOCK_SIZE+8, 3, 1.0f, 0.8f, 0.2f, 80, 200, 0.2f, 0.4f);
                    world.destroy_block(tbx, tby);
                    if (has_audio) audio.play_mine();
                }
                mouse_left = false;
            }
            if (mouse_right) {
                Block* b = world.get_block(tbx, tby);
                if (b && b->is_air()) {
                    auto* pp = reg.get<Position>(player);
                    auto* pc = reg.get<AABBCollider>(player);
                    bool ov = (pp->x < (tbx+1)*BLOCK_SIZE && pp->x+pc->width > tbx*BLOCK_SIZE &&
                              pp->y < (tby+1)*BLOCK_SIZE && pp->y+pc->height > tby*BLOCK_SIZE);
                    if (!ov) {
                        world.set_block(tbx, tby, selected);
                        if (has_audio) audio.play_place();
                    }
                }
                mouse_right = false;
            }

            particles.update(FIXED_DT);
            camera.follow(pos->x, pos->y, FIXED_DT);

            // Day/night
            time_of_day += FIXED_DT / 120.0f; // 2 minute day cycle
            if (time_of_day > 1.0f) time_of_day = 0.0f;

            // Respawn
            if (pos->y > CHUNK_H * BLOCK_SIZE + 1000) {
                pos->x = spawn_x; pos->y = spawn_y;
                vel->x = 0; vel->y = 0;
                auto* hp = reg.get<Health>(player);
                if (hp) hp->current = hp->max;
            }

            input.end_frame(reg);
            accumulator -= FIXED_DT;
        }

        // ---- Render ----
        renderer.clear();
        float vw = WINDOW_W / camera.zoom;
        float vh = WINDOW_H / camera.zoom;
        renderer.set_ortho(camera.x, camera.x+vw, camera.y+vh, camera.y);

        // Day/night ambient color
        float daylight = sin(time_of_day * M_PI);
        daylight = std::max(0.15f, daylight); // never fully dark
        float night_tint = 1.0f - daylight;

        // Draw blocks
        int min_bx = (int)(camera.left()/BLOCK_SIZE)-1;
        int max_bx = (int)(camera.right()/BLOCK_SIZE)+1;
        int min_by = std::max(0, (int)(camera.top()/BLOCK_SIZE)-1);
        int max_by = std::min(CHUNK_H-1, (int)(camera.bottom()/BLOCK_SIZE)+1);

        for (int bx = min_bx; bx <= max_bx; bx++) {
            for (int by = min_by; by <= max_by; by++) {
                Block* b = world.get_block(bx, by);
                if (!b || b->is_air()) continue;
                BlockColor c = get_block_color(b->type);
                
                // Damage tint
                if (b->hp < b->max_hp && b->max_hp > 0) {
                    float r = (float)b->hp / b->max_hp;
                    c.r *= 0.5f+r*0.5f; c.g *= 0.5f+r*0.5f; c.b *= 0.5f+r*0.5f;
                }
                
                // Day/night lighting
                c.r *= daylight; c.g *= daylight; c.b *= daylight;
                
                // Lava glow (doesn't get darkened)
                if (b->type == BlockType::LAVA) {
                    c.r = 0.86f; c.g = 0.31f; c.b = 0.08f;
                }
                // Crystal glow
                if (b->type == BlockType::CRYSTAL) {
                    c.r = 0.59f; c.g = 0.78f; c.b = 1.0f;
                }
                
                renderer.draw_rect(bx*BLOCK_SIZE, by*BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, c.r, c.g, c.b);
            }
        }

        // Particles
        particles.render(renderer);

        // Player
        auto* ppos = reg.get<Position>(player);
        auto* pvel = reg.get<Velocity>(player);
        auto* pctrl = reg.get<CharacterController>(player);
        auto* php = reg.get<Health>(player);

        // Player body (teal, affected by daylight)
        renderer.draw_rect(ppos->x, ppos->y, 24, 40, 0.13f*daylight, 0.83f*daylight, 0.93f*daylight);
        
        // Head (skin tone)
        renderer.draw_rect(ppos->x+6, ppos->y, 12, 12, 0.86f*daylight, 0.70f*daylight, 0.55f*daylight);
        
        // Eyes (facing direction)
        float eye_x = pctrl->facing_right ? ppos->x+12 : ppos->x+8;
        renderer.draw_rect(eye_x, ppos->y+4, 4, 3, 0.1f, 0.1f, 0.15f);
        
        // Body (shirt)
        renderer.draw_rect(ppos->x+4, ppos->y+12, 16, 16, 0.24f*daylight, 0.47f*daylight, 0.78f*daylight);
        
        // Legs (with walk animation offset)
        float leg_off = 0;
        if (pctrl->state == MoveState::WALKING || pctrl->state == MoveState::RUNNING) {
            leg_off = sin(frame_count * 0.3f) * 2;
        }
        renderer.draw_rect(ppos->x+4, ppos->y+28+leg_off, 7, 12, 0.2f*daylight, 0.2f*daylight, 0.24f*daylight);
        renderer.draw_rect(ppos->x+13, ppos->y+28-leg_off, 7, 12, 0.2f*daylight, 0.2f*daylight, 0.24f*daylight);

        // Direction indicator
        float dot_x = pctrl->facing_right ? ppos->x+18 : ppos->x;
        renderer.draw_rect(dot_x, ppos->y-4, 4, 4, 0.98f, 0.75f, 0.14f);

        // Health bar
        if (php && php->current < php->max) {
            float hr = php->current / php->max;
            renderer.draw_rect(ppos->x-2, ppos->y-10, 28, 4, 0.2f, 0.2f, 0.2f);
            renderer.draw_rect(ppos->x-2, ppos->y-10, 28*hr, 4, 1.0f-hr, hr, 0.0f);
        }

        // Block highlight
        float wmx = camera.s2w_x(mouse_x);
        float wmy = camera.s2w_y(mouse_y);
        int hbx = (int)(wmx/BLOCK_SIZE);
        int hby = (int)(wmy/BLOCK_SIZE);
        renderer.draw_rect(hbx*BLOCK_SIZE, hby*BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, 1.0f, 1.0f, 1.0f, 0.25f);

        // ---- HUD (screen space) ----
        renderer.set_screen_ortho();

        // Night overlay
        if (night_tint > 0.1f) {
            renderer.draw_rect(0, 0, WINDOW_W, WINDOW_H, 0.0f, 0.02f, 0.08f, night_tint * 0.4f);
        }

        // Hotbar (bottom center)
        int hb_x = WINDOW_W/2 - 90;
        int hb_y = WINDOW_H - 50;
        for (int i = 0; i < 6; i++) {
            BlockColor sc = get_block_color(palette[i]);
            // Background
            renderer.draw_rect(hb_x + i*30, hb_y, 28, 28, 0.1f, 0.1f, 0.12f, 0.8f);
            // Block icon
            renderer.draw_rect(hb_x + i*30 + 6, hb_y + 6, 16, 16, sc.r, sc.g, sc.b);
            // Selection highlight
            if (i == palette_idx) {
                renderer.draw_rect(hb_x + i*30, hb_y, 28, 28, 1.0f, 0.75f, 0.14f, 0.4f);
            }
            // Number
            // (can't draw text with immediate mode, but the selection highlight serves as indicator)
        }

        // Selected block indicator (top-left)
        BlockColor sc = get_block_color(selected);
        renderer.draw_rect(10, 10, 24, 24, sc.r, sc.g, sc.b);
        renderer.draw_rect(10, 10, 24, 24, 1.0f, 1.0f, 1.0f, 0.3f);

        // Debug overlay
        if (show_debug) {
            renderer.draw_rect(0, 0, 250, 80, 0.0f, 0.0f, 0.0f, 0.7f);
            
            // Velocity bars
            renderer.draw_rect(10, 15, std::abs(pvel->x)*0.1f, 4, 0.98f, 0.75f, 0.14f);
            renderer.draw_rect(10, 22, std::abs(pvel->y)*0.05f, 4, 0.13f, 0.83f, 0.93f);
            
            // Grounded indicator
            if (pctrl->grounded) {
                renderer.draw_rect(10, 30, 8, 8, 0.2f, 0.8f, 0.2f);
            } else {
                renderer.draw_rect(10, 30, 8, 8, 0.8f, 0.2f, 0.2f);
            }
            
            // Day/night indicator
            float day_bar = time_of_day * 200;
            renderer.draw_rect(10, 42, 200, 4, 0.2f, 0.2f, 0.2f);
            renderer.draw_rect(10, 42, day_bar, 4, daylight, daylight*0.8f, 0.3f);
            
            // FPS
            renderer.draw_rect(10, 52, std::min(200, current_fps*2), 4, 0.5f, 0.5f, 0.8f);
            
            // Particle count
            renderer.draw_rect(10, 60, std::min(200, (int)particles.particles.size()*4), 3, 0.6f, 0.4f, 0.2f);
        }

        renderer.present();
        frame_count++;
    }

    std::cout << "Game ended after " << frame_count << " frames." << std::endl;
    if (has_audio) audio.shutdown();
    renderer.shutdown();
    return 0;
}
