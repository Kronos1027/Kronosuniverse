#include <iostream>
#include <cassert>
#include <cmath>
#include "FastNoiseLite.h"

int main() {
    std::cout << "=== Noise Tests ===" << std::endl;
    fnl_state fnl = fnlCreateState();
    fnl.seed = 42;
    fnl.frequency = 0.1f;
    fnl.noise_type = FNL_NOISE_PERLIN;
    fnl.fractal_type = FNL_FRACTAL_FBM;
    fnl.octaves = 4;

    // Test 1: Deterministic
    float n1 = fnlGetNoise2D(&fnl, 13.7f, 7.3f);
    float n2 = fnlGetNoise2D(&fnl, 13.7f, 7.3f);
    assert(std::abs(n1 - n2) < 0.0001f);
    std::cout << "✓ Deterministic: n1=" << n1 << " n2=" << n2 << std::endl;

    // Test 2: Different seed
    fnl_state fnl2 = fnlCreateState();
    fnl2.seed = 999;
    fnl2.frequency = 0.1f;
    fnl2.noise_type = FNL_NOISE_PERLIN;
    float n3 = fnlGetNoise2D(&fnl2, 13.7f, 7.3f);
    assert(std::abs(n1 - n3) > 0.001f);
    std::cout << "✓ Different seed: n1=" << n1 << " n3=" << n3 << std::endl;

    // Test 3: Range [-1, 1]
    float min = 999, max = -999;
    for (int x = 0; x < 100; x++) {
        for (int y = 0; y < 100; y++) {
            float n = fnlGetNoise2D(&fnl, (float)x, (float)y);
            if (n < min) min = n;
            if (n > max) max = n;
        }
    }
    assert(min >= -1.0f && max <= 1.0f);
    std::cout << "✓ Range: [" << min << ", " << max << "]" << std::endl;

    std::cout << std::endl << "All noise tests passed! ✓" << std::endl;
    return 0;
}
