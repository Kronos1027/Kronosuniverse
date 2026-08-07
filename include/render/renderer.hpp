#pragma once
// KronoUniverse — Renderer (SDL2 + OpenGL 3.3)
// Renderização pixel art com suporte a atlas de texturas.

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <string>
#include <unordered_map>

namespace krono {

class Renderer {
public:
    bool init(int width, int height, const char* title) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
            return false;
        }
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);

        window_ = SDL_CreateWindow(
            title,
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            width, height,
            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
        );
        if (!window_) return false;

        gl_context_ = SDL_GL_CreateContext(window_);
        if (!gl_context_) return false;

        SDL_GL_SetSwapInterval(1);
        glViewport(0, 0, width, height);
        glClearColor(0.04f, 0.043f, 0.059f, 1.0f); // #0A0B0F
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        return true;
    }

    void clear() { glClear(GL_COLOR_BUFFER_BIT); }
    void present() { SDL_GL_SwapWindow(window_); }

    void draw_rect(float x, float y, float w, float h, float r, float g, float b, float a = 1.0f) {
        glColor4f(r, g, b, a);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);
        glEnd();
    }

    void set_ortho(int width, int height) {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, width, height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
    }

    void poll_events() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) should_quit_ = true;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) should_quit_ = true;
        }
    }

    bool should_quit() const { return should_quit_; }

    void shutdown() {
        if (gl_context_) SDL_GL_DeleteContext(gl_context_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    ~Renderer() { shutdown(); }

private:
    SDL_Window* window_ = nullptr;
    SDL_GLContext gl_context_ = nullptr;
    bool should_quit_ = false;
};

} // namespace krono
