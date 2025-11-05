//#include "Image.h"
#include "mesh.h"
#include "texture.h"
// Always include window first (because it includes glfw, which includes GL which needs to be included AFTER glew).
// Can't wait for modules to fix this stuff...
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glad/glad.h>
// Include glad before glfw3
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <imgui/imgui.h>
DISABLE_WARNINGS_POP()
#include <framework/shader.h>
#include <framework/window.h>
#include <functional>
#include <iostream>
#include <vector>
#include <framework/trackball.h>
#include <map>
#include "light.h"

#define SHADOWTEX_WIDTH 1024
#define SHADOWTEX_HEIGHT 1024

class Application {
public:
    Application()
        : m_window("Final Project", glm::ivec2(1024, 1024), OpenGLVersion::GL41)
        , m_defaultTexture(RESOURCE_ROOT "resources/checkerboard.png")
    {
        m_window.registerKeyCallback([this](int key, int scancode, int action, int mods) {
            if (action == GLFW_PRESS)
                onKeyPressed(key, mods);
            else if (action == GLFW_RELEASE)
                onKeyReleased(key, mods);
        });
        m_window.registerMouseMoveCallback(std::bind(&Application::onMouseMove, this, std::placeholders::_1));
        m_window.registerMouseButtonCallback([this](int button, int action, int mods) {
            if (action == GLFW_PRESS)
                onMouseClicked(button, mods);
            else if (action == GLFW_RELEASE)
                onMouseReleased(button, mods);
        });


        //std::vector<Mesh> cpuMeshes = loadMesh(RESOURCE_ROOT "resources/astronaut.obj");
        //std::cout << "Loaded cpu meshes" << std::endl;
        std::filesystem::path fileName{ "robo" }; //.obj and .mtl name should be same as folder name
        m_meshes = GPUMesh::loadMeshGPU(RESOURCE_ROOT "resources" / fileName / (fileName.string() + ".obj"));

        std::cout << "Meshes: " << m_meshes.size() << std::endl;

        loadAllTextures(fileName);

        try {
            ShaderBuilder defaultBuilder;
            defaultBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shader_vert.glsl");
            defaultBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/shader_frag.glsl");
            m_defaultShader = defaultBuilder.build();

            ShaderBuilder shadowBuilder;
            shadowBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shadow_vert.glsl");
            shadowBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "Shaders/shadow_frag.glsl");
            m_shadowShader = shadowBuilder.build();

            const Shader lightShader = ShaderBuilder().addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/light_vertex.glsl").addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/light_frag.glsl").build();
			ShaderBuilder lightShaderBuilder;
			lightShaderBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/light_vertex.glsl");
			lightShaderBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/light_frag.glsl");
			m_lightShader = lightShaderBuilder.build();

            // Any new shaders can be added below in similar fashion.
            // ==> Don't forget to reconfigure CMake when you do!
            //     Visual Studio: PROJECT => Generate Cache for ComputerGraphics
            //     VS Code: ctrl + shift + p => CMake: Configure => enter
            // ....
        } catch (ShaderLoadingException e) {
            std::cerr << e.what() << std::endl;
        }

        //*** For SHADOWS
        for (size_t i = 0; i < m_lightManager.getLights().size(); ++i) {
            GLuint texShadow;
            glGenTextures(1, &texShadow);
            glBindTexture(GL_TEXTURE_2D, texShadow);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, SHADOWTEX_WIDTH, SHADOWTEX_HEIGHT,
                0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

            m_shadowTextures.push_back(texShadow);

            // Create framebuffer
            GLuint framebuffer;
            glGenFramebuffers(1, &framebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texShadow, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            m_shadowFramebuffers.push_back(framebuffer);
        }

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void update()
    {
        while (!m_window.shouldClose()) {
            // This is your game loop
            // Put your real-time logic and rendering in here
            m_window.updateInput();

            // Use ImGui for easy input/output of ints, floats, strings, etc...
            ImGui::Begin("Window");
            ImGui::Checkbox("Mouse Controls", &m_useMouseControls);
            
            ImGui::Separator();
            ImGui::Text("Materials & Shading");
            ImGui::Checkbox("Material", &m_useMaterial);
            ImGui::Checkbox("PBR Shading", &m_usePBR);

            ImGui::Separator();
            ImGui::Text("Lights");
            ImGui::Checkbox("Shadows", &m_useShadows);
            ImGui::Checkbox("PCF", &m_usePCF);
            ImGui::SameLine();
            (ImGui::SliderInt("PCF Window", &m_PCFWindowSize, 3, 15, "%d"));
            if (m_PCFWindowSize % 2 == 0) m_PCFWindowSize += 1;
            ImGui::InputFloat3("Position", &m_lightManager.getLights()[m_lightManager.getSelectedIndex()].position.x); // changes x y and z
            ImGui::ColorEdit3("Color", &m_lightManager.getLights()[m_lightManager.getSelectedIndex()].color.r);
            ImGui::InputFloat3("Direction", &m_lightManager.getLights()[m_lightManager.getSelectedIndex()].direction.x); // changes x y and z
            ImGui::SliderAngle("Light FOV", &m_lightManager.getLights()[m_lightManager.getSelectedIndex()].fov, 60.0f, 120.0f);

            if (ImGui::Button("Add")) {
                m_lightManager.add();
            }

            ImGui::SameLine();

            if (ImGui::Button("Remove")) {
                m_lightManager.remove();
            }

            ImGui::SameLine();

            // Button for clearing lights
            if (ImGui::Button("Reset Lights")) {
                m_lightManager.reset();
            }

            // Display lights in scene
            std::vector<std::string> itemStrings = {};
            for (size_t i = 0; i < m_lightManager.getLights().size(); i++) {
                auto string = "Light " + std::to_string(i);
                itemStrings.push_back(string);
            }

            std::vector<const char*> itemCStrings = {};
            for (const auto& string : itemStrings) {
                itemCStrings.push_back(string.c_str());
            }

            int tempSelectedItem = static_cast<int>(m_lightManager.getSelectedIndex());
            if (ImGui::ListBox("Lights", &tempSelectedItem, itemCStrings.data(), (int)itemCStrings.size(), 4)) {
                m_lightManager.getSelectedIndex() = static_cast<size_t>(tempSelectedItem);
            }

            ImGui::End();

            // Clear the screen
            glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // ...
            glEnable(GL_DEPTH_TEST);

			/// use trackball logic to get view and projection matrices based on mouse input
            glm::mat4 projectionMatrix = m_projectionMatrix;
            glm::mat4 viewMatrix = m_viewMatrix;
            if (m_useMouseControls) {
                projectionMatrix = m_trackball.projectionMatrix();
                viewMatrix = m_trackball.viewMatrix();
            }
            const glm::mat4 mvpMatrix = projectionMatrix * viewMatrix * m_modelMatrix;
            // Normals should be transformed differently than positions (ignoring translations + dealing with scaling):
            // https://paroj.github.io/gltut/Illumination/Tut09%20Normal%20Transformation.html
            const glm::mat3 normalModelMatrix = glm::inverseTranspose(glm::mat3(m_modelMatrix));

            std::vector<glm::vec3> lightPositions;
            std::vector<glm::vec3> lightColors;

			// *** SHADOWS
            updateShadowTextures();
            std::vector<glm::mat4> lightMVPs;

            for (size_t lightIdx = 0; lightIdx < m_lightManager.getLights().size(); ++lightIdx) {
                const Light& light = m_lightManager.getLights()[lightIdx];

                glm::vec3 up = glm::vec3(0, 1, 0);
                glm::vec3 lightPos = light.position;
                glm::vec3 target = light.direction;
				float fov = light.fov;

                glm::mat4 lightView = glm::lookAt(lightPos, target, up);
                glm::mat4 lightProjection = glm::perspective(fov, 1.0f, 0.1f, 100.0f);
                glm::mat4 lightMVP = lightProjection * lightView * m_modelMatrix;
				lightMVPs.push_back(lightMVP);

                // Bind framebuffer for this light
                glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFramebuffers[lightIdx]);
                glViewport(0, 0, 1024, 1024);
                glClearDepth(1.0);
                glClear(GL_DEPTH_BUFFER_BIT);
                glEnable(GL_DEPTH_TEST);

                // Bind shadow shader
                m_shadowShader.bind();
                glUniformMatrix4fv(m_shadowShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(lightMVP));

                // Render each mesh
                for (GPUMesh& mesh : m_meshes) {
                    mesh.draw(m_shadowShader);
                }

                // Unbind framebuffer
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }

            // Reset viewport for main rendering
            glViewport(0, 0, m_window.getWindowSize().x, m_window.getWindowSize().y);

			// *** end SHADOWS


            for (const Light& light : m_lightManager.getLights()) {
                lightPositions.push_back(light.position);
                lightColors.push_back(light.color);
            }

            for (size_t i = 0; i < m_meshes.size(); ++i) {
                GPUMesh& mesh = m_meshes[i];
                m_defaultShader.bind();

                glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvpMatrix));
                //Uncomment this line when you use the modelMatrix (or fragmentPosition)
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(m_modelMatrix));
                glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(normalModelMatrix));

                // get all the textures from the map if the mesh has them
                std::filesystem::path kdTexPath;
                std::filesystem::path roughnessTexPath;
                std::filesystem::path metallicTexPath;
                std::filesystem::path normalTexPath;
                std::filesystem::path bumpTexPath;

                if (mesh.hasTextureCoords() && !mesh.m_texturePath.empty() && texturesMap.find(mesh.m_texturePath) != texturesMap.end())
                    kdTexPath = mesh.m_texturePath;
                if (mesh.hasRoughnessCoords() && !mesh.m_roughnessTexturePath.empty() && texturesMap.find(mesh.m_roughnessTexturePath) != texturesMap.end())
                    roughnessTexPath = mesh.m_roughnessTexturePath;
                if (mesh.hasRoughnessCoords() && !mesh.m_metallicTexturePath.empty() && texturesMap.find(mesh.m_metallicTexturePath) != texturesMap.end())
                    metallicTexPath = mesh.m_metallicTexturePath;
                if (mesh.hasNormalCoords() && !mesh.m_normalTexturePath.empty() && texturesMap.find(mesh.m_normalTexturePath) != texturesMap.end())
                    normalTexPath = mesh.m_normalTexturePath;
                if (mesh.hasNormalCoords() && !mesh.m_bumpTexturePath.empty() && texturesMap.find(mesh.m_bumpTexturePath) != texturesMap.end())
                    bumpTexPath = mesh.m_bumpTexturePath;

                if (!kdTexPath.empty()) { texturesMap.at(kdTexPath).bind(GL_TEXTURE0); glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_TRUE); }
                else { m_defaultTexture.bind(GL_TEXTURE0); glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_FALSE); }

                if (!roughnessTexPath.empty()) { texturesMap.at(roughnessTexPath).bind(GL_TEXTURE1); glUniform1i(m_defaultShader.getUniformLocation("hasRoughnessCoords"), GL_TRUE); }
                else { m_defaultTexture.bind(GL_TEXTURE1); glUniform1i(m_defaultShader.getUniformLocation("hasRoughnessCoords"), GL_FALSE); }

                if (!metallicTexPath.empty()) { texturesMap.at(metallicTexPath).bind(GL_TEXTURE2); glUniform1i(m_defaultShader.getUniformLocation("hasMetallicCoords"), GL_TRUE); }
                else { m_defaultTexture.bind(GL_TEXTURE2); glUniform1i(m_defaultShader.getUniformLocation("hasMetallicCoords"), GL_FALSE); }

                if (!normalTexPath.empty()) { texturesMap.at(normalTexPath).bind(GL_TEXTURE3); glUniform1i(m_defaultShader.getUniformLocation("hasNormalCoords"), GL_TRUE); }
                else { m_defaultTexture.bind(GL_TEXTURE3); glUniform1i(m_defaultShader.getUniformLocation("hasNormalCoords"), GL_FALSE); }

                if (!bumpTexPath.empty()) { texturesMap.at(bumpTexPath).bind(GL_TEXTURE4); glUniform1i(m_defaultShader.getUniformLocation("hasBumpCoords"), GL_TRUE); }
                else { m_defaultTexture.bind(GL_TEXTURE4); glUniform1i(m_defaultShader.getUniformLocation("hasBumpCoords"), GL_FALSE); }                // -- now we have all textures --

                glUniform1i(m_defaultShader.getUniformLocation("colorMap"), 0);
                glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), m_useMaterial);
                glUniform1i(m_defaultShader.getUniformLocation("usePBRShading"), m_usePBR);
                glUniform3fv(m_defaultShader.getUniformLocation("cameraPos"), 1, glm::value_ptr(m_trackball.position()));

                glUniform1i(m_defaultShader.getUniformLocation("numLights"), (int)lightPositions.size());
                glUniform3fv(m_defaultShader.getUniformLocation("lightPositions"), lightPositions.size(), glm::value_ptr(lightPositions[0]));
                glUniform3fv(m_defaultShader.getUniformLocation("lightColors"), lightColors.size(), glm::value_ptr(lightColors[0]));
                
				// *** SHADOWS
                glUniform1i(m_defaultShader.getUniformLocation("useShadows"), m_useShadows);
                glUniform1i(m_defaultShader.getUniformLocation("usePCF"), m_usePCF);
                glUniform1i(m_defaultShader.getUniformLocation("PCFWindowSize"), m_PCFWindowSize);

                glUniformMatrix4fv(m_defaultShader.getUniformLocation("lightMVPs"), lightMVPs.size(), GL_FALSE, glm::value_ptr(lightMVPs[0]));
				
                for (size_t idx = 0; idx < m_shadowTextures.size(); ++idx) {
                    glActiveTexture(GL_TEXTURE5 + idx);
                    glBindTexture(GL_TEXTURE_2D, m_shadowTextures[idx]);
                    std::string uniformName = "texShadow[" + std::to_string(idx) + "]";
                    glUniform1i(m_defaultShader.getUniformLocation(uniformName.c_str()), 5 + idx);
                }
                // *** end SHADOWS

                mesh.draw(m_defaultShader);
            }

			// Draw lights as squares
			drawLightsAsSquares(mvpMatrix);

            // Processes input and swaps the window buffer
            m_window.swapBuffers();
        }
    }

    // In here you can handle key presses
    // key - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__keys.html
    // mods - Any modifier keys pressed, like shift or control
    void onKeyPressed(int key, int mods)
    {
        //std::cout << "Key pressed: " << key << std::endl;
    }

    // In here you can handle key releases
    // key - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__keys.html
    // mods - Any modifier keys pressed, like shift or control
    void onKeyReleased(int key, int mods)
    {
        //std::cout << "Key released: " << key << std::endl;
    }

    // If the mouse is moved this function will be called with the x, y screen-coordinates of the mouse
    void onMouseMove(const glm::dvec2& cursorPos)
    {
        //std::cout << "Mouse at position: " << cursorPos.x << " " << cursorPos.y << std::endl;
    }
        
    // If one of the mouse buttons is pressed this function will be called
    // button - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__buttons.html
    // mods - Any modifier buttons pressed
    void onMouseClicked(int button, int mods)
    {
        //std::cout << "Pressed mouse button: " << button << std::endl;
    }

    // If one of the mouse buttons is released this function will be called
    // button - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__buttons.html
    // mods - Any modifier buttons pressed
    void onMouseReleased(int button, int mods)
    {
        //std::cout << "Released mouse button: " << button << std::endl;
    }

    void loadAllTextures(const std::filesystem::path& fileName) {
        for (size_t i = 0; i < m_meshes.size(); ++i) {
            const GPUMesh& mesh = m_meshes[i];
            std::filesystem::path kdTexPath = mesh.m_texturePath;
            std::filesystem::path roughnessTexPath = mesh.m_roughnessTexturePath;
            std::filesystem::path metallicTexPath = mesh.m_metallicTexturePath;
            std::filesystem::path normalTexPath = mesh.m_normalTexturePath;
            std::filesystem::path bumpTexPath = mesh.m_bumpTexturePath;

            std::cout << "Texture names: " << kdTexPath << roughnessTexPath << metallicTexPath << normalTexPath << bumpTexPath << std::endl;

            ////add textures to map, then use those
            if (!kdTexPath.empty()) {
                // Check if we've already loaded this texture
                if (texturesMap.find(kdTexPath.string()) == texturesMap.end()) {
                    // not found, put it to map
                    texturesMap.insert({ kdTexPath, Texture(RESOURCE_ROOT / fileName / kdTexPath) });
                }
                //else dont do anything because we already have the texture in the map
            }
            // if there is no texture associated skip it

            if (!roughnessTexPath.empty() && texturesMap.find(roughnessTexPath.string()) == texturesMap.end()) {
                texturesMap.insert({ roughnessTexPath, Texture(RESOURCE_ROOT / fileName / roughnessTexPath) });
            }

            if (!metallicTexPath.empty() && texturesMap.find(metallicTexPath.string()) == texturesMap.end()) {
                texturesMap.insert({ metallicTexPath, Texture(RESOURCE_ROOT / fileName / metallicTexPath) });
            }

            if (!normalTexPath.empty() && texturesMap.find(normalTexPath.string()) == texturesMap.end()) {
                texturesMap.insert({ normalTexPath, Texture(RESOURCE_ROOT / fileName / normalTexPath) });
            }

            if (!bumpTexPath.empty() && texturesMap.find(bumpTexPath.string()) == texturesMap.end()) {
                texturesMap.insert({ bumpTexPath, Texture(RESOURCE_ROOT / fileName / bumpTexPath) });
            }

        }

        std::cout << "Textrue map size: " << texturesMap.size() << std::endl;
    }

	void drawLightsAsSquares(glm::mat4 mvpMatrix) {
		m_lightShader.bind();
        {
            const glm::vec4 screenPos = mvpMatrix * glm::vec4(m_lightManager.getLights()[m_lightManager.getSelectedIndex()].position, 1.0f);
			const glm::vec3 color = m_lightManager.getLights()[m_lightManager.getSelectedIndex()].color;

            glPointSize(40.0f);
            glUniform4fv(m_lightShader.getUniformLocation("pos"), 1, glm::value_ptr(screenPos));
            glUniform3fv(m_lightShader.getUniformLocation("color"), 1, glm::value_ptr(color));
            //glBindVertexArray(vao);
            glDrawArrays(GL_POINTS, 0, 1);
            glBindVertexArray(0);
        }
        for (const Light& light : m_lightManager.getLights()) {
            const glm::vec4 screenPos = mvpMatrix * glm::vec4(light.position, 1.0f);
            const glm::vec3 color{ 1, 0, 0 };

            glPointSize(10.0f);
            glUniform4fv(m_lightShader.getUniformLocation("pos"), 1, glm::value_ptr(screenPos));
            glUniform3fv(m_lightShader.getUniformLocation("color"), 1, glm::value_ptr(light.color));
            //glBindVertexArray(vao);
            glDrawArrays(GL_POINTS, 0, 1);
            glBindVertexArray(0);

        }
    }

    void updateShadowTextures() {
        size_t numLights = m_lightManager.getLights().size();

		//add new textures if lights increased
        while (m_shadowTextures.size() < numLights) {
            GLuint texShadow;
            glGenTextures(1, &texShadow);
            glBindTexture(GL_TEXTURE_2D, texShadow);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, SHADOWTEX_WIDTH, SHADOWTEX_HEIGHT,
                0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            m_shadowTextures.push_back(texShadow);

            GLuint framebuffer;
            glGenFramebuffers(1, &framebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texShadow, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            m_shadowFramebuffers.push_back(framebuffer);
        }

        // remove excess textures if lights were taken out
        while (m_shadowTextures.size() > numLights) {
            glDeleteTextures(1, &m_shadowTextures.back());
            m_shadowTextures.pop_back();
            glDeleteFramebuffers(1, &m_shadowFramebuffers.back());
            m_shadowFramebuffers.pop_back();
        }
    }

private:
    Window m_window;

    // Shader for default rendering and for depth rendering
    Shader m_defaultShader;
    Shader m_shadowShader;
	Shader m_lightShader;

    std::vector<GLuint> m_shadowTextures;
    std::vector<GLuint> m_shadowFramebuffers;
    bool m_useShadows{ true };
	bool m_usePCF{ true };
	int m_PCFWindowSize{ 5 };

    LightManager m_lightManager;

    std::vector<GPUMesh> m_meshes;
    Texture m_defaultTexture;
    std::map<std::filesystem::path, Texture> texturesMap;
	bool m_useMaterial{ false };
    bool m_usePBR{ true };
    bool m_useMouseControls{ true };

    // Projection and view matrices for you to fill in and use
    //glm::mat4 m_projectionMatrix = glm::perspective(glm::radians(80.0f), 1.0f, 0.1f, 30.0f);
    //glm::mat4 m_viewMatrix = glm::lookAt(glm::vec3(-1, 1, -1), glm::vec3(0), glm::vec3(0, 1, 0));
    Trackball m_trackball{ &m_window, glm::radians(80.0f), glm::vec3(0.0f) }; // window, fov and look direction
    glm::mat4 m_projectionMatrix = glm::perspective(glm::radians(80.0f), 1.0f, 0.1f, 30.0f);
    glm::mat4 m_viewMatrix = glm::lookAt(glm::vec3(-1, 1, -1), glm::vec3(0), glm::vec3(0, 1, 0));
    glm::mat4 m_modelMatrix { 1.0f };
};

int main()
{
    Application app;
    app.update();

    return 0;
}
