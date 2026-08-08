// KronoUniverse — UI System Tests (v0.4)

#include "game/ui_system.hpp"
#include <cassert>
#include <iostream>

using namespace krono;

static int tests_passed = 0;
static int tests_total = 0;
#define TEST(name) tests_total++;
#define ENDTEST() tests_passed++; std::cout << "[OK] " << __func__ << std::endl;

static void test_font_text_width() {
    TEST("font text width calculates correctly");
    int w = BitmapFont::text_width("HELLO");
    assert(w > 0);
    assert(w == 5 * (5 + 1) * 2);  // 5 chars * 6 pixels * 2 scale = 60
    ENDTEST();
}

static void test_font_text_height() {
    TEST("font text height is 14");
    int h = BitmapFont::text_height();
    assert(h == 7 * 2);  // 7 pixels * 2 scale = 14
    ENDTEST();
}

static void test_font_char_index_letters() {
    TEST("font char index handles letters");
    assert(BitmapFont::char_index('A') == 0);
    assert(BitmapFont::char_index('Z') == 25);
    assert(BitmapFont::char_index('a') == 0);  // lowercase maps to uppercase
    assert(BitmapFont::char_index('z') == 25);
    ENDTEST();
}

static void test_font_char_index_digits() {
    TEST("font char index handles digits");
    assert(BitmapFont::char_index('0') == 26);
    assert(BitmapFont::char_index('9') == 35);
    ENDTEST();
}

static void test_font_char_index_punctuation() {
    TEST("font char index handles punctuation");
    assert(BitmapFont::char_index(' ') == 36);
    assert(BitmapFont::char_index('!') == 37);
    assert(BitmapFont::char_index('?') == 38);
    assert(BitmapFont::char_index('.') == 39);
    ENDTEST();
}

static void test_font_char_index_unknown() {
    TEST("font char index handles unknown as space");
    assert(BitmapFont::char_index('\t') == 36);
    assert(BitmapFont::char_index('\n') == 36);
    assert(BitmapFont::char_index((char)200) == 36);
    ENDTEST();
}

static void test_ui_element_add_button() {
    TEST("UIManager adds button");
    UIManager ui;
    int id = ui.add_button(100, 100, 200, 40, "CLICK ME");
    assert(id > 0);
    assert(ui.elements.size() == 1);
    assert(ui.elements[0].type == UIElement::BUTTON);
    assert(ui.elements[0].text == "CLICK ME");
    ENDTEST();
}

static void test_ui_element_add_panel() {
    TEST("UIManager adds panel");
    UIManager ui;
    int id = ui.add_panel(50, 50, 400, 300);
    assert(id > 0);
    assert(ui.elements[0].type == UIElement::PANEL);
    ENDTEST();
}

static void test_ui_element_add_label() {
    TEST("UIManager adds label");
    UIManager ui;
    int id = ui.add_label(10, 10, "HELLO WORLD");
    assert(id > 0);
    assert(ui.elements[0].type == UIElement::LABEL);
    ENDTEST();
}

static void test_ui_element_add_progress_bar() {
    TEST("UIManager adds progress bar");
    UIManager ui;
    int id = ui.add_progress_bar(10, 10, 200, 16, 0.5f);
    assert(id > 0);
    assert(ui.elements[0].type == UIElement::PROGRESS_BAR);
    assert(ui.elements[0].slider_value == 0.5f);
    ENDTEST();
}

static void test_ui_element_add_inventory_slot() {
    TEST("UIManager adds inventory slot");
    UIManager ui;
    int id = ui.add_inventory_slot(10, 10, 40, 0x0101, 4);
    assert(id > 0);
    assert(ui.elements[0].type == UIElement::INVENTORY_SLOT);
    assert(ui.elements[0].item_id == 0x0101);
    assert(ui.elements[0].item_count == 4);
    ENDTEST();
}

static void test_ui_element_contains() {
    TEST("UIElement contains detects point");
    UIElement e;
    e.x = 100; e.y = 100; e.w = 200; e.h = 40;
    assert(e.contains(150, 120));
    assert(e.contains(100, 100));  // corner
    assert(e.contains(300, 140));  // corner
    assert(!e.contains(50, 120));  // left outside
    assert(!e.contains(350, 120)); // right outside
    assert(!e.contains(150, 50));  // above
    assert(!e.contains(150, 200)); // below
    ENDTEST();
}

static void test_ui_clear() {
    TEST("UIManager clear removes all elements");
    UIManager ui;
    ui.add_button(0, 0, 100, 30, "A");
    ui.add_button(0, 0, 100, 30, "B");
    ui.add_button(0, 0, 100, 30, "C");
    assert(ui.elements.size() == 3);
    ui.clear();
    assert(ui.elements.size() == 0);
    ENDTEST();
}

static void test_ui_hover_detection() {
    TEST("UIManager detects hover");
    UIManager ui;
    ui.add_button(100, 100, 200, 40, "HOVER");
    ui.mouse_x = 150;
    ui.mouse_y = 120;
    ui.update();
    assert(ui.elements[0].hovered);
    ENDTEST();
}

static void test_ui_no_hover_when_outside() {
    TEST("UIManager doesn't hover when outside");
    UIManager ui;
    ui.add_button(100, 100, 200, 40, "NO HOVER");
    ui.mouse_x = 50;
    ui.mouse_y = 50;
    ui.update();
    assert(!ui.elements[0].hovered);
    ENDTEST();
}

static void test_ui_button_click_callback() {
    TEST("UIManager button click triggers callback");
    UIManager ui;
    bool clicked = false;
    ui.add_button(100, 100, 200, 40, "CLICK", [&]() { clicked = true; });
    ui.mouse_x = 150;
    ui.mouse_y = 120;
    ui.mouse_left_pressed = true;
    ui.update();
    assert(clicked);
    ENDTEST();
}

static void test_ui_button_no_click_when_outside() {
    TEST("UIManager button click doesn't trigger when outside");
    UIManager ui;
    bool clicked = false;
    ui.add_button(100, 100, 200, 40, "NO CLICK", [&]() { clicked = true; });
    ui.mouse_x = 50;
    ui.mouse_y = 50;
    ui.mouse_left_pressed = true;
    ui.update();
    assert(!clicked);
    ENDTEST();
}

static void test_ui_invisible_elements_skipped() {
    TEST("UIManager skips invisible elements");
    UIManager ui;
    int id = ui.add_button(100, 100, 200, 40, "INVISIBLE");
    ui.elements[0].visible = false;
    ui.mouse_x = 150;
    ui.mouse_y = 120;
    ui.update();
    assert(!ui.elements[0].hovered);
    ENDTEST();
}

static void test_ui_disabled_buttons_dont_hover() {
    TEST("UIManager disabled buttons don't hover");
    UIManager ui;
    ui.add_button(100, 100, 200, 40, "DISABLED");
    ui.elements[0].enabled = false;
    ui.mouse_x = 150;
    ui.mouse_y = 120;
    ui.update();
    assert(!ui.elements[0].hovered);
    ENDTEST();
}

static void test_ui_state_management() {
    TEST("UIManager state transitions work");
    UIManager ui;
    assert(ui.state == UIManager::STATE_MAIN_MENU);
    ui.state = UIManager::STATE_PLAYING;
    assert(ui.state == UIManager::STATE_PLAYING);
    ui.state = UIManager::STATE_PAUSED;
    assert(ui.state == UIManager::STATE_PAUSED);
    ENDTEST();
}

static void test_ui_unique_ids() {
    TEST("UIManager assigns unique IDs");
    UIManager ui;
    int id1 = ui.add_button(0, 0, 100, 30, "A");
    int id2 = ui.add_button(0, 0, 100, 30, "B");
    int id3 = ui.add_button(0, 0, 100, 30, "C");
    assert(id1 != id2);
    assert(id2 != id3);
    assert(id1 != id3);
    ENDTEST();
}

int main() {
    std::cout << "=== UI System Tests ===" << std::endl;
    test_font_text_width();
    test_font_text_height();
    test_font_char_index_letters();
    test_font_char_index_digits();
    test_font_char_index_punctuation();
    test_font_char_index_unknown();
    test_ui_element_add_button();
    test_ui_element_add_panel();
    test_ui_element_add_label();
    test_ui_element_add_progress_bar();
    test_ui_element_add_inventory_slot();
    test_ui_element_contains();
    test_ui_clear();
    test_ui_hover_detection();
    test_ui_no_hover_when_outside();
    test_ui_button_click_callback();
    test_ui_button_no_click_when_outside();
    test_ui_invisible_elements_skipped();
    test_ui_disabled_buttons_dont_hover();
    test_ui_state_management();
    test_ui_unique_ids();
    std::cout << "\n=== Results: " << tests_passed << "/" << tests_total << " ===" << std::endl;
    return tests_passed == tests_total ? 0 : 1;
}
