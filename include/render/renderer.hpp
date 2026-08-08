#pragma once
// KronoUniverse — Renderer (SDL2 + OpenGL Compatibility Profile)
// Usa OpenGL Compatibility Profile para suportar immediate mode (glBegin/glEnd)
// que é mais simples para MVP e funciona em praticamente qualquer GPU.

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <string>

namespace krono {

class Renderer {
public:
    bool init(int width, int height, const char* title) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
            SDL_Log("SDL_Init failed: %s", SDL_GetError());
            return false;
        }

        // Use Compatibility Profile (supports glBegin/glEnd)
        // This is CRITICAL — Core Profile 3.3 removed immediate mode
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);

        window_ = SDL_CreateWindow(
            title,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height,
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
        );
        if (!window_) {
            SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
            return false;
        }

        gl_context_ = SDL_GL_CreateContext(window_);
        if (!gl_context_) {
            SDL_Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
            return false;
        }

        // Enable VSync
        SDL_GL_SetSwapInterval(1);

        // Set viewport
        glViewport(0, 0, width, height);

        // Clear color to deep-tech background
        glClearColor(0.04f, 0.043f, 0.059f, 1.0f); // #0A0B0F

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST); // 2D — no depth needed

        screen_w_ = width;
        screen_h_ = height;

        SDL_Log("Renderer initialized: %dx%d, OpenGL compatibility profile", width, height);
        return true;
    }

    void clear() {
        glClear(GL_COLOR_BUFFER_BIT);
    }

    void present() {
        SDL_GL_SwapWindow(window_);
    }

    void draw_rect(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f) {
        glColor4f(r, g, b, a);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);
        glEnd();
    }

    void set_ortho(float left, float right, float bottom, float top) {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(left, right, bottom, top, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    void set_screen_ortho() {
        set_ortho(0, screen_w_, screen_h_, 0);
    }

    int screen_width() const { return screen_w_; }
    int screen_height() const { return screen_h_; }

    bool should_quit() const { return should_quit_; }
    void set_quit() { should_quit_ = true; }

    SDL_Window* window() { return window_; }

    void shutdown() {
        if (gl_context_) { SDL_GL_DeleteContext(gl_context_); gl_context_ = nullptr; }
        if (window_) { SDL_DestroyWindow(window_); window_ = nullptr; }
        SDL_Quit();
    }

    ~Renderer() { shutdown(); }

private:
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    bool should_quit_ = false;
    int screen_w_ = 1280;
    int screen_h_ = 720;
};

} // namespace krono
