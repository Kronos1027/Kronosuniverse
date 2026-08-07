#pragma once
// KronoUniverse — Physics Engine (Parte 5)
// Sistema de física de corpo rígido 2D simplificado.
//
// Função central: aplicar_impulso() (Parte 5.7 — usada por TODO efeito físico)
//
// Sistemas:
// - PhysicsSystem: gravidade, integração de velocidade, colisão AABB
// - CollisionSystem: detecção e resolução de colisão
// - ImpulseSystem: aplicação de forças/impulsos

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include <cmath>
#include <vector>

namespace krono {

// ---- Impulse (evento de força a ser aplicado) ----
struct Impulse {
    Entity target;
    float force_x, force_y;
    float point_x, point_y; // point of application (for torque)
};

// ---- Physics System ----
class PhysicsSystem {
public:
    float gravity_x = 0.0f;
    float gravity_y = 9.81f; // default Earth-like (overridden per planet)
    float linear_damping = 0.01f; // air resistance (low in vacuum)

    // Queue de impulsos a aplicar no próximo step
    void add_impulse(Entity e, float fx, float fy, float px = 0, float py = 0) {
        impulses_.push_back({e, fx, fy, px, py});
    }

    // Step de simulação (fixed timestep)
    void update(Registry& reg, float dt) {
        // 1. Apply gravity
        reg.each<Position, Velocity, Mass, RigidBody>([&](Entity e, Position& pos, Velocity& vel, Mass& mass, RigidBody& rb) {
            if (rb.is_static) return;
            vel.x += gravity_x * dt;
            vel.y += gravity_y * dt;
        });

        // 2. Apply queued impulses (Parte 5.7 — função central)
        for (auto& imp : impulses_) {
            auto* vel = reg.get<Velocity>(imp.target);
            auto* mass = reg.get<Mass>(imp.target);
            auto* rb = reg.get<RigidBody>(imp.target);
            if (!vel || !mass || !rb || rb->is_static) continue;
            // F = ma → Δv = F/m * dt (but impulse is instantaneous, so Δv = F/m)
            vel->x += imp.force_x / mass->value;
            vel->y += imp.force_y / mass->value;
            // TODO: torque = cross product of (point - center_of_mass) and force
        }
        impulses_.clear();

        // 3. Apply linear damping
        reg.each<Velocity, RigidBody>([&](Entity e, Velocity& vel, RigidBody& rb) {
            if (rb.is_static) return;
            vel.x *= (1.0f - linear_damping * dt);
            vel.y *= (1.0f - linear_damping * dt);
        });

        // 4. Integrate velocity → position
        reg.each<Position, Velocity, RigidBody>([&](Entity e, Position& pos, Velocity& vel, RigidBody& rb) {
            if (rb.is_static) return;
            pos.x += vel.x * dt;
            pos.y += vel.y * dt;
        });

        // 5. Collision detection (AABB)
        resolve_collisions(reg);
    }

private:
    std::vector<Impulse> impulses_;

    void resolve_collisions(Registry& reg) {
        // Collect all dynamic colliders
        struct ColliderInfo {
            Entity e;
            float x, y, w, h;
            bool is_static;
        };
        std::vector<ColliderInfo> colliders;
        reg.each<Position, AABBCollider, RigidBody>([&](Entity e, Position& pos, AABBCollider& col, RigidBody& rb) {
            colliders.push_back({
                e,
                pos.x + col.offset_x,
                pos.y + col.offset_y,
                col.width,
                col.height,
                rb.is_static
            });
        });

        // O(n²) for now — will use spatial hash in Phase 2
        for (size_t i = 0; i < colliders.size(); i++) {
            for (size_t j = i + 1; j < colliders.size(); j++) {
                auto& a = colliders[i];
                auto& b = colliders[j];
                // AABB overlap test
                float dx = (a.x + a.w/2) - (b.x + b.w/2);
                float dy = (a.y + a.h/2) - (b.y + b.h/2);
                float overlap_x = (a.w + b.w) / 2 - std::abs(dx);
                float overlap_y = (a.h + b.h) / 2 - std::abs(dy);
                if (overlap_x > 0 && overlap_y > 0) {
                    // Collision! Resolve along axis of least penetration
                    if (overlap_x < overlap_y) {
                        // Resolve X
                        float sign = (dx > 0) ? 1 : -1;
                        resolve_pair(reg, a, b, sign * overlap_x, 0);
                    } else {
                        // Resolve Y
                        float sign = (dy > 0) ? 1 : -1;
                        resolve_pair(reg, a, b, 0, sign * overlap_y);
                    }
                }
            }
        }
    }

    void resolve_pair(Registry& reg, const auto& a, const auto& b, float push_x, float push_y) {
        auto* rb_a = reg.get<RigidBody>(a.e);
        auto* rb_b = reg.get<RigidBody>(b.e);
        auto* vel_a = reg.get<Velocity>(a.e);
        auto* vel_b = reg.get<Velocity>(b.e);
        auto* mass_a = reg.get<Mass>(a.e);
        auto* mass_b = reg.get<Mass>(b.e);
        if (!rb_a || !rb_b || !vel_a || !vel_b) return;

        // If both static, skip
        if (rb_a->is_static && rb_b->is_static) return;

        // Calculate push ratio based on mass
        float total_mass = mass_a->value + mass_b->value;
        float ratio_a = rb_a->is_static ? 0 : (rb_b->is_static ? 1 : mass_b->value / total_mass);
        float ratio_b = rb_b->is_static ? 0 : (rb_a->is_static ? 1 : mass_a->value / total_mass);

        auto* pos_a = reg.get<Position>(a.e);
        auto* pos_b = reg.get<Position>(b.e);
        if (pos_a) { pos_a->x += push_x * ratio_a; pos_a->y += push_y * ratio_a; }
        if (pos_b) { pos_b->x -= push_x * ratio_b; pos_b->y -= push_y * ratio_b; }

        // Bounce (simple — reflect velocity along collision normal)
        if (push_x != 0) {
            float rel_vel = vel_a->x - vel_b->x;
            if ((push_x > 0 && rel_vel < 0) || (push_x < 0 && rel_vel > 0)) {
                float restitution = std::min(rb_a->restitution, rb_b->restitution);
                float impulse = -(1 + restitution) * rel_vel / (1/mass_a->value + 1/mass_b->value);
                if (!rb_a->is_static) vel_a->x += impulse / mass_a->value;
                if (!rb_b->is_static) vel_b->x -= impulse / mass_b->value;
            }
        }
        if (push_y != 0) {
            float rel_vel = vel_a->y - vel_b->y;
            if ((push_y > 0 && rel_vel < 0) || (push_y < 0 && rel_vel > 0)) {
                float restitution = std::min(rb_a->restitution, rb_b->restitution);
                float impulse = -(1 + restitution) * rel_vel / (1/mass_a->value + 1/mass_b->value);
                if (!rb_a->is_static) vel_a->y += impulse / mass_a->value;
                if (!rb_b->is_static) vel_b->y -= impulse / mass_b->value;
            }
        }
    }
};

} // namespace krono
