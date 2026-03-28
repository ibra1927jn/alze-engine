#pragma once

#include "RigidBody3D.h"
#include "math/Vector3D.h"
#include <vector>
#include <memory>
#include <cmath>

namespace engine {
namespace physics {

// ═══════════════════════════════════════════════════════════════
// N-Body Gravitation System (Newtonian & Barnes-Hut)
// ═══════════════════════════════════════════════════════════════

namespace GravConstants {
    // Universal Gravitational Constant (m³/kg·s²)
    constexpr float G = 6.67430e-11f; 
}

struct GravityBody {
    size_t rigidBodyIndex = 0;
    math::Vector3D position;
    math::Vector3D force;
    float mass = 0.0f;
};

// ── Barnes-Hut Octree Node ──────────────────────────────────────

struct BHOctreeNode {
    math::Vector3D centerMass = math::Vector3D::Zero;
    float totalMass = 0.0f;
    
    // Bounding Box
    math::Vector3D minBound;
    math::Vector3D maxBound;

    // A node can hold at most 1 body if it is a leaf, or be subdivided
    GravityBody* body = nullptr; 
    std::unique_ptr<BHOctreeNode> children[8];
    bool isLeaf = true;

    BHOctreeNode(const math::Vector3D& _minB, const math::Vector3D& _maxB) 
        : minBound(_minB), maxBound(_maxB) {}

    float getSize() const {
        return (maxBound - minBound).magnitude();
    }

    void insert(GravityBody* newBody) {
        if (totalMass == 0.0f) {
            // Empty leaf, just put it here
            body = newBody;
            totalMass = newBody->mass;
            centerMass = newBody->position;
            return;
        }

        // It has mass, so update COM
        math::Vector3D newCOM = (centerMass * totalMass + newBody->position * newBody->mass) 
                                * (1.0f / (totalMass + newBody->mass));
        centerMass = newCOM;
        totalMass += newBody->mass;

        if (isLeaf) {
            // Needs subdivision
            subdivide();
            insertIntoChildren(body);
            body = nullptr; // Internal node doesn't hold direct body
            isLeaf = false;
        }

        insertIntoChildren(newBody);
    }

private:
    void subdivide() {
        math::Vector3D c = (minBound + maxBound) * 0.5f;
        
        children[0] = std::make_unique<BHOctreeNode>(math::Vector3D(minBound.x, minBound.y, minBound.z), c);
        children[1] = std::make_unique<BHOctreeNode>(math::Vector3D(c.x, minBound.y, minBound.z), math::Vector3D(maxBound.x, c.y, c.z));
        children[2] = std::make_unique<BHOctreeNode>(math::Vector3D(minBound.x, c.y, minBound.z), math::Vector3D(c.x, maxBound.y, c.z));
        children[3] = std::make_unique<BHOctreeNode>(math::Vector3D(c.x, c.y, minBound.z), math::Vector3D(maxBound.x, maxBound.y, c.z));
        
        children[4] = std::make_unique<BHOctreeNode>(math::Vector3D(minBound.x, minBound.y, c.z), math::Vector3D(c.x, c.y, maxBound.z));
        children[5] = std::make_unique<BHOctreeNode>(math::Vector3D(c.x, minBound.y, c.z), math::Vector3D(maxBound.x, c.y, maxBound.z));
        children[6] = std::make_unique<BHOctreeNode>(math::Vector3D(minBound.x, c.y, c.z), math::Vector3D(c.x, maxBound.y, maxBound.z));
        children[7] = std::make_unique<BHOctreeNode>(c, maxBound);
    }

    void insertIntoChildren(GravityBody* b) {
        math::Vector3D c = (minBound + maxBound) * 0.5f;
        int index = 0;
        if (b->position.x > c.x) index |= 1;
        if (b->position.y > c.y) index |= 2;
        if (b->position.z > c.z) index |= 4;
        children[index]->insert(b);
    }
};

// ── N-Body System ────────────────────────────────────────────────

class GravityNBodySystem {
public:
    float G = GravConstants::G;
    float softening = 0.01f; // Avoid division by zero
    float theta = 0.5f;      // Barnes-Hut criteria (s/d < theta)
    bool useBarnesHut = true;

    // Global step
    void step(float dt, std::vector<RigidBody3D>& worldBodies) {
        if (worldBodies.size() < 2) return;

        // 1. Gather all masses
        std::vector<GravityBody> gBodies;
        gBodies.reserve(worldBodies.size());

        math::Vector3D minB = math::Vector3D(1e30f, 1e30f, 1e30f);
        math::Vector3D maxB = math::Vector3D(-1e30f, -1e30f, -1e30f);

        for (size_t i = 0; i < worldBodies.size(); i++) {
            auto& rb = worldBodies[i];
            if (rb.getMass() > 0.0f) {
                math::Vector3D pos = rb.position;
                gBodies.push_back({i, pos, math::Vector3D::Zero, rb.getMass()});
                
                minB.x = std::min(minB.x, pos.x); minB.y = std::min(minB.y, pos.y); minB.z = std::min(minB.z, pos.z);
                maxB.x = std::max(maxB.x, pos.x); maxB.y = std::max(maxB.y, pos.y); maxB.z = std::max(maxB.z, pos.z);
            }
        }

        if (gBodies.size() < 2) return;

        // Bounding box padding
        math::Vector3D pad = (maxB - minB) * 0.1f;
        if (pad.sqrMagnitude() < 1e-6f) pad = math::Vector3D(1, 1, 1);
        minB -= pad; maxB += pad;

        if (useBarnesHut && gBodies.size() > 64) {
            // Build Octree
            BHOctreeNode root(minB, maxB);
            for (auto& gb : gBodies) {
                root.insert(&gb);
            }

            // Compute forces
            for (auto& gb : gBodies) {
                computeForceBH(&root, &gb);
            }
        } else {
            // Direct O(N^2) formulation
            for (size_t i = 0; i < gBodies.size(); i++) {
                for (size_t j = i + 1; j < gBodies.size(); j++) {
                    math::Vector3D diff = gBodies[j].position - gBodies[i].position;
                    float distSqr = diff.sqrMagnitude();
                    float dist = std::sqrt(distSqr + softening * softening);
                    
                    float fMag = G * gBodies[i].mass * gBodies[j].mass / (dist * dist);
                    math::Vector3D force = diff * (fMag / dist); // diff/dist is direction
                    
                    gBodies[i].force += force;
                    gBodies[j].force -= force;
                }
            }
        }

        // 2. Apply forces to RigidBodies
        for (const auto& gb : gBodies) {
            worldBodies[gb.rigidBodyIndex].applyForce(gb.force);
        }
    }

private:
    void computeForceBH(BHOctreeNode* node, GravityBody* target) {
        if (!node || node->totalMass == 0.0f) return;

        math::Vector3D diff = node->centerMass - target->position;
        float distSqr = diff.sqrMagnitude();
        
        // Self check
        if (distSqr < 1e-10f && node->isLeaf) return;

        float dist = std::sqrt(distSqr + softening * softening);
        float s = node->getSize();

        if (node->isLeaf || (s / dist < theta)) {
            // Treat as single body
            float fMag = G * target->mass * node->totalMass / (dist * dist);
            target->force += diff * (fMag / dist);
        } else {
            // Recurse
            for (int i = 0; i < 8; i++) {
                computeForceBH(node->children[i].get(), target);
            }
        }
    }
};

} // namespace physics
} // namespace engine
