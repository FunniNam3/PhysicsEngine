#pragma once

#include "component.h"
#include <glm/glm.hpp>
#include "render/tiny_obj_loader.h"
#include <unordered_map>

struct DistanceConstraint {
    uint32_t i0, i1;
    float restLength;
    float compliance; // 0 = rigid, >0 = soft
    float lambda;     // persistent across frames
};

struct Edge {
    uint32_t a, b;
    bool operator==(const Edge& o) const {
        return (a == o.a && b == o.b) || (a == o.b && b == o.a);
    }
};

struct EdgeHash {
    size_t operator()(const Edge& e) const {
        return std::hash<uint32_t>()(e.a) ^
               std::hash<uint32_t>()(e.b);
    }
};

class SoftBody : public Component
{

    std::unordered_set<Edge, EdgeHash> edges;

public:
    std::vector<glm::vec3> restPositions;

    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> prevPositions;

    std::vector<float> invMass;

    std::vector<DistanceConstraint> constraints;

    explicit SoftBody(const std::string& modelPath) {
        std::cout << "Loading " << modelPath << std::endl;

        type = SOFTBODY;

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string err;

        bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, modelPath.c_str());
        if (!ok) {
            throw std::runtime_error(err);
        }

        // Map to keep only unique vertices by position
        struct Vec3Hash {
            size_t operator()(const glm::vec3& v) const {
                return std::hash<float>()(v.x) ^ std::hash<float>()(v.y) ^ std::hash<float>()(v.z);
            }
        };

        std::unordered_map<glm::vec3, uint32_t, Vec3Hash> posMap;
        std::vector<uint32_t> indices;

        // Collect vertices and triangulate faces
        for (const auto& shape : shapes) {
            size_t index_offset = 0;
            for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
                int fv = shape.mesh.num_face_vertices[f];
                if (fv < 3) continue; // ignore degenerate faces

                // Get face indices
                std::vector<uint32_t> faceIndices;
                for (int j = 0; j < fv; ++j) {
                    int idx = shape.mesh.indices[index_offset + j].vertex_index;
                    glm::vec3 pos(
                        attrib.vertices[3 * idx + 0],
                        attrib.vertices[3 * idx + 1],
                        attrib.vertices[3 * idx + 2]);

                    auto it = posMap.find(pos);
                    if (it == posMap.end()) {
                        uint32_t newIndex = restPositions.size();
                        restPositions.push_back(pos);
                        posMap[pos] = newIndex;
                        faceIndices.push_back(newIndex);
                    } else {
                        faceIndices.push_back(it->second);
                    }
                }

                // Triangulate quad or polygon face
                for (int t = 1; t < fv - 1; ++t) {
                    indices.push_back(faceIndices[0]);
                    indices.push_back(faceIndices[t]);
                    indices.push_back(faceIndices[t + 1]);
                }

                index_offset += fv;
            }
        }

        // Initialize positions
        positions = restPositions;

        // Tiny offset for first frame to ensure cube falls
        prevPositions = positions;
        for (auto& p : prevPositions) p.y -= 0.001f;

        invMass.resize(positions.size(), 1.0f);

        // Build unique edges from triangle indices
        for (size_t i = 0; i < indices.size(); i += 3) {
            uint32_t i0 = indices[i + 0];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];

            edges.insert({i0, i1});
            edges.insert({i1, i2});
            edges.insert({i2, i0});
        }

        // Create distance constraints
        for (const auto& e : edges) {
            float restLen = glm::length(restPositions[e.a] - restPositions[e.b]);
            constraints.push_back({e.a, e.b, restLen, 1e-6f, 0.0f});
        }

        std::cout << "Soft body initialized: "
                  << restPositions.size() << " vertices, "
                  << edges.size() << " edges, "
                  << constraints.size() << " constraints.\n";
    }

    void Integrate(float dt, glm::vec3 gravity)
    {
        for (size_t i = 0; i < positions.size(); ++i)
        {
            if (invMass[i] == 0.0f) continue;

            glm::vec3 temp = positions[i];
            glm::vec3 velocity = positions[i] - prevPositions[i];

            // Verlet integration
            positions[i] += velocity + gravity * dt * dt;
            prevPositions[i] = temp;
        }
    }


    void SolveDistanceConstraint(
    DistanceConstraint& c,
    float dt)
    {
        glm::vec3& p0 = positions[c.i0];
        glm::vec3& p1 = positions[c.i1];

        glm::vec3 d = p1 - p0;
        float len = glm::length(d);
        if (len < 1e-6f) return;

        float w0 = invMass[c.i0];
        float w1 = invMass[c.i1];

        float C = len - c.restLength;

        glm::vec3 grad = d / len;
        float wSum = w0 + w1;

        float alpha = c.compliance / (dt * dt);
        float deltaLambda =
            (-C - alpha * c.lambda) / (wSum + alpha);

        c.lambda += deltaLambda;

        p0 -= w0 * deltaLambda * grad;
        p1 += w1 * deltaLambda * grad;
    }

    void SolveConstraints(
    float dt,
    int iterations = 6)
    {
        for (int it = 0; it < iterations; ++it) {
            for (auto& c : constraints)
                SolveDistanceConstraint(c, dt);
        }
    }

    void SolveFloorCollision(float floorY = 0.0f) {
        for (size_t i = 0; i < positions.size(); ++i) {
            if (invMass[i] == 0.0f) continue; // skip fixed points

            // If below floor, push up
            if (positions[i].y < floorY) {
                float penetration = floorY - positions[i].y;
                positions[i].y += penetration;

                // Bounce factor (0 = no bounce, 1 = full)
                float bounce = 0.2f;
                prevPositions[i].y = positions[i].y + (prevPositions[i].y - positions[i].y) * (1.0f - bounce);
            }

        }
    }

    std::shared_ptr<Component> Clone() const override {
        return std::make_shared<SoftBody>(*this);
    }
};