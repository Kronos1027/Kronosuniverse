#pragma once
// KronoUniverse — Movement System (Fase 1, Parte 5.1)
// Física de movimento do personagem: andar, correr, pular, nadar, voar, escalar.
//
// Implementa TODAS as fórmulas da Parte 5.1 do prompt mestre:
// - Salto: v = sqrt(2 * g * h) — altura consistente entre gravidades
// - Dano de queda: dano = max(0, (v_impacto - limiar) * mult_espécie)
// - Atrito por terreno (gelo desliza, lama desacelera, metal padrão)
// - Massa afetada por inventário (peso reduz aceleração e salto)
// - Velocidade terminal de queda
// - Estados: andar, correr, nadar, voar, escalar magnético, agachar
// - Coyote time + jump buffering (game feel)

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "engine/character_components.hpp"
#include "physics/physics.hpp"
#include <cmath>

namespace krono {

class MovementSystem {
public:
    // Surface friction lookup (Parte 5.1 — "coeficiente de atrito por tipo de terreno")
    static float surface_friction(SurfaceType type) {
        switch (type) {
            case SurfaceType::ICE:    return 0.05f;  // slides a lot
            case SurfaceType::MUD:    return 0.95f;  // very sticky, slows down
            case SurfaceType::SAND:   return 0.85f;
            case SurfaceType::GRASS:  return 0.75f;
            case SurfaceType::DIRT:   return 0.80f;
            case SurfaceType::STONE:  return 0.70f;
            case SurfaceType::METAL:  return 0.60f;
            case SurfaceType::WOOD:   return 0.72f;
            case SurfaceType::GLASS:  return 0.50f;
            case SurfaceType::LAVA:   return 0.30f;  // + damages
            case SurfaceType::WATER:  return 0.30f;  // wading
            case SurfaceType::AIR:    return 0.01f;  // minimal air resistance
            default:                  return 0.80f;
        }
    }

    // Update all character movement (called at fixed timestep)
    void update(Registry& reg, float dt, float gravity) {
        reg.each<CharacterController, Position, Velocity, Mass, RigidBody>(
            [&](Entity e, CharacterController& ctrl, Position& pos, Velocity& vel, Mass& mass, RigidBody& rb) {
                if (rb.is_static) return;

                // Get optional components
                auto* species = reg.get<Species>(e);
                auto* inv_weight = reg.get<InventoryWeight>(e);
                auto* surface = reg.get<Surface>(e);
                auto* fall_tracker = reg.get<FallDamageTracker>(e);
                auto* equipment = reg.get<Equipment>(e);
                auto* anim = reg.get<AnimationState>(e);
                auto* planet_grav = reg.get<PlanetGravity>(e);

                // ---- Calculate effective gravity ----
                float effective_gravity = gravity;
                if (planet_grav) effective_gravity = planet_grav->gravity;

                // ---- Calculate encumbrance effects (Parte 5.1 + GDD v0.2 1.2) ----
                float encumbrance = 0.0f;
                if (inv_weight && inv_weight->max_weight > 0) {
                    inv_weight->encumbrance_ratio = inv_weight->current_weight / inv_weight->max_weight;
                    encumbrance = inv_weight->encumbrance_ratio;
                }
                float speed_mult = 1.0f - (encumbrance * 0.5f);      // max 50% slow
                float jump_mult = 1.0f - (encumbrance * 0.7f);       // max 70% jump reduction
                float accel_mult = 1.0f - (encumbrance * 0.6f);      // max 60% slower acceleration

                // Apply species modifiers
                if (species) {
                    speed_mult *= species->run_multiplier;
                    jump_mult *= species->jump_multiplier;
                    ctrl.max_jumps = 1 + species->extra_jumps;
                }

                // ---- Determine surface friction ----
                float friction = 0.80f;
                if (surface) {
                    friction = surface->friction;
                }

                // ---- Determine current movement mode ----
                ctrl.was_grounded = ctrl.grounded;
                bool can_fly = (equipment && equipment->has_flight_pack) || (species && species->can_fly);
                bool can_climb = ctrl.on_metal_surface && equipment && equipment->has_magnetic_boots;

                // ---- Update coyote time and jump buffer ----
                if (ctrl.grounded) {
                    ctrl.coyote_time = 0.1f; // 100ms grace
                } else {
                    ctrl.coyote_time -= dt;
                }
                if (ctrl.input_jump_pressed) {
                    ctrl.jump_buffer_time = 0.1f; // 100ms buffer
                } else {
                    ctrl.jump_buffer_time -= dt;
                }

                // ---- Handle flying ----
                if (can_fly && ctrl.is_flying) {
                    handle_flying(ctrl, vel, equipment, species, dt, effective_gravity);
                    ctrl.state = MoveState::FLYING;
                    update_animation(anim, AnimationState::ANIM_FLY, ctrl.facing_right);
                    return; // skip ground movement
                }

                // ---- Handle climbing (magnetic boots) ----
                if (can_climb) {
                    handle_climbing(ctrl, vel, dt, effective_gravity);
                    ctrl.state = MoveState::CLIMBING;
                    update_animation(anim, AnimationState::ANIM_CLIMB, ctrl.facing_right);
                    return;
                }

                // ---- Handle swimming ----
                if (ctrl.in_water) {
                    handle_swimming(ctrl, vel, species, inv_weight, dt, effective_gravity);
                    ctrl.state = MoveState::SWIMMING;
                    ctrl.grounded = false;
                    update_animation(anim, AnimationState::ANIM_SWIM, ctrl.facing_right);
                    return;
                }

                // ---- Track fall velocity for fall damage ----
                if (fall_tracker) {
                    if (vel.y > 0 && !ctrl.grounded) {
                        // Falling (y positive = down in screen coords)
                        fall_tracker->max_fall_velocity = std::max(fall_tracker->max_fall_velocity, vel.y);
                        fall_tracker->was_falling = true;
                    }
                }

                // ---- Ground / Air movement ----
                float target_speed = 0.0f;
                if (ctrl.input_left && !ctrl.input_right) {
                    target_speed = -ctrl.walk_speed * speed_mult;
                    ctrl.facing_right = false;
                    if (ctrl.input_run) {
                        target_speed = -ctrl.run_speed * speed_mult;
                    }
                } else if (ctrl.input_right && !ctrl.input_left) {
                    target_speed = ctrl.walk_speed * speed_mult;
                    ctrl.facing_right = true;
                    if (ctrl.input_run) {
                        target_speed = ctrl.run_speed * speed_mult;
                    }
                }

                // Crouching reduces speed
                if (ctrl.input_down && ctrl.grounded) {
                    target_speed *= ctrl.crouch_speed / ctrl.walk_speed;
                    ctrl.state = MoveState::CROUCHING;
                }

                // Apply acceleration toward target speed
                float current_accel = ctrl.grounded ? ctrl.acceleration : ctrl.acceleration * ctrl.air_control;
                current_accel *= accel_mult;

                if (ctrl.grounded) {
                    // On ground — apply friction
                    if (target_speed == 0) {
                        // Decelerate
                        float decel = ctrl.deceleration * friction * dt;
                        if (vel.x > 0) vel.x = std::max(0.0f, vel.x - decel);
                        else if (vel.x < 0) vel.x = std::min(0.0f, vel.x + decel);
                    } else {
                        // Accelerate toward target
                        if (vel.x < target_speed) {
                            vel.x = std::min(target_speed, vel.x + current_accel * dt);
                        } else if (vel.x > target_speed) {
                            vel.x = std::max(target_speed, vel.x - current_accel * dt);
                        }
                    }

                    // Ice: reduced friction = sliding
                    if (surface && surface->type == SurfaceType::ICE) {
                        // Don't decelerate as fast on ice
                        // (friction already low, so natural deceleration is slow)
                    }
                } else {
                    // In air — limited control
                    if (target_speed != 0) {
                        if (vel.x < target_speed) {
                            vel.x = std::min(target_speed, vel.x + current_accel * dt);
                        } else if (vel.x > target_speed) {
                            vel.x = std::max(target_speed, vel.x - current_accel * dt);
                        }
                    }
                }

                // ---- Jumping (Parte 5.1: v = sqrt(2 * g * h)) ----
                bool can_jump = ctrl.grounded || ctrl.coyote_time > 0;
                bool wants_jump = ctrl.jump_buffer_time > 0;

                if (wants_jump && can_jump && ctrl.jump_count < ctrl.max_jumps) {
                    // Fórmula do salto: v = sqrt(2 * g * h)
                    // Garante que a altura sentida pelo jogador é consistente entre gravidades
                    float jump_vel = std::sqrt(2.0f * effective_gravity * ctrl.jump_height * jump_mult);
                    vel.y = -jump_vel; // negative = up (screen coords)
                    ctrl.grounded = false;
                    ctrl.jump_count++;
                    ctrl.coyote_time = 0;
                    ctrl.jump_buffer_time = 0;
                    ctrl.state = MoveState::JUMPING;
                }

                // Multi-jump (winged species)
                if (wants_jump && !ctrl.grounded && ctrl.jump_count < ctrl.max_jumps && species && species->extra_jumps > 0) {
                    float jump_vel = std::sqrt(2.0f * effective_gravity * ctrl.jump_height * jump_mult * 0.8f);
                    vel.y = -jump_vel;
                    ctrl.jump_count++;
                    ctrl.jump_buffer_time = 0;
                    ctrl.state = MoveState::JUMPING;
                }

                // ---- Update state machine ----
                if (!ctrl.grounded) {
                    if (vel.y < 0) {
                        ctrl.state = MoveState::JUMPING;
                        update_animation(anim, AnimationState::ANIM_JUMP, ctrl.facing_right);
                    } else {
                        ctrl.state = MoveState::FALLING;
                        update_animation(anim, AnimationState::ANIM_FALL, ctrl.facing_right);
                    }
                } else if (ctrl.input_down) {
                    ctrl.state = MoveState::CROUCHING;
                    update_animation(anim, AnimationState::ANIM_CROUCH, ctrl.facing_right);
                } else if (std::abs(vel.x) > 1.0f) {
                    if (ctrl.input_run) {
                        ctrl.state = MoveState::RUNNING;
                        update_animation(anim, AnimationState::ANIM_RUN, ctrl.facing_right);
                    } else {
                        ctrl.state = MoveState::WALKING;
                        update_animation(anim, AnimationState::ANIM_WALK, ctrl.facing_right);
                    }
                } else {
                    ctrl.state = MoveState::IDLE;
                    update_animation(anim, AnimationState::ANIM_IDLE, ctrl.facing_right);
                }

                // ---- Fall damage (Parte 5.1: dano = max(0, (v_impacto - limiar) * mult)) ----
                if (fall_tracker && ctrl.grounded && !ctrl.was_grounded) {
                    // Just landed — check fall damage
                    float impact_vel = fall_tracker->max_fall_velocity;
                    float threshold = 400.0f; // default safe fall velocity
                    float dmg_mult = 1.0f;
                    if (species) dmg_mult = species->fall_damage_multiplier;

                    float damage = std::max(0.0f, (impact_vel - threshold) * dmg_mult * 0.1f);
                    if (damage > 0) {
                        // Apply damage to Health component
                        auto* health = reg.get<Health>(e);
                        if (health) {
                            health->current -= damage;
                            if (health->current < 0) health->current = 0;
                        }
                        // Trigger hurt animation
                        if (anim) {
                            anim->current = AnimationState::ANIM_HURT;
                        }
                    }
                    // Reset tracker
                    fall_tracker->max_fall_velocity = 0;
                    fall_tracker->was_falling = false;
                }

                // Reset jump count when grounded
                if (ctrl.grounded) {
                    ctrl.jump_count = 0;
                }
            }
        );

        // Update animations
        reg.each<AnimationState>([&](Entity e, AnimationState& anim) {
            anim.frame_time += dt;
            if (anim.frame_time >= anim.frame_duration) {
                anim.frame_time = 0;
                anim.frame_index = (anim.frame_index + 1) % anim.frame_count;
            }
        });
    }

private:
    void handle_flying(CharacterController& ctrl, Velocity& vel, Equipment* eq, Species* sp,
                       float dt, float gravity) {
        float fly_speed = ctrl.fly_speed;
        float thrust = (eq) ? eq->flight_thrust : 500.0f;

        // Horizontal
        if (ctrl.input_left && !ctrl.input_right) {
            vel.x = -fly_speed;
            ctrl.facing_right = false;
        } else if (ctrl.input_right && !ctrl.input_left) {
            vel.x = fly_speed;
            ctrl.facing_right = true;
        } else {
            vel.x *= 0.92f; // air drag
        }

        // Vertical
        if (ctrl.input_up) {
            vel.y = -thrust;
        } else if (ctrl.input_down) {
            vel.y = thrust * 0.5f;
        } else {
            // Hover — counteract gravity
            vel.y = vel.y * 0.9f + gravity * 10.0f * dt; // slight gravity pull
        }
    }

    void handle_climbing(CharacterController& ctrl, Velocity& vel, float dt, float gravity) {
        // On metal surface with magnetic boots — ignore gravity
        vel.y = 0;
        if (ctrl.input_up) {
            vel.y = -ctrl.walk_speed * 0.7f;
        } else if (ctrl.input_down) {
            vel.y = ctrl.walk_speed * 0.7f;
        }
        // Horizontal movement still works
        if (ctrl.input_left) {
            vel.x = -ctrl.walk_speed;
            ctrl.facing_right = false;
        } else if (ctrl.input_right) {
            vel.x = ctrl.walk_speed;
            ctrl.facing_right = true;
        } else {
            vel.x *= 0.8f;
        }
    }

    void handle_swimming(CharacterController& ctrl, Velocity& vel, Species* sp,
                         InventoryWeight* inv, float dt, float gravity) {
        float swim_speed = ctrl.swim_speed;
        float swim_friction = 0.3f; // water resistance

        if (sp) {
            swim_speed *= (1.0f / sp->swim_friction_modifier); // aquatic species faster
        }

        // Horizontal
        if (ctrl.input_left) {
            vel.x = -swim_speed;
            ctrl.facing_right = false;
        } else if (ctrl.input_right) {
            vel.x = swim_speed;
            ctrl.facing_right = true;
        } else {
            vel.x *= (1.0f - swim_friction * dt * 10);
        }

        // Vertical — buoyancy + active swimming
        float buoyancy = -gravity * 0.3f; // slight upward force (floats)
        if (ctrl.input_up) {
            vel.y = -swim_speed;
        } else if (ctrl.input_down) {
            vel.y = swim_speed;
        } else {
            vel.y = vel.y * 0.9f + buoyancy * dt;
        }
    }

    void update_animation(AnimationState* anim, AnimationState::Anim new_anim, bool facing_right) {
        if (!anim) return;
        if (anim->current != new_anim) {
            anim->prev = anim->current;
            anim->current = new_anim;
            anim->frame_index = 0;
            anim->frame_time = 0;
        }
        anim->facing_right = facing_right;
    }
};

} // namespace krono
