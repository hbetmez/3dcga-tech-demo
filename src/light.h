#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "Texture.h" // adjust path as needed

struct Light {
    glm::vec3 position;
    glm::vec3 color;
    //bool is_spotlight;
    //bool peels_depth;
    //glm::vec3 direction;
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