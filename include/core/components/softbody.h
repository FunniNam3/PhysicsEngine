#pragma once

#include "component.h"
#include <cmath>
#include <glm/glm.hpp>
#include "render/tiny_obj_loader.h"
#include <unordered_map>

class SoftBody final : public Component
{
    struct Constraint {
        virtual ~Constraint() = default;

        float lambda = 0.f; // persistent across frames

        virtual void solve(
            float dt,
            std::vector<glm::vec3> &positions,
            std::vector<float> &invMass,
            float maxCorrection,
            std::vector<glm::vec3> &centroids,
            std::vector<float> &invCentroidMass
        ) = 0;

        virtual std::string type() { return "Constraint"; }

        [[nodiscard]] virtual std::unique_ptr<Constraint> clone() const = 0;
    };

    struct DistanceConstraint final : Constraint {
        uint32_t a, b;
        float restDistance;
        float compliance; // 0 = rigid, >0 = soft

        void solve(
            const float dt, std::vector<glm::vec3> &positions, std::vector<float> &invMass, float maxCorrection,
            std::vector<glm::vec3> &centroids, std::vector<float> &invCentroidMass) override {
            glm::vec3 &p0 = positions[a];
            glm::vec3 &p1 = positions[b];

            const glm::vec3 d = p1 - p0;
            const float len = length(d);
            if (len < 1e-6f) return;

            const float w0 = invMass[a];
            const float w1 = invMass[b];

            const float C = len - restDistance;

            const glm::vec3 grad = d / len;

            const float wSum = w0 + w1;
            if (wSum < 1e-8f) return; // Prevent division by 0

            const float alpha = compliance / (dt * dt);
            float dLambda = (-C - alpha * lambda) / (wSum + alpha);

            dLambda = glm::clamp(dLambda, -maxCorrection, maxCorrection);

            lambda += dLambda;

            p0 -= w0 * dLambda * grad;
            p1 += w1 * dLambda * grad;
        }

        DistanceConstraint(const uint32_t _a, const uint32_t _b, const float _restingValue,
                           const float _compliance): a(_a), b(_b),
                                                     restDistance(_restingValue), compliance(_compliance) {
        }

        std::string type() override { return "Distance Constraint"; }

        [[nodiscard]] std::unique_ptr<Constraint> clone() const override {
            return std::make_unique<DistanceConstraint>(*this);
        }
    };

    struct VolumeConstraint final : Constraint {
        uint32_t a, b, c, d;
        float restVolume;
        float compliance; // 0 = rigid, >0 = soft

        void solve(const float dt, std::vector<glm::vec3> &positions, std::vector<float> &invMass, float maxCorrection,
                   std::vector<glm::vec3> &centroids, std::vector<float> &invCentroidMass) override {
            glm::vec3 aV = positions[a], bV = positions[b], cV = positions[c], dV = centroids[d];

            // current volume
            float V = dot(cross(bV - aV, cV - aV), dV - aV) / 6.0f;

            float cVol = V - std::abs(restVolume);

            // gradients
            glm::vec3 gradA = cross(bV - cV, dV - cV) / 6.0f;
            glm::vec3 gradB = cross(cV - aV, dV - aV) / 6.0f;
            glm::vec3 gradC = cross(aV - bV, dV - bV) / 6.0f;
            glm::vec3 gradD = cross(bV - aV, cV - aV) / 6.0f;

            float wSum = invMass[a] * dot(gradA, gradA)
                         + invMass[b] * dot(gradB, gradB)
                         + invMass[c] * dot(gradC, gradC)
                         + invCentroidMass[d] * dot(gradD, gradD);

            if (wSum < 1e-8f) return;

            float alpha = compliance / (dt * dt);
            float dLambda = (-cVol - alpha * lambda) / (wSum + alpha);

            float maxMove = 0.0f;
            maxMove = std::max(maxMove, glm::length(invMass[a] * dLambda * gradA));
            maxMove = std::max(maxMove, glm::length(invMass[b] * dLambda * gradB));
            maxMove = std::max(maxMove, glm::length(invMass[c] * dLambda * gradC));
            maxMove = std::max(maxMove, glm::length(invCentroidMass[d] * dLambda * gradD));

            if (maxMove > maxCorrection && maxMove > 0.0f) {
                dLambda *= maxCorrection / maxMove;
            }

            lambda += dLambda;

            positions[a] += invMass[a] * dLambda * gradA;
            positions[b] += invMass[b] * dLambda * gradB;
            positions[c] += invMass[c] * dLambda * gradC;
            centroids[d] += invCentroidMass[d] * dLambda * gradD;
        }

        VolumeConstraint(const uint32_t _a, const uint32_t _b, const uint32_t _c, const uint32_t _d,
                         const float _restVolume,
                         const float _compliance): a(_a), b(_b), c(_c), d(_d), restVolume(_restVolume),
                                                   compliance(_compliance) {
        }

        std::string type() override { return "Volume Constraint"; }

        [[nodiscard]] std::unique_ptr<Constraint> clone() const override {
            return std::make_unique<VolumeConstraint>(*this);
        }
    };

public:
    std::vector<glm::vec3> restPositions;
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> prevPositions;
    std::vector<float> invMass;

    std::vector<glm::vec3> restCentroids;
    std::vector<glm::vec3> centroids;
    std::vector<glm::vec3> prevCentroids;
    std::vector<float> invCentroidMass;

    std::vector<std::unique_ptr<Constraint> > constraints;

    SoftBody(const SoftBody &other) : Component(other),
                                      restPositions(other.restPositions),
                                      positions(other.positions),
                                      prevPositions(other.prevPositions),
                                      invMass(other.invMass),
                                      restCentroids(other.restCentroids),
                                      centroids(other.centroids),
                                      prevCentroids(other.prevCentroids),
                                      invCentroidMass(other.invCentroidMass) {
        constraints.reserve(other.constraints.size());
        for (auto &c: other.constraints)
            constraints.push_back(c->clone());
    }

    explicit SoftBody(const std::string& modelPath) {
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
                    constraints.push_back(std::make_unique<DistanceConstraint>(
                        verts[i],
                        verts[j],
                        0.0f,
                        1e-6f // compliance
                    ));
                }
            }
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

        for (const auto &e: structuralEdges) {
            float restLen = glm::length(restPositions[e.a] - restPositions[e.b]);

            constraints.push_back(std::make_unique<DistanceConstraint>(
                e.a,
                e.b,
                restLen,
                1e-6f // compliance
            ));
        }

        struct Triangle {
            uint32_t v[3];
        };

        std::vector<Triangle> triangles;
        triangles.reserve(indices.size() / 3);

        for (size_t i = 0; i < indices.size(); i += 3) {
            triangles.push_back({indices[i], indices[i + 1], indices[i + 2]});
        }

        struct EdgeAdj {
            uint32_t triA = UINT32_MAX;
            uint32_t triB = UINT32_MAX;
        };

        std::unordered_map<EdgeKey, EdgeAdj, EdgeKeyHash> edgeAdj;
        edgeAdj.reserve(indices.size());

        for (uint32_t t = 0; t < triangles.size(); ++t) {
            auto &tri = triangles[t];

            for (int e = 0; e < 3; ++e) {
                uint32_t a = tri.v[e];
                uint32_t b = tri.v[(e + 1) % 3];

                EdgeKey key(a, b);
                auto &adj = edgeAdj[key];

                if (adj.triA == UINT32_MAX)
                    adj.triA = t;
                else
                    adj.triB = t;
            }
        }

        auto oppositeVertex = [](const Triangle &t, uint32_t a, uint32_t b) {
            for (uint32_t v: t.v)
                if (v != a && v != b)
                    return v;
            return UINT32_MAX; // should never happen
        };

        float bendingCompliance = 5e-4f;

        for (auto &[edge, adj]: edgeAdj) {
            // Only internal edges
            if (adj.triA == UINT32_MAX || adj.triB == UINT32_MAX)
                continue;

            const Triangle &tA = triangles[adj.triA];
            const Triangle &tB = triangles[adj.triB];

            uint32_t v2 = oppositeVertex(tA, edge.a, edge.b);
            uint32_t v3 = oppositeVertex(tB, edge.a, edge.b);

            if (v2 == UINT32_MAX || v3 == UINT32_MAX)
                continue;

            float restLen = glm::length(
                restPositions[v2] - restPositions[v3]
            );

            constraints.push_back(std::make_unique<DistanceConstraint>(
                v2,
                v3,
                restLen,
                bendingCompliance // compliance
            ));
        }

        // TODO Add a centroid link to triangle centers and pressure
        for (auto &tri: triangles) {
            uint32_t centroidIndex = restCentroids.size();

            uint32_t a = tri.v[0], b = tri.v[1], c = tri.v[2], d = centroidIndex;

            glm::vec3 centroidPos = (restPositions[tri.v[0]] + restPositions[tri.v[1]] + restPositions[tri.v[2]]) /
                                    3.0f;

            restCentroids.push_back(centroidPos);
            invCentroidMass.push_back(invMass[a] + invMass[b] + invMass[c]);

            float volumeConstraint = 1e-4f;

            glm::vec3 p0 = restPositions[a];
            glm::vec3 p1 = restPositions[b];
            glm::vec3 p2 = restPositions[c];
            glm::vec3 p3 = restCentroids[d];

            float restVol = std::abs(dot(cross(p1 - p0, p2 - p0), p3 - p0) / 6.0f);

            constraints.push_back(std::make_unique<VolumeConstraint>(
                a, b, c, d, restVol, volumeConstraint
            ));
        }

        centroids = restCentroids;

        // Tiny offset for first frame to ensure cube falls
        prevCentroids = centroids;
        for (auto &c: prevCentroids) c.y -= 0.001f;

        invCentroidMass.resize(centroids.size(), 1.0f);

        std::cout << "Soft body initialized: "
                << restPositions.size() << " vertices, "
                << structuralEdges.size() << " edges, "
                << constraints.size() << " constraints,"
                << centroids.size() << " centroids\n";
    }

    void Integrate(const float dt, const glm::vec3 gravity) {
        // vertices
        for (size_t i = 0; i < positions.size(); ++i) {
            if (invMass[i] == 0.0f) continue;

            const glm::vec3 temp = positions[i];
            glm::vec3 velocity = positions[i] - prevPositions[i];

            // Verlet integration
            positions[i] += velocity + gravity * dt * dt;
            prevPositions[i] = temp;
        }

        // centroids
        for (size_t i = 0; i < centroids.size(); ++i) {
            if (invCentroidMass[i] == 0.0f) continue;

            const glm::vec3 temp = centroids[i];
            glm::vec3 velocity = centroids[i] - prevCentroids[i];
            centroids[i] += velocity + gravity * dt * dt;
            prevCentroids[i] = temp;
        }
    }

    void SolveConstraints(
        const float dt,
        const float maxCorrection,
        const int iterations = 8) {
        for (const auto &c: constraints) {
            c->lambda = 0.0f;
        }

        for (int it = 0; it < iterations; ++it) {
            for (const auto &constraint: constraints) {
                if (constraint) {
                    constraint->solve(dt, positions, invMass, maxCorrection, centroids, invCentroidMass);
                }
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