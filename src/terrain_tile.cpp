// terrain_tile.cpp
#include "terrain_tile.h"
#include <glm/gtc/type_ptr.hpp>

static void computeNormals(const std::vector<glm::vec3>& pos, int N, float dx, float dz,
    std::vector<glm::vec3>& outN)
{
    outN.resize(pos.size());
    auto idx = [N](int r, int c) { return r * N + c; };

    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            int r0 = std::max(r - 1, 0), r1 = std::min(r + 1, N - 1);
            int c0 = std::max(c - 1, 0), c1 = std::min(c + 1, N - 1);

            const glm::vec3& L = pos[idx(r, c0)];
            const glm::vec3& R = pos[idx(r, c1)];
            const glm::vec3& D = pos[idx(r0, c)];
            const glm::vec3& U = pos[idx(r1, c)];

            glm::vec3 dPosdx = (R - L) / (float)((c1 - c0) ? (c1 - c0) : 1);
            glm::vec3 dPosdz = (U - D) / (float)((r1 - r0) ? (r1 - r0) : 1);

            // x is column direction, z is row direction; adjust if your grid layout differs
            glm::vec3 n = glm::normalize(glm::cross(dPosdz, dPosdx));
            outN[idx(r, c)] = n;
        }
    }
}

void buildTerrainTile(TerrainTile& tile, const TerrainParams& P)
{
    const int N = P.vertsPerEdge;                 // e.g., 129
    const float S = P.tileSize;                   // e.g., 256
    const float step = S / float(N - 1);          // grid spacing
    const float x0 = tile.id.tx * S;
    const float z0 = tile.id.tz * S;

    // Vertex layout: position (3f) + normal (3f)
    struct V { glm::vec3 p; glm::vec3 n; };

    std::vector<glm::vec3> positions; positions.resize(N * N);
    auto idx = [N](int r, int c) { return r * N + c; };

    // Build positions from world-space sampling
    glm::vec3 aabbMin(1e9f), aabbMax(-1e9f);
    for (int r = 0; r < N; ++r) {
        for (int c = 0; c < N; ++c) {
            float x = x0 + c * step;
            float z = z0 + r * step;
            float y = terrainHeight(x, z, P);
            glm::vec3 p(x, y, z);
            positions[idx(r, c)] = p;
            aabbMin = glm::min(aabbMin, p);
            aabbMax = glm::max(aabbMax, p);
        }
    }

    // Compute normals via central differences
    std::vector<glm::vec3> normals;
    computeNormals(positions, N, step, step, normals);

    // Pack vertices (localizing positions is optional; we’ll keep them in world space)
    std::vector<V> verts; verts.resize(N * N);
    for (int r = 0; r < N; ++r)
        for (int c = 0; c < N; ++c) {
            verts[idx(r, c)].p = positions[idx(r, c)];
            verts[idx(r, c)].n = normals[idx(r, c)];
        }

    // Build triangle indices: row strips
    std::vector<uint32_t> indices;
    indices.reserve((N - 1) * (N - 1) * 6);
    for (int r = 0; r < N - 1; ++r) {
        for (int c = 0; c < N - 1; ++c) {
            uint32_t i0 = idx(r, c);
            uint32_t i1 = idx(r, c + 1);
            uint32_t i2 = idx(r + 1, c);
            uint32_t i3 = idx(r + 1, c + 1);
            indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
            indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
        }
    }

    // Upload GL
    if (!tile.vao) glGenVertexArrays(1, &tile.vao);
    if (!tile.vbo) glGenBuffers(1, &tile.vbo);
    if (!tile.ibo) glGenBuffers(1, &tile.ibo);

    glBindVertexArray(tile.vao);

    glBindBuffer(GL_ARRAY_BUFFER, tile.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(V), verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tile.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

    // layout: location 0 = position, location 1 = normal
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, p));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, n));

    glBindVertexArray(0);

    tile.indexCount = (GLsizei)indices.size();
    tile.aabbMin = aabbMin;
    tile.aabbMax = aabbMax;
    tile.ready = true;
}
