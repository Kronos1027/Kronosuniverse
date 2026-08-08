#pragma once
// KronoUniverse — UI System (v0.4)
//
// Sistema completo de interface:
// - Main Menu (New Game, Continue, Settings, Quit)
// - Pause Menu (Resume, Save, Settings, Main Menu)
// - Settings (video, audio, controls)
// - Inventory Grid (visual slots)
// - Crafting Menu (recipe list)
// - Player Stats Panel
// - Death Screen
// - HUD overlays (health, XP, hotbar, minimap)
// - Mouse cursor + click detection
// - Button hover/press states
// - Text rendering using bitmap font

#include "engine/ecs.hpp"
#include "engine/components.hpp"
#include "game/inventory.hpp"
#include "game/crafting_system.hpp"
#include "game/combat_system.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <vector>
#include <string>
#include <functional>
#include <cstdint>
#include <cmath>

namespace krono {

// ---- Bitmap font (5x7 pixel characters) ----
// Each character is 5 wide x 7 tall, 1px spacing
// We render via draw_rect calls — no texture needed
class BitmapFont {
public:
    static constexpr int CHAR_W = 5;
    static constexpr int CHAR_H = 7;
    static constexpr int SCALE = 2; // pixels per font pixel

    // Simple 5x7 font for A-Z, 0-9, and basic punctuation
    // Stored as 7 rows of 5 bits each
    static const uint8_t FONT_DATA[][7];

    static int text_width(const std::string& text) {
        return (int)text.size() * (CHAR_W + 1) * SCALE;
    }

    static int text_height() {
        return CHAR_H * SCALE;
    }

    // Draw text using renderer's draw_rect
    template<typename Renderer>
    static void draw(Renderer& r, float x, float y, const std::string& text,
                     float cr = 1.0f, float cg = 1.0f, float cb = 1.0f, float ca = 1.0f) {
        float cx = x;
        for (char c : text) {
            int idx = char_index(c);
            if (idx >= 0) {
                const uint8_t* glyph = FONT_DATA[idx];
                for (int row = 0; row < CHAR_H; row++) {
                    uint8_t bits = glyph[row];
                    for (int col = 0; col < CHAR_W; col++) {
                        if (bits & (1 << (4 - col))) {
                            r.draw_rect(cx + col * SCALE, y + row * SCALE, SCALE, SCALE, cr, cg, cb, ca);
                        }
                    }
                }
            }
            cx += (CHAR_W + 1) * SCALE;
        }
    }

    static int char_index(char c) {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a';  // lowercase maps to uppercase
        if (c >= '0' && c <= '9') return 26 + (c - '0');
        switch (c) {
            case ' ': return 36;
            case '!': return 37;
            case '?': return 38;
            case '.': return 39;
            case ',': return 40;
            case ':': return 41;
            case ';': return 42;
            case '-': return 43;
            case '+': return 44;
            case '/': return 45;
            case '*': return 46;
            case '=': return 47;
            case '(': return 48;
            case ')': return 49;
            case '<': return 50;
            case '>': return 51;
            case '[': return 52;
            case ']': return 53;
            case '_': return 54;
            case '|': return 55;
            case '#': return 56;
            case '%': return 57;
            case '@': return 58;
            case '$': return 59;
            case '&': return 60;
            case '^': return 61;
            case '~': return 62;
            case '`': return 63;
            case '\'': return 64;
            case '"': return 65;
            case '\\': return 66;
            default: return 36;  // space for unknown
        }
    }
};

// ---- UI Element ----
struct UIElement {
    enum Type : uint8_t {
        BUTTON,
        PANEL,
        LABEL,
        IMAGE,
        SLIDER,
        TEXT_INPUT,
        PROGRESS_BAR,
        INVENTORY_SLOT,
        HOTBAR_SLOT,
    };
    Type type = BUTTON;
    float x = 0, y = 0, w = 0, h = 0;
    std::string text;
    std::string tooltip;
    float r = 0.5f, g = 0.5f, b = 0.5f, a = 1.0f;     // background
    float tr = 1.0f, tg = 1.0f, tb = 1.0f, ta = 1.0f;  // text
    float br = 0.3f, bg = 0.5f, bb = 0.8f, ba = 0.0f;  // border (ba=0 means no border)
    float hover_r = 0.7f, hover_g = 0.7f, hover_b = 0.8f;  // hover color
    bool hovered = false;
    bool pressed = false;
    bool visible = true;
    bool enabled = true;
    int id = 0;
    // For slider
    float slider_value = 0.5f;
    float slider_min = 0, slider_max = 1;
    // For inventory slot
    uint16_t item_id = 0;
    int item_count = 0;
    // For image/slot
    float img_r = 1, img_g = 1, img_b = 1;
    // Click callback
    std::function<void()> on_click;

    bool contains(float mx, float my) const {
        return mx >= x && mx <= x + w && my >= y && my <= y + h;
    }
};

// ---- UI Manager ----
class UIManager {
public:
    std::vector<UIElement> elements;
    int next_id = 1;

    enum ScreenState : uint8_t {
        STATE_MAIN_MENU,
        STATE_SETTINGS,
        STATE_PLAYING,
        STATE_PAUSED,
        STATE_INVENTORY,
        STATE_CRAFTING,
        STATE_STATS,
        STATE_DEATH,
        STATE_NEW_GAME,
    };

    ScreenState state = STATE_MAIN_MENU;
    ScreenState prev_state = STATE_MAIN_MENU;

    int mouse_x = 0, mouse_y = 0;
    bool mouse_left_pressed = false;
    bool mouse_left_clicked = false;  // edge-triggered
    int selected_index = 0;  // for keyboard navigation

    // Add element
    int add(const UIElement& e) {
        UIElement copy = e;
        copy.id = next_id++;
        elements.push_back(copy);
        return copy.id;
    }

    int add_button(float x, float y, float w, float h, const std::string& text,
                   std::function<void()> on_click = nullptr,
                   float r = 0.15f, float g = 0.18f, float b = 0.22f) {
        UIElement e;
        e.type = UIElement::BUTTON;
        e.x = x; e.y = y; e.w = w; e.h = h;
        e.text = text;
        e.r = r; e.g = g; e.b = b; e.a = 0.95f;
        e.br = 0.3f; e.bg = 0.5f; e.bb = 0.8f; e.ba = 1.0f;
        e.on_click = on_click;
        return add(e);
    }

    int add_panel(float x, float y, float w, float h,
                  float r = 0.05f, float g = 0.06f, float b = 0.08f, float a = 0.95f) {
        UIElement e;
        e.type = UIElement::PANEL;
        e.x = x; e.y = y; e.w = w; e.h = h;
        e.r = r; e.g = g; e.b = b; e.a = a;
        e.br = 0.3f; e.bg = 0.5f; e.bb = 0.8f; e.ba = 1.0f;
        return add(e);
    }

    int add_label(float x, float y, const std::string& text,
                  float tr = 1.0f, float tg = 1.0f, float tb = 1.0f, float ta = 1.0f) {
        UIElement e;
        e.type = UIElement::LABEL;
        e.x = x; e.y = y;
        e.w = (float)BitmapFont::text_width(text);
        e.h = (float)BitmapFont::text_height();
        e.text = text;
        e.tr = tr; e.tg = tg; e.tb = tb; e.ta = ta;
        return add(e);
    }

    int add_progress_bar(float x, float y, float w, float h, float value,
                         float r = 0.2f, float g = 0.7f, float b = 0.2f) {
        UIElement e;
        e.type = UIElement::PROGRESS_BAR;
        e.x = x; e.y = y; e.w = w; e.h = h;
        e.slider_value = value;
        e.r = 0.1f; e.g = 0.1f; e.b = 0.12f; e.a = 0.9f;
        e.br = r; e.bg = g; e.bb = b; e.ba = 1.0f;
        return add(e);
    }

    int add_inventory_slot(float x, float y, float size, uint16_t item_id = 0, int count = 0) {
        UIElement e;
        e.type = UIElement::INVENTORY_SLOT;
        e.x = x; e.y = y; e.w = size; e.h = size;
        e.r = 0.1f; e.g = 0.12f; e.b = 0.15f; e.a = 0.95f;
        e.br = 0.4f; e.bg = 0.4f; e.bb = 0.5f; e.ba = 1.0f;
        e.item_id = item_id;
        e.item_count = count;
        return add(e);
    }

    void clear() { elements.clear(); }

    // Update hover states and clicks
    void update() {
        mouse_left_clicked = false;
        for (auto& e : elements) {
            if (!e.visible) continue;
            e.hovered = e.enabled && e.contains((float)mouse_x, (float)mouse_y);
            if (e.hovered && mouse_left_pressed && e.on_click) {
                e.on_click();
            }
        }
        mouse_left_pressed = false;
    }

    // Render all visible elements
    template<typename Renderer>
    void render(Renderer& r) {
        for (auto& e : elements) {
            if (!e.visible) continue;
            float cr = e.r, cg = e.g, cb = e.b;
            if (e.hovered && e.enabled) {
                cr = e.hover_r; cg = e.hover_g; cb = e.hover_b;
            }
            if (!e.enabled) {
                cr *= 0.5f; cg *= 0.5f; cb *= 0.5f;
            }

            // Background
            r.draw_rect(e.x, e.y, e.w, e.h, cr, cg, cb, e.a);

            // Border
            if (e.ba > 0) {
                // Top
                r.draw_rect(e.x, e.y, e.w, 2, e.br, e.bg, e.bb, e.ba);
                // Bottom
                r.draw_rect(e.x, e.y + e.h - 2, e.w, 2, e.br, e.bg, e.bb, e.ba);
                // Left
                r.draw_rect(e.x, e.y, 2, e.h, e.br, e.bg, e.bb, e.ba);
                // Right
                r.draw_rect(e.x + e.w - 2, e.y, 2, e.h, e.br, e.bg, e.bb, e.ba);
            }

            // Type-specific rendering
            switch (e.type) {
                case UIElement::BUTTON: {
                    // Center text
                    float tw = BitmapFont::text_width(e.text);
                    float tx = e.x + (e.w - tw) / 2;
                    float ty = e.y + (e.h - BitmapFont::text_height()) / 2;
                    BitmapFont::draw(r, tx, ty, e.text, e.tr, e.tg, e.tb, e.ta);
                    break;
                }
                case UIElement::LABEL: {
                    BitmapFont::draw(r, e.x, e.y, e.text, e.tr, e.tg, e.tb, e.ta);
                    break;
                }
                case UIElement::PROGRESS_BAR: {
                    float val = std::max(0.0f, std::min(1.0f, e.slider_value));
                    r.draw_rect(e.x + 2, e.y + 2, (e.w - 4) * val, e.h - 4,
                               e.br, e.bg, e.bb, e.ba);
                    break;
                }
                case UIElement::INVENTORY_SLOT: {
                    if (e.item_id != 0) {
                        // Draw colored block representing item
                        // Color based on item id high byte (category)
                        uint8_t cat = (e.item_id >> 8) & 0xFF;
                        float ir = ((cat * 37) & 0xFF) / 255.0f;
                        float ig = ((cat * 73) & 0xFF) / 255.0f;
                        float ib = ((cat * 149) & 0xFF) / 255.0f;
                        r.draw_rect(e.x + 4, e.y + 4, e.w - 8, e.h - 8, ir, ig, ib, 1.0f);
                        // Count badge
                        if (e.item_count > 1) {
                            std::string cnt = std::to_string(e.item_count);
                            float bw = BitmapFont::text_width(cnt);
                            r.draw_rect(e.x + e.w - bw - 6, e.y + e.h - 12, bw + 4, 10,
                                       0, 0, 0, 0.8f);
                            BitmapFont::draw(r, e.x + e.w - bw - 4, e.y + e.h - 11, cnt,
                                           1.0f, 1.0f, 1.0f);
                        }
                    }
                    // Selection highlight
                    if (e.hovered) {
                        r.draw_rect(e.x, e.y, e.w, 2, 1.0f, 0.85f, 0.2f);
                        r.draw_rect(e.x, e.y + e.h - 2, e.w, 2, 1.0f, 0.85f, 0.2f);
                        r.draw_rect(e.x, e.y, 2, e.h, 1.0f, 0.85f, 0.2f);
                        r.draw_rect(e.x + e.w - 2, e.y, 2, e.h, 1.0f, 0.85f, 0.2f);
                    }
                    break;
                }
                default: break;
            }
        }
    }
};

// ---- Define font data (5x7 bitmap) ----
// 26 letters + 10 digits + space + 30 punctuation = 67 chars
inline const uint8_t BitmapFont::FONT_DATA[][7] = {
    // A
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    // B
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
    // C
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
    // D
    {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C},
    // E
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
    // F
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
    // G
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F},
    // H
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    // I
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
    // J
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C},
    // K
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
    // L
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
    // M
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
    // N
    {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11},
    // O
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    // P
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    // Q
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
    // R
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    // S
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},
    // T
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    // U
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    // V
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
    // W
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A},
    // X
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
    // Y
    {0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04},
    // Z
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},
    // 0
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    // 1
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    // 2
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
    // 3
    {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},
    // 4
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    // 5
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    // 6
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    // 7
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    // 8
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    // 9
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
    // space
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // !
    {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04},
    // ?
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04},
    // .
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04},
    // ,
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x08},
    // :
    {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00},
    // ;
    {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x08},
    // -
    {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},
    // +
    {0x00, 0x04, 0x04, 0x0E, 0x04, 0x04, 0x00},
    // /
    {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10},
    // *
    {0x00, 0x11, 0x0A, 0x1F, 0x0A, 0x11, 0x00},
    // =
    {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00},
    // (
    {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02},
    // )
    {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08},
    // <
    {0x01, 0x02, 0x04, 0x08, 0x04, 0x02, 0x01},
    // >
    {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08},
    // [
    {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E},
    // ]
    {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E},
    // _
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F},
    // |
    {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    // #
    {0x0A, 0x1F, 0x0A, 0x0A, 0x1F, 0x0A, 0x00},
    // %
    {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03},
    // @
    {0x0E, 0x11, 0x15, 0x15, 0x18, 0x10, 0x0E},
    // $
    {0x04, 0x1E, 0x15, 0x1E, 0x15, 0x1E, 0x04},
    // &
    {0x0C, 0x12, 0x10, 0x0C, 0x12, 0x11, 0x0E},
    // ^
    {0x04, 0x0A, 0x11, 0x00, 0x00, 0x00, 0x00},
    // ~
    {0x00, 0x00, 0x08, 0x1F, 0x02, 0x00, 0x00},
    // `
    {0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00},
    // '
    {0x04, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00},
    // "
    {0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00},
    // \
    {0x10, 0x08, 0x08, 0x04, 0x02, 0x02, 0x01},
};

} // namespace krono
