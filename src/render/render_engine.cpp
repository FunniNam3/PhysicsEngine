#include "render/render_engine.h"

#include "render/tiny_obj_loader.h"
#include "core/components/component.h"
#include "core/components/light.h"
#include "core/components/transform.h"

#include <fmt/core.h>

void RenderEngine::Init()
{
	ShadersInit();
}

void LoadOBJ(
    const std::string &filename,
    std::vector<glm::vec3> &outPositions,
    std::vector<glm::vec3> &outNormals,
    std::vector<glm::vec2> &outTexCoords,
    std::vector<unsigned int> &outIndices) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err,
                                filename.c_str());

    if (!err.empty())
        std::cerr << "TinyOBJ error: " << err << std::endl;

    if (!ret)
        throw std::runtime_error("Failed to load OBJ: " + filename);

    // Map unique vertex combination to index
    struct Vertex {
        int posIdx, normIdx, texIdx;

        bool operator==(const Vertex &other) const {
            return posIdx == other.posIdx &&
                   normIdx == other.normIdx &&
                   texIdx == other.texIdx;
        }
    };

    struct VertexHash {
        size_t operator()(const Vertex &v) const {
            return std::hash<int>()(v.posIdx) ^
                   std::hash<int>()(v.normIdx << 1) ^
                   std::hash<int>()(v.texIdx << 2);
        }
    };

    std::unordered_map<Vertex, unsigned int, VertexHash> uniqueVertices;

    // Loop through all shapes
    for (const auto &shape: shapes) {
        for (size_t f = 0; f < shape.mesh.indices.size(); ++f) {
            tinyobj::index_t idx = shape.mesh.indices[f];

            Vertex v{idx.vertex_index, idx.normal_index, idx.texcoord_index};

            auto it = uniqueVertices.find(v);
            if (it != uniqueVertices.end()) {
                outIndices.push_back(it->second);
            } else {
                glm::vec3 pos(0.0f);
                glm::vec3 norm(0.0f);
                glm::vec2 tex(0.0f);

                // Positions
                if (v.posIdx >= 0) {
                    pos.x = attrib.vertices[3 * v.posIdx + 0];
                    pos.y = attrib.vertices[3 * v.posIdx + 1];
                    pos.z = attrib.vertices[3 * v.posIdx + 2];
                }

                // Normals
                if (v.normIdx >= 0) {
                    norm.x = attrib.normals[3 * v.normIdx + 0];
                    norm.y = attrib.normals[3 * v.normIdx + 1];
                    norm.z = attrib.normals[3 * v.normIdx + 2];
                }

                // Texture coords
                if (v.texIdx >= 0) {
                    tex.x = attrib.texcoords[2 * v.texIdx + 0];
                    tex.y = attrib.texcoords[2 * v.texIdx + 1];
                }

                unsigned int newIndex = static_cast<unsigned int>(outPositions.size());
                outPositions.push_back(pos);
                outNormals.push_back(norm);
                outTexCoords.push_back(tex);
                outIndices.push_back(newIndex);

                uniqueVertices[v] = newIndex;
            }
        }
    }

    std::cout << "Loaded OBJ: " << filename
            << " (" << outPositions.size() << " vertices, "
            << outIndices.size() / 3 << " triangles)\n";
}


void RenderEngine::LoadModel(const std::string &path) {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    std::vector<unsigned int> indices;

    LoadOBJ(path, positions, normals, texcoords, indices);

    GLuint posVBO, norVBO, texVBO, ebo, vao;

    // Position VBO (dynamic for soft bodies)
    glGenBuffers(1, &posVBO);
    glBindBuffer(GL_ARRAY_BUFFER, posVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 positions.size() * sizeof(glm::vec3),
                 positions.data(),
                 GL_DYNAMIC_DRAW);

    // Normal VBO (static)
    glGenBuffers(1, &norVBO);
    glBindBuffer(GL_ARRAY_BUFFER, norVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 normals.size() * sizeof(glm::vec3),
                 normals.data(),
                 GL_STATIC_DRAW);

    // Texture coordinate VBO (static)
    glGenBuffers(1, &texVBO);
    glBindBuffer(GL_ARRAY_BUFFER, texVBO);
    glBufferData(GL_ARRAY_BUFFER,
                 texcoords.size() * sizeof(glm::vec2),
                 texcoords.data(),
                 GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 indices.size() * sizeof(unsigned int),
                 indices.data(),
                 GL_STATIC_DRAW);

    // Create VAO
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Bind VBOs to VAO
    glBindBuffer(GL_ARRAY_BUFFER, posVBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr); // pos at location 0
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, norVBO);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr); // normal at location 1
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, texVBO);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, nullptr); // texcoord at location 2
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

    glBindVertexArray(0); // unbind VAO

    // Store buffers & counts
    posBuffMap[path] = posVBO;
    norBuffMap[path] = norVBO;
    texBuffMap[path] = texVBO;
    indexBuffMap[path] = ebo;
    vaoMap[path] = vao;
    indexCountMap[path] = indices.size();
}


// Function to generate a normal for a face (using the cross product of two edges)
glm::vec3 RenderEngine::GenerateNormal(const std::vector<glm::vec3>& faceVertices)
{
    // Ensure there are exactly three vertices
    if (faceVertices.size() != 3) {
        std::cerr << "Error: Cannot generate normal for a non-triangle face." << std::endl;
        return glm::vec3(0.0f); // Return a zero normal
    }

    glm::vec3 v0 = faceVertices[0];
    glm::vec3 v1 = faceVertices[1];
    glm::vec3 v2 = faceVertices[2];

    // Calculate two edges
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;

    // Calculate the normal using the cross product
    glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));

    return normal;
}

void RenderEngine::ShadersInit()
{
	program.SetShadersFileName(shadersPath + verts[0],
            shadersPath + frags[0]);
	program.Init();

    shadow.SetShadersFileName(shadersPath + verts[1],shadersPath + frags[1]);
    shadow.Init();
}

void RenderEngine::MapShadows(GLuint depthMapFBO, GLuint const shadowWidth,  GLuint const shadowHeight)
{
    glCullFace(GL_FRONT);
    glUseProgram(shadow.GetPID());
    glViewport(0, 0, shadowWidth, shadowHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    shadow.Bind();

    const auto& scene = mainEngine->GetCurrScene();

    glm::mat4 lightSpaceMatrix = scene->getLightSpaceMatrix();
    shadow.SendUniformData(lightSpaceMatrix, "lightSpaceMatrix");

    for (const auto& model : scene->GetModels())
    {
        const auto& objTransform = std::dynamic_pointer_cast<Transform>( model->components[TRANSFORM]);
        const auto& objModel = std::dynamic_pointer_cast<Model>(model->components[MODEL]);


        if (!objTransform || !objModel) continue; // essential components must exist

        std::string& modelPath = objModel->modelPath;

        // Check if the position buffer is already loaded
        if (posBuffMap.find(modelPath) == posBuffMap.end()) {
            // Load model buffers if they are not already loaded
            LoadModel(modelPath);
        }

        glm::mat4 modelMatrix(1.0f);
        modelMatrix = glm::translate(glm::mat4(1.0f), objTransform->position)
            * glm::rotate(glm::mat4(1.0f), glm::radians(objTransform->rotation[0]), glm::vec3(1.0f, 0.0f, 0.0f))
            * glm::rotate(glm::mat4(1.0f), glm::radians(objTransform->rotation[1]), glm::vec3(0.0f, 1.0f, 0.0f))
            * glm::rotate(glm::mat4(1.0f), glm::radians(objTransform->rotation[2]), glm::vec3(0.0f, 0.0f, 1.0f))
            * glm::scale(glm::mat4(1.0f), objTransform->scale);

        shadow.SendAttributeData(posBuffMap[modelPath], "aPos", 3);

        shadow.SendUniformData(modelMatrix, "model");

        glDrawArrays(GL_TRIANGLES, 0, posBuffMap[modelPath] / 3);
    }

    shadow.Unbind();

    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void RenderEngine::Display(glm::vec4 viewportInfo, GLuint depthMap)
{
    // TODO
    // Fix code so that all faces are rendered properly
    // Right now only top and bottom face are being simulated since theres only 8 vertices being updated in code

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glUseProgram(program.GetPID());
    glViewport(viewportInfo.x, height - viewportInfo.w - viewportInfo.y,
               viewportInfo.z, viewportInfo.w);

    glEnable(GL_DEPTH_TEST);

    const auto& scene = mainEngine->GetCurrScene();
    if (!scene) return;

    camera = scene->GetCurrCamera();
    if (!camera) return;

    camera->SetAspect(viewportInfo.z, viewportInfo.w);

    glm::mat4 projection = camera->GetProjectionMatrix();
    glm::mat4 view = camera->GetViewMatrix();

    glm::vec3 lightPosition = scene->getlightEye();
    glm::mat4 lightSpaceMatrix = scene->getLightSpaceMatrix();

    // Render each model
    for (const auto &model: scene->GetModels()) {
        auto transform = std::dynamic_pointer_cast<Transform>(model->components[TRANSFORM]);
        auto material = std::dynamic_pointer_cast<Material>(model->components[MATERIAL]);
        auto modelComp = std::dynamic_pointer_cast<Model>(model->components[MODEL]);
        auto softBody = std::dynamic_pointer_cast<SoftBody>(model->components[SOFTBODY]);

        if (!transform || !modelComp) continue;

        const std::string &path = modelComp->modelPath;

        if (vaoMap.find(path) == vaoMap.end())
            LoadModel(path);

        GLuint vao = vaoMap[path];
        GLuint posVBO = posBuffMap[path];
        size_t count = indexCountMap[path];


        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, posVBO);

        // Update soft-body positions
        if (softBody) {
            glBufferSubData(GL_ARRAY_BUFFER, 0,
                            softBody->positions.size() * sizeof(glm::vec3),
                            softBody->positions.data());
        }

        glm::mat4 modelMatrix = transform->GetModelMatrix(biasMap[path]);
        glm::mat4 normalMatrix = glm::transpose(glm::inverse(modelMatrix));

        program.Bind();

        program.SendUniformData(modelMatrix, "model");
        program.SendUniformData(view, "view");
        program.SendUniformData(projection, "projection");
        program.SendUniformData(normalMatrix, "modelInverseTranspose");

        program.SendUniformData(lightSpaceMatrix, "lightSpaceMatrix");
        program.SendUniformData(lightPosition, "lightPosition");

        // Shadow map
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        program.SendUniformData(0, "shadowMap");

        // Material
        if (material) {
            program.SendUniformData(material->ambient, "ka");
            program.SendUniformData(material->diffuse, "kd");
            program.SendUniformData(material->specular, "ks");
            program.SendUniformData(material->shininess, "s");
        }

        // Lights
        const auto &lights = scene->GetLights();
        for (size_t i = 0; i < lights.size(); ++i) {
            auto lTransform = std::dynamic_pointer_cast<Transform>(lights[i]->components[TRANSFORM]);
            auto lComp = std::dynamic_pointer_cast<PointLight>(lights[i]->components[LIGHT]);
            if (!lTransform || !lComp) continue;

            std::string base = fmt::format("lights[{}]", i);
            program.SendUniformData(lTransform->position, (base + ".position").c_str());
            program.SendUniformData(lComp->color, (base + ".color").c_str());
        }

        // Draw
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(count), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        program.Unbind();
    }
}

void RenderEngine::CharacterCallback(GLFWwindow *window, unsigned int key) {
}


void RenderEngine::FrameBufferSizeCallback(GLFWwindow* lWindow, int width, int height)
{
	glViewport(0, 0, width, height);
}