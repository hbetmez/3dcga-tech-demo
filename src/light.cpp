#include "Light.h"

LightManager::LightManager()
    : selectedLightIndex(0)
{
    reset();
}

void LightManager::reset()
{
    lights.clear();
    add();
    selectedLightIndex = 0;
}

void LightManager::add()
{
    lights.push_back(Light{ glm::vec3(0.5, 0.5, 0.5), glm::vec3(1), glm::vec3(0.0), glm::radians(80.0f) });
}

void LightManager::remove()
{
    if (lights.size() > 1) {
        lights.erase(lights.begin() + selectedLightIndex);
        if (selectedLightIndex >= lights.size()) {
            selectedLightIndex = static_cast<int>(lights.size()) - 1;
        }
    }
}