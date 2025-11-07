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
#include <stb/stb_image.h>
#include "terrain_manager.h"
#include "terrain_tile.h"
#include "terrain_noise.h"

#define SHADOWTEX_WIDTH 1024
#define SHADOWTEX_HEIGHT 1024

struct BezierPoint {
    float t;
    float length; // from start / entire lenght (normalized)
};

class Application {
public:

    void updateBezierPoints() {
        std::vector<glm::vec3> tempCurvePoints;
        std::vector<BezierPoint> tempConstBezierLookup;

        glm::vec3 prevPoint = cubicBezier(m_curveControlPoints[0], m_curveControlPoints[1], m_curveControlPoints[2], m_curveControlPoints[3], 0.0f);
        tempCurvePoints.push_back(prevPoint);
        tempConstBezierLookup.push_back({ 0.0f, 0.0f });
        float sumLength = 0.0f;

        for (int i = 0; i < 3; ++i) {
            int base = i * 3;

            
            // divide each curve into 100 points
            for (float t = 0.0f + 1.0f / curveSampleCount; t <= 1.0f; t += 1.0f / curveSampleCount) {
                glm::vec3 tempPoint = cubicBezier(m_curveControlPoints[base], m_curveControlPoints[base + 1], m_curveControlPoints[base + 2], m_curveControlPoints[base + 3], t);
                sumLength += glm::length(tempPoint - prevPoint);
                tempCurvePoints.push_back(tempPoint);
                tempConstBezierLookup.push_back({ (i + t) / 3.0f, sumLength});
                prevPoint = tempPoint;
            }
        }
        // Table Lenth normalization
        for (auto& tableEntry : tempConstBezierLookup) {
            tableEntry.length = tableEntry.length / sumLength;
        }
        m_curvePoints = tempCurvePoints;
        m_constBezierLookupTable = tempConstBezierLookup;

    }
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

        // scroll callback
        m_window.registerScrollCallback([this](const glm::dvec2& offset) {onScroll(offset); });
        m_window.registerMouseButtonCallback([this](int button, int action, int mods) {
            if (action == GLFW_PRESS)
                onMouseClicked(button, mods);
            else if (action == GLFW_RELEASE)
                onMouseReleased(button, mods);
        });

		std::string skyboxPath = RESOURCE_ROOT "resources/space/";
        std::vector<std::string> faces{
            skyboxPath + "right.png",
            skyboxPath + "left.png",
            skyboxPath + "top.png",
            skyboxPath + "bottom.png",
            skyboxPath + "front.png",
            skyboxPath + "back.png"
        };

        float skyboxVertices[] = {
            // positions          
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f
        };

        glGenVertexArrays(1, &m_skyboxVAO);
        glGenBuffers(1, &m_skyboxVBO);
        glBindVertexArray(m_skyboxVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_skyboxVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glBindVertexArray(0);

        m_skyboxTexture = loadCubemap(faces);


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

			const Shader skyboxShader = ShaderBuilder().addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/skybox_vert.glsl").addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/skybox_frag.glsl").build();
			ShaderBuilder skyboxShaderBuilder;
			skyboxShaderBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/skybox_vert.glsl");
			skyboxShaderBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/skybox_frag.glsl");
			m_skyboxShader = skyboxShaderBuilder.build();

            // Line Shader for Curve
            ShaderBuilder lineBuilder;
            lineBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/line_vert.glsl");
            lineBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/line_frag.glsl");
            m_lineShader = lineBuilder.build();

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

        // Curve Visualization Setup
        ///////////////////////// start
        updateBezierPoints();

        glGenVertexArrays(1, &m_curveVAO);
        glBindVertexArray(m_curveVAO);

        glGenBuffers(1, &m_curveVBO);
        glBindBuffer(GL_ARRAY_BUFFER, m_curveVBO);
        glBufferData(GL_ARRAY_BUFFER, m_curvePoints.size() * sizeof(glm::vec3), m_curvePoints.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glBindVertexArray(0);
        ///////////////////////// end
 
        // Mesh positions
        m_meshesPositions.push_back(glm::vec3(0, 0, 0)); // main character position
        m_meshesPositions.push_back(glm::vec3(0, 0, 0)); // Object that moves along the curve


        // Terrain Gen
        m_terrainParams = TerrainParams{};         // defaults from header
        m_terrainParams.tileSize = 256.0f;
        m_terrainParams.vertsPerEdge = 129;
        m_terrainParams.heightScale = 30.0f;
        m_terrainParams.baseFrequency = 1.0f / 200.0f;
        m_terrainParams.octaves = 6;
        m_terrainParams.lacunarity = 2.0f;
        m_terrainParams.gain = 0.5f;
        m_terrainParams.seed = 1337u;             // pick any

        m_terrain.setParams(m_terrainParams);
        m_terrain.setRingRadius(m_tileRing);
    }

    void drawMesh(GPUMesh& mesh, const glm::mat4 &mvpMatrixLoc, const glm::mat3& normalModelMatrixLoc, 
        std::vector<glm::vec3> lightPositions, std::vector<glm::vec3> lightColors, std::vector<glm::mat4> lightMVPs) {
        m_defaultShader.bind();

        glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvpMatrixLoc));
        //Uncomment this line when you use the modelMatrix (or fragmentPosition)
        glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(m_modelMatrix));
        glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(normalModelMatrixLoc));

        // get all the textures from the map if the mesh has them
        std::filesystem::path kdTexPath;
        std::filesystem::path roughnessTexPath;
        std::filesystem::path metallicTexPath;
        std::filesystem::path normalTexPath;
        std::filesystem::path kaTexPath;
		std::filesystem::path emissiveTexPath;

        if (mesh.hasTextureCoords() && !mesh.m_texturePath.empty() && texturesMap.find(mesh.m_texturePath) != texturesMap.end())
            kdTexPath = mesh.m_texturePath;
        if (mesh.hasRoughnessCoords() && !mesh.m_roughnessTexturePath.empty() && texturesMap.find(mesh.m_roughnessTexturePath) != texturesMap.end())
            roughnessTexPath = mesh.m_roughnessTexturePath;
        if (mesh.hasRoughnessCoords() && !mesh.m_metallicTexturePath.empty() && texturesMap.find(mesh.m_metallicTexturePath) != texturesMap.end())
            metallicTexPath = mesh.m_metallicTexturePath;
        if (mesh.hasNormalCoords() && !mesh.m_normalTexturePath.empty() && texturesMap.find(mesh.m_normalTexturePath) != texturesMap.end())
            normalTexPath = mesh.m_normalTexturePath;
        if (mesh.hasKaCoords() && !mesh.m_KaTexturePath.empty() && texturesMap.find(mesh.m_KaTexturePath) != texturesMap.end())
            kaTexPath = mesh.m_KaTexturePath;
		if (mesh.hasEmissiveCoords() && !mesh.m_emissiveTexturePath.empty() && texturesMap.find(mesh.m_emissiveTexturePath) != texturesMap.end())
			emissiveTexPath = mesh.m_emissiveTexturePath;

		glActiveTexture(GL_TEXTURE0);
        if (!kdTexPath.empty()) { texturesMap.at(kdTexPath).bind(GL_TEXTURE0); glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_TRUE); }
        else { m_defaultTexture.bind(GL_TEXTURE0); glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_FALSE); }

		glActiveTexture(GL_TEXTURE1);
        if (!roughnessTexPath.empty()) { texturesMap.at(roughnessTexPath).bind(GL_TEXTURE1); glUniform1i(m_defaultShader.getUniformLocation("hasRoughnessCoords"), GL_TRUE); }
        else { m_defaultTexture.bind(GL_TEXTURE1); glUniform1i(m_defaultShader.getUniformLocation("hasRoughnessCoords"), GL_FALSE); }

		glActiveTexture(GL_TEXTURE2);
        if (!metallicTexPath.empty()) { texturesMap.at(metallicTexPath).bind(GL_TEXTURE2); glUniform1i(m_defaultShader.getUniformLocation("hasMetallicCoords"), GL_TRUE); }
        else { m_defaultTexture.bind(GL_TEXTURE2); glUniform1i(m_defaultShader.getUniformLocation("hasMetallicCoords"), GL_FALSE); }

		glActiveTexture(GL_TEXTURE3);
        if (!normalTexPath.empty()) { texturesMap.at(normalTexPath).bind(GL_TEXTURE3); glUniform1i(m_defaultShader.getUniformLocation("hasNormalCoords"), GL_TRUE); }
        else { m_defaultTexture.bind(GL_TEXTURE3); glUniform1i(m_defaultShader.getUniformLocation("hasNormalCoords"), GL_FALSE); }

		glActiveTexture(GL_TEXTURE4);
        if (!kaTexPath.empty()) { texturesMap.at(kaTexPath).bind(GL_TEXTURE4); glUniform1i(m_defaultShader.getUniformLocation("hasKaCoords"), GL_TRUE); }
        else { m_defaultTexture.bind(GL_TEXTURE4); glUniform1i(m_defaultShader.getUniformLocation("hasKaCoords"), GL_FALSE); }

		glActiveTexture(GL_TEXTURE5);
        if (!emissiveTexPath.empty()) { texturesMap.at(emissiveTexPath).bind(GL_TEXTURE5); glUniform1i(m_defaultShader.getUniformLocation("hasEmissiveCoords"), GL_TRUE); }
        else { m_defaultTexture.bind(GL_TEXTURE5); glUniform1i(m_defaultShader.getUniformLocation("hasEmissiveCoords"), GL_FALSE); }


        glUniform1i(m_defaultShader.getUniformLocation("colorMap"), 0);
		glUniform1i(m_defaultShader.getUniformLocation("roughnessMap"), 1);
		glUniform1i(m_defaultShader.getUniformLocation("metallicMap"), 2);
		glUniform1i(m_defaultShader.getUniformLocation("normalMap"), 3);
		glUniform1i(m_defaultShader.getUniformLocation("kaMap"), 4);
		glUniform1i(m_defaultShader.getUniformLocation("emissiveMap"), 5);
        glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), m_useMaterial);
        glUniform1i(m_defaultShader.getUniformLocation("usePBRShading"), m_usePBR);
		glUniform1i(m_defaultShader.getUniformLocation("useAOTex"), m_useAOTex);
		glUniform1i(m_defaultShader.getUniformLocation("useEmissive"), m_useEmissive);
        glUniform3fv(m_defaultShader.getUniformLocation("cameraPos"), 1, glm::value_ptr(m_trackball.position()));

        // ** environment mapping
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_skyboxTexture);
        glUniform1i(m_defaultShader.getUniformLocation("environmentMap"), 6);
		glUniform1i(m_defaultShader.getUniformLocation("useEnvironmentMapping"), m_useEnvironmentMapping);

        // *** SHADOWS
        glUniform1i(m_defaultShader.getUniformLocation("useShadows"), m_useShadows);
        glUniform1i(m_defaultShader.getUniformLocation("usePCF"), m_usePCF);
        glUniform1i(m_defaultShader.getUniformLocation("PCFWindowSize"), m_PCFWindowSize);

        glUniformMatrix4fv(m_defaultShader.getUniformLocation("lightMVPs"), lightMVPs.size(), GL_FALSE, glm::value_ptr(lightMVPs[0]));

        for (size_t idx = 0; idx < m_shadowTextures.size(); ++idx) {
            glActiveTexture(GL_TEXTURE7 + idx); //important: update texture slot if you need more textures
            glBindTexture(GL_TEXTURE_2D, m_shadowTextures[idx]);
            std::string uniformName = "texShadow[" + std::to_string(idx) + "]";
            // Important: update the constant value below to correspond to next available texture index
            glUniform1i(m_defaultShader.getUniformLocation(uniformName.c_str()), 7 + idx);
        }
        // *** end SHADOWS

        glUniform1i(m_defaultShader.getUniformLocation("numLights"), (int)lightPositions.size());
        glUniform3fv(m_defaultShader.getUniformLocation("lightPositions"), lightPositions.size(), glm::value_ptr(lightPositions[0]));
        glUniform3fv(m_defaultShader.getUniformLocation("lightColors"), lightColors.size(), glm::value_ptr(lightColors[0]));

        mesh.draw(m_defaultShader);
    }

    void CharacterWalkAnimation(const glm::mat4& viewProjMatrix,
        const glm::mat3& normalModelMatrixLoc,
        const std::vector<glm::vec3>& lightPositions,
        const std::vector<glm::vec3>& lightColors,
        float time,
        std::vector<glm::mat4> &lightMVPs)
    {
        
        glm::mat4 bodyTransform = glm::mat4(1.0f);
        float animTime = isWalking ? (time - walkStartTime) : 0.0f;

        float legSwingParam = glm::radians(30.0f) * sin(animTime * 4.0f);
        float armSwingParam = glm::radians(30.0f) * sin(animTime * 4.0f + glm::pi<float>());
        // Offsets from origin to joints
        const float footY = 0.2185f;
        const float legY = 0.3804f;
        const float handY = 0.5217f;
        const float armY = 0.6023f;

        // body
        drawMesh(m_meshes[0], viewProjMatrix * bodyTransform, normalModelMatrixLoc, lightPositions, lightColors, lightMVPs);

        

        // legs
        glm::mat4 rightLegTransform = glm::mat4(1.0f);
        glm::mat4 leftLegTransform = glm::mat4(1.0f);

        if (isWalking) {
            rightLegTransform = bodyTransform
                * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, legY, 0.0f))
                * glm::rotate(glm::mat4(1.0f), legSwingParam, glm::vec3(1, 0, 0))
                * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -legY, 0.0f));

            leftLegTransform = bodyTransform
                * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, legY, 0.0f))
                * glm::rotate(glm::mat4(1.0f), -legSwingParam, glm::vec3(1, 0, 0))
                * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -legY, 0.0f));
        }
        
        drawMesh(m_meshes[3], viewProjMatrix * rightLegTransform, normalModelMatrixLoc, lightPositions, lightColors, lightMVPs);
        drawMesh(m_meshes[4], viewProjMatrix * leftLegTransform, normalModelMatrixLoc, lightPositions, lightColors, lightMVPs);

        // Foots
        glm::mat4 rightFootTransform = glm::mat4(1.0f);
        glm::mat4 leftFootTransform = glm::mat4(1.0f);

        if (isWalking) {
            rightFootTransform = rightLegTransform
                * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, footY, 0.0f))
                * glm::rotate(glm::mat4(1.0f), -0.5f * legSwingParam, glm::vec3(1, 0, 0))
                * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -footY, 0.0f));

            leftFootTransform = leftLegTransform
                * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, footY, 0.0f))
                * glm::rotate(glm::mat4(1.0f), 0.5f * legSwingParam, glm::vec3(1, 0, 0))
                * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -footY, 0.0f));
        }
        
        drawMesh(m_meshes[1], viewProjMatrix * rightFootTransform, normalModelMatrixLoc, lightPositions, lightColors, lightMVPs);
        drawMesh(m_meshes[2], viewProjMatrix * leftFootTransform, normalModelMatrixLoc, lightPositions, lightColors, lightMVPs);

        // arms
        glm::mat4 rightArmTransform = glm::mat4(1.0f);
        glm::mat4 leftArmTransform = glm::mat4(1.0f);
        if (isWalking) {
            rightArmTransform = bodyTransform
                * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, armY, 0.0f))
                * glm::rotate(glm::mat4(1.0f), armSwingParam, glm::vec3(1, 0, 0))
                * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -armY, 0.0f));

            leftArmTransform = bodyTransform
                * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, armY, 0.0f))
                * glm::rotate(glm::mat4(1.0f), -armSwingParam, glm::vec3(1, 0, 0))
                * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -armY, 0.0f));
        }
        
        drawMesh(m_meshes[7], viewProjMatrix * rightArmTransform, normalModelMatrixLoc, lightPositions, lightColors, lightMVPs);
        drawMesh(m_meshes[8], viewProjMatrix * leftArmTransform, normalModelMatrixLoc, lightPositions, lightColors, lightMVPs);

        // Hands
        glm::mat4 rightHandTransform = glm::mat4(1.0f);
        glm::mat4 leftHandTransform = glm::mat4(1.0f);
        if (isWalking) {
            rightHandTransform = rightArmTransform
                * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, handY, 0.0f))
                * glm::rotate(glm::mat4(1.0f), 0.5f * armSwingParam, glm::vec3(1, 0, 0))
                * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -handY, 0.0f));

            leftHandTransform = leftArmTransform
                * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, handY, 0.0f))
                * glm::rotate(glm::mat4(1.0f), -0.5f * armSwingParam, glm::vec3(1, 0, 0))
                * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -handY, 0.0f));
        }
        
        drawMesh(m_meshes[5], viewProjMatrix * rightHandTransform, normalModelMatrixLoc, lightPositions, lightColors, lightMVPs);
        drawMesh(m_meshes[6], viewProjMatrix * leftHandTransform, normalModelMatrixLoc, lightPositions, lightColors, lightMVPs);

        // Head
        drawMesh(m_meshes[9], viewProjMatrix * bodyTransform, normalModelMatrixLoc, lightPositions, lightColors, lightMVPs);
    }

    glm::vec3 cubicBezier(const glm::vec3& p0, const glm::vec3& p1,
        const glm::vec3& p2, const glm::vec3& p3, float t)
    {
        glm::vec3 point = (1.0f - t) * (1.0f - t) * (1.0f - t) * p0;
        point += 3 * (1.0f - t) * (1.0f - t) * t * p1;
        point += 3 * (1.0f - t) * t * t * p2;
        point += t * t * t * p3;
        return point;
    }

    glm::vec3 getOnCurvePoint(float t) {
        t = glm::clamp(t, 0.0f, 1.0f);
        int curveIdx = static_cast<int>(t * 3.0f);
        if (curveIdx > 2) curveIdx = 2;

        float newT = (t * 3.0f) - curveIdx;
        int base = curveIdx * 3;

        return cubicBezier(m_curveControlPoints[base], m_curveControlPoints[base + 1], m_curveControlPoints[base + 2], m_curveControlPoints[base + 3], newT);

    }

    float lengthToT(float l)
    {
        if (l <= 0.0f) return 0.0f;
        if (l >= 1.0f) return 1.0f;

        for (size_t i = 1; i < m_constBezierLookupTable.size(); ++i) {
            if (l <= m_constBezierLookupTable[i].length) {
                const BezierPoint& prev = m_constBezierLookupTable[i - 1];
                const BezierPoint& curr = m_constBezierLookupTable[i];
                
                // linear interpolation
                float ratio = (l - prev.length) / (curr.length - prev.length);
                return glm::mix(prev.t, curr.t, ratio);
            }
        }
        
    }

    glm::vec3 cubicBezierTangent(const glm::vec3& p0, const glm::vec3& p1,
        const glm::vec3& p2, const glm::vec3& p3, float t)
    {
        // It is just derivative of the bezier curve.
        glm::vec3 point = 3.0f * (1.0f - t) * (1.0f - t) * (p1 - p0) + 6.0f * (1.0f - t) * t * (p2 - p1) + 3.0f * t * t * (p3 - p2);
        return point;
    }

    void update()
    {
        while (!m_window.shouldClose()) {
            float currentTime = glfwGetTime();
            // This is your game loop
            // Put your real-time logic and rendering in here
            m_window.updateInput();

             // 0 = fixed, 1 = mouse trackball, 2 = thirdperson
            const char* cameraModes[] = { "Fixed Camera", "Mouse Trackball", "Third Person", "Top View"};

            // Use ImGui for easy input/output of ints, floats, strings, etc...
            ImGui::Begin("Window");
            ImGui::Combo("Camera Mode", &cameraMode, cameraModes, IM_ARRAYSIZE(cameraModes));
            ImGui::Checkbox("Show Bezier Curve", &m_showCurve);
            ImGui::Checkbox("Move Along Curve", &m_moveAlongCurve);
            ImGui::Checkbox("Const Speed", &m_constSpeedMoveCurve);
            ImGui::SliderFloat("On-Curve Speed", &onCurveMoveSpeed, 0.0f, 0.2f, "%.3f");
            
            ImGui::Checkbox("Walking animation", &isWalking);
            if (isWalking) m_useEmissive = true;
            else m_useEmissive = false;
            
            ImGui::Separator();
            ImGui::Text("Materials & Shading");
            ImGui::Checkbox("Material", &m_useMaterial);
            ImGui::Checkbox("PBR Shading", &m_usePBR);
			
            ImGui::Checkbox("AO Texture", &m_useAOTex);
			ImGui::Checkbox("Emissive", &m_useEmissive);

            ImGui::Separator();
            ImGui::Text("Environment Mapping");
            ImGui::Checkbox("Use Environment Mapping", &m_useEnvironmentMapping);

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

            ImGui::Separator();
            ImGui::Text("Terrain");
            ImGui::Checkbox("Draw Terrain", &m_drawTerrain);
            ImGui::SliderInt("Tile Ring Radius", &m_tileRing, 1, 6);
            if (m_tileRing != m_terrain.ringRadius())
                m_terrain.setRingRadius(m_tileRing);

            ImGui::SliderFloat("Height Scale", &m_terrainParams.heightScale, 1.0f, 200.0f, "%.1f");
            ImGui::SliderFloat("Base Freq (1/m)", &m_terrainParams.baseFrequency, 1.0f / 1000.0f, 1.0f / 10.0f, "%.5f", ImGuiSliderFlags_Logarithmic);
            ImGui::SliderInt("Octaves", &m_terrainParams.octaves, 1, 10);
            ImGui::SliderFloat("Lacunarity", &m_terrainParams.lacunarity, 1.5f, 3.0f);
            ImGui::SliderFloat("Gain", &m_terrainParams.gain, 0.2f, 0.8f);
            if (ImGui::Button("Apply Terrain Params")) {
                m_terrain.destroy();
                m_terrain.setParams(m_terrainParams);
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

            if (m_moveAlongCurve) {
                float t = fmod(currentTime * onCurveMoveSpeed, 1.0f);
                if (m_constSpeedMoveCurve) {
                    t = lengthToT(t);
                }
                glm::vec3 onCurvePos = getOnCurvePoint(t);
                m_meshesPositions[1] = onCurvePos;
            }

			/// use trackball logic to get view and projection matrices based on mouse input
            glm::mat4 projectionMatrix = m_projectionMatrix;
            glm::mat4 viewMatrix = m_viewMatrix;

            glm::vec3 characterPosition = !m_meshesPositions.empty() ? m_meshesPositions[0] : glm::vec3(0.0f);
            glm::vec3 objectOnCurvePos = m_meshesPositions.size() >= 2 ? m_meshesPositions[1] : glm::vec3(0.0f);

            if (cameraMode == 1) {
                projectionMatrix = m_trackball.projectionMatrix();
                viewMatrix = m_trackball.viewMatrix();
            } else if (cameraMode == 2) {
                // third person
                glm::vec3 offset;
                offset.x = m_camDistance * cos(m_camPitch) * sin(m_camYaw);
                offset.y = m_camDistance * sin(m_camPitch);
                offset.z = m_camDistance * cos(m_camPitch) * cos(m_camYaw);

                glm::vec3 cameraPos = characterPosition + offset;
                glm::vec3 targetPos = characterPosition + glm::vec3(0, 0.5217, 0);

                viewMatrix = glm::lookAt(cameraPos, targetPos, glm::vec3(0, 1, 0));
            } else if (cameraMode == 3) {
                // top down static cam
                glm::vec3 cameraPos = characterPosition + glm::vec3(0.0f, m_camDistance, 0.0f);
                glm::vec3 targetPos = characterPosition;

                viewMatrix = glm::lookAt(cameraPos, targetPos, glm::vec3(0.0f, 0.0f, 1.0f));
            }

            
            glm::mat4 modelMatrixCharacter = glm::translate(glm::mat4(1.0f), characterPosition);
            glm::mat4 modelMatrixCurveObj = glm::translate(glm::mat4(1.0f), objectOnCurvePos);

            // Character MVP
            const glm::mat4 mvpMatrixCharacter = projectionMatrix * viewMatrix * modelMatrixCharacter;

            // Object on Curve
            const glm::mat4 mvpMatrixCurveObj = projectionMatrix * viewMatrix * modelMatrixCurveObj;

            // Static MVP
            const glm::mat4 mvpMatrixStatic = projectionMatrix * viewMatrix * m_modelMatrix;
            // Normals should be transformed differently than positions (ignoring translations + dealing with scaling):
            // https://paroj.github.io/gltut/Illumination/Tut09%20Normal%20Transformation.html
            const glm::mat3 normalModelMatrix = glm::inverseTranspose(glm::mat3(m_modelMatrix));

            std::vector<glm::vec3> lightPositions;
            std::vector<glm::vec3> lightColors;

            for (const Light& light : m_lightManager.getLights()) {
                lightPositions.push_back(light.position);
                lightColors.push_back(light.color);
            }

			// *** SHADOWS
            updateShadowTextures();
            std::vector<glm::mat4> lightMVPs;

            // build light mvps
            for (size_t lightIdx = 0; lightIdx < m_lightManager.getLights().size(); ++lightIdx) {
                const Light& light = m_lightManager.getLights()[lightIdx];

                glm::vec3 up = glm::vec3(0, 1, 0);
                glm::vec3 lightPos = light.position;
                glm::vec3 target = light.direction;
				float fov = light.fov;

                glm::mat4 lightView = glm::lookAt(lightPos, target, up);
                glm::mat4 lightProjection = glm::perspective(fov, 1.0f, 0.1f, 100.0f);
                glm::mat4 lightMVP = lightProjection * lightView; //* m_modelMatrix
				lightMVPs.push_back(lightMVP);
            }

            // render shadows
            for (size_t lightIdx = 0; lightIdx < m_lightManager.getLights().size(); ++lightIdx) {
                // Bind framebuffer for this light
                glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFramebuffers[lightIdx]);
                glViewport(0, 0, 1024, 1024);
                glClearDepth(1.0);
                glClear(GL_DEPTH_BUFFER_BIT);
                glEnable(GL_DEPTH_TEST);

                // Bind shadow shader
                m_shadowShader.bind();
                //glUniformMatrix4fv(m_shadowShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(lightMVP));

                // Render each mesh
                CharacterWalkAnimation(lightMVPs[lightIdx], normalModelMatrix, lightPositions, lightColors, currentTime, lightMVPs);

                // this is for STATIC SHADOWS
                for (size_t meshIdx = 10; meshIdx < m_meshes.size(); ++meshIdx) {
                    GPUMesh& mesh = m_meshes[meshIdx];

                    glUniformMatrix4fv(m_shadowShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(lightMVPs[lightIdx]));

                    mesh.draw(m_shadowShader);
                }

                // Unbind framebuffer
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
            }

            // Reset viewport for main rendering
            glViewport(0, 0, m_window.getWindowSize().x, m_window.getWindowSize().y);

			// *** end SHADOWS

            // Terrain Gen
            const glm::mat4 viewProj = projectionMatrix * viewMatrix;

            // ... your character/lights/shadows as before ...

            // Draw the terrain early (opaque)
            if (m_drawTerrain) {
                // We’ll reuse your default shader (it expects many uniforms; set the essentials it needs).
                // Provide cameraPos if your shader expects it (you already set it when drawing meshes).
                // If your default shader samples PBR textures that aren’t bound for terrain, consider
                // adding a simple terrain branch or a dedicated terrain shader later.
                m_defaultShader.bind();

                // Minimal light uniforms (reusing your existing arrays) are already set before drawMesh,
                // but if the shader requires them, set/keep them consistent here as well.

                m_terrain.updateAndRender(m_trackball.position(), viewProj, m_defaultShader);
            }


            if (!isWalking) walkStartTime = currentTime;
            CharacterWalkAnimation(mvpMatrixCharacter, normalModelMatrix, lightPositions, lightColors, currentTime, lightMVPs);

            // Temporary using character instead of drone
            CharacterWalkAnimation(mvpMatrixCurveObj, normalModelMatrix, lightPositions, lightColors, currentTime, lightMVPs);

            

			// Draw lights as squares
			drawLightsAsSquares(mvpMatrixStatic);

            // Drawing the Curve
            if (m_showCurve) {
                m_lineShader.bind();
                glUniformMatrix4fv(m_lineShader.getUniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(mvpMatrixStatic));

                glBindVertexArray(m_curveVAO);
                glLineWidth(10.0f);
                glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)m_curvePoints.size());
                glBindVertexArray(0);
            }
            

			// Draw skybox last
            drawSkybox(projectionMatrix, viewMatrix);

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
        if (cameraMode == 2) {
            float dx = static_cast<float>(cursorPos.x - m_lastMousePos.x);
            float dy = static_cast<float>(cursorPos.y - m_lastMousePos.y);

            m_camYaw += dx * sensitivity;
            m_camPitch -= dy * sensitivity;

            m_camPitch = glm::clamp(m_camPitch, glm::radians(-89.9f), glm::radians(89.9f));
        }
        m_lastMousePos = cursorPos;
    }

    void onScroll(const glm::dvec2& offset) {
        if (cameraMode == 2 || cameraMode == 3) {
            m_camDistance -= static_cast<float>(offset.y) * 0.3f; 
            m_camDistance = glm::clamp(m_camDistance, 1.0f, 15.0f);
        }
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
            std::filesystem::path kaTexPath = mesh.m_KaTexturePath;
			std::filesystem::path emissiveTexPath = mesh.m_emissiveTexturePath;

            std::cout << "Texture names: " << kdTexPath << roughnessTexPath << metallicTexPath << normalTexPath << kaTexPath << emissiveTexPath << std::endl;

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

            if (!kaTexPath.empty() && texturesMap.find(kaTexPath.string()) == texturesMap.end()) {
                texturesMap.insert({ kaTexPath, Texture(RESOURCE_ROOT / fileName / kaTexPath) });
            }

            if (!emissiveTexPath.empty() && texturesMap.find(emissiveTexPath.string()) == texturesMap.end()) {
                texturesMap.insert({ emissiveTexPath, Texture(RESOURCE_ROOT / fileName / emissiveTexPath) });
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

    GLuint loadCubemap(std::vector<std::string> faces) {
        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

        int width, height, nrChannels;
        for (unsigned int i = 0; i < faces.size(); i++) {
            unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
            if (data) {
                GLenum format;
                if (nrChannels == 1)
                    format = GL_RED;
                else if (nrChannels == 3)
                    format = GL_RGB;
                else if (nrChannels == 4)
                    format = GL_RGBA;

                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                    0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
                stbi_image_free(data);
            }
            else {
                std::cout << "Cubemap failed to load at path: " << faces[i] << std::endl;
                stbi_image_free(data);
            }
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        return textureID;
    }

    void drawSkybox(const glm::mat4& projectionMatrix, const glm::mat4& viewMatrix) {
		glDepthFunc(GL_LEQUAL); // sky will be drawn at the farthest depth

        m_skyboxShader.bind();

		// no translation for skybox view matrix
        glm::mat4 view = glm::mat4(glm::mat3(viewMatrix));

        glUniformMatrix4fv(m_skyboxShader.getUniformLocation("view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(m_skyboxShader.getUniformLocation("projection"), 1, GL_FALSE, glm::value_ptr(projectionMatrix));

        // bind textures
        glBindVertexArray(m_skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_skyboxTexture);
        glUniform1i(m_skyboxShader.getUniformLocation("skybox"), 0);

        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);
    }


private:
    Window m_window;

    // Shader for default rendering and for depth rendering
    Shader m_defaultShader;
    Shader m_shadowShader;
	Shader m_lightShader;
	Shader m_skyboxShader;
    GLuint m_skyboxVAO;
    GLuint m_skyboxVBO;

    GLuint m_skyboxTexture;
	bool m_useEnvironmentMapping{ true };

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
	bool m_useAOTex{ true };
	bool m_useEmissive{ false };
    int cameraMode = 0;

    // Projection and view matrices for you to fill in and use
    //glm::mat4 m_projectionMatrix = glm::perspective(glm::radians(80.0f), 1.0f, 0.1f, 30.0f);
    //glm::mat4 m_viewMatrix = glm::lookAt(glm::vec3(-1, 1, -1), glm::vec3(0), glm::vec3(0, 1, 0));
    Trackball m_trackball{ &m_window, glm::radians(80.0f), glm::vec3(0.0f) }; // window, fov and look direction
    glm::mat4 m_projectionMatrix = glm::perspective(glm::radians(80.0f), 1.0f, 0.1f, 30.0f);
    glm::mat4 m_viewMatrix = glm::lookAt(glm::vec3(-1, 1, -1), glm::vec3(0), glm::vec3(0, 1, 0));
    glm::mat4 m_modelMatrix { 1.0f };

    bool isWalking = false;
    float walkStartTime = 0.0f;

    // Third-person camera parameters
    float m_camYaw = glm::radians(180.0f);   
    float m_camPitch = glm::radians(10.0f);  
    float m_camDistance = 2.0f;             
    glm::vec2 m_lastMousePos{ 0.0f };
    float sensitivity = 0.01f;
    
    std::vector<glm::vec3> m_meshesPositions;

    // Curve instances
    std::vector<glm::vec3> m_curveControlPoints =
    {
        {0, 0, 0},   // curve 1 p0 
        {1, 0.2, 1},   // curve 1 p1 
        {-1, 0.4, 1},   // curve 1 p2 
        {-1, 0.6, -1},  // curve 1 p3, curve 2 p0 
        {1, 0.8, -1}, // curve 2 p1 
        {1, 1, 1},  // curve 2 p2 
        {-1, 1.2, 1},   // curve 2 p3, curve 3 p0 
        {-1, 1.4, -1},   // curve 3 p1 
        {1, 1.6, -1},   // curve 3 p2 
        {1, 1.8, 1}    // curve 3 p3 
    };
    bool m_showCurve = false;
    bool m_moveAlongCurve = true;
    bool m_constSpeedMoveCurve = false;
    
    Shader m_lineShader;
    GLuint m_curveVAO = 0, m_curveVBO = 0;
    std::vector<glm::vec3> m_curvePoints;
    std::vector<BezierPoint> m_constBezierLookupTable;
    float curveSampleCount = 100.0f;
    float onCurveMoveSpeed = 0.06;

    // Terraing gen
    TerrainManager m_terrain;
    TerrainParams  m_terrainParams;
    bool m_drawTerrain = true;
    int  m_tileRing = 2; // 2 -> 25 tiles

};

int main()
{
    Application app;
    app.update();

    return 0;
}
