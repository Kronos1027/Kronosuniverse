// KronoUniverse — Weather & Lighting Tests (v0.3)

#include "game/weather_system.hpp"
#include "game/lighting_system.hpp"
#include <cassert>
#include <iostream>

using namespace krono;

static int tests_passed = 0;
static int tests_total = 0;
#define TEST(name) tests_total++;
#define ENDTEST() tests_passed++; std::cout << "[OK] " << __func__ << std::endl;

static void test_weather_decide_desert() {
    TEST("desert biome picks clear or sandstorm");
    WeatherSystem w;
    int sandstorm_count = 0;
    int clear_count = 0;
    for (int i = 0; i < 1000; i++) {
        w.decide_for_biome(BiomeType::DESERT);
        if (w.target == WeatherType::SANDSTORM) sandstorm_count++;
        if (w.target == WeatherType::CLEAR) clear_count++;
    }
    assert(sandstorm_count > 0);
    assert(clear_count > 0);
    assert(sandstorm_count + clear_count == 1000);  // never rains in desert
    ENDTEST();
}

static void test_weather_tundra_snow() {
    TEST("tundra biome produces snow");
    WeatherSystem w;
    int snow_count = 0;
    for (int i = 0; i < 1000; i++) {
        w.decide_for_biome(BiomeType::TUNDRA);
        if (w.target == WeatherType::SNOW) snow_count++;
    }
    assert(snow_count > 0);
    ENDTEST();
}

static void test_weather_rain_spawns_particles() {
    TEST("rain spawns particles");
    WeatherSystem w;
    w.current = WeatherType::RAIN;
    w.target = WeatherType::RAIN;
    w.transition = 1.0f;
    w.intensity = 1.0f;
    w.update(0.1f, 0, 0, 1280, 720);
    assert(w.particles.size() > 0);
    ENDTEST();
}

static void test_weather_snow_slow_fall() {
    TEST("snow falls slower than rain");
    WeatherSystem w;
    w.current = WeatherType::SNOW;
    w.target = WeatherType::SNOW;
    w.transition = 1.0f;
    w.update(0.1f, 0, 0, 1280, 720);
    // Snow particles should have low Y velocity (less than 200, well below rain's 400)
    bool found = false;
    for (auto& p : w.particles) {
        assert(p.vy < 200);
        found = true;
    }
    assert(found);
    ENDTEST();
}

static void test_storm_lightning() {
    TEST("storm produces lightning flashes");
    WeatherSystem w;
    w.current = WeatherType::STORM;
    w.target = WeatherType::STORM;
    w.transition = 1.0f;
    w.intensity = 1.0f;
    w.lightning_timer = 0;  // trigger immediately
    
    w.update(0.01f, 0, 0, 1280, 720);
    assert(w.get_lightning_intensity() > 0);
    ENDTEST();
}

static void test_clear_no_particles() {
    TEST("clear weather has no particles");
    WeatherSystem w;
    w.current = WeatherType::CLEAR;
    w.target = WeatherType::CLEAR;
    w.transition = 1.0f;
    w.update(1.0f, 0, 0, 1280, 720);
    assert(w.particles.size() == 0);
    ENDTEST();
}

static void test_fog_accumulates() {
    TEST("fog density accumulates over time");
    WeatherSystem w;
    w.current = WeatherType::FOG;
    w.target = WeatherType::FOG;
    w.transition = 1.0f;
    float before = w.get_fog_density();
    w.update(5.0f, 0, 0, 1280, 720);
    float after = w.get_fog_density();
    assert(after > before);
    ENDTEST();
}

static void test_storm_is_stormy() {
    TEST("is_stormy returns true for storm");
    WeatherSystem w;
    w.current = WeatherType::STORM;
    assert(w.is_stormy());
    w.current = WeatherType::CLEAR;
    assert(!w.is_stormy());
    ENDTEST();
}

static void test_lighting_add_light() {
    TEST("adding light source works");
    LightingSystem ls;
    size_t before = ls.lights.size();
    ls.add_light(LightSource::TORCH, 100, 100, 80);
    assert(ls.lights.size() == before + 1);
    ENDTEST();
}

static void test_lighting_at_source() {
    TEST("light intensity is high at light source");
    LightingSystem ls;
    ls.ambient_light = 0;  // dark
    ls.add_light(LightSource::TORCH, 100, 100, 80);
    auto result = ls.get_light_at(100, 100);
    assert(result.r > 0.5f);  // bright at source
    ENDTEST();
}

static void test_lighting_far_from_source() {
    TEST("light intensity is low far from source");
    LightingSystem ls;
    ls.ambient_light = 0;
    ls.add_light(LightSource::TORCH, 100, 100, 80);
    auto result = ls.get_light_at(500, 500);
    assert(result.r < 0.2f);  // very dim
    ENDTEST();
}

static void test_lighting_multiple_lights() {
    TEST("multiple lights add up");
    LightingSystem ls;
    ls.ambient_light = 0;
    ls.add_light(LightSource::TORCH, 100, 100, 80);
    auto single = ls.get_light_at(100, 100);
    ls.add_light(LightSource::TORCH, 100, 100, 80);
    auto doubled = ls.get_light_at(100, 100);
    assert(doubled.r > single.r);
    ENDTEST();
}

static void test_lighting_lava_orange() {
    TEST("lava glow has orange tint");
    LightingSystem ls;
    ls.ambient_light = 0;
    ls.add_light(LightSource::LAVA_GLOW, 100, 100, 80);
    auto result = ls.get_light_at(100, 100);
    assert(result.r > result.b);  // more red than blue
    ENDTEST();
}

static void test_lighting_crystal_cyan() {
    TEST("crystal glow has cyan tint");
    LightingSystem ls;
    ls.ambient_light = 0;
    ls.add_light(LightSource::CRYSTAL_GLOW, 100, 100, 80);
    auto result = ls.get_light_at(100, 100);
    assert(result.b > result.r);  // more blue than red
    ENDTEST();
}

static void test_lighting_day_night_ambient() {
    TEST("ambient light changes with time of day");
    LightingSystem ls;
    ls.update(0.01f, 0, 0, 0.0f);  // midnight
    float night_light = ls.ambient_light;
    ls.update(0.01f, 0, 0, 0.5f);  // noon
    float day_light = ls.ambient_light;
    assert(day_light > night_light);
    ENDTEST();
}

static void test_lighting_max_lights() {
    TEST("lighting system caps at MAX_LIGHTS");
    LightingSystem ls;
    for (int i = 0; i < LightingSystem::MAX_LIGHTS + 10; i++) {
        ls.add_light(LightSource::TORCH, (float)i, 0, 50);
    }
    assert((int)ls.lights.size() <= LightingSystem::MAX_LIGHTS);
    ENDTEST();
}

static void test_lighting_temporary_expires() {
    TEST("temporary lights expire after lifetime");
    LightingSystem ls;
    ls.add_light(LightSource::EXPLOSION, 100, 100, 80);
    size_t before = ls.lights.size();
    ls.update(1.0f, 0, 0, 0.5f);  // > 0.5s lifetime
    assert(ls.lights.size() < before);
    ENDTEST();
}

int main() {
    std::cout << "=== Weather & Lighting Tests ===" << std::endl;
    test_weather_decide_desert();
    test_weather_tundra_snow();
    test_weather_rain_spawns_particles();
    test_weather_snow_slow_fall();
    test_storm_lightning();
    test_clear_no_particles();
    test_fog_accumulates();
    test_storm_is_stormy();
    test_lighting_add_light();
    test_lighting_at_source();
    test_lighting_far_from_source();
    test_lighting_multiple_lights();
    test_lighting_lava_orange();
    test_lighting_crystal_cyan();
    test_lighting_day_night_ambient();
    test_lighting_max_lights();
    test_lighting_temporary_expires();
    
    std::cout << "\n=== Results: " << tests_passed << "/" << tests_total << " ===" << std::endl;
    return tests_passed == tests_total ? 0 : 1;
}
