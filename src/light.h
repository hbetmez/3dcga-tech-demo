#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "Texture.h"

struct Light {
    glm::vec3 position;
    glm::vec3 color;
	glm::vec3 direction;
    float fov;
    //bool is_spotlight;
    //bool peels_depth;
    //bool has_texture;
    //Texture texture;
};

class LightManager {
public:
    LightManager();

    void reset();
    void add();
    void remove();

    std::vector<Light>& getLights() { return lights; }
    int& getSelectedIndex() { return selectedLightIndex; }

private:
    std::vector<Light> lights;
    int selectedLightIndex;
};