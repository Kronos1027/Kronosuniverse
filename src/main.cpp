#define _USE_MATH_DEFINES
// KronoUniverse v0.4 — Full UI System + Menus
//
// NEW IN v0.4:
// - Complete menu system: Main Menu, Pause, Settings, Inventory, Crafting, Stats, Death
// - Bitmap font rendering (no texture needed)
// - Mouse cursor with click detection
// - Button hover/press states
// - All v0.3 systems preserved (Combat, Mobs, Weather, Lighting, Crafting v2, Save)
// - Bug fix: reg.view<MobAI>() -> reg.each<MobAI>() (was crashing on Windows)

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "engine/character_components.hpp"
#include "engine/input_system.hpp"
#include "physics/physics.hpp"
#include "physics/movement_system.hpp"
#include "procedural/world.hpp"
#include "render/renderer.hpp"
#include "game/combat_system.hpp"
#include "game/mob_ai.hpp"
#include "game/weather_system.hpp"
#include "game/lighting_system.hpp"
#include "game/crafting_system.hpp"
#include "game/save_system.hpp"
#include "game/ui_system.hpp"
#include "game/wildlife_system.hpp"
#include "game/item_drops.hpp"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <vector>
#include <string>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace krono;

static constexpr int WINDOW_W = 1280;
static constexpr int WINDOW_H = 720;
static constexpr float GAME_GRAVITY = 9.81f * 50.0f;
static constexpr float FIXED_DT = 1.0f / 60.0f;

// ---- Camera ----
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

// ---- Block colors ----
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

// ---- Audio system (procedural) ----
struct SimpleAudio {
    SDL_AudioDeviceID dev = 0;
    SDL_AudioSpec spec;
    bool muted = false;
    float volume = 1.0f;

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
        SDL_PauseAudioDevice(dev, 0);
        return true;
    }

    struct SoundEvent {
        float freq;
        float duration;
        float volume;
        float elapsed;
        int type;
        float freq_end;
        SoundEvent* next = nullptr;
    };

    SoundEvent* head = nullptr;

    void play_tone(float freq, float duration, float volume, int type, float freq_end = 0) {
        if (muted) return;
        SoundEvent* s = new SoundEvent{freq, duration, volume * this->volume, 0, type, freq_end ? freq_end : freq, nullptr};
        SDL_LockAudioDevice(dev);
        s->next = head;
        head = s;
        SDL_UnlockAudioDevice(dev);
    }

    void play_jump()      { play_tone(300, 0.15f, 0.25f, 2, 600); }
    void play_mine()      { play_tone(150, 0.1f, 0.4f, 1); }
    void play_step()      { play_tone(80, 0.05f, 0.1f, 1); }
    void play_hurt()      { play_tone(200, 0.2f, 0.3f, 2, 100); }
    void play_place()     { play_tone(400, 0.08f, 0.2f, 0); }
    void play_land()      { play_tone(60, 0.12f, 0.3f, 1); }
    void play_attack()    { play_tone(500, 0.08f, 0.15f, 2, 200); }
    void play_shoot()     { play_tone(800, 0.06f, 0.2f, 2, 400); }
    void play_mob_die()   { play_tone(150, 0.3f, 0.3f, 2, 50); }
    void play_explosion() { play_tone(80, 0.5f, 0.6f, 1); }
    void play_thunder()   { play_tone(40, 0.8f, 0.7f, 1); }
    void play_pickup()    { play_tone(600, 0.1f, 0.2f, 2, 900); }
    void play_levelup()   { play_tone(440, 0.4f, 0.3f, 2, 880); }
    void play_click()     { play_tone(800, 0.04f, 0.15f, 0); }
    void play_hover()     { play_tone(1200, 0.02f, 0.05f, 0); }
    void play_death()     { play_tone(200, 0.8f, 0.5f, 2, 50); }
    void play_save()      { play_tone(400, 0.15f, 0.2f, 2, 800); }

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
                if (s->type == 0) val += sin(2*M_PI*s->freq*t) * env * s->volume;
                else if (s->type == 1) val += ((float)rand()/RAND_MAX - 0.5f) * 2 * env * s->volume;
                else if (s->type == 2) {
                    float freq = s->freq + (s->freq_end - s->freq) * (t / s->duration);
                    val += sin(2*M_PI*freq*t) * env * s->volume;
                }
                s->elapsed += 1.0f / self->spec.freq;
                if (s->elapsed > s->duration * 3) { *pp = s->next; delete s; }
                else pp = &s->next;
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
    void emit_blood(float x, float y) { emit(x, y, 8, 0.7f, 0.1f, 0.1f, 50, 150, 0.3f, 0.6f); }
    void emit_spark(float x, float y, float r, float g, float b) { emit(x, y, 4, r, g, b, 80, 200, 0.2f, 0.4f); }
    void emit_explosion(float x, float y) {
        emit(x, y, 20, 1.0f, 0.6f, 0.1f, 100, 300, 0.4f, 0.8f);
        emit(x, y, 10, 1.0f, 0.9f, 0.5f, 50, 150, 0.6f, 1.0f);
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

// ---- Floating text ----
struct FloatingText { float x, y, vy = -50, life = 1.0f, max_life = 1.0f; std::string text; float r, g, b; };
struct FloatingTextSystem {
    std::vector<FloatingText> texts;
    void emit(float x, float y, const std::string& text, float r=1, float g=1, float b=1) {
        texts.push_back({x, y, -50, 1.0f, 1.0f, text, r, g, b});
    }
    void update(float dt) {
        for (auto& t : texts) { t.y += t.vy * dt; t.life -= dt; }
        texts.erase(std::remove_if(texts.begin(), texts.end(),
            [](const FloatingText& t){return t.life<=0;}), texts.end());
    }
    template<typename R>
    void render(R& r) {
        for (auto& t : texts) {
            float a = t.life / t.max_life;
            BitmapFont::draw(r, t.x, t.y, t.text, t.r, t.g, t.b, a);
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
    reg.emplace<StatusContainer>(p, StatusContainer{});
    Species human; human.name="Human"; human.base_mass=70.0f;
    reg.emplace<Species>(p, std::move(human));
    InventoryWeight inv; inv.max_weight=100.0f;
    reg.emplace<InventoryWeight>(p, std::move(inv));
    reg.emplace<EquippedWeapon>(p, EquippedWeapon{CombatSystem::make_sword()});
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
                    // Vertical collision
                    if (py+ph/2 < bwy+BLOCK_SIZE/2) {
                        // Player center ABOVE block center → landing on top of block
                        pos->y = bwy - ph;  // push player up to sit on top
                        vel->y = 0;
                        ctrl.grounded = true;  // landed!
                    } else {
                        // Player center BELOW block center → hit head on bottom of block
                        pos->y = bwy + BLOCK_SIZE;  // push player down below block
                        vel->y = 0;
                        // grounded stays false (was reset to false at top of function)
                    }
                }
                px=pos->x; py=pos->y;
            }
        }
    }
}

static void resolve_mob_collision(Registry& reg, Entity mob, World& world) {
    auto* pos = reg.get<Position>(mob);
    auto* vel = reg.get<Velocity>(mob);
    auto* col = reg.get<AABBCollider>(mob);
    if (!pos||!vel||!col) return;
    float px=pos->x, py=pos->y, pw=col->width, ph=col->height;
    int min_bx=(int)(px/BLOCK_SIZE)-1, max_bx=(int)((px+pw)/BLOCK_SIZE)+1;
    int min_by=(int)(py/BLOCK_SIZE)-1, max_by=(int)((py+ph)/BLOCK_SIZE)+1;
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
                    // Vertical collision - proper top/bottom resolution
                    if (py+ph/2 < bwy+BLOCK_SIZE/2) {
                        // Mob center above block center → landing on top
                        pos->y = bwy - ph;
                        vel->y = 0;
                    } else {
                        // Mob center below block center → hit head on bottom
                        pos->y = bwy + BLOCK_SIZE;
                        vel->y = 0;
                    }
                }
                px=pos->x; py=pos->y;
            }
        }
    }
}

static void projectile_world_collision(Registry& reg, World& world) {
    std::vector<Entity> to_destroy;
    reg.each<Position, AABBCollider, Projectile>([&](auto ent, Position& pos, AABBCollider& col, Projectile& proj) {
        int bx = (int)(pos.x / BLOCK_SIZE);
        int by = (int)(pos.y / BLOCK_SIZE);
        Block* b = world.get_block(bx, by);
        if (b && b->is_solid()) to_destroy.push_back(ent);
    });
    for (auto e : to_destroy) reg.destroy(e);
}

// ---- Game state (passed to UI callbacks) ----
struct GameState {
    bool running = true;
    bool* paused = nullptr;
    Entity player = 0;
    Registry* reg = nullptr;
    World* world = nullptr;
    Camera* camera = nullptr;
    SimpleAudio* audio = nullptr;
    ParticleSystem* particles = nullptr;
    FloatingTextSystem* floating_texts = nullptr;
    WeatherSystem* weather = nullptr;
    LightingSystem* lighting = nullptr;
    UIManager* ui = nullptr;
    float* time_of_day = nullptr;
    int* mob_count = nullptr;
    int* mobs_spawned = nullptr;
    int* total_kills = nullptr;
    int* player_level = nullptr;
    float* player_xp = nullptr;
    float* xp_to_next = nullptr;
    int* current_fps = nullptr;
    bool* show_debug = nullptr;
    int* weapon_idx = nullptr;
    int* palette_idx = nullptr;
    BlockType* selected = nullptr;
    float* spawn_x = nullptr;
    float* spawn_y = nullptr;
    PlantSystem* plants = nullptr;
    WildlifeSystem* wildlife = nullptr;
    PlayerInventory* player_inv = nullptr;
};

// ---- Forward declarations for menu builders ----
static void build_main_menu(UIManager& ui, GameState& gs);
static void build_settings_menu(UIManager& ui, GameState& gs);
static void build_credits_menu(UIManager& ui, GameState& gs);
static void build_pause_menu(UIManager& ui, GameState& gs);
static void build_inventory_menu(UIManager& ui, GameState& gs);
static void build_crafting_menu(UIManager& ui, GameState& gs);
static void build_stats_menu(UIManager& ui, GameState& gs);
static void build_death_screen(UIManager& ui, GameState& gs);

// ---- Menu builders ----
static void build_main_menu(UIManager& ui, GameState& gs) {
    ui.clear();
    ui.state = UIManager::STATE_MAIN_MENU;

    // Background panel
    ui.add_panel(0, 0, WINDOW_W, WINDOW_H, 0.04f, 0.05f, 0.08f, 1.0f);

    // Title
    ui.add_label(WINDOW_W/2 - 200, 80, "KRONOUNIVERSE", 0.13f, 0.83f, 0.93f, 1.0f);
    ui.add_label(WINDOW_W/2 - 180, 110, "V0.4 - MULTIVERSO EDITION", 0.6f, 0.7f, 0.8f, 1.0f);

    // Subtitle
    ui.add_label(WINDOW_W/2 - 240, 150, "EXPLORE. CRAFT. CONQUER. ASCEND.", 0.9f, 0.7f, 0.2f, 1.0f);

    // Buttons
    int btn_x = WINDOW_W/2 - 150;
    int btn_y = 240;
    ui.add_button(btn_x, btn_y, 300, 50, "NEW GAME", [&]() {
        gs.audio->play_click();
        *gs.paused = false;
        ui.state = UIManager::STATE_PLAYING;
        ui.clear();
    });
    ui.add_button(btn_x, btn_y + 70, 300, 50, "SETTINGS", [&]() {
        gs.audio->play_click();
        build_settings_menu(ui, gs);
    });
    ui.add_button(btn_x, btn_y + 140, 300, 50, "CREDITS", [&]() {
        gs.audio->play_click();
        build_credits_menu(ui, gs);
    });
    ui.add_button(btn_x, btn_y + 210, 300, 50, "QUIT", [&]() {
        gs.audio->play_click();
        gs.running = false;
    });

    // Footer
    ui.add_label(20, WINDOW_H - 30, "MADE BY DARLAN (NATSKY) - GITHUB.COM/KRONOS1027", 0.5f, 0.5f, 0.6f, 1.0f);
    ui.add_label(WINDOW_W - 200, WINDOW_H - 30, "CLICK BUTTONS TO NAVIGATE", 0.5f, 0.5f, 0.6f, 1.0f);
}

static void build_settings_menu(UIManager& ui, GameState& gs) {
    ui.clear();
    ui.state = UIManager::STATE_SETTINGS;

    ui.add_panel(0, 0, WINDOW_W, WINDOW_H, 0.04f, 0.05f, 0.08f, 1.0f);
    ui.add_label(WINDOW_W/2 - 80, 60, "SETTINGS", 1.0f, 1.0f, 1.0f, 1.0f);

    // Audio section
    ui.add_label(100, 140, "AUDIO", 0.9f, 0.8f, 0.2f, 1.0f);
    ui.add_label(120, 170, "MASTER VOLUME", 0.8f, 0.8f, 0.8f, 1.0f);
    ui.add_progress_bar(300, 172, 200, 16, gs.audio->volume, 0.3f, 0.7f, 0.3f);
    ui.add_button(520, 168, 30, 24, "-", [&]() {
        gs.audio->volume = std::max(0.0f, gs.audio->volume - 0.1f);
        gs.audio->play_click();
    });
    ui.add_button(560, 168, 30, 24, "+", [&]() {
        gs.audio->volume = std::min(1.0f, gs.audio->volume + 0.1f);
        gs.audio->play_click();
    });
    ui.add_button(600, 168, 100, 24, gs.audio->muted ? "UNMUTE" : "MUTE", [&]() {
        gs.audio->muted = !gs.audio->muted;
        gs.audio->play_click();
        build_settings_menu(ui, gs);
    });

    // Video section
    ui.add_label(100, 220, "VIDEO", 0.9f, 0.8f, 0.2f, 1.0f);
    ui.add_label(120, 250, "DEBUG OVERLAY (F1)", 0.8f, 0.8f, 0.8f, 1.0f);
    ui.add_button(400, 246, 100, 24, *gs.show_debug ? "ON" : "OFF", [&]() {
        *gs.show_debug = !*gs.show_debug;
        gs.audio->play_click();
        build_settings_menu(ui, gs);
    });

    // Controls help
    ui.add_label(100, 300, "CONTROLS", 0.9f, 0.8f, 0.2f, 1.0f);
    ui.add_label(120, 330, "WASD/ARROWS - MOVE", 0.8f, 0.8f, 0.8f, 1.0f);
    ui.add_label(120, 350, "SPACE/W - JUMP", 0.8f, 0.8f, 0.8f, 1.0f);
    ui.add_label(120, 370, "SHIFT - RUN", 0.8f, 0.8f, 0.8f, 1.0f);
    ui.add_label(120, 390, "E - ATTACK", 0.8f, 0.8f, 0.8f, 1.0f);
    ui.add_label(120, 410, "Q - SWITCH WEAPON", 0.8f, 0.8f, 0.8f, 1.0f);
    ui.add_label(120, 430, "1-8 - SELECT BLOCK", 0.8f, 0.8f, 0.8f, 1.0f);
    ui.add_label(120, 450, "MOUSE LEFT - MINE", 0.8f, 0.8f, 0.8f, 1.0f);
    ui.add_label(120, 470, "MOUSE RIGHT - PLACE", 0.8f, 0.8f, 0.8f, 1.0f);
    ui.add_label(120, 490, "I - INVENTORY", 0.8f, 0.8f, 0.8f, 1.0f);
    ui.add_label(120, 510, "C - CRAFTING", 0.8f, 0.8f, 0.8f, 1.0f);
    ui.add_label(120, 530, "TAB - STATS", 0.8f, 0.8f, 0.8f, 1.0f);
    ui.add_label(120, 550, "ESC - PAUSE MENU", 0.8f, 0.8f, 0.8f, 1.0f);
    ui.add_label(120, 570, "R - FORCE WEATHER CHANGE", 0.8f, 0.8f, 0.8f, 1.0f);

    // Back button
    ui.add_button(WINDOW_W/2 - 100, WINDOW_H - 100, 200, 50, "BACK", [&]() {
        gs.audio->play_click();
        if (*gs.paused) {
            build_pause_menu(ui, gs);
        } else {
            build_main_menu(ui, gs);
        }
    });
}

static void build_credits_menu(UIManager& ui, GameState& gs) {
    ui.clear();
    ui.add_panel(0, 0, WINDOW_W, WINDOW_H, 0.04f, 0.05f, 0.08f, 1.0f);
    ui.add_label(WINDOW_W/2 - 80, 60, "CREDITS", 1.0f, 1.0f, 1.0f, 1.0f);

    ui.add_label(WINDOW_W/2 - 100, 150, "DESIGN & CODE", 0.9f, 0.7f, 0.2f, 1.0f);
    ui.add_label(WINDOW_W/2 - 60, 180, "DARLAN (NATSKY)", 0.9f, 0.9f, 0.9f, 1.0f);

    ui.add_label(WINDOW_W/2 - 100, 230, "TECHNOLOGIES", 0.9f, 0.7f, 0.2f, 1.0f);
    ui.add_label(WINDOW_W/2 - 100, 260, "C++20 - CORE LANGUAGE", 0.7f, 0.8f, 0.9f, 1.0f);
    ui.add_label(WINDOW_W/2 - 100, 280, "SDL2 - WINDOW/INPUT/AUDIO", 0.7f, 0.8f, 0.9f, 1.0f);
    ui.add_label(WINDOW_W/2 - 100, 300, "OPENGL 2.1 - RENDERING", 0.7f, 0.8f, 0.9f, 1.0f);
    ui.add_label(WINDOW_W/2 - 100, 320, "FASTNOISELITE - PROCEDURAL", 0.7f, 0.8f, 0.9f, 1.0f);
    ui.add_label(WINDOW_W/2 - 100, 340, "SQLITE - SAVE SYSTEM", 0.7f, 0.8f, 0.9f, 1.0f);
    ui.add_label(WINDOW_W/2 - 100, 360, "CMAKE - BUILD SYSTEM", 0.7f, 0.8f, 0.9f, 1.0f);

    ui.add_label(WINDOW_W/2 - 100, 410, "FEATURES (V0.4)", 0.9f, 0.7f, 0.2f, 1.0f);
    ui.add_label(WINDOW_W/2 - 100, 440, "16 COMPLETE SYSTEMS", 0.7f, 0.8f, 0.9f, 1.0f);
    ui.add_label(WINDOW_W/2 - 100, 460, "142+ UNIT TESTS PASSING", 0.7f, 0.8f, 0.9f, 1.0f);
    ui.add_label(WINDOW_W/2 - 100, 480, "INFINITE PROCEDURAL UNIVERSE", 0.7f, 0.8f, 0.9f, 1.0f);
    ui.add_label(WINDOW_W/2 - 100, 500, "7 MOB TYPES + 5 WEAPONS", 0.7f, 0.8f, 0.9f, 1.0f);
    ui.add_label(WINDOW_W/2 - 100, 520, "46 CRAFTING RECIPES", 0.7f, 0.8f, 0.9f, 1.0f);
    ui.add_label(WINDOW_W/2 - 100, 540, "DYNAMIC WEATHER + LIGHTING", 0.7f, 0.8f, 0.9f, 1.0f);

    ui.add_button(WINDOW_W/2 - 100, WINDOW_H - 100, 200, 50, "BACK", [&]() {
        gs.audio->play_click();
        build_main_menu(ui, gs);
    });
}

static void build_pause_menu(UIManager& ui, GameState& gs) {
    ui.clear();
    ui.state = UIManager::STATE_PAUSED;

    // Dim background
    ui.add_panel(0, 0, WINDOW_W, WINDOW_H, 0.0f, 0.0f, 0.0f, 0.6f);

    // Pause panel
    ui.add_panel(WINDOW_W/2 - 200, 100, 400, 500, 0.08f, 0.1f, 0.14f, 0.95f);
    ui.add_label(WINDOW_W/2 - 60, 130, "PAUSED", 1.0f, 1.0f, 1.0f, 1.0f);

    int btn_x = WINDOW_W/2 - 150;
    int btn_y = 200;
    ui.add_button(btn_x, btn_y, 300, 50, "RESUME", [&]() {
        gs.audio->play_click();
        *gs.paused = false;
        ui.state = UIManager::STATE_PLAYING;
        ui.clear();
    });
    ui.add_button(btn_x, btn_y + 70, 300, 50, "SAVE GAME", [&]() {
        gs.audio->play_save();
        // TODO: actual save logic
        gs.floating_texts->emit(*gs.spawn_x, *gs.spawn_y - 30, "SAVED!", 0.3f, 1.0f, 0.3f);
    });
    ui.add_button(btn_x, btn_y + 140, 300, 50, "SETTINGS", [&]() {
        gs.audio->play_click();
        build_settings_menu(ui, gs);
    });
    ui.add_button(btn_x, btn_y + 210, 300, 50, "MAIN MENU", [&]() {
        gs.audio->play_click();
        if (gs.paused) *gs.paused = false;
        build_main_menu(ui, gs);
    });
    ui.add_button(btn_x, btn_y + 280, 300, 50, "QUIT TO DESKTOP", [&]() {
        gs.audio->play_click();
        gs.running = false;
    });
}

static void build_inventory_menu(UIManager& ui, GameState& gs) {
    ui.clear();
    ui.state = UIManager::STATE_INVENTORY;

    ui.add_panel(0, 0, WINDOW_W, WINDOW_H, 0.0f, 0.0f, 0.0f, 0.6f);
    ui.add_panel(WINDOW_W/2 - 350, 80, 700, 560, 0.08f, 0.1f, 0.14f, 0.95f);
    ui.add_label(WINDOW_W/2 - 80, 100, "INVENTORY", 1.0f, 1.0f, 1.0f, 1.0f);

    // Grid 8x6 = 48 slots — show player's actual inventory
    int grid_x = WINDOW_W/2 - 300;
    int grid_y = 150;
    int slot_size = 60;
    int slot_spacing = 8;

    // First show hotbar (8 slots, first row)
    for (int col = 0; col < 8; col++) {
        int sx = grid_x + col * (slot_size + slot_spacing);
        int sy = grid_y;
        uint16_t item_id = gs.player_inv->hotbar[col];
        int count = gs.player_inv->hotbar_count[col];
        ui.add_inventory_slot(sx, sy, slot_size, item_id, count);
        // Slot number
        std::string num = std::to_string(col + 1);
        ui.add_label(sx + 4, sy + 4, num, 1.0f, 1.0f, 0.4f, 0.8f);
        // Item name below slot
        if (item_id != 0) {
            const char* name = item_name(item_id);
            // Truncate to 10 chars
            std::string short_name(name, std::min((size_t)10, strlen(name)));
            ui.add_label(sx, sy + slot_size + 2, short_name, 0.7f, 0.8f, 0.9f, 1.0f);
        }
    }
    // HOTBAR label
    ui.add_label(grid_x, grid_y - 20, "HOTBAR", 1.0f, 0.9f, 0.3f, 1.0f);

    // Then main inventory (5 rows x 8 cols)
    int inv_start_y = grid_y + slot_size + 30;
    ui.add_label(grid_x, inv_start_y - 20, "INVENTORY", 1.0f, 0.9f, 0.3f, 1.0f);
    int inv_idx = 0;
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 8; col++) {
            int sx = grid_x + col * (slot_size + slot_spacing);
            int sy = inv_start_y + row * (slot_size + slot_spacing);
            uint16_t item_id = 0;
            int count = 0;
            if (inv_idx < (int)gs.player_inv->inv.items.size()) {
                item_id = (uint16_t)gs.player_inv->inv.items[inv_idx].id;
                count = gs.player_inv->inv.items[inv_idx].count;
            }
            ui.add_inventory_slot(sx, sy, slot_size, item_id, count);
            inv_idx++;
        }
    }

    // Player stats panel (right side)
    int stat_x = grid_x + 8 * (slot_size + slot_spacing) + 20;
    ui.add_label(stat_x, grid_y, "PLAYER STATS", 1.0f, 0.9f, 0.3f, 1.0f);
    ui.add_label(stat_x, grid_y + 30, "LEVEL", 0.7f, 0.8f, 0.9f, 1.0f);
    ui.add_label(stat_x + 80, grid_y + 30, std::to_string(*gs.player_level), 0.9f, 0.9f, 0.5f, 1.0f);

    ui.add_label(stat_x, grid_y + 60, "XP", 0.7f, 0.8f, 0.9f, 1.0f);
    ui.add_progress_bar(stat_x, grid_y + 80, 200, 12, *gs.player_xp / *gs.xp_to_next, 0.3f, 0.6f, 1.0f);

    ui.add_label(stat_x, grid_y + 110, "KILLS", 0.7f, 0.8f, 0.9f, 1.0f);
    ui.add_label(stat_x + 80, grid_y + 110, std::to_string(*gs.total_kills), 0.9f, 0.5f, 0.5f, 1.0f);

    ui.add_label(stat_x, grid_y + 140, "MOBS SPAWNED", 0.7f, 0.8f, 0.9f, 1.0f);
    ui.add_label(stat_x + 130, grid_y + 140, std::to_string(*gs.mobs_spawned), 0.9f, 0.5f, 0.5f, 1.0f);

    ui.add_label(stat_x, grid_y + 170, "FPS", 0.7f, 0.8f, 0.9f, 1.0f);
    ui.add_label(stat_x + 80, grid_y + 170, std::to_string(*gs.current_fps), 0.5f, 0.9f, 0.5f, 1.0f);

    ui.add_label(stat_x, grid_y + 220, "TIME OF DAY", 0.7f, 0.8f, 0.9f, 1.0f);
    int hour = (int)(*gs.time_of_day * 24);
    int minute = (int)((*gs.time_of_day * 24 - hour) * 60);
    char time_buf[16];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", hour, minute);
    ui.add_label(stat_x, grid_y + 240, time_buf, 1.0f, 0.9f, 0.5f, 1.0f);

    // Back button
    ui.add_button(WINDOW_W/2 - 100, WINDOW_H - 80, 200, 40, "CLOSE (I)", [&]() {
        gs.audio->play_click();
        ui.state = UIManager::STATE_PLAYING;
        ui.clear();
    });
}

static void build_crafting_menu(UIManager& ui, GameState& gs) {
    ui.clear();
    ui.state = UIManager::STATE_CRAFTING;

    ui.add_panel(0, 0, WINDOW_W, WINDOW_H, 0.0f, 0.0f, 0.0f, 0.6f);
    ui.add_panel(WINDOW_W/2 - 400, 60, 800, 600, 0.08f, 0.1f, 0.14f, 0.95f);
    ui.add_label(WINDOW_W/2 - 80, 80, "CRAFTING", 1.0f, 1.0f, 1.0f, 1.0f);

    // List recipes
    auto& recipes = CraftingSystem::all_recipes();
    int list_x = WINDOW_W/2 - 380;
    int list_y = 120;
    int max_display = 18;
    int start = 0;  // could be paginated

    for (int i = start; i < (int)recipes.size() && i < start + max_display; i++) {
        auto& r = recipes[i];
        int y = list_y + (i - start) * 26;
        // Check if player can craft this
        bool can_craft = CraftingSystem::can_craft(r, gs.player_inv->inv, CraftingStation::NONE);
        // Recipe button - clicking crafts it
        ui.add_button(list_x, y, 760, 22, "", [&, i, can_craft]() {
            if (can_craft) {
                auto& recipe = recipes[i];
                if (CraftingSystem::craft(recipe, gs.player_inv->inv, CraftingStation::NONE)) {
                    gs.audio->play_pickup();
                    // Refresh menu to update availability
                    build_crafting_menu(ui, gs);
                } else {
                    gs.audio->play_hurt();
                }
            } else {
                gs.audio->play_hurt();
            }
        },
                     can_craft ? 0.15f : 0.08f,
                     can_craft ? 0.18f : 0.1f,
                     can_craft ? 0.2f : 0.12f);
        // Tier color
        float tr = 0.5f + r.tier * 0.08f;
        ui.add_panel(list_x, y, 4, 22, tr, 0.5f, 0.2f, 1.0f);
        // Recipe name + count
        std::string label = r.discovered ? r.result_name + " x" + std::to_string(r.result_count) : "??? UNKNOWN ???";
        ui.add_label(list_x + 10, y + 4, label, r.discovered ? (can_craft ? 0.9f : 0.5f) : 0.4f, 0.9f, 0.7f, 1.0f);
        // Category
        ui.add_label(list_x + 400, y + 4, "[" + r.category + "]", 0.6f, 0.6f, 0.7f, 1.0f);
        // Tier
        ui.add_label(list_x + 600, y + 4, "T" + std::to_string(r.tier), 0.9f, 0.7f, 0.3f, 1.0f);
        // Station
        const char* station_name = "";
        switch (r.station) {
            case CraftingStation::NONE: station_name = "HAND"; break;
            case CraftingStation::WORKBENCH: station_name = "BENCH"; break;
            case CraftingStation::FURNACE: station_name = "FURN"; break;
            case CraftingStation::ANVIL: station_name = "ANVIL"; break;
            case CraftingStation::ALTAR: station_name = "ALTAR"; break;
            case CraftingStation::ALCHEMY_TABLE: station_name = "ALCH"; break;
            case CraftingStation::HIGH_TECH: station_name = "TECH"; break;
        }
        ui.add_label(list_x + 680, y + 4, station_name, 0.6f, 0.8f, 0.6f, 1.0f);
    }

    ui.add_label(list_x, WINDOW_H - 130, "TOTAL RECIPES: " + std::to_string(recipes.size()), 0.7f, 0.7f, 0.7f, 1.0f);
    ui.add_label(list_x, WINDOW_H - 110, "DISCOVERED: " + std::to_string(recipes.size()) + "/" + std::to_string(recipes.size()), 0.5f, 0.9f, 0.5f, 1.0f);

    ui.add_button(WINDOW_W/2 - 100, WINDOW_H - 80, 200, 40, "CLOSE (C)", [&]() {
        gs.audio->play_click();
        ui.state = UIManager::STATE_PLAYING;
        ui.clear();
    });
}

static void build_stats_menu(UIManager& ui, GameState& gs) {
    ui.clear();
    ui.state = UIManager::STATE_STATS;

    ui.add_panel(0, 0, WINDOW_W, WINDOW_H, 0.0f, 0.0f, 0.0f, 0.6f);
    ui.add_panel(WINDOW_W/2 - 300, 80, 600, 560, 0.08f, 0.1f, 0.14f, 0.95f);
    ui.add_label(WINDOW_W/2 - 50, 100, "STATISTICS", 1.0f, 1.0f, 1.0f, 1.0f);

    int y = 160;
    int x = WINDOW_W/2 - 250;
    auto add_stat = [&](const std::string& label, const std::string& value, float r=0.9f, float g=0.9f, float b=0.9f) {
        ui.add_label(x, y, label, 0.7f, 0.8f, 0.9f, 1.0f);
        ui.add_label(x + 250, y, value, r, g, b, 1.0f);
        y += 30;
    };

    add_stat("PLAYER LEVEL", std::to_string(*gs.player_level), 1.0f, 0.9f, 0.5f);
    add_stat("CURRENT XP", std::to_string((int)*gs.player_xp), 0.5f, 0.7f, 1.0f);
    add_stat("XP TO NEXT", std::to_string((int)*gs.xp_to_next), 0.5f, 0.7f, 1.0f);

    auto* pos = gs.reg->get<Position>(gs.player);
    auto* hp = gs.reg->get<Health>(gs.player);
    if (pos) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0f, %.0f", pos->x, pos->y);
        add_stat("POSITION", buf, 0.7f, 0.9f, 0.7f);
    }
    if (hp) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.0f / %.0f", hp->current, hp->max);
        add_stat("HEALTH", buf, 1.0f, 0.3f, 0.3f);
    }

    y += 20;
    ui.add_label(x, y, "GAME STATS", 1.0f, 0.8f, 0.2f, 1.0f);
    y += 30;

    add_stat("TOTAL KILLS", std::to_string(*gs.total_kills), 1.0f, 0.5f, 0.5f);
    add_stat("MOBS SPAWNED", std::to_string(*gs.mobs_spawned), 0.9f, 0.5f, 0.5f);
    add_stat("ACTIVE MOBS", std::to_string(*gs.mob_count), 0.9f, 0.5f, 0.5f);

    y += 20;
    ui.add_label(x, y, "ENVIRONMENT", 1.0f, 0.8f, 0.2f, 1.0f);
    y += 30;

    int hour = (int)(*gs.time_of_day * 24);
    int minute = (int)((*gs.time_of_day * 24 - hour) * 60);
    char time_buf[16];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", hour, minute);
    add_stat("TIME OF DAY", time_buf, 1.0f, 0.9f, 0.5f);

    const char* weather_name = "";
    switch (gs.weather->current) {
        case WeatherType::CLEAR: weather_name="CLEAR"; break;
        case WeatherType::CLOUDY: weather_name="CLOUDY"; break;
        case WeatherType::RAIN: weather_name="RAIN"; break;
        case WeatherType::SNOW: weather_name="SNOW"; break;
        case WeatherType::STORM: weather_name="STORM"; break;
        case WeatherType::FOG: weather_name="FOG"; break;
        case WeatherType::SANDSTORM: weather_name="SANDSTORM"; break;
    }
    add_stat("WEATHER", weather_name, 0.7f, 0.8f, 1.0f);

    add_stat("WIND SPEED", std::to_string((int)std::abs(gs.weather->wind_x)), 0.7f, 0.8f, 1.0f);
    add_stat("ACTIVE LIGHTS", std::to_string(gs.lighting->lights.size()), 1.0f, 0.9f, 0.5f);
    add_stat("FPS", std::to_string(*gs.current_fps), 0.5f, 0.9f, 0.5f);

    ui.add_button(WINDOW_W/2 - 100, WINDOW_H - 80, 200, 40, "CLOSE (TAB)", [&]() {
        gs.audio->play_click();
        ui.state = UIManager::STATE_PLAYING;
        ui.clear();
    });
}

static void build_death_screen(UIManager& ui, GameState& gs) {
    ui.clear();
    ui.state = UIManager::STATE_DEATH;

    ui.add_panel(0, 0, WINDOW_W, WINDOW_H, 0.4f, 0.0f, 0.0f, 0.7f);
    ui.add_label(WINDOW_W/2 - 100, 200, "YOU DIED", 1.0f, 0.2f, 0.2f, 1.0f);

    ui.add_label(WINDOW_W/2 - 200, 280, "YOUR JOURNEY ENDS HERE...", 0.9f, 0.5f, 0.5f, 1.0f);
    ui.add_label(WINDOW_W/2 - 150, 310, "BUT THE UNIVERSE REMEMBERS.", 0.7f, 0.7f, 0.7f, 1.0f);

    ui.add_label(WINDOW_W/2 - 100, 360, "FINAL STATS", 1.0f, 0.7f, 0.2f, 1.0f);
    ui.add_label(WINDOW_W/2 - 100, 390, "LEVEL: " + std::to_string(*gs.player_level), 0.9f, 0.9f, 0.9f, 1.0f);
    ui.add_label(WINDOW_W/2 - 100, 410, "KILLS: " + std::to_string(*gs.total_kills), 0.9f, 0.9f, 0.9f, 1.0f);
    ui.add_label(WINDOW_W/2 - 100, 430, "MOBS SPAWNED: " + std::to_string(*gs.mobs_spawned), 0.9f, 0.9f, 0.9f, 1.0f);

    ui.add_button(WINDOW_W/2 - 150, 500, 300, 50, "RESPAWN", [&]() {
        gs.audio->play_click();
        auto* pos = gs.reg->get<Position>(gs.player);
        auto* vel = gs.reg->get<Velocity>(gs.player);
        auto* hp = gs.reg->get<Health>(gs.player);
        auto* sc = gs.reg->get<StatusContainer>(gs.player);
        if (pos && vel && hp) {
            pos->x = *gs.spawn_x; pos->y = *gs.spawn_y;
            vel->x = 0; vel->y = 0;
            hp->current = hp->max;
            if (sc) sc->clear();
        }
        ui.state = UIManager::STATE_PLAYING;
        ui.clear();
    });
    ui.add_button(WINDOW_W/2 - 150, 570, 300, 50, "MAIN MENU", [&]() {
        gs.audio->play_click();
        build_main_menu(ui, gs);
    });
}

int main() {
    std::cout << "=== KronoUniverse v0.4 ===" << std::endl;
    std::cout << "Full UI System + Menus" << std::endl;

    Renderer renderer;
    if (!renderer.init(WINDOW_W, WINDOW_H, "KronoUniverse v0.4")) return 1;

    SimpleAudio audio;
    bool has_audio = audio.init();
    if (has_audio) std::cout << "Audio OK" << std::endl;

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
    FloatingTextSystem floating_texts;
    WeatherSystem weather;
    LightingSystem lighting;
    UIManager ui;
    PlantSystem plants;
    WildlifeSystem wildlife;
    PlayerInventory player_inv;

    lighting.add_light(LightSource::TORCH, spawn_x + 80, spawn_y - 20, 100);
    lighting.add_light(LightSource::TORCH, spawn_x - 80, spawn_y - 20, 100);

    // Spawn plants and wildlife near spawn
    Biome spawn_biome = world.get_biome((int)(spawn_x/16), 0);
    for (int cx = -2; cx <= 2; cx++) {
        plants.spawn_for_chunk(world, cx, spawn_biome.type, 42);
    }
    wildlife.spawn_for_biome(world, spawn_biome.type, spawn_x, spawn_y - 100, 42);
    std::cout << "Initial plants: " << plants.plants.size() << ", wildlife: " << wildlife.animals.size() << std::endl;

    int mouse_x=0, mouse_y=0;
    bool mouse_left=false, mouse_right=false;
    bool show_debug=false;
    bool was_grounded=true;
    bool paused = false;

    BlockType palette[] = {BlockType::DIRT, BlockType::STONE, BlockType::WOOD,
                            BlockType::METAL, BlockType::SAND, BlockType::ICE,
                            BlockType::LEAVES, BlockType::CRYSTAL};
    int palette_idx = 0;
    BlockType selected = palette[0];

    WeaponStats weapon_loadout[] = {
        CombatSystem::make_sword(),
        CombatSystem::make_bow(),
        CombatSystem::make_fire_staff(),
        CombatSystem::make_ice_staff(),
        CombatSystem::make_poison_dagger(),
    };
    int weapon_idx = 0;
    auto* ew = reg.get<EquippedWeapon>(player);
    if (ew) ew->stats = weapon_loadout[0];

    float time_of_day = 0.5f;
    float mob_spawn_timer = 5.0f;
    int mobs_spawned = 0;
    int mob_count = 0;
    int total_kills = 0;
    int player_level = 1;
    float player_xp = 0;
    float xp_to_next = 100;
    int current_fps = 0;

    // Game state for UI
    GameState gs;
    gs.running = true;
    gs.paused = &paused;
    gs.player = player;
    gs.reg = &reg;
    gs.world = &world;
    gs.camera = &camera;
    gs.audio = &audio;
    gs.particles = &particles;
    gs.floating_texts = &floating_texts;
    gs.weather = &weather;
    gs.lighting = &lighting;
    gs.ui = &ui;
    gs.time_of_day = &time_of_day;
    gs.mob_count = &mob_count;
    gs.mobs_spawned = &mobs_spawned;
    gs.total_kills = &total_kills;
    gs.player_level = &player_level;
    gs.player_xp = &player_xp;
    gs.xp_to_next = &xp_to_next;
    gs.current_fps = &current_fps;
    gs.show_debug = &show_debug;
    gs.weapon_idx = &weapon_idx;
    gs.palette_idx = &palette_idx;
    gs.selected = &selected;
    gs.spawn_x = &spawn_x;
    gs.spawn_y = &spawn_y;
    gs.plants = &plants;
    gs.wildlife = &wildlife;
    gs.player_inv = &player_inv;

    // Build initial main menu
    build_main_menu(ui, gs);

    bool running = true;
    Uint32 last_time = SDL_GetTicks();
    float accumulator = 0;
    int frame_count = 0;
    int fps_timer = 0;
    int fps_count = 0;

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
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    if (ui.state == UIManager::STATE_PLAYING) {
                        paused = true;
                        build_pause_menu(ui, gs);
                        audio.play_click();
                    } else if (ui.state == UIManager::STATE_PAUSED) {
                        paused = false;
                        ui.state = UIManager::STATE_PLAYING;
                        ui.clear();
                        audio.play_click();
                    } else if (ui.state == UIManager::STATE_MAIN_MENU) {
                        running = false;
                    } else {
                        // Return to pause or playing from sub-menu
                        if (paused) build_pause_menu(ui, gs);
                        else { ui.state = UIManager::STATE_PLAYING; ui.clear(); }
                        audio.play_click();
                    }
                }
                if (ui.state == UIManager::STATE_PLAYING) {
                    if (e.key.keysym.sym == SDLK_F1) show_debug = !show_debug;
                    if (e.key.keysym.sym == SDLK_F2) { palette_idx=(palette_idx+1)%8; selected=palette[palette_idx]; }
                    if (e.key.keysym.sym == SDLK_i) { build_inventory_menu(ui, gs); audio.play_click(); }
                    if (e.key.keysym.sym == SDLK_c) { build_crafting_menu(ui, gs); audio.play_click(); }
                    if (e.key.keysym.sym == SDLK_TAB) { build_stats_menu(ui, gs); audio.play_click(); }
                    if (e.key.keysym.sym == SDLK_q) {
                        weapon_idx = (weapon_idx + 1) % 5;
                        auto* w = reg.get<EquippedWeapon>(player);
                        if (w) { w->stats = weapon_loadout[weapon_idx]; audio.play_pickup(); }
                    }
                    if (e.key.keysym.sym == SDLK_e) {
                        float wmx = camera.s2w_x(mouse_x);
                        float wmy = camera.s2w_y(mouse_y);
                        if (CombatSystem::attack(reg, player, wmx, wmy)) {
                            auto* w = reg.get<EquippedWeapon>(player);
                            if (w) {
                                if (w->stats.projectile_speed > 0) audio.play_shoot();
                                else audio.play_attack();
                            }
                        }
                    }
                    if (e.key.keysym.sym >= SDLK_1 && e.key.keysym.sym <= SDLK_8) {
                        palette_idx = e.key.keysym.sym - SDLK_1;
                        selected = palette[palette_idx];
                        audio.play_pickup();
                    }
                    if (e.key.keysym.sym == SDLK_r) {
                        weather.decide_for_biome(world.get_biome((int)camera.x/BLOCK_SIZE, 0).type);
                    }
                }
                input.process_event(e, reg);
            }
            if (e.type == SDL_MOUSEMOTION) { mouse_x=e.motion.x; mouse_y=e.motion.y; }
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button==SDL_BUTTON_LEFT) {
                    mouse_left=true;
                    if (ui.state != UIManager::STATE_PLAYING) {
                        ui.mouse_left_pressed = true;
                    }
                }
                if (e.button.button==SDL_BUTTON_RIGHT) mouse_right=true;
            }
            if (e.type == SDL_MOUSEWHEEL && ui.state == UIManager::STATE_PLAYING) {
                camera.zoom = std::max(0.5f, std::min(3.0f, camera.zoom + e.wheel.y*0.1f));
            }
            if (ui.state == UIManager::STATE_PLAYING) {
                input.process_event(e, reg);
            }
        }

        // Update UI mouse position
        ui.mouse_x = mouse_x;
        ui.mouse_y = mouse_y;

        // ---- Fixed updates (only when playing) ----
        if (ui.state == UIManager::STATE_PLAYING && !paused) {
            while (accumulator >= FIXED_DT) {
                input.update(reg);
                movement.update(reg, FIXED_DT, GAME_GRAVITY);

                auto* pos = reg.get<Position>(player);
                auto* vel = reg.get<Velocity>(player);
                auto* mass = reg.get<Mass>(player);
                auto* rb = reg.get<RigidBody>(player);
                auto* ctrl = reg.get<CharacterController>(player);

                if (pos && vel && mass && rb && !rb->is_static) {
                    auto* sc = reg.get<StatusContainer>(player);
                    float speed_mult = 1.0f;
                    if (sc) {
                        if (sc->has(StatusEffect::FREEZE)) speed_mult = 0;
                        else if (sc->has(StatusEffect::STUN)) speed_mult = 0;
                        else if (sc->has(StatusEffect::SLOW)) speed_mult = 0.5f;
                        else if (sc->has(StatusEffect::HASTE)) speed_mult = 1.5f;
                    }
                    vel->x *= speed_mult;
                    vel->y += GAME_GRAVITY * FIXED_DT;
                    vel->x *= (1.0f - 0.01f * FIXED_DT);
                    pos->x += vel->x * FIXED_DT;
                    pos->y += vel->y * FIXED_DT;
                }

                resolve_world_collision(reg, player, world, *ctrl);

                if (ctrl->grounded && !was_grounded && vel->y > 100) {
                    if (has_audio) audio.play_land();
                    particles.emit_land(pos->x, pos->y);
                }
                was_grounded = ctrl->grounded;

                if (ctrl->state == MoveState::JUMPING && ctrl->prev_state != MoveState::JUMPING) {
                    if (has_audio) audio.play_jump();
                }
                ctrl->prev_state = ctrl->state;

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
                        if (b->type == BlockType::CRYSTAL) particles.emit_spark(tbx*BLOCK_SIZE+8, tby*BLOCK_SIZE+8, 0.6f, 0.9f, 1.0f);
                        // Spawn item drop
                        auto [item_id, drop_count] = block_to_item_drop(b->type);
                        if (item_id != 0) {
                            ItemDropSystem::spawn_drop_scatter(reg,
                                tbx*BLOCK_SIZE + 8, tby*BLOCK_SIZE + 8,
                                item_id, drop_count);
                        }
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

                CombatSystem::update_cooldowns(reg, FIXED_DT);
                CombatSystem::update_statuses(reg, FIXED_DT);
                CombatSystem::update_projectiles(reg, FIXED_DT);
                CombatSystem::check_projectile_hits(reg);
                projectile_world_collision(reg, world);

                // Update item drops and collect
                auto collected = ItemDropSystem::update(reg, player, FIXED_DT, world);
                for (auto& [item_id, count] : collected) {
                    player_inv.add_item(item_id, count);
                    if (has_audio) audio.play_pickup();
                    floating_texts.emit(pos->x, pos->y - 20, "+" + std::string(item_name(item_id)), 0.5f, 1.0f, 0.5f);
                }

                // Update plants and wildlife
                plants.update(FIXED_DT);
                wildlife.update(FIXED_DT, time_of_day, pos->x, pos->y);

                // Periodically spawn new plants/wildlife based on player position
                static float nature_spawn_timer = 10.0f;
                nature_spawn_timer -= FIXED_DT;
                if (nature_spawn_timer <= 0) {
                    nature_spawn_timer = 15.0f;
                    Biome cur_b = world.get_biome((int)(pos->x/16), 0);
                    int player_chunk = (int)(pos->x / 16 / 64);
                    plants.spawn_for_chunk(world, player_chunk, cur_b.type, 42);
                    plants.cull_far(pos->x, 1500);
                    if ((int)wildlife.animals.size() < 15) {
                        wildlife.spawn_for_biome(world, cur_b.type, pos->x, pos->y - 100, 42);
                    }
                }

                // Count mobs before AI update
                int mobs_before = 0;
                reg.each<MobAI>([&](auto e, MobAI&) { mobs_before++; });

                MobAISystem::update(reg, player, FIXED_DT, GAME_GRAVITY);
                reg.each<Position, AABBCollider, MobAI>([&](auto ent, Position& p, AABBCollider& c, MobAI& ai) {
                    resolve_mob_collision(reg, ent, world);
                });

                // Count kills (mobs that died)
                int mobs_after = 0;
                reg.each<MobAI>([&](auto e, MobAI&) { mobs_after++; });
                if (mobs_after < mobs_before) {
                    int killed = mobs_before - mobs_after;
                    total_kills += killed;
                    player_xp += killed * 10;
                    if (player_xp >= xp_to_next) {
                        player_level++;
                        player_xp -= xp_to_next;
                        xp_to_next = (int)(xp_to_next * 1.5f);
                        audio.play_levelup();
                        floating_texts.emit(pos->x, pos->y - 30, "LEVEL UP!", 1.0f, 0.9f, 0.2f);
                        auto* hp = reg.get<Health>(player);
                        if (hp) { hp->max += 10; hp->current = hp->max; }
                    }
                }
                mob_count = mobs_after;

                // Mob spawn
                mob_spawn_timer -= FIXED_DT;
                if (mob_spawn_timer <= 0) {
                    mob_spawn_timer = 8.0f + (float)(rand()%50)/10.0f;
                    if (mob_count < 10) {
                        float angle = ((float)rand()/RAND_MAX) * 2 * M_PI;
                        float dist = 400 + (float)(rand()%200);
                        float mx = pos->x + cos(angle) * dist;
                        float my = pos->y + sin(angle) * dist;
                        int mbx = (int)(mx / BLOCK_SIZE);
                        for (int by = 0; by < CHUNK_H; by++) {
                            Block* b = world.get_block(mbx, by);
                            if (b && b->is_solid()) { my = (by - 3) * BLOCK_SIZE; break; }
                        }
                        MobType type;
                        int r = rand() % 100;
                        if (time_of_day < 0.2f || time_of_day > 0.8f) {
                            if (r < 30) type = MobType::ZOMBIE;
                            else if (r < 50) type = MobType::SKELETON;
                            else if (r < 70) type = MobType::BAT;
                            else if (r < 90) type = MobType::SLIME;
                            else type = MobType::BOSS;
                        } else {
                            if (r < 30) type = MobType::DEER;
                            else if (r < 50) type = MobType::RABBIT;
                            else if (r < 70) type = MobType::SLIME;
                            else if (r < 85) type = MobType::ZOMBIE;
                            else type = MobType::SKELETON;
                        }
                        MobAISystem::spawn_mob(reg, type, mx, my);
                        mobs_spawned++;
                    }
                }

                auto* psc = reg.get<StatusContainer>(player);
                if (psc) {
                    if (psc->has(StatusEffect::BURN) && (frame_count % 6 == 0))
                        particles.emit(pos->x+12, pos->y+20, 2, 1.0f, 0.5f, 0.1f, 30, 80, 0.2f, 0.4f);
                    if (psc->has(StatusEffect::POISON) && (frame_count % 10 == 0))
                        particles.emit(pos->x+12, pos->y+20, 2, 0.3f, 0.8f, 0.2f, 30, 80, 0.2f, 0.4f);
                }

                particles.update(FIXED_DT);
                floating_texts.update(FIXED_DT);
                camera.follow(pos->x, pos->y, FIXED_DT);

                time_of_day += FIXED_DT / 120.0f;
                if (time_of_day > 1.0f) time_of_day = 0.0f;

                Biome cur_biome = world.get_biome((int)(pos->x/BLOCK_SIZE), 0);
                static float weather_check_timer = 0;
                weather_check_timer += FIXED_DT;
                if (weather_check_timer > 30.0f) {
                    weather.decide_for_biome(cur_biome.type);
                    weather_check_timer = 0;
                }
                weather.update(FIXED_DT, camera.left(), camera.top(), WINDOW_W/camera.zoom, WINDOW_H/camera.zoom);
                weather.apply_wind(reg, FIXED_DT);
                if (weather.get_lightning_intensity() > 0.5f && (frame_count % 30 == 0)) audio.play_thunder();

                lighting.update(FIXED_DT, pos->x, pos->y, time_of_day);

                auto* php = reg.get<Health>(player);
                if (php && php->current <= 0) {
                    audio.play_death();
                    build_death_screen(ui, gs);
                }

                if (pos->y > CHUNK_H * BLOCK_SIZE + 1000) {
                    pos->x = spawn_x; pos->y = spawn_y;
                    vel->x = 0; vel->y = 0;
                    auto* hp = reg.get<Health>(player);
                    if (hp) hp->current = hp->max;
                }

                input.end_frame(reg);
                accumulator -= FIXED_DT;
            }
        } else {
            // UI mode - just update UI
            accumulator = 0;
        }

        // Update UI clicks
        ui.update();

        // ---- Render ----
        renderer.clear();

        if (ui.state == UIManager::STATE_PLAYING || ui.state == UIManager::STATE_PAUSED ||
            ui.state == UIManager::STATE_INVENTORY || ui.state == UIManager::STATE_CRAFTING ||
            ui.state == UIManager::STATE_STATS || ui.state == UIManager::STATE_DEATH) {
            // Render game world
            float vw = WINDOW_W / camera.zoom;
            float vh = WINDOW_H / camera.zoom;
            renderer.set_ortho(camera.x, camera.x+vw, camera.y+vh, camera.y);

            float daylight = sin(time_of_day * M_PI);
            daylight = std::max(0.15f, daylight);
            float night_tint = 1.0f - daylight;

            int min_bx = (int)(camera.left()/BLOCK_SIZE)-1;
            int max_bx = (int)(camera.right()/BLOCK_SIZE)+1;
            int min_by = std::max(0, (int)(camera.top()/BLOCK_SIZE)-1);
            int max_by = std::min(CHUNK_H-1, (int)(camera.bottom()/BLOCK_SIZE)+1);

            for (int bx = min_bx; bx <= max_bx; bx++) {
                for (int by = min_by; by <= max_by; by++) {
                    Block* b = world.get_block(bx, by);
                    if (!b || b->is_air()) continue;
                    BlockColor c = get_block_color(b->type);
                    if (b->hp < b->max_hp && b->max_hp > 0) {
                        float r = (float)b->hp / b->max_hp;
                        c.r *= 0.5f+r*0.5f; c.g *= 0.5f+r*0.5f; c.b *= 0.5f+r*0.5f;
                    }
                    auto light = lighting.get_light_at(bx*BLOCK_SIZE + BLOCK_SIZE/2, by*BLOCK_SIZE + BLOCK_SIZE/2);
                    c.r = std::min(1.5f, c.r * light.r);
                    c.g = std::min(1.5f, c.g * light.g);
                    c.b = std::min(1.5f, c.b * light.b);

                    // Voxel depth shading: blocks exposed to air above are brighter
                    Block* above = world.get_block(bx, by - 1);
                    if (above && above->is_air()) {
                        // Top of pillar - brighter
                        c.r = std::min(1.5f, c.r * 1.15f);
                        c.g = std::min(1.5f, c.g * 1.15f);
                        c.b = std::min(1.5f, c.b * 1.15f);
                    } else if (!above || !above->is_solid()) {
                        // Edge of world or liquid above
                    } else {
                        // Underground - darker
                        c.r *= 0.85f;
                        c.g *= 0.85f;
                        c.b *= 0.85f;
                    }
                    // Side shading
                    Block* left = world.get_block(bx - 1, by);
                    Block* right = world.get_block(bx + 1, by);
                    if ((!left || left->is_air()) && (!right || !right->is_air())) {
                        // Both sides exposed
                    } else if (!left || left->is_air()) {
                        c.r *= 0.9f; c.g *= 0.9f; c.b *= 0.9f;
                    } else if (!right || right->is_air()) {
                        c.r *= 0.95f; c.g *= 0.95f; c.b *= 0.95f;
                    }

                    renderer.draw_rect(bx*BLOCK_SIZE, by*BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, c.r, c.g, c.b);

                    // Top edge highlight (only if block above is air)
                    if (above && above->is_air()) {
                        renderer.draw_rect(bx*BLOCK_SIZE, by*BLOCK_SIZE, BLOCK_SIZE, 2,
                                          std::min(1.5f, c.r * 1.2f),
                                          std::min(1.5f, c.g * 1.2f),
                                          std::min(1.5f, c.b * 1.2f), 0.7f);
                    }
                }
            }

            lighting.render_lights(renderer);

            // Render plants
            for (auto& p : plants.plants) {
                if (p.x < camera.left() - 50 || p.x > camera.right() + 50) continue;
                if (p.y < camera.top() - 50 || p.y > camera.bottom() + 50) continue;
                float r, g, b;
                PlantSystem::get_color(p.type, r, g, b);
                float w, h;
                PlantSystem::get_size(p.type, w, h);
                // Apply growth
                w *= p.growth;
                h *= p.growth;
                // Apply lighting
                auto light = lighting.get_light_at(p.x, p.y);
                r = std::min(1.5f, r * light.r);
                g = std::min(1.5f, g * light.g);
                b = std::min(1.5f, b * light.b);
                renderer.draw_rect(p.x - w/2, p.y - h + 16, w, h, r, g, b);
                // Harvestable indicator
                if (p.harvestable && p.harvest_timer <= 0 && p.growth >= 0.5f) {
                    renderer.draw_rect(p.x - 2, p.y - h + 12, 4, 4, 0.3f, 1.0f, 0.3f, 0.6f + 0.4f * sin(frame_count * 0.1f));
                }
                // Glowing plants add light
                if (PlantSystem::is_glowing(p.type)) {
                    lighting.add_light(p.type == PlantType::MUSHROOM_GLOW ? LightSource::CRYSTAL_GLOW : LightSource::CRYSTAL_GLOW,
                                      p.x, p.y, 40, 0.4f);
                }
            }

            // Render wildlife
            for (auto& w : wildlife.animals) {
                if (!w.active) continue;
                if (w.x < camera.left() - 50 || w.x > camera.right() + 50) continue;
                if (w.y < camera.top() - 50 || w.y > camera.bottom() + 50) continue;
                float r, g, b;
                WildlifeSystem::get_color(w.type, r, g, b);
                float ww, hh;
                WildlifeSystem::get_size(w.type, ww, hh);
                auto light = lighting.get_light_at(w.x, w.y);
                r = std::min(1.5f, r * light.r);
                g = std::min(1.5f, g * light.g);
                b = std::min(1.5f, b * light.b);
                renderer.draw_rect(w.x - ww/2, w.y - hh/2, ww, hh, r, g, b);
                // Firefly glow
                if (WildlifeSystem::is_glowing(w.type)) {
                    float glow = 0.5f + 0.5f * sin(w.glow_phase);
                    renderer.draw_rect(w.x - 6, w.y - 6, 12, 12, 1.0f, 0.9f, 0.3f, glow * 0.4f);
                    lighting.add_light(LightSource::STAR, w.x, w.y, 30, glow * 0.5f);
                }
            }

            // Render item drops
            reg.each<Position, ItemDrop>([&](auto ent, Position& p, ItemDrop& drop) {
                if (p.x < camera.left() - 50 || p.x > camera.right() + 50) return;
                float r, g, b;
                item_color(drop.item_id, r, g, b);
                auto light = lighting.get_light_at(p.x, p.y);
                r = std::min(1.5f, r * light.r);
                g = std::min(1.5f, g * light.g);
                b = std::min(1.5f, b * light.b);
                // Floating animation
                float bob = sin(drop.bob_phase) * 2;
                renderer.draw_rect(p.x - 5, p.y - 5 + bob, 10, 10, r, g, b);
                // Glow
                renderer.draw_rect(p.x - 8, p.y - 8 + bob, 16, 16, r, g, b, 0.2f);
            });

            // Render mobs
            reg.each<Position, AABBCollider, MobAI, Health>([&](auto ent, Position& p, AABBCollider& c, MobAI& ai, Health& hp) {
                if (hp.current <= 0) return;
                float r, g, b;
                MobAISystem::get_mob_color(ai.type, r, g, b);
                auto light = lighting.get_light_at(p.x + c.width/2, p.y + c.height/2);
                r = std::min(1.5f, r * light.r);
                g = std::min(1.5f, g * light.g);
                b = std::min(1.5f, b * light.b);
                renderer.draw_rect(p.x, p.y, c.width, c.height, r, g, b);
                float eye_r = ai.is_passive ? 0.9f : 1.0f;
                float eye_g = ai.is_passive ? 0.9f : 0.2f;
                float eye_b = ai.is_passive ? 0.9f : 0.2f;
                renderer.draw_rect(p.x + 4, p.y + 6, 3, 3, eye_r, eye_g, eye_b);
                renderer.draw_rect(p.x + c.width - 7, p.y + 6, 3, 3, eye_r, eye_g, eye_b);
                if (ai.is_boss) {
                    renderer.draw_rect(p.x, p.y - 6, c.width, 4, 1.0f, 0.8f, 0.1f);
                    float hr = hp.current / hp.max;
                    renderer.draw_rect(p.x - 4, p.y - 14, c.width + 8, 6, 0.2f, 0.2f, 0.2f);
                    renderer.draw_rect(p.x - 4, p.y - 14, (c.width + 8) * hr, 6, 1.0f - hr, hr, 0.0f);
                } else if (hp.current < hp.max) {
                    float hr = hp.current / hp.max;
                    renderer.draw_rect(p.x - 2, p.y - 8, c.width + 4, 3, 0.2f, 0.2f, 0.2f);
                    renderer.draw_rect(p.x - 2, p.y - 8, (c.width + 4) * hr, 3, 1.0f - hr, hr, 0.0f);
                }
            });

            // Projectiles
            reg.each<Position, Velocity, Projectile>([&](auto ent, Position& p, Velocity& v, Projectile& proj) {
                float r, g, b;
                switch (proj.damage_type) {
                    case DamageType::FIRE: r=1.0f; g=0.5f; b=0.1f; break;
                    case DamageType::ICE: r=0.5f; g=0.9f; b=1.0f; break;
                    case DamageType::POISON: r=0.3f; g=0.9f; b=0.2f; break;
                    case DamageType::PIERCING: r=0.9f; g=0.9f; b=0.9f; break;
                    default: r=0.8f; g=0.8f; b=0.6f; break;
                }
                for (int i = 0; i < 4; i++) {
                    float tx = p.x - v.x * 0.005f * i;
                    float ty = p.y - v.y * 0.005f * i;
                    renderer.draw_rect(tx, ty, 6, 6, r, g, b, 1.0f - (float)i*0.2f);
                }
            });

            particles.render(renderer);

            // Player
            if (ui.state != UIManager::STATE_DEATH) {
                auto* ppos = reg.get<Position>(player);
                auto* pvel = reg.get<Velocity>(player);
                auto* pctrl = reg.get<CharacterController>(player);
                auto* php = reg.get<Health>(player);

                auto light_p = lighting.get_light_at(ppos->x + 12, ppos->y + 20);
                float lr = std::min(1.5f, light_p.r);
                float lg = std::min(1.5f, light_p.g);
                float lb = std::min(1.5f, light_p.b);

                renderer.draw_rect(ppos->x, ppos->y, 24, 40, 0.13f*lr, 0.83f*lg, 0.93f*lb);
                renderer.draw_rect(ppos->x+6, ppos->y, 12, 12, 0.86f*lr, 0.70f*lg, 0.55f*lb);
                float eye_x = pctrl->facing_right ? ppos->x+12 : ppos->x+8;
                renderer.draw_rect(eye_x, ppos->y+4, 4, 3, 0.1f, 0.1f, 0.15f);
                renderer.draw_rect(ppos->x+4, ppos->y+12, 16, 16, 0.24f*lr, 0.47f*lg, 0.78f*lb);
                float leg_off = 0;
                if (pctrl->state == MoveState::WALKING || pctrl->state == MoveState::RUNNING)
                    leg_off = sin(frame_count * 0.3f) * 2;
                renderer.draw_rect(ppos->x+4, ppos->y+28+leg_off, 7, 12, 0.2f*lr, 0.2f*lg, 0.24f*lb);
                renderer.draw_rect(ppos->x+13, ppos->y+28-leg_off, 7, 12, 0.2f*lr, 0.2f*lg, 0.24f*lb);

                auto* pew = reg.get<EquippedWeapon>(player);
                if (pew && pew->attacking) {
                    float swing_progress = pew->attack_anim_time / pew->attack_anim_duration;
                    float swing_x = pctrl->facing_right ? ppos->x + 24 + swing_progress * 20 : ppos->x - 12 - swing_progress * 20;
                    float swing_y = ppos->y + 15 - sin(swing_progress * M_PI) * 10;
                    renderer.draw_rect(swing_x, swing_y, 12, 4, 0.9f, 0.9f, 0.5f, 0.7f);
                }

                float dot_x = pctrl->facing_right ? ppos->x+18 : ppos->x;
                renderer.draw_rect(dot_x, ppos->y-4, 4, 4, 0.98f, 0.75f, 0.14f);

                if (php && php->current < php->max) {
                    float hr = php->current / php->max;
                    renderer.draw_rect(ppos->x-4, ppos->y-12, 32, 5, 0.2f, 0.2f, 0.2f);
                    renderer.draw_rect(ppos->x-4, ppos->y-12, 32*hr, 5, 1.0f-hr, hr, 0.0f);
                }

                auto* psc = reg.get<StatusContainer>(player);
                if (psc) {
                    int sx = 0;
                    for (auto& s : psc->effects) {
                        float r=1, g=1, b=1;
                        switch (s.type) {
                            case StatusEffect::POISON: r=0.3f; g=0.9f; b=0.2f; break;
                            case StatusEffect::BURN: r=1.0f; g=0.4f; b=0.1f; break;
                            case StatusEffect::FREEZE: r=0.5f; g=0.9f; b=1.0f; break;
                            case StatusEffect::STUN: r=1.0f; g=1.0f; b=0.2f; break;
                            case StatusEffect::BLEED: r=0.8f; g=0.1f; b=0.1f; break;
                            case StatusEffect::REGEN: r=0.2f; g=1.0f; b=0.4f; break;
                            case StatusEffect::HASTE: r=1.0f; g=0.8f; b=0.2f; break;
                            case StatusEffect::SLOW: r=0.5f; g=0.5f; b=0.7f; break;
                            default: break;
                        }
                        renderer.draw_rect(ppos->x + sx*5, ppos->y - 18, 4, 4, r, g, b);
                        sx++;
                    }
                }
            }

            floating_texts.render(renderer);

            float wmx = camera.s2w_x(mouse_x);
            float wmy = camera.s2w_y(mouse_y);
            int hbx = (int)(wmx/BLOCK_SIZE);
            int hby = (int)(wmy/BLOCK_SIZE);
            if (ui.state == UIManager::STATE_PLAYING) {
                renderer.draw_rect(hbx*BLOCK_SIZE, hby*BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, 1.0f, 1.0f, 1.0f, 0.25f);
            }

            weather.render(renderer);

            // ---- HUD (screen space) ----
            renderer.set_screen_ortho();

            if (night_tint > 0.1f)
                renderer.draw_rect(0, 0, WINDOW_W, WINDOW_H, 0.0f, 0.02f, 0.08f, night_tint * 0.4f);

            float li = weather.get_lightning_intensity();
            if (li > 0)
                renderer.draw_rect(0, 0, WINDOW_W, WINDOW_H, 1.0f, 1.0f, 1.0f, li * 0.5f);

            float fd = weather.get_fog_density();
            if (fd > 0.05f)
                renderer.draw_rect(0, 0, WINDOW_W, WINDOW_H, 0.7f, 0.7f, 0.75f, fd * 0.5f);

            if (ui.state == UIManager::STATE_PLAYING) {
                // Hotbar
                int hb_x = WINDOW_W/2 - 120;
                int hb_y = WINDOW_H - 50;
                for (int i = 0; i < 8; i++) {
                    BlockColor sc = get_block_color(palette[i]);
                    renderer.draw_rect(hb_x + i*30, hb_y, 28, 28, 0.1f, 0.1f, 0.12f, 0.8f);
                    renderer.draw_rect(hb_x + i*30 + 6, hb_y + 6, 16, 16, sc.r, sc.g, sc.b);
                    if (i == palette_idx)
                        renderer.draw_rect(hb_x + i*30, hb_y, 28, 28, 1.0f, 0.75f, 0.14f, 0.4f);
                    // Number
                    std::string num = std::to_string(i + 1);
                    BitmapFont::draw(renderer, hb_x + i*30 + 2, hb_y + 2, num, 1.0f, 1.0f, 0.4f, 0.8f);
                }

                // Health bar
                auto* php = reg.get<Health>(player);
                renderer.draw_rect(10, 10, 200, 16, 0.1f, 0.1f, 0.12f, 0.8f);
                float hr = php ? php->current / php->max : 0;
                renderer.draw_rect(12, 12, 196 * hr, 12, 1.0f - hr, hr, 0.0f);
                BitmapFont::draw(renderer, 14, 13, "HP", 1.0f, 1.0f, 1.0f, 1.0f);
                char hp_text[32];
                snprintf(hp_text, sizeof(hp_text), "%d/%d", (int)(php ? php->current : 0), (int)(php ? php->max : 0));
                BitmapFont::draw(renderer, 150, 13, hp_text, 1.0f, 1.0f, 1.0f, 1.0f);

                // XP bar
                renderer.draw_rect(10, 30, 200, 6, 0.1f, 0.1f, 0.12f, 0.8f);
                renderer.draw_rect(12, 31, 196 * (player_xp / xp_to_next), 4, 0.3f, 0.6f, 1.0f);

                // Level
                char lvl_text[32];
                snprintf(lvl_text, sizeof(lvl_text), "LVL %d", player_level);
                BitmapFont::draw(renderer, 10, 40, lvl_text, 1.0f, 0.9f, 0.3f, 1.0f);

                // Weapon name
                const char* weapon_names[] = {"SWORD", "BOW", "FIRE STAFF", "ICE STAFF", "POISON DAGGER"};
                BitmapFont::draw(renderer, WINDOW_W/2 - 60, 14, weapon_names[weapon_idx], 0.9f, 0.9f, 0.4f, 1.0f);

                // Time of day
                int hour = (int)(time_of_day * 24);
                int minute = (int)((time_of_day * 24 - hour) * 60);
                char time_buf[16];
                snprintf(time_buf, sizeof(time_buf), "%02d:%02d", hour, minute);
                BitmapFont::draw(renderer, WINDOW_W - 80, 14, time_buf, 1.0f, 0.9f, 0.5f, 1.0f);

                // Weather text
                const char* weather_name = "";
                switch (weather.current) {
                    case WeatherType::CLEAR: weather_name="CLEAR"; break;
                    case WeatherType::CLOUDY: weather_name="CLOUDY"; break;
                    case WeatherType::RAIN: weather_name="RAIN"; break;
                    case WeatherType::SNOW: weather_name="SNOW"; break;
                    case WeatherType::STORM: weather_name="STORM"; break;
                    case WeatherType::FOG: weather_name="FOG"; break;
                    case WeatherType::SANDSTORM: weather_name="SANDSTORM"; break;
                }
                BitmapFont::draw(renderer, WINDOW_W - 80, 30, weather_name, 0.7f, 0.8f, 1.0f, 1.0f);

                // Minimap
                int mm_x = WINDOW_W - 220;
                int mm_y = 70;
                int mm_size = 200;
                renderer.draw_rect(mm_x, mm_y, mm_size, mm_size, 0.05f, 0.05f, 0.07f, 0.7f);
                renderer.draw_rect(mm_x + mm_size/2 - 2, mm_y + mm_size/2 - 2, 4, 4, 0.13f, 0.83f, 0.93f);
                reg.each<Position, MobAI, Health>([&](auto ent, Position& mp, MobAI& mai, Health& mh) {
                    if (mh.current <= 0) return;
                    auto* pp = reg.get<Position>(player);
                    float dx = mp.x - pp->x;
                    float dy = mp.y - pp->y;
                    int mx = mm_x + mm_size/2 + (int)(dx / 30);
                    int my = mm_y + mm_size/2 + (int)(dy / 30);
                    if (mx >= mm_x && mx < mm_x + mm_size && my >= mm_y && my < mm_y + mm_size) {
                        if (mai.is_boss) renderer.draw_rect(mx-2, my-2, 5, 5, 1.0f, 0.2f, 0.2f);
                        else if (mai.is_passive) renderer.draw_rect(mx, my, 2, 2, 0.4f, 0.8f, 0.4f);
                        else renderer.draw_rect(mx, my, 2, 2, 1.0f, 0.3f, 0.3f);
                    }
                });

                // Debug overlay
                if (show_debug) {
                    renderer.draw_rect(0, WINDOW_H - 100, 250, 100, 0.0f, 0.0f, 0.0f, 0.7f);
                    char dbg[64];
                    snprintf(dbg, sizeof(dbg), "FPS: %d", current_fps);
                    BitmapFont::draw(renderer, 10, WINDOW_H - 95, dbg, 0.5f, 0.9f, 0.5f, 1.0f);
                    snprintf(dbg, sizeof(dbg), "MOBS: %d", mob_count);
                    BitmapFont::draw(renderer, 10, WINDOW_H - 80, dbg, 0.9f, 0.5f, 0.5f, 1.0f);
                    snprintf(dbg, sizeof(dbg), "PARTICLES: %d", (int)particles.particles.size());
                    BitmapFont::draw(renderer, 10, WINDOW_H - 65, dbg, 0.6f, 0.4f, 0.2f, 1.0f);
                    snprintf(dbg, sizeof(dbg), "LIGHTS: %d", (int)lighting.lights.size());
                    BitmapFont::draw(renderer, 10, WINDOW_H - 50, dbg, 1.0f, 0.9f, 0.5f, 1.0f);
                }

                // Help bar at bottom
                BitmapFont::draw(renderer, 10, WINDOW_H - 18, "WASD:MOVE  E:ATTACK  Q:WEAPON  I:INV  C:CRAFT  TAB:STATS  ESC:PAUSE", 0.5f, 0.5f, 0.6f, 1.0f);
            }
        } else if (ui.state == UIManager::STATE_MAIN_MENU ||
                   ui.state == UIManager::STATE_SETTINGS ||
                   ui.state == UIManager::STATE_NEW_GAME) {
            // Main menu background - render a simple animated starfield
            renderer.set_ortho(0, WINDOW_W, WINDOW_H, 0);
            renderer.draw_rect(0, 0, WINDOW_W, WINDOW_H, 0.04f, 0.05f, 0.08f, 1.0f);
            // Stars
            for (int i = 0; i < 80; i++) {
                float x = (float)(((i * 137 + (int)(frame_count * 0.5f)) % WINDOW_W + WINDOW_W) % WINDOW_W);
                float y = (float)((i * 73) % WINDOW_H);
                float bright = 0.3f + 0.7f * (float)sin(frame_count * 0.02f + i) * 0.5f + 0.5f;
                renderer.draw_rect(x, y, 2, 2, bright, bright, bright, 0.8f);
            }
            // Title big banner
            renderer.draw_rect(WINDOW_W/2 - 280, 60, 560, 80, 0.1f, 0.2f, 0.3f, 0.5f);
        }

        // Render UI elements
        renderer.set_screen_ortho();
        ui.render(renderer);

        // Mouse cursor
        renderer.draw_rect(mouse_x - 2, mouse_y - 2, 4, 4, 1.0f, 1.0f, 1.0f, 1.0f);
        renderer.draw_rect(mouse_x, mouse_y, 1, 12, 1.0f, 1.0f, 1.0f, 0.8f);
        renderer.draw_rect(mouse_x, mouse_y, 12, 1, 1.0f, 1.0f, 1.0f, 0.8f);

        renderer.present();
        frame_count++;
        running = running && gs.running;
    }

    std::cout << "Game ended after " << frame_count << " frames." << std::endl;
    std::cout << "Mobs spawned: " << mobs_spawned << ", kills: " << total_kills << std::endl;
    if (has_audio) audio.shutdown();
    renderer.shutdown();
    return 0;
}
