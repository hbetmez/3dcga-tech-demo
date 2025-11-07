#include "terrain_manager.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

void TerrainManager::destroy() {
    for (auto& kv : tiles_) {
        kv.second.tile.destroy();
    }
    tiles_.clear();
}

void TerrainManager::ensureTile(int tx, int tz) {
    TileID id{ tx, tz };
    auto it = tiles_.find(id);
    if (it == tiles_.end()) {
        TileRecord rec;
        rec.tile.id = id;
        buildTerrainTile(rec.tile, params_);
        rec.lastTouchedFrame = frameCounter_;
        tiles_.emplace(id, std::move(rec));
    }
    else {
        it->second.lastTouchedFrame = frameCounter_;
    }
}

void TerrainManager::updateAndRender(const glm::vec3& cameraPos,
    const glm::mat4& viewProj,
    Shader& shader)
{
    ++frameCounter_;

    // which tile is our camera in?
    int curTx = (int)std::floor( cameraPos.x / params_.tileSize);
    int curTz = (int)std::floor( cameraPos.z / params_.tileSize);

    // ensuring ring of tiles exists
    for (int dz = -ringRadius_; dz <=  ringRadius_; ++dz) {
        for (int dx = -ringRadius_; dx <= ringRadius_; ++dx) {
            ensureTile(curTx + dx, curTz + dz);
        }
    }

    // Far tiles won't be displayed
    const int keepRadius = ringRadius_ + 2;
    std::vector<TileID> toErase;
    toErase.reserve(tiles_.size());
    for (auto& kv : tiles_) {
        int dx = kv.first.tx - curTx;
        int dz = kv.first.tz - curTz;
        if (std::abs(dx) > keepRadius || std::abs(dz) > keepRadius) {
            toErase.push_back(kv.first);
        }
    }
    for (auto& id : toErase) {
        auto it = tiles_.find(id);
        if (it != tiles_.end()) { it->second.tile.destroy(); tiles_.erase(it); }
    }

    // Drawing visible tiles
    shader.bind();

    // same as static model and normal model in applixation.cpp
    const glm::mat4 model = glm::mat4(1.0f);
    const glm::mat3  normalModel = glm::mat3(1.0f);

    glUniformMatrix4fv(shader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(shader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(normalModel));

    for (auto& kv : tiles_) {
        auto& t = kv.second.tile;
        if (!t.ready) continue;

        if (!aabbFrustumTestStub(t.aabbMin, t.aabbMax )) continue;

        glm::mat4 mvp = viewProj * model;
        glUniformMatrix4fv(shader.getUniformLocation( "mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvp));

        glBindVertexArray(t.vao);
        glDrawElements(GL_TRIANGLES, t.indexCount, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
}
