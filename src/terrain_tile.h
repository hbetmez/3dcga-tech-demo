#pragma once
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <vector>
#include <utility>
#include "terrain_noise.h"

struct TileID { int tx = 0; int tz = 0; };
inline bool operator==(const TileID& a, const TileID& b) { return a.tx == b.tx && a.tz == b.tz; }

struct TileIDHasher {
    size_t operator()(const TileID& k) const {
        uint64_t x = (uint64_t)(uint32_t)k.tx;
        uint64_t z = (uint64_t)(uint32_t)k.tz;
        uint64_t h =  (x << 32) ^ (z * 0x9E3779B185EBCA87ull);

        h ^= (h >> 33); h *= 0xff51afd7ed558ccdULL;
        h ^= (h >> 33); h *= 0xc4ceb9fe1a85ec53ULL;
        h ^= (h >> 33);
        return (size_t)h;
    }
};

struct TerrainTile {
    TileID id{};
    glm::vec3 aabbMin{ 0 }, aabbMax{ 0 };

    GLuint vao = 0, vbo = 0, ibo = 0;
    GLsizei indexCount = 0;
    bool ready = false;

    void destroy() {
        if (ibo) glDeleteBuffers(1, &ibo), ibo = 0;
        if (vbo) glDeleteBuffers(1, &vbo), vbo = 0;
        if (vao) glDeleteVertexArrays(1, &vao), vao = 0;
        indexCount = 0;
        ready = false;
    }
};

void buildTerrainTile(TerrainTile& tile, const TerrainParams& params);
