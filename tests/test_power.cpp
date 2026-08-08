// KronoUniverse — Power System Tests (v0.6)

#include "game/power_system.hpp"
#include <cassert>
#include <iostream>

using namespace krono;

static int tests_passed = 0;
static int tests_total = 0;
#define TEST(name) tests_total++;
#define ENDTEST() tests_passed++; std::cout << "[OK] " << __func__ << std::endl;

static World create_test_world() {
    return World(42);
}

static Entity create_player(Registry& reg, float x, float y) {
    Entity p = reg.create();
    reg.emplace<Position>(p, Position{x, y});
    return p;
}

static void test_add_power_element() {
    TEST("add power element");
    PowerSystem ps;
    PowerElement e;
    e.type = PowerElementType::WIRE;
    e.x = 10; e.y = 10;
    ps.add(e);
    assert(ps.elements.size() == 1);
    ENDTEST();
}

static void test_no_duplicate_at_same_pos() {
    TEST("no duplicate at same position");
    PowerSystem ps;
    PowerElement e;
    e.type = PowerElementType::WIRE;
    e.x = 10; e.y = 10;
    ps.add(e);
    ps.add(e);
    assert(ps.elements.size() == 1);
    ENDTEST();
}

static void test_remove_element() {
    TEST("remove power element");
    PowerSystem ps;
    PowerElement e;
    e.x = 10; e.y = 10;
    ps.add(e);
    assert(ps.elements.size() == 1);
    ps.remove(10, 10);
    assert(ps.elements.size() == 0);
    ENDTEST();
}

static void test_find_element() {
    TEST("find element by position");
    PowerSystem ps;
    PowerElement e;
    e.x = 10; e.y = 10;
    e.type = PowerElementType::LAMP;
    ps.add(e);
    PowerElement* found = ps.find(10, 10);
    assert(found != nullptr);
    assert(found->type == PowerElementType::LAMP);
    assert(ps.find(99, 99) == nullptr);
    ENDTEST();
}

static void test_source_provides_power() {
    TEST("source provides power");
    PowerSystem ps;
    World world = create_test_world();
    Registry reg;
    Entity player = create_player(reg, 0, 0);
    PowerElement src;
    src.type = PowerElementType::SOURCE;
    src.is_source = true;
    src.x = 10; src.y = 10;
    ps.add(src);
    ps.update(0.1f, world, player);
    assert(ps.find(10, 10)->power_level == 15);
    ENDTEST();
}

static void test_power_propagates_through_wire() {
    TEST("power propagates through wire");
    PowerSystem ps;
    World world = create_test_world();
    Registry reg;
    Entity player = create_player(reg, 0, 0);
    PowerElement src;
    src.type = PowerElementType::SOURCE;
    src.is_source = true;
    src.x = 10; src.y = 10;
    ps.add(src);
    PowerElement wire;
    wire.type = PowerElementType::WIRE;
    wire.x = 11; wire.y = 10;
    ps.add(wire);
    ps.update(0.1f, world, player);
    assert(ps.find(11, 10)->power_level == 14);  // power decreases by 1
    ENDTEST();
}

static void test_power_decreases_with_distance() {
    TEST("power decreases with distance");
    PowerSystem ps;
    World world = create_test_world();
    Registry reg;
    Entity player = create_player(reg, 0, 0);
    PowerElement src;
    src.type = PowerElementType::SOURCE;
    src.is_source = true;
    src.x = 10; src.y = 10;
    ps.add(src);
    // Add chain of wires
    for (int i = 1; i <= 5; i++) {
        PowerElement wire;
        wire.type = PowerElementType::WIRE;
        wire.x = 10 + i; wire.y = 10;
        ps.add(wire);
    }
    ps.update(0.1f, world, player);
    assert(ps.find(11, 10)->power_level == 14);
    assert(ps.find(12, 10)->power_level == 13);
    assert(ps.find(13, 10)->power_level == 12);
    assert(ps.find(14, 10)->power_level == 11);
    assert(ps.find(15, 10)->power_level == 10);
    ENDTEST();
}

static void test_power_doesnt_reach_past_15() {
    TEST("power doesn't reach past 15 blocks");
    PowerSystem ps;
    World world = create_test_world();
    Registry reg;
    Entity player = create_player(reg, 0, 0);
    PowerElement src;
    src.type = PowerElementType::SOURCE;
    src.is_source = true;
    src.x = 10; src.y = 10;
    ps.add(src);
    // Add 20 wires
    for (int i = 1; i <= 20; i++) {
        PowerElement wire;
        wire.type = PowerElementType::WIRE;
        wire.x = 10 + i; wire.y = 10;
        ps.add(wire);
    }
    ps.update(0.1f, world, player);
    // Power at position 15 (5 blocks away) should be 10
    assert(ps.find(15, 10)->power_level > 0);
    // Power at position 25 (15 blocks away) should be 0 (too far)
    assert(ps.find(25, 10)->power_level == 0);
    ENDTEST();
}

static void test_lever_provides_power_when_active() {
    TEST("lever provides power when active");
    PowerSystem ps;
    World world = create_test_world();
    Registry reg;
    Entity player = create_player(reg, 0, 0);
    PowerElement lever;
    lever.type = PowerElementType::LEVER;
    lever.x = 10; lever.y = 10;
    lever.active = true;
    ps.add(lever);
    ps.update(0.1f, world, player);
    assert(ps.find(10, 10)->output_power == 15);
    ENDTEST();
}

static void test_lever_no_power_when_inactive() {
    TEST("lever has no power when inactive");
    PowerSystem ps;
    World world = create_test_world();
    Registry reg;
    Entity player = create_player(reg, 0, 0);
    PowerElement lever;
    lever.type = PowerElementType::LEVER;
    lever.x = 10; lever.y = 10;
    lever.active = false;
    ps.add(lever);
    ps.update(0.1f, world, player);
    assert(ps.find(10, 10)->output_power == 0);
    ENDTEST();
}

static void test_toggle_lever() {
    TEST("toggle lever switches state");
    PowerSystem ps;
    PowerElement lever;
    lever.type = PowerElementType::LEVER;
    lever.x = 10; lever.y = 10;
    lever.active = false;
    ps.add(lever);
    ps.toggle_lever(10, 10);
    assert(ps.find(10, 10)->active);
    ps.toggle_lever(10, 10);
    assert(!ps.find(10, 10)->active);
    ENDTEST();
}

static void test_button_press_activates_temporarily() {
    TEST("button press activates temporarily");
    PowerSystem ps;
    World world = create_test_world();
    Registry reg;
    Entity player = create_player(reg, 0, 0);
    PowerElement button;
    button.type = PowerElementType::BUTTON;
    button.x = 10; button.y = 10;
    button.active = false;
    ps.add(button);
    ps.press_button(10, 10);
    ps.update(0.1f, world, player);
    assert(ps.find(10, 10)->output_power == 15);
    // Wait for cooldown - update multiple times to deplete 1.0 sec
    for (int i = 0; i < 15; i++) {
        ps.update(0.1f, world, player);
    }
    assert(ps.find(10, 10)->output_power == 0);
    ENDTEST();
}

static void test_lamp_lights_up_when_powered() {
    TEST("lamp lights up when powered");
    PowerSystem ps;
    World world = create_test_world();
    Registry reg;
    Entity player = create_player(reg, 0, 0);
    PowerElement src;
    src.type = PowerElementType::SOURCE;
    src.is_source = true;
    src.x = 10; src.y = 10;
    ps.add(src);
    PowerElement wire;
    wire.type = PowerElementType::WIRE;
    wire.x = 11; wire.y = 10;
    ps.add(wire);
    PowerElement lamp;
    lamp.type = PowerElementType::LAMP;
    lamp.x = 12; lamp.y = 10;
    ps.add(lamp);
    ps.update(0.1f, world, player);
    assert(ps.find(12, 10)->active);
    ENDTEST();
}

static void test_count_active() {
    TEST("count active elements");
    PowerSystem ps;
    World world = create_test_world();
    Registry reg;
    Entity player = create_player(reg, 0, 0);
    PowerElement src;
    src.type = PowerElementType::SOURCE;
    src.is_source = true;
    src.x = 10; src.y = 10;
    ps.add(src);
    for (int i = 1; i <= 3; i++) {
        PowerElement wire;
        wire.type = PowerElementType::WIRE;
        wire.x = 10 + i; wire.y = 10;
        ps.add(wire);
    }
    ps.update(0.1f, world, player);
    // Source + 3 wires should all be active
    assert(ps.count_active() == 4);
    ENDTEST();
}

static void test_get_lamp_brightness() {
    TEST("get lamp brightness");
    PowerSystem ps;
    World world = create_test_world();
    Registry reg;
    Entity player = create_player(reg, 0, 0);
    PowerElement src;
    src.type = PowerElementType::SOURCE;
    src.is_source = true;
    src.x = 10; src.y = 10;
    ps.add(src);
    PowerElement lamp;
    lamp.type = PowerElementType::LAMP;
    lamp.x = 11; lamp.y = 10;
    ps.add(lamp);
    ps.update(0.1f, world, player);
    // Lamp at distance 1 should have power 14, brightness 14/15
    float brightness = ps.get_lamp_brightness(11, 10);
    assert(brightness > 0.9f);
    ENDTEST();
}

static void test_power_color_differs_when_active() {
    TEST("power color differs when active");
    float r1, g1, b1, r2, g2, b2;
    PowerSystem::get_color(PowerElementType::WIRE, false, r1, g1, b1);
    PowerSystem::get_color(PowerElementType::WIRE, true, r2, g2, b2);
    assert(r1 != r2 || g1 != g2 || b1 != b2);
    ENDTEST();
}

int main() {
    std::cout << "=== Power System Tests ===" << std::endl;
    test_add_power_element();
    test_no_duplicate_at_same_pos();
    test_remove_element();
    test_find_element();
    test_source_provides_power();
    test_power_propagates_through_wire();
    test_power_decreases_with_distance();
    test_power_doesnt_reach_past_15();
    test_lever_provides_power_when_active();
    test_lever_no_power_when_inactive();
    test_toggle_lever();
    test_button_press_activates_temporarily();
    test_lamp_lights_up_when_powered();
    test_count_active();
    test_get_lamp_brightness();
    test_power_color_differs_when_active();
    std::cout << "\n=== Results: " << tests_passed << "/" << tests_total << " ===" << std::endl;
    return tests_passed == tests_total ? 0 : 1;
}
