#include "mesh.h"
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <fmt/format.h>
DISABLE_WARNINGS_POP()
#include <iostream>
#include <vector>

GPUMaterial::GPUMaterial(const Material& material) :
    kd(material.kd),
    ks(material.ks),
    shininess(material.shininess),
    transparency(material.transparency),
    roughness(material.roughness),
    metallic(material.metallic),
    ior(material.ior)
    //texturePath(material.kdTexturePath)
{}

GPUMesh::GPUMesh(const Mesh& cpuMesh)
{
    // Create uniform buffer to store mesh material (https://learnopengl.com/Advanced-OpenGL/Advanced-GLSL)
    GPUMaterial gpuMaterial(cpuMesh.material);
    glGenBuffers(1, &m_uboMaterial);
    glBindBuffer(GL_UNIFORM_BUFFER, m_uboMaterial);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(GPUMaterial), &gpuMaterial, GL_STATIC_READ);

    // Figure out if this mesh has texture coordinates
    m_hasTextureCoords = static_cast<bool>(cpuMesh.material.kdTexturePath.empty() == false) ;
    m_hasRoughnessCoords = static_cast<bool>(cpuMesh.material.roughnessTexturePath.empty() == false);
    m_hasMetallicCoords = static_cast<bool>(cpuMesh.material.metallicTexturePath.empty() == false);
    m_hasNormalCoords = static_cast<bool>(cpuMesh.material.normalTexturePath.empty() == false);
    m_hasKaCoords = static_cast<bool>(cpuMesh.material.kaTexturePath.empty() == false);
	m_hasEmissiveCoords = static_cast<bool>(cpuMesh.material.emissiveTexturePath.empty() == false);

    if (m_hasTextureCoords) m_texturePath = cpuMesh.material.kdTexturePath;
    if (m_hasRoughnessCoords) m_roughnessTexturePath = cpuMesh.material.roughnessTexturePath;
    if (m_hasMetallicCoords) m_metallicTexturePath = cpuMesh.material.metallicTexturePath;

    if (m_hasNormalCoords) m_normalTexturePath = cpuMesh.material.normalTexturePath;
    if (m_hasKaCoords) m_KaTexturePath = cpuMesh.material.kaTexturePath;
	if (m_hasEmissiveCoords) m_emissiveTexturePath = cpuMesh.material.emissiveTexturePath;

    // Create VAO and bind it so subsequent creations of VBO and IBO are bound to this VAO
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    // Create vertex buffer object (VBO)
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(cpuMesh.vertices.size() * sizeof(decltype(cpuMesh.vertices)::value_type)), cpuMesh.vertices.data(), GL_STATIC_DRAW);

    // Create index buffer object (IBO)
    glGenBuffers(1, &m_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(cpuMesh.triangles.size() * sizeof(decltype(cpuMesh.triangles)::value_type)), cpuMesh.triangles.data(), GL_STATIC_DRAW);

    // Tell OpenGL that we will be using vertex attributes 0, 1 and 2.
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
	glEnableVertexAttribArray(3); //tangent
    glEnableVertexAttribArray(4); //bitangent

    // We tell OpenGL what each vertex looks like and how they are mapped to the shader (location = ...).
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoord));
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, bitangent));

    // Reuse all attributes for each instance
    glVertexAttribDivisor(0, 0);
    glVertexAttribDivisor(1, 0);
    glVertexAttribDivisor(2, 0);
    glVertexAttribDivisor(3, 0);
    glVertexAttribDivisor(4, 0);

    // Each triangle has 3 vertices.
    m_numIndices = static_cast<GLsizei>(3 * cpuMesh.triangles.size());
}

GPUMesh::GPUMesh(GPUMesh&& other)
{
    moveInto(std::move(other));
}

GPUMesh::~GPUMesh()
{
    freeGpuMemory();
}

GPUMesh& GPUMesh::operator=(GPUMesh&& other)
{
    moveInto(std::move(other));
    return *this;
}

std::vector<GPUMesh> GPUMesh::loadMeshGPU(std::filesystem::path filePath, bool normalize) {
    if (!std::filesystem::exists(filePath))
        throw MeshLoadingException(fmt::format("File {} does not exist", filePath.string().c_str()));

    // Generate GPU-side meshes for all sub-meshes
    std::vector<Mesh> subMeshes = loadMesh(filePath, { .normalizeVertexPositions = normalize });
    std::vector<GPUMesh> gpuMeshes;
    for (const Mesh& mesh : subMeshes) { gpuMeshes.emplace_back(mesh); }

    return gpuMeshes;
}

bool GPUMesh::hasTextureCoords() const
{
    return m_hasTextureCoords;
}

bool GPUMesh::hasRoughnessCoords() const
{
    return m_hasRoughnessCoords;
}

bool GPUMesh::hasMetallicCoords() const
{
    return m_hasMetallicCoords;
}

bool GPUMesh::hasNormalCoords() const
{
    return m_hasNormalCoords;
}

bool GPUMesh::hasKaCoords() const
{
    return m_hasKaCoords;
}

bool GPUMesh::hasEmissiveCoords() const
{
	return m_hasEmissiveCoords;
}

void GPUMesh::draw(const Shader& drawingShader)
{
    // Bind material data uniform (we assume that the uniform buffer objects is always called 'Material')
    // Yes, we could define the binding inside the shader itself, but that would break on OpenGL versions below 4.2
    drawingShader.bindUniformBlock("Material", 0, m_uboMaterial);

    // Draw the mesh's triangles
    glBindVertexArray(m_vao);
    glDrawElements(GL_TRIANGLES, m_numIndices, GL_UNSIGNED_INT, nullptr);
}

void GPUMesh::moveInto(GPUMesh&& other)
{
    freeGpuMemory();
    m_numIndices = other.m_numIndices;
    m_hasTextureCoords = other.m_hasTextureCoords;
    m_hasRoughnessCoords = other.m_hasRoughnessCoords;
    m_hasMetallicCoords = other.m_hasMetallicCoords;
    m_hasNormalCoords = other.m_hasNormalCoords;
    m_hasKaCoords = other.m_hasKaCoords;
	m_hasEmissiveCoords = other.m_hasEmissiveCoords;

    m_texturePath = std::move(other.m_texturePath);
    m_roughnessTexturePath = std::move(other.m_roughnessTexturePath);
    m_metallicTexturePath = std::move(other.m_metallicTexturePath); 
    m_normalTexturePath = std::move(other.m_normalTexturePath); 
    m_KaTexturePath = std::move(other.m_KaTexturePath); 
	m_emissiveTexturePath = std::move(other.m_emissiveTexturePath);

    m_ibo = other.m_ibo;
    m_vbo = other.m_vbo;
    m_vao = other.m_vao;
    m_uboMaterial = other.m_uboMaterial;

    other.m_numIndices = 0;
    other.m_hasTextureCoords = other.m_hasTextureCoords;
    other.m_hasRoughnessCoords = other.m_hasRoughnessCoords;
    other.m_hasMetallicCoords = other.m_hasMetallicCoords;
    other.m_hasNormalCoords = other.m_hasNormalCoords;
    other.m_hasKaCoords = other.m_hasKaCoords;
	other.m_hasEmissiveCoords = other.m_hasEmissiveCoords;
    other.m_ibo = INVALID;
    other.m_vbo = INVALID;
    other.m_vao = INVALID;
    other.m_uboMaterial = INVALID;
}

void GPUMesh::freeGpuMemory()
{
    if (m_vao != INVALID)
        glDeleteVertexArrays(1, &m_vao);
    if (m_vbo != INVALID)
        glDeleteBuffers(1, &m_vbo);
    if (m_ibo != INVALID)
        glDeleteBuffers(1, &m_ibo);
    if (m_uboMaterial != INVALID)
        glDeleteBuffers(1, &m_uboMaterial);
}
