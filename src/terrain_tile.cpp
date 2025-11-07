#include "terrain_tile.h"
#include <glm/gtc/type_ptr.hpp>

// Computing normals of the surface (This code inspired from existing implementations), 
// we are not that familiar about implementation but we know what exactly it do
static void computeNormals(const std::vector<glm::vec3>& pos, int N, float dx, float dz,
    std::vector<glm::vec3>& outN)
{
    outN.resize(pos.size());
    auto idx = [N](int r, int c) { 
        return r * N + c; 
    }; // lambda expression for 2D index to 1D

    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {

            int r0 = std::max(r - 1, 0), r1 = std::min(r + 1, N - 1);
            int c0 = std::max(c - 1, 0), c1 = std::min(c + 1, N - 1);

            const glm::vec3& L = pos[idx(r, c0)];
            const glm::vec3& R = pos[idx(r, c1)];

            const glm::vec3& D = pos[idx(r0, c)];
            const glm::vec3& U = pos[idx(r1, c)];

            glm::vec3 dPosdx = (R - L) / float((c1 - c0) ? (c1 - c0) : 1);
            glm::vec3 dPosdz = (U - D) / float((r1 - r0) ? (r1 - r0) : 1);

            glm::vec3 n = glm::normalize(glm::cross(dPosdz, dPosdx));
            outN[idx(r, c)] = n;
        }
    }
}

// Inspired by existing implementation but implemented by ourself
void buildTerrainTile(TerrainTile& tile, const TerrainParams& P)
{
    const int N = P.vertsPerEdge;
    const float S = P.tileSize;
    const float step = S / float(N - 1);
    const float x0 = tile.id.tx * S;
    const float z0 = tile.id.tz * S;

    struct V {
        glm::vec3 p; //point
        glm::vec3 n; // normal
        glm::vec2 uv; // added for uv texture mapping in application.cpp
    };

    std::vector<glm::vec3> positions(N * N); // position space (NXN grid)

    auto idx = [N](int r, int c) { 
        return r * N + c; 
    }; // lambda expression for 2D index to 1D

    glm::vec3 min(1e9f), max(-1e9f);

    // positions
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            float x = x0 + j * step;
            float z = z0 + i * step;
            float y = terrainHeight(x, z, P);

            glm::vec3 p(x, y, z);
            positions[ idx(i, j)] = p;

            min = glm::min(min, p);
            max = glm::max(max, p);
        }
    }

    // normals
    std::vector<glm::vec3> normals;
    computeNormals(positions, N, step, step, normals);

    // pack vertices
    std::vector<V> verts(N * N);
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            V v;
            v.p = positions[idx(i,j)];
            v.n = normals[idx(i,j)];

            // UV calc
            v.uv = glm::vec2(
                float(j) / float(N - 1),
                float(i) / float(N - 1)
            );

            verts[idx(i,j)] = v;
        }
    }

    // indices
    std::vector<uint32_t> indices;
    indices.reserve((N - 1) * (N - 1) * 6);

    for (int r = 0; r < N - 1; ++r) {
        for (int c = 0; c < N - 1; ++c) {
            uint32_t i0 = idx(r, c);
            uint32_t i1 = idx(r, c + 1);
            uint32_t i2 = idx(r + 1, c);
            uint32_t i3 = idx(r + 1, c + 1);

            indices.push_back(i0); indices.push_back(i2); indices.push_back(i1); // triangle 1 (same dir)
            indices.push_back(i1); indices.push_back(i2); indices.push_back(i3); // triangle 2 (same dir)
        }
    }

    // upload to GL
    if (!tile.vao) glGenVertexArrays(1, &tile.vao);
    if (!tile.vbo) glGenBuffers(1, &tile.vbo);
    if (!tile.ibo) glGenBuffers(1, &tile.ibo);

    glBindVertexArray(tile.vao);

    glBindBuffer(GL_ARRAY_BUFFER, tile.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(V), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tile.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    // position =>> location 0
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, p));

    // normal =>> location 1
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, n));

    // uv =>> location 2
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, uv));

    glBindVertexArray(0);

    tile.indexCount = (GLsizei)indices.size();
    tile.aabbMin = min;
    tile.aabbMax = max;
    tile.ready = true;
}
