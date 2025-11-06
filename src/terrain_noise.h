#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <cmath>

// ---- Tunables (good defaults to start; units in your world meters)
struct TerrainParams {
    float tileSize = 256.0f;      // meters per tile edge
    int   vertsPerEdge = 129;     // 129 -> 128 quads; shares edges well
    float heightScale = 30.0f;    // max terrain elevation in meters

    // fBm parameters
    int   octaves = 6;
    float baseFrequency = 1.0f / 200.0f; // larger -> more hills per meter
    float lacunarity = 2.0f;             // frequency multiplier per octave
    float gain = 0.5f;                   // amplitude falloff per octave
    uint32_t seed = 1234567u;            // change for different worlds
};

// Small integer hash for gradient directions
inline uint32_t pcgHash(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}

// 2D gradient noise (Perlin-style, fast & deterministic)
inline float gradNoise2D(float x, float y, uint32_t seed) {
    int xi = (int)std::floor(x);
    int yi = (int)std::floor(y);
    float xf = x - xi;
    float yf = y - yi;

    auto grad = [&](int ix, int iy) {
        uint32_t h = pcgHash((uint32_t)(ix) * 374761393u ^ (uint32_t)(iy) * 668265263u ^ seed);
        // 8 gradient directions on the unit circle:
        float a = float((h >> 1) & 7) * (3.14159265f / 4.0f);
        return glm::vec2(std::cos(a), std::sin(a));
        };

    glm::vec2 g00 = grad(xi, yi);
    glm::vec2 g10 = grad(xi + 1, yi);
    glm::vec2 g01 = grad(xi, yi + 1);
    glm::vec2 g11 = grad(xi + 1, yi + 1);

    glm::vec2 d00(xf, yf);
    glm::vec2 d10(xf - 1, yf);
    glm::vec2 d01(xf, yf - 1);
    glm::vec2 d11(xf - 1, yf - 1);

    auto dot = [](const glm::vec2& a, const glm::vec2& b) { return a.x * b.x + a.y * b.y; };

    auto fade = [](float t) { return t * t * t * (t * (t * 6 - 15) + 10); }; // Perlin's 5th-order
    float u = fade(xf);
    float v = fade(yf);

    float n00 = dot(g00, d00);
    float n10 = dot(g10, d10);
    float n01 = dot(g01, d01);
    float n11 = dot(g11, d11);

    float nx0 = glm::mix(n00, n10, u);
    float nx1 = glm::mix(n01, n11, u);
    return glm::mix(nx0, nx1, v);
}

// Fractal Brownian Motion (sum of octaves)
inline float fbm2D(float x, float y, const TerrainParams& p) {
    float f = p.baseFrequency;
    float a = 1.0f;
    float sum = 0.0f;
    float ampSum = 0.0f;

    for (int i = 0; i < p.octaves; ++i) {
        sum += gradNoise2D(x * f, y * f, p.seed + i * 1013u) * a;
        ampSum += a;
        f *= p.lacunarity;
        a *= p.gain;
    }
    return sum / ampSum; // normalize to roughly [-1,1]
}

// Sample height at world (x,z). Deterministic across tiles.
inline float terrainHeight(float x, float z, const TerrainParams& p) {
    // Optional: slight domain warp to break patterns
    float warp = 0.5f;
    float wx = gradNoise2D(x * 0.01f, z * 0.01f, p.seed ^ 0xABCDEFu) * warp;
    float wz = gradNoise2D((x + 1000.0f) * 0.01f, (z - 1000.0f) * 0.01f, p.seed ^ 0x123456u) * warp;

    float h = fbm2D(x + wx * 50.0f, z + wz * 50.0f, p);
    // Shape curve: raise a little to emphasize peaks
    h = std::copysign(std::pow(std::abs(h), 1.2f), h);
    return h * p.heightScale;
}
