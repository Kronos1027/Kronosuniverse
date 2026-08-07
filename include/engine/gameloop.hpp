#pragma once
// KronoUniverse — Game Loop (fixed timestep 60Hz + render interpolation)
// ADR-003: Simulação a 60Hz fixa, renderização a qualquer framerate.

#include <chrono>
#include <cstdint>
#include <functional>

namespace krono {

class GameLoop {
public:
    static constexpr double FIXED_DT = 1.0 / 60.0; // 60Hz simulation
    static constexpr double MAX_FRAME_TIME = 0.25; // prevent spiral of death

    using UpdateFn = std::function<void(double dt)>;
    using RenderFn = std::function<void(double alpha)>;

    GameLoop(UpdateFn update_fn, RenderFn render_fn)
        : update_(std::move(update_fn)), render_(std::move(render_fn)) {}

    void run() {
        running_ = true;
        auto current_time = clock::now();
        double accumulator = 0.0;

        while (running_) {
            auto new_time = clock::now();
            double frame_time = std::chrono::duration<double>(new_time - current_time).count();
            current_time = new_time;

            if (frame_time > MAX_FRAME_TIME) {
                frame_time = MAX_FRAME_TIME; // prevent spiral of death
            }

            accumulator += frame_time;

            // Fixed timestep updates
            while (accumulator >= FIXED_DT) {
                update_(FIXED_DT);
                accumulator -= FIXED_DT;
            }

            // Render with interpolation
            double alpha = accumulator / FIXED_DT;
            render_(alpha);
        }
    }

    void stop() { running_ = false; }

private:
    using clock = std::chrono::steady_clock;
    UpdateFn update_;
    RenderFn render_;
    bool running_ = false;
};

} // namespace krono
