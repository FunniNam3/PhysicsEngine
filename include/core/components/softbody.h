#pragma once

#include "component.h"
#include <cmath>
#include <glm/glm.hpp>
#include "render/tiny_obj_loader.h"
#include <unordered_map>

class SoftBody final : public Component
{
    struct Triangle {
        uint32_t v[3];
    };

    struct Constraint {
        virtual ~Constraint() = default;

        float lambda = 0.f; // persistent across frames

        virtual void solve(
            float dt,
            std::vector<glm::vec3> &positions,
            std::vector<float> &invMass,
            float maxCorrection
        ) = 0;

        virtual std::string type() { return "Constraint"; }
    };

    // TODO debug constraints to make sure they work properly
    // TODO Remove Constraint struct since it is not needed anymore

    struct DistanceConstraint final : Constraint {
        uint32_t a, b;
        float restDistance;
        float *compliance; // 0 = rigid, >0 = soft

        void solve(
            const float dt,
            std::vector<glm::vec3> &positions,
            std::vector<float> &invMass,
            const float maxCorrection
        ) override {
            glm::vec3 &p0 = positions[a];
            glm::vec3 &p1 = positions[b];

            const glm::vec3 d = p1 - p0;
            const float len = length(d);
            if (len < 1e-8f) return;

            const float w0 = invMass[a];
            const float w1 = invMass[b];

            const float C = len - restDistance;

            const glm::vec3 grad = d / len;

            const float wSum = w0 + w1;
            if (abs(wSum) < 1e-8f) return; // Prevent division by 0

            const float alpha = (*compliance) / (dt * dt);
            float dLambda = (-C - alpha * lambda) / (wSum + alpha);
            dLambda = glm::clamp(dLambda, -maxCorrection, maxCorrection);
            lambda += dLambda;

            p0 -= w0 * dLambda * grad;
            p1 += w1 * dLambda * grad;
        }

        DistanceConstraint(const uint32_t _a, const uint32_t _b, const float _restingValue,
                           float *_compliance): a(_a), b(_b),
                                                restDistance(_restingValue), compliance(_compliance) {
        }

        std::string type() override { return "Distance Constraint"; }

        [[nodiscard]] std::unique_ptr<DistanceConstraint> clone() const {
            return std::make_unique<DistanceConstraint>(*this);
        }
    };

    struct GlobalVolumeConstraint final : Constraint {
        std::vector<uint32_t> vertices;
        std::vector<Triangle> triangles;
        std::vector<glm::vec3> grad;
        float restVolume;
        float *compliance;
        float *pressureStrength;

        void solve(
            const float dt,
            std::vector<glm::vec3> &positions,
            std::vector<float> &invMass,
            const float maxCorrection
        ) override {
            const float V = computeVolume(positions, triangles);
            const float C = V - restVolume;

            if (abs(C) < 1e-8f) {
                return;
            }

            std::fill(grad.begin(), grad.end(), glm::vec3(0));

            for (const auto &[v]: triangles) {
                grad[v[0]] += cross(positions[v[1]], positions[v[2]]) / 6.0f;
                grad[v[1]] += cross(positions[v[2]], positions[v[0]]) / 6.0f;
                grad[v[2]] += cross(positions[v[0]], positions[v[1]]) / 6.0f;
            }

            // Compute delta lambda denominator
            const float alpha = (*compliance) / (dt * dt);
            float denominator = alpha;
            for (const auto i: vertices)
                denominator += invMass[i] * dot(grad[i], grad[i]);

            if (abs(denominator) > 1e-8f) {
                // const float pressureTerm = (*pressureStrength) * restVolume;
                // float dLambda = (-C + pressureTerm - alpha * lambda) / denominator;
                float dLambda = (-C - alpha * lambda) / denominator;
                dLambda = glm::clamp(dLambda, -maxCorrection, maxCorrection);

                for (const auto i: vertices)
                    positions[i] += invMass[i] * dLambda * grad[i];

                lambda += dLambda;
            };
        }

        GlobalVolumeConstraint(
            const std::vector<uint32_t> &_vertices,
            const std::vector<Triangle> &_triangles,
            const std::vector<glm::vec3> &restPositions,
            float *_compliance,
            float *_pressureStrength
        )
            : vertices(_vertices),
              triangles(_triangles),
              compliance(_compliance),
              pressureStrength(_pressureStrength) {
            // Compute rest volume from initial configuration
            restVolume = computeVolume(restPositions, triangles);
            grad.resize(vertices.size(), glm::vec3(0));

            // Ensure positive orientation
            if (restVolume < 0.0f)
                restVolume = -restVolume;
        }

        std::string type() override { return "Global Volume Constraint"; }

        [[nodiscard]] std::unique_ptr<GlobalVolumeConstraint> clone() const {
            return std::make_unique<GlobalVolumeConstraint>(*this);
        }
    };

    static float computeVolume(
        const std::vector<glm::vec3> &positions,
        const std::vector<Triangle> &tris
    ) {
        float V = 0.0f;
        for (const auto &[v]: tris) {
            const glm::vec3 &a = positions[v[0]];
            const glm::vec3 &b = positions[v[1]];
            const glm::vec3 &c = positions[v[2]];
            V += dot(a, cross(b, c));
        }
        return V / 6.0f;
    }

    // struct BendingConstraint final : Constraint {
    //     uint32_t a, b, c, d;
    //     float restingAngle;
    //     float *compliance; // 0 = rigid, >0 = soft
    //     void solve(const float dt,
    //         std::vector<glm::vec3> &positions,
    //         std::vector<float> &invMass,
    //         const float maxCorrection) override {
    //
    //     }
    //     std::string type() override { return "Bending Constraint"; }
    //     [[nodiscard]] std::unique_ptr<BendingConstraint> clone() const {
    //         return std::make_unique<BendingConstraint>(*this);
    //     }
    // };

public:
    std::vector<glm::vec3> restPositions;
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> prevPositions;
    std::vector<float> invMass;

    std::vector<glm::vec3> restCentroids;

    float edgeCompliance;
    float bendingCompliance;
    float volumeCompliance;
    float pressureStrength;

    std::vector<std::unique_ptr<DistanceConstraint> > distanceConstraints;
    std::unique_ptr<GlobalVolumeConstraint> globalVolumeConstraint;

    [[nodiscard]] float *getVars() const {
        return new float[]{
            edgeCompliance,
            bendingCompliance,
            volumeCompliance,
            pressureStrength
        };
    }

    SoftBody(const SoftBody &other) : Component(other),
                                      restPositions(other.restPositions),
                                      positions(other.positions),
                                      prevPositions(other.prevPositions),
                                      invMass(other.invMass),
                                      restCentroids(other.restCentroids),
                                      edgeCompliance(other.edgeCompliance),
                                      bendingCompliance(other.bendingCompliance),
                                      volumeCompliance(other.volumeCompliance),
                                      pressureStrength(other.pressureStrength),
                                      globalVolumeConstraint(other.globalVolumeConstraint->clone()) {
        distanceConstraints.reserve(other.distanceConstraints.size());
        for (const std::unique_ptr<DistanceConstraint> &c: other.distanceConstraints)
            distanceConstraints.push_back(c->clone());
    }

    explicit SoftBody(
        const std::string &modelPath,
        const glm::mat4 &modelMatrix,
        const float _edgeCompliance = 1e-6f,
        const float _bendingCompliance = 5e-5f,
        const float _volumeCompliance = 1e-5f,
        const float _pressureStrength = 0.3f
    )
        : edgeCompliance(_edgeCompliance),
          bendingCompliance(_bendingCompliance),
          volumeCompliance(_volumeCompliance),
          pressureStrength(_pressureStrength) {
        type = SOFTBODY;

        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string err;

        struct PhysVertex {
            int v, n, t;

            bool operator==(const PhysVertex &other) const {
                return v == other.v && n == other.n && t == other.t;
            }
        };

        struct PhysVertexHash {
            size_t operator()(const PhysVertex &p) const {
                const size_t h1 = std::hash<int>()(p.v);
                const size_t h2 = std::hash<int>()(p.n);
                const size_t h3 = std::hash<int>()(p.t);
                return h1 ^ (h2 << 1) ^ (h3 << 2);
            }
        };

        bool ok = LoadObj(&attrib, &shapes, &materials, &err, modelPath.c_str());
        if (!ok) {
            throw std::runtime_error(err);
        }

        std::unordered_map<PhysVertex, uint32_t, PhysVertexHash> vertexMap;
        std::vector<uint32_t> indices;

        // Collect vertices and triangulate faces
        for (const auto& shape : shapes) {
            for (const auto &idx: shape.mesh.indices) {
                PhysVertex pv{
                    idx.vertex_index,
                    idx.normal_index,
                    idx.texcoord_index
                };

                if (vertexMap.count(pv) == 0) {
                    glm::vec3 pos(
                        attrib.vertices[3 * idx.vertex_index + 0],
                        attrib.vertices[3 * idx.vertex_index + 1],
                        attrib.vertices[3 * idx.vertex_index + 2]
                    );

                    uint32_t newIndex = restPositions.size();
                    restPositions.push_back(pos);
                    vertexMap[pv] = newIndex;
                }
            }

            size_t index_offset = 0;

            for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
                int fv = shape.mesh.num_face_vertices[f];
                if (fv < 3) continue;

                std::vector<uint32_t> face;

                for (int j = 0; j < fv; ++j) {
                    auto &idx = shape.mesh.indices[index_offset + j];
                    PhysVertex pv{idx.vertex_index, idx.normal_index, idx.texcoord_index};
                    face.push_back(vertexMap[pv]);
                }

                for (int t = 1; t < fv - 1; ++t) {
                    indices.push_back(face[0]);
                    indices.push_back(face[t]);
                    indices.push_back(face[t + 1]);
                }

                index_offset += fv;
            }
        }

        struct Position {
            float x, y, z;

            bool operator==(const Position &other) const {
                return glm::epsilonEqual(x, other.x, 1e-8f) && glm::epsilonEqual(y, other.y, 1e-8f) &&
                       glm::epsilonEqual(z, other.z, 1e-8f);
            }
        };

        struct PositionHash {
            size_t operator()(const Position &p) const {
                const size_t h1 = std::hash<float>()(p.x);
                const size_t h2 = std::hash<float>()(p.y);
                const size_t h3 = std::hash<float>()(p.z);
                return h1 ^ (h2 << 1) ^ (h3 << 2);
            }
        };


        std::unordered_map<Position, std::vector<uint32_t>, PositionHash> buckets;
        for (uint32_t i = 0; i < restPositions.size(); ++i) {
            glm::vec3 &p = restPositions[i];
            Position key{
                p.x, p.y, p.z,
            };
            buckets[key].push_back(i);
        }

        for (auto &[key, verts]: buckets) {
            if (verts.size() < 2) continue;

            for (size_t i = 0; i < verts.size(); ++i) {
                for (size_t j = i + 1; j < verts.size(); ++j) {
                    distanceConstraints.push_back(std::make_unique<DistanceConstraint>(
                        verts[i],
                        verts[j],
                        0.0f,
                        &edgeCompliance
                    ));
                }
            }
        }

        for (auto &pos: restPositions) {
            auto temp = glm::vec4(pos.x, pos.y, pos.z, 1.0f);
            pos = glm::vec3(modelMatrix * temp);
        }

        // Initialize positions
        positions = restPositions;

        // Tiny offset for first frame to ensure cube falls
        prevPositions = positions;
        for (auto& p : prevPositions) p.y -= 0.001f;

        invMass.resize(positions.size(), 1.0f);

        struct Edge {
            uint32_t a, b;
        };

        struct EdgeKey {
            uint32_t a, b;

            EdgeKey(uint32_t i, uint32_t j) {
                a = std::min(i, j);
                b = std::max(i, j);
            }

            bool operator==(const EdgeKey &other) const {
                return a == other.a && b == other.b;
            }
        };

        struct EdgeKeyHash {
            size_t operator()(const EdgeKey& e) const {
                return std::hash<uint32_t>()(e.a) ^ (std::hash<uint32_t>()(e.b) << 1);
            }
        };

        std::unordered_map<EdgeKey, int, EdgeKeyHash> edgeCounts;
        edgeCounts.reserve(indices.size());

        for (size_t i = 0; i < indices.size(); i += 3) {
            uint32_t i0 = indices[i + 0];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];

            if (i0 == i1 || i1 == i2 || i2 == i0)
                continue;

            edgeCounts[EdgeKey(i0, i1)]++;
            edgeCounts[EdgeKey(i1, i2)]++;
            edgeCounts[EdgeKey(i2, i0)]++;
        }

        std::vector<Edge> structuralEdges;
        structuralEdges.reserve(edgeCounts.size());

        float minEdgeLen = FLT_MAX;
        for (auto &[e, _]: edgeCounts) {
            float len = glm::length(restPositions[e.a] - restPositions[e.b]);
            minEdgeLen = std::min(minEdgeLen, len);
        }

        for (auto &[e, count]: edgeCounts) {
            bool isStructural =
                    (count == 2) || // internal edge
                    (count == 1); // boundary edge

            // Filter out diagonals (heuristic but robust)
            if (isStructural) {
                structuralEdges.push_back({e.a, e.b});
            }
        }

        for (const auto &[a, b]: structuralEdges) {
            float restLen = length(restPositions[a] - restPositions[b]);

            distanceConstraints.push_back(std::make_unique<DistanceConstraint>(
                a,
                b,
                restLen,
                &edgeCompliance
            ));
        }

        std::vector<Triangle> triangles;
        triangles.reserve(indices.size() / 3);

        for (size_t i = 0; i < indices.size(); i += 3) {
            triangles.push_back({indices[i], indices[i + 1], indices[i + 2]});
        }

        // struct EdgeAdj {
        //     uint32_t triA = UINT32_MAX;
        //     uint32_t triB = UINT32_MAX;
        // };
        //
        // std::unordered_map<EdgeKey, EdgeAdj, EdgeKeyHash> edgeAdj;
        // edgeAdj.reserve(indices.size());
        //
        // for (uint32_t t = 0; t < triangles.size(); ++t) {
        //     auto &tri = triangles[t];
        //
        //     for (int e = 0; e < 3; ++e) {
        //         uint32_t a = tri.v[e];
        //         uint32_t b = tri.v[(e + 1) % 3];
        //
        //         EdgeKey key(a, b);
        //         auto &adj = edgeAdj[key];
        //
        //         if (adj.triA == UINT32_MAX)
        //             adj.triA = t;
        //         else
        //             adj.triB = t;
        //     }
        // }
        //
        // auto oppositeVertex = [](const Triangle &t, uint32_t a, uint32_t b) {
        //     for (uint32_t v: t.v)
        //         if (v != a && v != b)
        //             return v;
        //     return UINT32_MAX; // should never happen
        // };
        //
        // for (auto &[edge, adj]: edgeAdj) {
        //     // Only internal edges
        //     if (adj.triA == UINT32_MAX || adj.triB == UINT32_MAX)
        //         continue;
        //
        //     const Triangle &tA = triangles[adj.triA];
        //     const Triangle &tB = triangles[adj.triB];
        //
        //     uint32_t v2 = oppositeVertex(tA, edge.a, edge.b);
        //     uint32_t v3 = oppositeVertex(tB, edge.a, edge.b);
        //
        //     if (v2 == UINT32_MAX || v3 == UINT32_MAX)
        //         continue;
        //
        //     float restLen = glm::length(
        //         restPositions[v2] - restPositions[v3]
        //     );
        //
        //     distanceConstraints.push_back(std::make_unique<DistanceConstraint>(
        //         v2,
        //         v3,
        //         restLen,
        //         &bendingCompliance // compliance
        //     ));
        // }

        globalVolumeConstraint = std::make_unique<GlobalVolumeConstraint>(
            indices,
            triangles,
            restPositions, // rest pose
            &volumeCompliance,
            &pressureStrength
        );

        std::cout << "Soft body initialized: "
                << restPositions.size() << " vertices, "
                << structuralEdges.size() << " edges, "
                << distanceConstraints.size() << " constraints";
    }

    void Integrate(const float dt, const glm::vec3 gravity) {
        // vertices
        for (size_t i = 0; i < positions.size(); ++i) {
            if (invMass[i] == 0.0f) continue;

            glm::vec3 velocity = positions[i] - prevPositions[i];

            // Verlet integration
            prevPositions[i] = positions[i];
            positions[i] += velocity + gravity * dt * dt;
        }
    }

    void SolveConstraints(
        const float dt,
        const float maxCorrection,
        const int iterations = 8) {
        // Set Lambdas to 0
        for (const auto &distanceConstraint: distanceConstraints) {
            if (distanceConstraint) {
                distanceConstraint->lambda = 0.f;
            }
        }

        if (globalVolumeConstraint) {
            globalVolumeConstraint->lambda = 0.f;
        }

        // Solve Constraints
        for (int it = 0; it < iterations; ++it) {
            for (const auto &distanceConstraint: distanceConstraints) {
                if (distanceConstraint) {
                    distanceConstraint->solve(dt, positions, invMass, maxCorrection);
                }
            }
            if(globalVolumeConstraint) {
                globalVolumeConstraint->solve(dt, positions, invMass, maxCorrection);
            }
        }
    }

    void SolveFloorCollision(const float floorY = 0.0f, const float bounce = 0.2f) { // Bounce factor (0 = no bounce, 1 = full)
        for (size_t i = 0; i < positions.size(); ++i) {
            if (invMass[i] == 0.0f) continue; // skip fixed points

            // If below floor, push up
            if (positions[i].y < floorY) {
                const float penetration = floorY - positions[i].y;
                positions[i].y += penetration;

                prevPositions[i].y = positions[i].y + (prevPositions[i].y - positions[i].y) * (1.0f - bounce);
            }

        }
    }

    [[nodiscard]] std::shared_ptr<Component> Clone() const override {
        return std::make_shared<SoftBody>(*this);
    }
};