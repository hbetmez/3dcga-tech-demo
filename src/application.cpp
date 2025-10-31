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

class Application {
public:
    Application()
        : m_window("Final Project", glm::ivec2(1024, 1024), OpenGLVersion::GL41)
        , m_texture(RESOURCE_ROOT "resources/checkerboard.png")
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

        m_astronaut = GPUMesh::loadMeshGPU(RESOURCE_ROOT "resources/AstronoutTest.obj");

        try {
            ShaderBuilder defaultBuilder;
            defaultBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shader_vert.glsl");
            defaultBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/shader_frag.glsl");
            m_defaultShader = defaultBuilder.build();

            ShaderBuilder shadowBuilder;
            shadowBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shadow_vert.glsl");
            shadowBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "Shaders/shadow_frag.glsl");
            m_shadowShader = shadowBuilder.build();

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

        // Curve Visualization Setup
        ///////////////////////// start
        for (int i = 0; i < 3; ++i) {
            int base = i * 3;
            for (float t = 0.0f; t <= 1.0f; t += 0.01f) {
                m_curvePoints.push_back(cubicBezier(
                    m_curveControlPoints[base], m_curveControlPoints[base + 1],
                    m_curveControlPoints[base + 2], m_curveControlPoints[base + 3], t));
            }
        }

        glGenVertexArrays(1, &m_curveVAO);
        glBindVertexArray(m_curveVAO);

        glGenBuffers(1, &m_curveVBO);
        glBindBuffer(GL_ARRAY_BUFFER, m_curveVBO);
        glBufferData(GL_ARRAY_BUFFER, m_curvePoints.size() * sizeof(glm::vec3), m_curvePoints.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glBindVertexArray(0);
        ///////////////////////// end
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

    glm::vec3 cubicBezierTangent(const glm::vec3& p0, const glm::vec3& p1,
        const glm::vec3& p2, const glm::vec3& p3, float t)
    {
        // It is just derivative of the bezier curve.
        glm::vec3 point = 3.0f * (1.0f - t) * (1.0f - t) * (p1 - p0) + 6.0f * (1.0f - t) * t * (p2 - p1) + 3.0f * t * t * (p3 - p2);
        return point;
    }

    void updateCameraAlongPath(float time) {
        int numberOfCurves = 3;
        float t = fmod(time * 0.1f, 1.0f); // remainder
        float curveLength = 1.0f / numberOfCurves; // [0,1/3] is first segment, [1/3,2/3] second segment...
        int curveIndex = std::min(int(t / curveLength), numberOfCurves - 1);
        float localT = (t - (curveIndex * curveLength)) / curveLength;

        float firstControlPoint = curveIndex * 3;
        glm::vec3 pos = cubicBezier(m_curveControlPoints[firstControlPoint],
            m_curveControlPoints[firstControlPoint + 1],
            m_curveControlPoints[firstControlPoint + 2],
            m_curveControlPoints[firstControlPoint + 3],
            localT);
        glm::vec3 tangent = glm::normalize(cubicBezierTangent(
            m_curveControlPoints[firstControlPoint],
            m_curveControlPoints[firstControlPoint + 1],
            m_curveControlPoints[firstControlPoint + 2],
            m_curveControlPoints[firstControlPoint + 3], localT));

            glm::vec3 up = glm::vec3(0, 1, 0);
            glm::vec3 right = glm::normalize(glm::cross(tangent, up));
            up = glm::cross(right, tangent); // making sure all 3 are perpendicular to each other.
            m_viewMatrix = glm::lookAt(pos, pos + tangent, up);

    }

    void drawSubMesh(const glm::mat4& mvp, Shader& shader, GPUMesh& mesh, const glm::mat4& normalModelMatrix) {
        shader.bind();
        glUniformMatrix4fv(shader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvp));
        //Uncomment this line when you use the modelMatrix (or fragmentPosition)
        //glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(m_modelMatrix));
        glUniformMatrix3fv(shader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(normalModelMatrix));
        if (mesh.hasTextureCoords()) {
            m_texture.bind(GL_TEXTURE0);
            glUniform1i(shader.getUniformLocation("colorMap"), 0);
            glUniform1i(shader.getUniformLocation("hasTexCoords"), GL_TRUE);
            glUniform1i(shader.getUniformLocation("useMaterial"), GL_FALSE);
        }
        else {
            glUniform1i(shader.getUniformLocation("hasTexCoords"), GL_FALSE);
            glUniform1i(shader.getUniformLocation("useMaterial"), m_useMaterial);
        }
        mesh.draw(shader);
    }

    void update()
    {
        int dummyInteger = 0; // Initialized to 0
        while (!m_window.shouldClose()) {
            // This is your game loop
            // Put your real-time logic and rendering in here
            m_window.updateInput();

            // Use ImGui for easy input/output of ints, floats, strings, etc...
            ImGui::Begin("Window");
            ImGui::InputInt("This is an integer input", &dummyInteger); // Use ImGui::DragInt or ImGui::DragFloat for larger range of numbers.
            ImGui::Text("Value is: %i", dummyInteger); // Use C printf formatting rules (%i is a signed integer)
            ImGui::Checkbox("Use material if no texture", &m_useMaterial);

            // Drawing bezier curve
            ImGui::Checkbox("Show Camera Path", &m_showCurve);
            ImGui::Checkbox("Move Camera Along Path", &m_moveAlongCurve);
               
            
            ImGui::End();

            // Clear the screen
            glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // ...
            glEnable(GL_DEPTH_TEST);

            const glm::mat4 mvpMatrix = m_projectionMatrix * m_viewMatrix * m_modelMatrix;
            // Normals should be transformed differently than positions (ignoring translations + dealing with scaling):
            // https://paroj.github.io/gltut/Illumination/Tut09%20Normal%20Transformation.html
            const glm::mat3 normalModelMatrix = glm::inverseTranspose(glm::mat3(m_modelMatrix));

            // Camera Movement:
            if (m_moveAlongCurve) {
                updateCameraAlongPath(glfwGetTime());
            }
            else {
                // Default camera position will be here!!
                m_viewMatrix = glm::lookAt(glm::vec3(-1, 1, -1), glm::vec3(0), glm::vec3(0, 1, 0));
            }
            
            
            for (GPUMesh& mesh : m_astronaut) {
                drawSubMesh(mvpMatrix, m_defaultShader, mesh, normalModelMatrix);
            }
            
            
            // Showing the Curve
            if (m_showCurve) {
                m_lineShader.bind();
                glm::mat4 mvp = m_projectionMatrix * m_viewMatrix * glm::mat4(1.0f);
                glUniformMatrix4fv(m_lineShader.getUniformLocation("mvp"), 1, GL_FALSE, glm::value_ptr(mvp));

                glBindVertexArray(m_curveVAO);
                glLineWidth(2.0f);
                glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)m_curvePoints.size());
                glBindVertexArray(0);
            }


            // Processes input and swaps the window buffer
            m_window.swapBuffers();
        }
    }

    // In here you can handle key presses
    // key - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__keys.html
    // mods - Any modifier keys pressed, like shift or control
    void onKeyPressed(int key, int mods)
    {
        std::cout << "Key pressed: " << key << std::endl;
    }

    // In here you can handle key releases
    // key - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__keys.html
    // mods - Any modifier keys pressed, like shift or control
    void onKeyReleased(int key, int mods)
    {
        std::cout << "Key released: " << key << std::endl;
    }

    // If the mouse is moved this function will be called with the x, y screen-coordinates of the mouse
    void onMouseMove(const glm::dvec2& cursorPos)
    {
        std::cout << "Mouse at position: " << cursorPos.x << " " << cursorPos.y << std::endl;
    }

    // If one of the mouse buttons is pressed this function will be called
    // button - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__buttons.html
    // mods - Any modifier buttons pressed
    void onMouseClicked(int button, int mods)
    {
        std::cout << "Pressed mouse button: " << button << std::endl;
    }

    // If one of the mouse buttons is released this function will be called
    // button - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__buttons.html
    // mods - Any modifier buttons pressed
    void onMouseReleased(int button, int mods)
    {
        std::cout << "Released mouse button: " << button << std::endl;
    }

private:
    Window m_window;

    // Shader for default rendering and for depth rendering
    Shader m_defaultShader;
    Shader m_shadowShader;

    std::vector<GPUMesh> m_astronaut;
    Texture m_texture;
    bool m_useMaterial { true };

    // Projection and view matrices for you to fill in and use
    glm::mat4 m_projectionMatrix = glm::perspective(glm::radians(80.0f), 1.0f, 0.1f, 30.0f);
    glm::mat4 m_viewMatrix = glm::lookAt(glm::vec3(-1, 1, -1), glm::vec3(0), glm::vec3(0, 1, 0));
    glm::mat4 m_modelMatrix { 1.0f };

    // Curve instances
    std::vector<glm::vec3> m_curveControlPoints = 
    {
        {0, 1, 5},   // curve 1 p0 
        {3, 3, 3},   // curve 1 p1 
        {6, 3, 0},   // curve 1 p2 
        {9, 1, -2},  // curve 1 p3, curve 2 p0 
        {12, -1, -1}, // curve 2 p1 
        {15, -2, 2},  // curve 2 p2 
        {18, 0, 5},   // curve 2 p3, curve 3 p0 
        {21, 2, 7},   // curve 3 p1 
        {24, 3, 5},   // curve 3 p2 
        {27, 1, 2}    // curve 3 p3 
    };
    bool m_showCurve = true;
    bool m_moveAlongCurve = true;
    Shader m_lineShader;
    GLuint m_curveVAO = 0, m_curveVBO = 0;
    std::vector<glm::vec3> m_curvePoints;

    glm::mat4 m_lastViewMatrix = m_viewMatrix;
};

int main()
{
    Application app;
    app.update();

    return 0;
}
