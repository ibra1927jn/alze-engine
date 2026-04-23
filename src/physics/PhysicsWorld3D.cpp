// PhysicsWorld3D.cpp — Implementación de PhysicsWorld3D::step,
// solveSingleContact, narrowphaseTest
#include "PhysicsWorld3D.h"
#include "CollisionSolver3D.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include "DynamicBVH3D.h"
#include "core/FrameAllocator.h"
#include "core/JobSystem.h"

namespace engine {
namespace physics { // ── PhysicsWorld3D ─────────────────────────────────────────────

void PhysicsWorld3D::step(float dt) {
    // Reset O(1) FrameAllocator for this frame
    if (!core::FrameAllocator::isInitialized()) {
        core::FrameAllocator::init(10 * 1024 * 1024); // 10MB
    }
    core::FrameAllocator::reset();

    const float subDt = dt / static_cast<float>(subSteps);
    
    for (int step = 0; step < subSteps; step++) {
        // 1. Apply gravity
        for (auto& body : m_bodies) {
            if (!body.isDynamic() || body.isSleeping() || body.m_removed) continue;
            body.applyForce(gravity * body.getMass());
        }

        // 2. Broadphase
        for (int i = 0; i < static_cast<int>(m_bodies.size()); i++) {
            auto& b = m_bodies[i];
            if (b.m_removed) continue;
            if (b.bvhNodeId == -1) {
                b.bvhNodeId = m_broadphase.insert(i, b.getWorldAABB());
            } else {
                if (b.isDynamic() && !b.isSleeping()) {
                    m_broadphase.update(b.bvhNodeId, b.getWorldAABB(), b.velocity * subDt);
                }
            }
        }
        const auto& pairs = m_broadphase.getPotentialPairs();

        // 3. Narrowphase + warm starting
        std::swap(m_contacts, m_prevContacts);
        m_contacts.clear();

        std::sort(m_prevContacts.begin(), m_prevContacts.end(),
            [](const Contact3D& a, const Contact3D& b) { return a.contactHash < b.contactHash; });

        for (const auto& [idA, idB] : pairs) {
            int a = static_cast<int>(idA);
            int b = static_cast<int>(idB);
            if (m_bodies[a].isStatic() && m_bodies[b].isStatic()) continue;
            if (m_bodies[a].isSleeping() && m_bodies[b].isSleeping()) continue;
            if (m_bodies[a].m_removed || m_bodies[b].m_removed) continue;

            ContactInfo info = narrowphaseTest(a, b);
            if (info.hasContact) {
                Contact3D contact;
                contact.bodyA = a;
                contact.bodyB = b;
                contact.normal = info.normal;
                contact.contactPoint = info.contactPoint;
                contact.localPointA = m_bodies[a].getOrientation().conjugate().rotate(info.contactPoint - m_bodies[a].position);
                contact.localPointB = m_bodies[b].getOrientation().conjugate().rotate(info.contactPoint - m_bodies[b].position);
                contact.penetration = info.penetration;
                contact.contactHash = computeContactHash(a, b, info.contactPoint);

                auto it = std::lower_bound(m_prevContacts.begin(), m_prevContacts.end(),
                    contact.contactHash,
                    [](const Contact3D& c, uint32_t h) { return c.contactHash < h; });
                if (it != m_prevContacts.end() && it->contactHash == contact.contactHash) {
                    contact.normalImpulse   = it->normalImpulse;
                    contact.tangentImpulse1 = it->tangentImpulse1;
                    contact.tangentImpulse2 = it->tangentImpulse2;
                }

                m_contacts.push_back(contact);

                // Persistent Manifold: keep up to 3 old contacts for this pair
                int kept = 0;
                for (const auto& oldC : m_prevContacts) {
                    if (oldC.bodyA == a && oldC.bodyB == b) {
                        if (kept >= 3) break;
                        
                        math::Vector3D pA = m_bodies[a].position + m_bodies[a].getOrientation().rotate(oldC.localPointA);
                        math::Vector3D pB = m_bodies[b].position + m_bodies[b].getOrientation().rotate(oldC.localPointB);
                        
                        math::Vector3D diff = pB - pA;
                        float distNorm = diff.dot(oldC.normal); // Expected to be <= 0 for penetration
                        math::Vector3D proj = diff - oldC.normal * distNorm;
                        
                        // Reject if separated or slipped too much
                        if (distNorm < 0.05f && proj.sqrMagnitude() < 0.01f) {
                            // Reject if too close to the NEW point
                            if (math::Vector3D::sqrDistance(pA, info.contactPoint) > 0.005f) {
                                Contact3D c = oldC;
                                c.contactPoint = (pA + pB) * 0.5f;
                                c.penetration = std::max(0.0f, -distNorm);
                                // c retains its old normalImpulse and tangentImpulses!
                                m_contacts.push_back(c);
                                kept++;
                            }
                        }
                    }
                }
                m_bodies[a].wake();
                m_bodies[b].wake();
            }
        }

        // 4. Build Islands & Manage Sleeping
        for (auto& b : m_bodies) {
            b.islandId = -1;
        }

        int currentIslandId = 0;
        int maxBodies = static_cast<int>(m_bodies.size());

        // Construir lista de adyacencia una vez — O(C) en vez de O(N*C) por body
        // Usar FrameAllocator para evitar heap alloc de vector<vector<int>> cada substep
        int* adjCounts = core::FrameAllocator::alloc<int>(maxBodies);
        std::memset(adjCounts, 0, sizeof(int) * maxBodies);
        for (const auto& contact : m_contacts) {
            adjCounts[contact.bodyA]++;
            adjCounts[contact.bodyB]++;
        }
        // Prefijos acumulados para indexar un arreglo flat
        int* adjOffsets = core::FrameAllocator::alloc<int>(maxBodies + 1);
        adjOffsets[0] = 0;
        for (int i = 0; i < maxBodies; ++i) {
            adjOffsets[i + 1] = adjOffsets[i] + adjCounts[i];
        }
        int totalAdj = adjOffsets[maxBodies];
        int* adjData = core::FrameAllocator::alloc<int>(totalAdj > 0 ? totalAdj : 1);
        // Reusar adjCounts como write cursors
        std::memset(adjCounts, 0, sizeof(int) * maxBodies);
        for (const auto& contact : m_contacts) {
            int a = contact.bodyA, b = contact.bodyB;
            adjData[adjOffsets[a] + adjCounts[a]++] = b;
            adjData[adjOffsets[b] + adjCounts[b]++] = a;
        }

        // We need to keep track of islands for parallel solving
        struct IslandData {
            core::FrameArray<int> bodies;
            IslandData() : bodies(0) {} 
            IslandData(int maxB) : bodies(maxB) {}
        };
        
        core::FrameArray<IslandData> islands(maxBodies);

        for (int i = 0; i < maxBodies; ++i) {
            if (!m_bodies[i].isDynamic() || m_bodies[i].islandId != -1 || m_bodies[i].m_removed) continue;

            // Start new island (BFS) using FrameArray to avoid heap allocations
            islands.push_back(IslandData(maxBodies));
            IslandData& currentIsland = islands.back();

            core::FrameArray<int> stack_array(maxBodies);
            
            stack_array.push_back(i);
            m_bodies[i].islandId = currentIslandId;
            
            bool islandCanSleep = true;

            while (!stack_array.empty()) {
                int bodyIdx = stack_array.back();
                stack_array.pop_back();
                currentIsland.bodies.push_back(bodyIdx);
                
                auto& b = m_bodies[bodyIdx];
                if (b.velocity.sqrMagnitude() > 0.05f || b.angularVelocity.sqrMagnitude() > 0.05f) {
                    islandCanSleep = false;
                }

                // Buscar vecinos via lista de adyacencia flat — O(1) por vecino
                for (int ni = adjOffsets[bodyIdx]; ni < adjOffsets[bodyIdx] + adjCounts[bodyIdx]; ++ni) {
                    int neighborIdx = adjData[ni];
                    auto& neighbor = m_bodies[neighborIdx];
                    if (neighbor.isDynamic() && neighbor.islandId == -1) {
                        neighbor.islandId = currentIslandId;
                        stack_array.push_back(neighborIdx);
                    }
                }
            }

            // Update sleep state for the entire island
            if (islandCanSleep) {
                for (int idx : currentIsland.bodies) {
                    m_bodies[idx].addSleepTimer(subDt);
                    if (m_bodies[idx].getSleepTimer() > 1.0f) { // 1 second to sleep
                        m_bodies[idx].forceSleep();
                        m_bodies[idx].velocity = math::Vector3D::Zero;
                        m_bodies[idx].angularVelocity = math::Vector3D::Zero;
                    }
                }
            } else {
                for (int idx : currentIsland.bodies) {
                    m_bodies[idx].wake();
                }
            }

            currentIslandId++;
        }

        // 5. Solve Contacts & Constraints
        
        // Split contacts into islands for parallel solving
        if (m_jobSystem && m_jobSystem->isRunning() && currentIslandId > 1) {
            // Allocate arrays of contact pointers per island in FrameAllocator
            core::FrameArray<Contact3D*>* islandContacts = core::FrameAllocator::alloc<core::FrameArray<Contact3D*>>(currentIslandId);
            for (int i = 0; i < currentIslandId; i++) {
                // Manually construct the FrameArray in place
                new(&islandContacts[i]) core::FrameArray<Contact3D*>(static_cast<int>(m_contacts.size()));
            }

            for (auto& c : m_contacts) {
                int iId = m_bodies[c.bodyA].islandId;
                if (iId == -1) iId = m_bodies[c.bodyB].islandId;
                if (iId != -1) {
                    islandContacts[iId].push_back(&c); // Store pointer to original contact
                }
            }

            // Pre-step for all bodies and constraints (these are global)
            m_solver.preStep(m_contacts, m_bodies, subDt);
            m_constraintSolver.preStep(m_bodies, subDt);

            // Parallel dispatch over islands
            m_jobSystem->parallel_for(0, currentIslandId, 1, [&](int start, int end) {
                for (int i = start; i < end; i++) {
                    for (int iter = 0; iter < m_solver.iterations; iter++) {
                        for (Contact3D* c_ptr : islandContacts[i]) {
                            solveSingleContact(*c_ptr);
                        }
                    }
                }
            });
            // Constraints: una sola pasada, la iteracion interna la maneja ConstraintSolver3D
            // (antes iteraba m_solver.iterations * m_constraintSolver.iterations = 80x)
            m_constraintSolver.solve(m_bodies);

        } else {
            // Sequential fallback (guaranteed to work)
            m_solver.preStep(m_contacts, m_bodies, subDt);
            m_constraintSolver.preStep(m_bodies, subDt);

            for (int iter = 0; iter < m_solver.iterations; iter++) {
                for (auto& c : m_contacts)
                    solveSingleContact(c);
            }
            // Constraints: una sola llamada, iteracion interna en ConstraintSolver3D
            m_constraintSolver.solve(m_bodies);
        }

        // 6. Integrate (WITH CCD)
        for (int i = 0; i < static_cast<int>(m_bodies.size()); ++i) {
            auto& body = m_bodies[i];
            if (!body.isDynamic() || body.isSleeping() || body.m_removed) continue;
            
            if (body.isBullet && body.velocity.sqrMagnitude() > 0.1f) {
                float velMag = body.velocity.magnitude();
                float subVelocity = velMag * subDt;
                float radius = 0.0f;
                if (body.shape == RigidBody3D::Shape::SPHERE) radius = body.sphereRadius;
                else if (body.shape == RigidBody3D::Shape::CAPSULE) radius = body.capsuleRadius;
                else if (body.shape == RigidBody3D::Shape::BOX) radius = std::min({body.boxHalfExtents.x, body.boxHalfExtents.y, body.boxHalfExtents.z});

                float minToi = 1.0f;
                int hitIdx = -1;
                math::Vector3D hitNormal;

                // 1. Analytical TOI against other spheres -> Perfectly exact Sub-stepping
                if (body.shape == RigidBody3D::Shape::SPHERE) {
                    for (int j = 0; j < static_cast<int>(m_bodies.size()); ++j) {
                        if (i == j) continue;
                        const auto& other = m_bodies[j];
                        if (other.shape == RigidBody3D::Shape::SPHERE) {
                            float toi = sphereVsSphereTOI(body.getWorldSphere(), body.position, body.velocity * subDt,
                                                          other.getWorldSphere(), other.position, other.isDynamic() ? other.velocity * subDt : math::Vector3D::Zero);
                            if (toi >= 0.0f && toi < minToi) {
                                minToi = toi;
                                hitIdx = j;
                                math::Vector3D pA = body.position + (body.velocity * subDt) * toi;
                                math::Vector3D pB = other.position + (other.isDynamic() ? other.velocity * subDt : math::Vector3D::Zero) * toi;
                                hitNormal = (pA - pB).normalized();
                            }
                        }
                    }
                }

                // 2. Generic Continuous Raycast (Sphere-cast approximation) against walls/terrain
                if (minToi == 1.0f) {
                    Ray3D ccdRay(body.position, body.velocity / velMag);
                    int rayHitIdx = -1;
                    RayHit3D hit = raycast(ccdRay, rayHitIdx);
                    if (hit.hit && rayHitIdx != i && hit.distance < subVelocity + radius) {
                        float safeDist = std::max(0.0f, hit.distance - radius - 0.005f);
                        float toi = safeDist / subVelocity;
                        if (toi < minToi) {
                            minToi = std::max(0.0f, toi);
                            hitIdx = rayHitIdx;
                            hitNormal = hit.normal;
                        }
                    }
                }

                if (hitIdx != -1 && minToi <= 1.0f) {
                    RigidBody3D& other = m_bodies[hitIdx];
                    if (!other.isBullet) {
                        body.integrate(subDt * minToi); // Integrate exactly up to TOI (Time of Impact)
                        
                        math::Vector3D relVel = body.velocity - other.velocity;
                        float vn = relVel.dot(hitNormal);
                        if (vn < 0.0f) {
                            float restitution = std::max(body.material.restitution, other.material.restitution);
                            float j = -(1.0f + restitution) * vn;
                            j /= (body.getInvMass() + other.getInvMass());
                            math::Vector3D impulse = hitNormal * j;
                            body.velocity += impulse * body.getInvMass();
                            if (other.isDynamic()) {
                                other.velocity -= impulse * other.getInvMass();
                                other.wake(); // Impact wakes the other body
                            }
                        }
                        continue;
                    }
                }
            }
            body.integrate(subDt);
        }

        // 7. Position correction
        {
            const float correctionSlop   = 0.005f;
            const float correctionFactor = 0.4f;
            for (auto& c : m_contacts) {
                if (c.penetration <= correctionSlop) continue;
                RigidBody3D& A = m_bodies[c.bodyA];
                RigidBody3D& B = m_bodies[c.bodyB];
                float invMassA = A.getInvMass(), invMassB = B.getInvMass();
                float totalInvMass = invMassA + invMassB;
                if (totalInvMass < 1e-8f) continue;
                float correction = correctionFactor * (c.penetration - correctionSlop) / totalInvMass;
                math::Vector3D corr = c.normal * correction;
                if (A.isDynamic()) A.position -= corr * invMassA;
                if (B.isDynamic()) B.position += corr * invMassB;
            }
        }

        for (auto& body : m_bodies)
            if (body.isDynamic() && !body.m_removed) body.updateSleep(subDt);
    }

    // 9. Thermal system (if connected)
    if (m_thermalSystem) {
        m_thermalSystem->step(dt, m_bodies);
    }
    if (m_fluidSystem) {
        m_fluidSystem->step(dt);
    }
    if (m_emSystem) {
        m_emSystem->step(dt, m_bodies);
    }
    if (m_softBodySystem) {
        m_softBodySystem->step(dt);
    }
    if (m_gravityNBodySystem) {
        m_gravityNBodySystem->step(dt, m_bodies);
    }
    if (m_waveSystem) {
        m_waveSystem->step(dt, m_bodies);
    }

    // Fire contact callbacks
    if (onContact) {
        for (const auto& c : m_contacts) {
            if (c.normalImpulse > 0.01f)
                onContact(c.bodyA, c.bodyB, c.contactPoint, c.normalImpulse);
        }
    }
}

RayHit3D PhysicsWorld3D::raycast(const Ray3D& ray, int& hitBodyIndex) const {
    RayHit3D closest;
    closest.distance = 1e30f;
    hitBodyIndex = -1;

    // Usar BVH broadphase para descartar bodies rapidamente — O(log N)
    m_broadphase.raycast(ray, [&](int bodyIdx) -> bool {
        if (bodyIdx < 0 || bodyIdx >= static_cast<int>(m_bodies.size())) return true;
        if (m_bodies[bodyIdx].m_removed) return true;

        RayHit3D hit;
        if (m_bodies[bodyIdx].shape == RigidBody3D::Shape::SPHERE)
            hit = rayVsSphere(ray, m_bodies[bodyIdx].getWorldSphere());
        else if (m_bodies[bodyIdx].shape == RigidBody3D::Shape::CAPSULE)
            hit = rayVsCapsule(ray, m_bodies[bodyIdx].getWorldCapsule());
        else
            hit = rayVsAABB(ray, m_bodies[bodyIdx].getWorldAABB());

        if (hit.hit && hit.distance < closest.distance) {
            closest = hit;
            hitBodyIndex = bodyIdx;
        }
        return true; // continuar buscando para encontrar el mas cercano
    });

    return closest;
}

void PhysicsWorld3D::solveSingleContact(Contact3D& c) {
    RigidBody3D& A = m_bodies[c.bodyA];
    RigidBody3D& B = m_bodies[c.bodyB];
    const math::Vector3D& rA = c.rA;
    const math::Vector3D& rB = c.rB;

    // Normal impulse
    math::Vector3D velA = A.velocity + A.angularVelocity.cross(rA);
    math::Vector3D velB = B.velocity + B.angularVelocity.cross(rB);
    math::Vector3D relVel = velB - velA;
    float vn = relVel.dot(c.normal);
    float lambda = c.normalMass * (-(vn + c.bias));
    float newImpulse = std::max(c.normalImpulse + lambda, 0.0f);
    lambda = newImpulse - c.normalImpulse;
    c.normalImpulse = newImpulse;
    math::Vector3D P = c.normal * lambda;
    A.applyImpulseAtArm(-P, rA);
    B.applyImpulseAtArm(P, rB);

    // Friction
    velA = A.velocity + A.angularVelocity.cross(rA);
    velB = B.velocity + B.angularVelocity.cross(rB);
    relVel = velB - velA;
    float vt1 = relVel.dot(c.tangent1);
    float vt2 = relVel.dot(c.tangent2);
    float lt1 = c.tangentMass1 * (-vt1);
    float lt2 = c.tangentMass2 * (-vt2);
    float newT1 = c.tangentImpulse1 + lt1;
    float newT2 = c.tangentImpulse2 + lt2;
    float maxF = c.friction * c.normalImpulse;
    float magSq = newT1 * newT1 + newT2 * newT2;
    if (magSq > maxF * maxF && magSq > 1e-16f) {
        float s = maxF / std::sqrt(magSq);
        newT1 *= s; newT2 *= s;
    }
    lt1 = newT1 - c.tangentImpulse1;
    lt2 = newT2 - c.tangentImpulse2;
    c.tangentImpulse1 = newT1;
    c.tangentImpulse2 = newT2;
    math::Vector3D fP = c.tangent1 * lt1 + c.tangent2 * lt2;
    A.applyImpulseAtArm(-fP, rA);
    B.applyImpulseAtArm(fP, rB);
}

ContactInfo PhysicsWorld3D::narrowphaseTest(int a, int b) {
    const RigidBody3D& bodyA = m_bodies[a];
    const RigidBody3D& bodyB = m_bodies[b];
    using S = RigidBody3D::Shape;

    if (bodyA.shape == S::SPHERE && bodyB.shape == S::SPHERE)
        return sphereVsSphere(bodyA.getWorldSphere(), bodyB.getWorldSphere());
    if (bodyA.shape == S::SPHERE && bodyB.shape == S::BOX) {
        ContactInfo info = obbVsSphere(bodyB.getWorldOBB(), bodyA.getWorldSphere());
        if (info.hasContact) info.normal = -info.normal;
        return info;
    }
    if (bodyA.shape == S::BOX && bodyB.shape == S::SPHERE)
        return obbVsSphere(bodyA.getWorldOBB(), bodyB.getWorldSphere());
    if (bodyA.shape == S::BOX && bodyB.shape == S::BOX)
        return obbVsOBB(bodyA.getWorldOBB(), bodyB.getWorldOBB());
    if (bodyA.shape == S::CAPSULE && bodyB.shape == S::SPHERE)
        return capsuleVsSphere(bodyA.getWorldCapsule(), bodyB.getWorldSphere());
    if (bodyA.shape == S::SPHERE && bodyB.shape == S::CAPSULE) {
        ContactInfo info = capsuleVsSphere(bodyB.getWorldCapsule(), bodyA.getWorldSphere());
        if (info.hasContact) info.normal = -info.normal;
        return info;
    }
    if (bodyA.shape == S::CAPSULE && bodyB.shape == S::CAPSULE)
        return capsuleVsCapsule(bodyA.getWorldCapsule(), bodyB.getWorldCapsule());
    if (bodyA.shape == S::CAPSULE && bodyB.shape == S::BOX)
        return capsuleVsOBB(bodyA.getWorldCapsule(), bodyB.getWorldOBB());
    if (bodyA.shape == S::BOX && bodyB.shape == S::CAPSULE) {
        ContactInfo info = capsuleVsOBB(bodyB.getWorldCapsule(), bodyA.getWorldOBB());
        if (info.hasContact) info.normal = -info.normal;
        return info;
    }
    if (bodyA.shape == S::SPHERE && bodyB.shape == S::HEIGHTFIELD && bodyB.heightfield)
        return sphereVsHeightfield(bodyA.getWorldSphere(), *bodyB.heightfield, bodyB.position);
    if (bodyA.shape == S::HEIGHTFIELD && bodyB.shape == S::SPHERE && bodyA.heightfield) {
        ContactInfo info = sphereVsHeightfield(bodyB.getWorldSphere(), *bodyA.heightfield, bodyA.position);
        if (info.hasContact) info.normal = -info.normal;
        return info;
    }

    // ── ConvexHull via GJK+EPA (generic fallback for any pair) ──
    if (bodyA.shape == S::CONVEX_HULL || bodyB.shape == S::CONVEX_HULL) {
        auto getShape = [](const RigidBody3D& body, OBB3D& obb, SphereCollider& sphere,
                           CapsuleCollider& capsule, ConvexHullCollider& hull) -> int {
            if (body.shape == S::CONVEX_HULL && body.convexHull) {
                hull = body.getWorldConvexHull();
                return 0;
            } else if (body.shape == S::SPHERE) {
                sphere = body.getWorldSphere();
                return 1;
            } else if (body.shape == S::BOX) {
                obb = body.getWorldOBB();
                return 2;
            } else if (body.shape == S::CAPSULE) {
                capsule = body.getWorldCapsule();
                return 3;
            }
            return -1;
        };

        OBB3D obbA, obbB;
        SphereCollider sphereA, sphereB;
        CapsuleCollider capsuleA, capsuleB;
        ConvexHullCollider hullA, hullB;
        int typeA = getShape(bodyA, obbA, sphereA, capsuleA, hullA);
        int typeB = getShape(bodyB, obbB, sphereB, capsuleB, hullB);

        // Dispatch using GJK+EPA for all combinations
        ContactInfo info;
        if (typeA == 0 && typeB == 0) info = gjkEpaContact(hullA, hullB);
        else if (typeA == 0 && typeB == 1) info = gjkEpaContact(hullA, sphereB);
        else if (typeA == 0 && typeB == 2) info = gjkEpaContact(hullA, obbB);
        else if (typeA == 0 && typeB == 3) info = gjkEpaContact(hullA, capsuleB);
        else if (typeA == 1 && typeB == 0) { info = gjkEpaContact(hullB, sphereA); if (info.hasContact) info.normal = -info.normal; }
        else if (typeA == 2 && typeB == 0) { info = gjkEpaContact(hullB, obbA); if (info.hasContact) info.normal = -info.normal; }
        else if (typeA == 3 && typeB == 0) { info = gjkEpaContact(hullB, capsuleA); if (info.hasContact) info.normal = -info.normal; }
        return info;
    }

    return ContactInfo{};
}

uint32_t PhysicsWorld3D::computeContactHash(int bodyA, int bodyB, const math::Vector3D& contactPoint) {
    // Delegate to canonical inline implementation in CollisionSolver3D.h
    return physics::computeContactHash(bodyA, bodyB, contactPoint);
}

} // namespace physics
} // namespace engine
