#pragma once
// KronoUniverse — Audio System (SDL_mixer)
// Sons procedurais (sem arquivos externos) + música ambiental.

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <unordered_map>
#include <string>
#include <cmath>
#include <cstdlib>

namespace krono {

class AudioSystem {
public:
    bool init() {
        if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) != 0) {
            SDL_Log("Mix_OpenAudio failed: %s", Mix_GetError());
            return false;
        }
        Mix_AllocateChannels(32);
        SDL_Log("AudioSystem initialized: 44100Hz, 32 channels");
        return true;
    }

    Mix_Chunk* generate_sound(const std::string& type) {
        int freq = 44100;
        int duration_ms = 100;
        int samples = (freq * duration_ms) / 1000;

        if (type == "explosion") duration_ms = 300;
        else if (type == "land") duration_ms = 150;
        else if (type == "step") duration_ms = 50;

        samples = (freq * duration_ms) / 1000;
        Uint8* data = new Uint8[samples * 2];

        for (int i = 0; i < samples; i++) {
            float t = (float)i / freq;
            float wave = 0.0f;

            if (type == "jump") {
                float pitch = 200 + 600 * (float)i / samples;
                wave = sin(2 * M_PI * pitch * t) * 0.3f * exp(-t * 5);
            } else if (type == "mine") {
                float env = exp(-t * 15);
                float noise = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
                wave = noise * env * 0.4f;
            } else if (type == "step") {
                float env = exp(-t * 30);
                wave = sin(2 * M_PI * 80 * t) * env * 0.2f;
            } else if (type == "hurt") {
                float pitch = 400 - 300 * (float)i / samples;
                wave = sin(2 * M_PI * pitch * t) * 0.3f * exp(-t * 5);
            } else if (type == "place") {
                float env = exp(-t * 10);
                wave = sin(2 * M_PI * 300 * t) * env * 0.25f;
            } else if (type == "explosion") {
                float env = exp(-t * 5);
                float noise = ((float)rand() / RAND_MAX - 0.5f) * 2.0f;
                float rumble = sin(2 * M_PI * 50 * t) * 0.5f;
                wave = (noise + rumble) * env * 0.5f;
            } else if (type == "land") {
                float env = exp(-t * 12);
                wave = sin(2 * M_PI * 60 * t) * env * 0.35f;
            } else {
                float env = exp(-t * 8);
                wave = sin(2 * M_PI * 440 * t) * env * 0.2f;
            }

            int16_t val = (int16_t)(wave * 32767);
            data[i*2] = val & 0xFF;
            data[i*2+1] = (val >> 8) & 0xFF;
        }

        SDL_RWops* rw = SDL_RWFromMem(data, samples * 2);
        Mix_Chunk* chunk = Mix_LoadWAV_RW(rw, 0);
        delete[] data; // Mix_LoadWAV_RW copies data
        sounds_[type] = chunk;
        return chunk;
    }

    void play(const std::string& name, int volume = 64) {
        auto it = sounds_.find(name);
        if (it != sounds_.end() && it->second) {
            Mix_VolumeChunk(it->second, volume);
            Mix_PlayChannel(-1, it->second, 0);
        }
    }

    void play_3d(const std::string& name, float sx, float sy, float lx, float ly, int max_dist = 500) {
        auto it = sounds_.find(name);
        if (it == sounds_.end() || !it->second) return;
        float dx = sx - lx, dy = sy - ly;
        float dist = sqrt(dx*dx + dy*dy);
        if (dist > max_dist) return;
        float vol_ratio = 1.0f - (dist / max_dist);
        int vol = (int)(vol_ratio * 128);
        int ch = Mix_PlayChannel(-1, it->second, 0);
        if (ch >= 0) {
            Mix_Volume(ch, vol);
            float pan = std::max(-1.0f, std::min(1.0f, dx / max_dist));
            Uint8 left = (pan < 0) ? 255 : (Uint8)(255 * (1.0f - pan));
            Uint8 right = (pan > 0) ? 255 : (Uint8)(255 * (1.0f + pan));
            Mix_SetPanning(ch, left, right);
        }
    }

    void shutdown() {
        for (auto& [name, chunk] : sounds_) {
            if (chunk) Mix_FreeChunk(chunk);
        }
        sounds_.clear();
        Mix_CloseAudio();
    }

    ~AudioSystem() { shutdown(); }

private:
    std::unordered_map<std::string, Mix_Chunk*> sounds_;
};

} // namespace krono
