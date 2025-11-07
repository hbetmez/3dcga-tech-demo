#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <cmath>


// PERLIN NOISE IMPLEMENTATION

// Terrain parameters for perlin noise
struct TerrainParams {
    float tileSize = 256.0f;
    int   vertsPerEdge = 129;
    float heightScale = 40.0f;

    int   octaves = 8;
    float baseFrequency = 1.0f / 100.0f;
    float lacunarity = 1.0f;
    float gain = 0.6f;
    uint32_t seed =999331u;
};

// Permutation table for perlin noise
static const int PERM[512] = {
    151,160,137,91,90,15,
    131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
    8,99,37,240,21,10,23,
    190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
    35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,
    168, 68,175,74,165,71,134,139,48,27,166,77,146,158,231,83,
    111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
    102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208,
    89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,
    186, 3,64,52,217,226,250,124,123,5,202,38,147,118,126,255,
    82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,
    183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167,
    43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,
    185,112,104,218,246,97,228,251,34,242,193,238,210,144,12,191,
    179,162,241, 81,51,145,235,249,14,239,107,49,192,214, 31,181,
    199,106,157,184, 84,204,176,115,121,50,45,127,  4,150,254,

    // Repeated it to prevent modding 256 + i. I searched that it is common approach
    151,160,137,91,90,15,
    131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
    8,99,37,240,21,10,23,
    190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
    35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,
    168, 68,175,74,165,71,134,139,48,27,166,77,146,158,231,83,
    111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
    102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208,
    89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,
    186, 3,64,52,217,226,250,124,123,5,202,38,147,118,126,255,
    82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,223,
    183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167,
    43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,
    185,112,104,218,246,97,228,251,34,242,193,238,210,144,12,191,
    179,162,241, 81,51,145,235,249,14,239,107,49,192,214, 31,181,
    199,106,157,184, 84,204,176,115,121,50,45,127,  4,150,254
};

// Perlin's fade curve 6t^5 - 15t^4 + 10t^3
inline float fade(float t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
}

// gradient directions in 2D
inline float grad(int hash, float x, float y) {
    switch (hash & 7) { // lowest 3 bit is extracted
    case 0: return  x + y; // direction: (1,1)
    case 1: return  x - y; // direction: (1,-1)
    case 2: return -x + y; // direction: (-1,1)
    case 3: return -x - y; // direction: (-1,-1)
    case 4: return  x;     // direction: (1,0)
    case 5: return -x;     // direction: (-1,0)
    case 6: return  y;     // direction: (0,1)
    default:return -y;     // direction: (0,-1)
    }
}


// 2D PERLIN NOISE
inline float gradNoise2D(float x, float y, uint32_t seed) {
    x += (float)((seed & 0xFF)); // 8 bit mask for seed (last 8th bit)
    y += (float)(((seed >> 8) & 0xFF)); // 8 bit mask for seed (before last 8th bit)

    // making sure they are in between [0,255] for table lookup
    int X = (int)floor(x) & 255; 
    int Y = (int)floor(y) & 255;


    int aa = PERM[PERM[X] + Y]; // (X,Y)
    int ab = PERM[PERM[X] + Y + 1]; //(X,Y+1)
    int ba = PERM[PERM[X + 1] + Y]; // (X+1, Y)
    int bb = PERM[PERM[X + 1] + Y + 1]; // (X+1, Y+1)

    float xFraction = x - floor(x);
    float yFraction = y - floor(y);

    float u = fade(xFraction);
    float v = fade(yFraction);

    float x1 = grad(aa, xFraction, yFraction);
    float x2 = grad(ba, xFraction - 1, yFraction);
    float y1 = glm::mix(x1, x2, u);

    x1 = grad(ab, xFraction, yFraction - 1);
    x2 = grad(bb, xFraction - 1, yFraction - 1);
    float y2 = glm::mix(x1, x2, u);

    return glm::mix(y1, y2, v);
}


//  Fractal Brownian Motion
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
    return sum / ampSum;
}

// Finaly terrain height
inline float terrainHeight(float x, float y, const TerrainParams& p) {
    float warping= 0.5f;
    float wx = gradNoise2D(x * 0.01f, y * 0.01f , p.seed ^ 0xABCDEFu ) * warping ;
    float wz = gradNoise2D((x + 1111.0f ) * 0.01f, (y - 1111.0f) * 0.01f, p.seed ^ 0x123457u) * warping;

    float h = fbm2D(x + wx * 75.0f,y + wz * 75.0f, p);

    h = std::copysign( std::pow( std::abs(h), 1.2f), h);

    return h * p.heightScale;
}
