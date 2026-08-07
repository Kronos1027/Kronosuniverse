#pragma once
// KronoUniverse — Input System (Fase 1)
// Captura input do teclado/controle e traduz em comandos de movimento.
//
// Mapeamento:
//   A / ← = mover esquerda
//   D / → = mover direita
//   W / ↑ = pular (no chão) / nadar pra cima / voar pra cima
//   S / ↓ = agachar / nadar pra baixo / voar pra baixo
//   Shift = correr
//   Space = pular (também)
//   F = alternar voo (se equipado)
//
// O InputSystem é separado do MovementSystem para:
// 1. Permitir futuramente input por controle, toque, rede (multiplayer)
// 2. Manter a lógica de movimento puramente física (testável sem SDL)

#include <SDL2/SDL.h>
#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "engine/character_components.hpp"

namespace krono {

class InputSystem {
public:
    // Poll SDL events and update all CharacterController components
    void update(Registry& reg) {
        const Uint8* keystate = SDL_GetKeyboardState(nullptr);

        reg.each<CharacterController>([&](Entity e, CharacterController& ctrl) {
            // Movement
            ctrl.input_left = keystate[SDL_SCANCODE_A] || keystate[SDL_SCANCODE_LEFT];
            ctrl.input_right = keystate[SDL_SCANCODE_D] || keystate[SDL_SCANCODE_RIGHT];
            ctrl.input_up = keystate[SDL_SCANCODE_W] || keystate[SDL_SCANCODE_UP];
            ctrl.input_down = keystate[SDL_SCANCODE_S] || keystate[SDL_SCANCODE_DOWN];
            ctrl.input_run = keystate[SDL_SCANCODE_LSHIFT] || keystate[SDL_SCANCODE_RSHIFT];

            // Jump (edge-triggered — only on key press, not hold)
            // SDL_GetKeyboardState doesn't track edges, so we use SDL events for jump
            // The jump_pressed flag is set by process_event() below
        });
    }

    // Process a single SDL event (call for each event in the queue)
    void process_event(SDL_Event& e, Registry& reg) {
        if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            auto key = e.key.keysym.scancode;
            if (key == SDL_SCANCODE_SPACE || key == SDL_SCANCODE_W || key == SDL_SCANCODE_UP) {
                // Set jump_pressed on all entities with CharacterController
                // (in single-player, there's only one)
                reg.each<CharacterController>([&](Entity ent, CharacterController& ctrl) {
                    ctrl.input_jump_pressed = true;
                });
            }
        }
    }

    // Clear edge-triggered inputs at end of frame
    void end_frame(Registry& reg) {
        reg.each<CharacterController>([&](Entity e, CharacterController& ctrl) {
            ctrl.input_jump_pressed = false;
        });
    }
};

} // namespace krono
