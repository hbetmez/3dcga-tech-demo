#pragma once
#include <unordered_map>
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "terrain_tile.h"
#include <framework/shader.h>

class TerrainManager {
public:
    TerrainManager() = default;

    void setParams(const TerrainParams& p) { 
        params_ = p;
    }
    const TerrainParams& params() const { 
        return params_;
    }

    // Per frame call with camera world position (viewProj is used for culling)
    void updateAndRender(const glm::vec3& cameraPos,
        const glm::mat4& viewProj,
        Shader& shader);

    // UI helper funcitons
    void setRingRadius(int r) { 
        ringRadius_ = glm::clamp(r, 1, 20);
    }
    int  ringRadius() const { 
        return ringRadius_;
    }

    void destroy();

private:
    TerrainParams params_{};
    int ringRadius_ = 2; // radius = 2 means that (2*2 + 1)^2 = 25 tiles are visible

    struct TileRecord { 
        TerrainTile tile;
        int lastTouchedFrame = -1;
    };

    std::unordered_map<TileID, TileRecord, TileIDHasher> tiles_;
    int frameCounter_ = 0;

    // internal helper functions
    void ensureTile(int tx, int tz);
    static bool aabbFrustumTestStub(const glm::vec3&, const glm::vec3&) { 
        return true;
    }
};
