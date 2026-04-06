#pragma once

#include "math/Vector3D.h"
#include <vector>
#include <cmath>
#include <memory>
#include <array>

namespace engine {
namespace physics {

// ═══════════════════════════════════════════════════════════════
// Soft Body Particle (XPBD)
// ═══════════════════════════════════════════════════════════════

struct XPBDParticle {
    math::Vector3D position;
    math::Vector3D prevPosition;
    math::Vector3D velocity;
    
    float mass = 1.0f;
    float invMass = 1.0f;
    
    bool isStatic = false; // Intentionally pinned particle

    void setMass(float m) {
        if (m <= 0.0f) {
            mass = 0.0f;
            invMass = 0.0f;
            isStatic = true;
        } else {
            mass = m;
            invMass = 1.0f / m;
            isStatic = false;
        }
    }
};

// ═══════════════════════════════════════════════════════════════
// XPBD Constraints Base
// ═══════════════════════════════════════════════════════════════

struct XPBDConstraint {
    float compliance = 0.0f; // Softness (0 = rigid constraint, >0 = softer)
    float lambda = 0.0f;     // Required for XPBD (accumulated Lagrange multiplier)
    
    // ── Solid Mechanics ─────────────────────────────────────────
    float yieldStrength = 1e30f;      // Pa — No yield by default
    float ultimateStrength = 1e30f;   // Pa — Unbreakable by default
    float fatigueLimit = 1e30f;       // Pa — Infinite life by default
    float fatigueSNExponent = 3.0f;   // Basquin exponent
    float elasticModulus = 1e9f;      // Young's modulus
    float crossSectionArea = 1e-4f;   // m² (1cm x 1cm)
    
    float plasticStrain = 0.0f;       // Accumulated plastic deformation 
    float fatigueDamage = 0.0f;       // Miner's rule damage [0..1]
    bool broken = false;              // If true, constraint is ignored

    virtual ~XPBDConstraint() = default;
    
    // Solves positional constraint and updates particle predicted positions.
    // dtInv is 1.0f / dt.
    virtual void solve(std::vector<XPBDParticle>& particles, float dt) = 0;
    
    // Applies plastic deformation and fracture mechanics after solver converges
    virtual void applySolidMechanics(float dt) = 0;
};

// ═══════════════════════════════════════════════════════════════
// Distance Constraint (for stretch and shear in cloth/ropes)
// C(x1, x2) = |x1 - x2| - d0 = 0
// ═══════════════════════════════════════════════════════════════

struct XPBDDistanceConstraint : public XPBDConstraint {
    int p1 = 0, p2 = 0;
    float restDistance = 0.0f;

    XPBDDistanceConstraint(int _p1, int _p2, float _restDist, float _compliance = 0.0f)
        : p1(_p1), p2(_p2), restDistance(_restDist) {
        compliance = _compliance;
    }

    void solve(std::vector<XPBDParticle>& particles, float dt) override {
        if (dt <= 0.0f) return;
        auto& pt1 = particles[p1];
        auto& pt2 = particles[p2];

        float w1 = pt1.invMass;
        float w2 = pt2.invMass;
        float wSum = w1 + w2;
        if (wSum == 0.0f) return;

        math::Vector3D diff = pt1.position - pt2.position;
        float len = diff.magnitude();
        if (len < 1e-6f) return;

        math::Vector3D n = diff * (1.0f / len);
        float C = len - restDistance;

        // XPBD formulation
        float alpha = compliance / (dt * dt);
        float dLambda = (-C - alpha * lambda) / (wSum + alpha);
        
        math::Vector3D pCorrection = n * dLambda;

        if (!pt1.isStatic) pt1.position += pCorrection * w1;
        if (!pt2.isStatic) pt2.position -= pCorrection * w2;
        
        lambda += dLambda;
    }
    
    void applySolidMechanics(float dt) override {
        if (broken || dt <= 0.0f) return;
        if (crossSectionArea <= 0.0f) return;

        // F = lambda / dt^2
        float force = std::abs(lambda) / (dt * dt);
        float stress = force / crossSectionArea; // Pa
        
        // 1. Ultimate Fracture (Immediate break)
        if (stress > ultimateStrength) {
            broken = true;
            return;
        }
        
        // 2. Plastic Yield (Permanent Deformation)
        if (stress > yieldStrength) {
            // How much stress exceeded yield
            float excessStress = stress - yieldStrength;
            // delta Strain = sigma / E
            float deltaStrain = (elasticModulus > 0.0f) ? excessStress / elasticModulus : 0.0f;
            plasticStrain += deltaStrain;
            
            // Adjust restDistance (permanent stretch)
            // L_new = L_0 * (1 + plasticStrain)
            restDistance = restDistance * (1.0f + deltaStrain); 
        }
        
        // 3. Fatigue (Cyclic damage approximation per step)
        // Palmgren-Miner rule: D = sum(n_i / N_i). Here we assume each step applies a micro-cycle
        if (stress > fatigueLimit && stress > 0.0f) {
            // N = (ultimate / stress)^m approx curve
            float N_cycles = std::pow(ultimateStrength / stress, fatigueSNExponent);
            if (N_cycles > 0.0f) {
                // We consider 1 time step as somehow relative to 1 oscillation, 
                // or we just accumulate damage proportionally.
                // For a 60Hz sim, let's treat it as dt cycles.
                fatigueDamage += dt / N_cycles;
            }
            if (fatigueDamage >= 1.0f) {
                broken = true;
            }
        }
    }
};

// ═══════════════════════════════════════════════════════════════
// Bending Constraint (for cloth/sheet stiffness)
// Using a simple distance constraint between neighbors of neighbors
// ═══════════════════════════════════════════════════════════════

struct XPBDBendingConstraint : public XPBDConstraint {
    int p1 = 0, p2 = 0;
    float restDistance = 0.0f;

    XPBDBendingConstraint(int _p1, int _p2, float _restDist, float _compliance = 0.0f)
        : p1(_p1), p2(_p2), restDistance(_restDist) {
        compliance = _compliance;
    }

    void solve(std::vector<XPBDParticle>& particles, float dt) override {
        if (dt <= 0.0f) return;
        auto& pt1 = particles[p1];
        auto& pt2 = particles[p2];

        float w1 = pt1.invMass;
        float w2 = pt2.invMass;
        float wSum = w1 + w2;
        if (wSum == 0.0f) return;

        math::Vector3D diff = pt1.position - pt2.position;
        float len = diff.magnitude();
        if (len < 1e-6f) return;

        math::Vector3D n = diff * (1.0f / len);
        float C = len - restDistance;

        float alpha = compliance / (dt * dt);
        float dLambda = (-C - alpha * lambda) / (wSum + alpha);
        
        math::Vector3D pCorrection = n * dLambda;

        if (!pt1.isStatic) pt1.position += pCorrection * w1;
        if (!pt2.isStatic) pt2.position -= pCorrection * w2;
        
        lambda += dLambda;
    }
    
    void applySolidMechanics(float dt) override {
        if (broken || dt <= 0.0f) return;
        if (crossSectionArea <= 0.0f) return;
        float force = std::abs(lambda) / (dt * dt);
        float stress = force / crossSectionArea;
        if (stress > ultimateStrength) { broken = true; return; }
        if (stress > yieldStrength) {
            float excessStress = stress - yieldStrength;
            float deltaStrain = (elasticModulus > 0.0f) ? excessStress / elasticModulus : 0.0f;
            plasticStrain += deltaStrain;
            restDistance = restDistance * (1.0f + deltaStrain); 
        }
        if (stress > fatigueLimit) {
            float N_cycles = std::pow(ultimateStrength / stress, fatigueSNExponent);
            if (N_cycles > 0.0f) fatigueDamage += dt / N_cycles;
            if (fatigueDamage >= 1.0f) broken = true;
        }
    }
};

// Volume Preservation Constraint (for inflated soft bodies / jelly)
struct XPBDVolumeConstraint : public XPBDConstraint {
    std::vector<std::array<int, 3>> triangles;
    float restVolume = 0.0f;
    float pressure = 1.0f;

    XPBDVolumeConstraint(const std::vector<std::array<int, 3>>& tris, float _compliance = 0.0f)
        : triangles(tris) { compliance = _compliance; }

    void computeRestVolume(const std::vector<XPBDParticle>& particles) {
        restVolume = computeVolume(particles);
    }

    float computeVolume(const std::vector<XPBDParticle>& particles) const {
        float vol = 0.0f;
        for (const auto& tri : triangles) {
            const auto& p0 = particles[tri[0]].position;
            const auto& p1 = particles[tri[1]].position;
            const auto& p2 = particles[tri[2]].position;
            vol += p0.dot(p1.cross(p2));
        }
        return vol / 6.0f;
    }

    void solve(std::vector<XPBDParticle>& particles, float dt) override {
        if (dt <= 0.0f) return;
        if (triangles.empty() || std::abs(restVolume) < 1e-8f) return;
        float currentVol = computeVolume(particles);
        float C = currentVol - restVolume * pressure;
        float alpha = compliance / (dt * dt);

        std::vector<math::Vector3D> grads(particles.size(), math::Vector3D::Zero);
        for (const auto& tri : triangles) {
            const auto& p0 = particles[tri[0]].position;
            const auto& p1 = particles[tri[1]].position;
            const auto& p2 = particles[tri[2]].position;
            grads[tri[0]] += p1.cross(p2) * (1.0f / 6.0f);
            grads[tri[1]] += p2.cross(p0) * (1.0f / 6.0f);
            grads[tri[2]] += p0.cross(p1) * (1.0f / 6.0f);
        }

        float wGradSum = 0.0f;
        for (size_t i = 0; i < particles.size(); i++) {
            if (!particles[i].isStatic)
                wGradSum += particles[i].invMass * grads[i].sqrMagnitude();
        }
        if (wGradSum < 1e-10f) return;

        float dLambda = (-C - alpha * lambda) / (wGradSum + alpha);
        for (size_t i = 0; i < particles.size(); i++) {
            if (!particles[i].isStatic)
                particles[i].position += grads[i] * (dLambda * particles[i].invMass);
        }
        lambda += dLambda;
    }

    void applySolidMechanics(float /*dt*/) override {}
};

// ═══════════════════════════════════════════════════════════════
// SoftBody3D — Container for a single soft body (cloth, rope, jelly)
// ═══════════════════════════════════════════════════════════════

class SoftBody3D {
public:
    std::vector<XPBDParticle> m_particles;
    std::vector<std::unique_ptr<XPBDConstraint>> m_constraints;
    
    // Assign material properties to all constraints
    void setMaterial(const PhysicsMaterial& mat) {
        for (auto& c : m_constraints) {
            c->yieldStrength     = mat.yieldStrength;
            c->ultimateStrength  = mat.ultimateStrength;
            c->fatigueLimit      = mat.fatigueLimit;
            c->fatigueSNExponent = mat.fatigueSNExponent;
            c->elasticModulus    = mat.elasticModulus;
            if (mat.crossSectionArea > 0.0f) {
                c->crossSectionArea = mat.crossSectionArea;
            }
        }
    }

    // Body transform info (optional, mainly for rendering)
    math::Vector3D positionOffset = math::Vector3D::Zero;

    /// Get current total kinetic energy
    float getKineticEnergy() const {
        float E = 0;
        for (const auto& p : m_particles) {
            if (!p.isStatic) E += 0.5f * p.mass * p.velocity.sqrMagnitude();
        }
        return E;
    }

    // ── Factories for common shapes ─────────────────────────────

    /// Create a 1D Rope / Chain
    static SoftBody3D createRope(const math::Vector3D& startPos, const math::Vector3D& endPos, 
                                 int numSegments, float totalMass, float compliance = 1e-4f) 
    {
        SoftBody3D body;
        if (numSegments < 1) return body;
        
        int nParticles = numSegments + 1;
        float massPerParticle = totalMass / nParticles;
        float segmentLength = (endPos - startPos).magnitude() / numSegments;
        math::Vector3D dir = (endPos - startPos).normalized();

        for (int i = 0; i < nParticles; i++) {
            XPBDParticle p;
            p.position = startPos + dir * (i * segmentLength);
            p.prevPosition = p.position;
            p.velocity = math::Vector3D::Zero;
            p.setMass(massPerParticle);
            body.m_particles.push_back(p);
        }

        // Structural constraints
        for (int i = 0; i < nParticles - 1; i++) {
            body.m_constraints.push_back(std::make_unique<XPBDDistanceConstraint>(
                i, i + 1, segmentLength, compliance));
        }

        return body;
    }

    /// Create a 2D Cloth patch
    static SoftBody3D createCloth(const math::Vector3D& topLeft, 
                                  const math::Vector3D& edgeU,  // Edge from topLeft to topRt
                                  const math::Vector3D& edgeV,  // Edge from topLeft to btmLft
                                  int resU, int resV, float totalMass, 
                                  float stretchCompliance = 1e-6f, 
                                  float bendCompliance = 1e-4f) 
    {
        SoftBody3D body;
        if (resU < 2 || resV < 2) return body;

        int nParticles = resU * resV;
        float massPerParticle = totalMass / nParticles;

        math::Vector3D stepU = edgeU * (1.0f / (resU - 1));
        math::Vector3D stepV = edgeV * (1.0f / (resV - 1));

        // Generate particles
        for (int v = 0; v < resV; v++) {
            for (int u = 0; u < resU; u++) {
                XPBDParticle p;
                p.position = topLeft + stepU * static_cast<float>(u) + stepV * static_cast<float>(v);
                p.prevPosition = p.position;
                p.velocity = math::Vector3D::Zero;
                p.setMass(massPerParticle);
                body.m_particles.push_back(p);
            }
        }

        auto index = [&](int u, int v) { return v * resU + u; };

        // Constraints
        for (int v = 0; v < resV; v++) {
            for (int u = 0; u < resU; u++) {
                // Structural constraints (stretch)
                if (u < resU - 1) { // Horizontal
                    float dist = stepU.magnitude();
                    body.m_constraints.push_back(std::make_unique<XPBDDistanceConstraint>(
                        index(u, v), index(u + 1, v), dist, stretchCompliance));
                }
                if (v < resV - 1) { // Vertical
                    float dist = stepV.magnitude();
                    body.m_constraints.push_back(std::make_unique<XPBDDistanceConstraint>(
                        index(u, v), index(u, v + 1), dist, stretchCompliance));
                }
                // Shear constraints (diagonals)
                if (u < resU - 1 && v < resV - 1) {
                    float distDiag = (stepU + stepV).magnitude();
                    body.m_constraints.push_back(std::make_unique<XPBDDistanceConstraint>(
                        index(u, v), index(u + 1, v + 1), distDiag, stretchCompliance * 2.0f));
                    
                    float distDiag2 = (stepU - stepV).magnitude();
                    body.m_constraints.push_back(std::make_unique<XPBDDistanceConstraint>(
                        index(u + 1, v), index(u, v + 1), distDiag2, stretchCompliance * 2.0f));
                }
                // Bending constraints (jump by 2)
                if (u < resU - 2) {
                    float dist = (stepU * 2.0f).magnitude();
                    body.m_constraints.push_back(std::make_unique<XPBDBendingConstraint>(
                        index(u, v), index(u + 2, v), dist, bendCompliance));
                }
                if (v < resV - 2) {
                    float dist = (stepV * 2.0f).magnitude();
                    body.m_constraints.push_back(std::make_unique<XPBDBendingConstraint>(
                        index(u, v), index(u, v + 2), dist, bendCompliance));
                }
            }
        }
        return body;
    }
};

// ═══════════════════════════════════════════════════════════════
// SoftBodySystem — Solves all soft bodies using XPBD
// ═══════════════════════════════════════════════════════════════

class SoftBodySystem {
public:
    math::Vector3D gravity = math::Vector3D(0, -9.80665f, 0);
    int solverIterations = 10;
    
    // Simple floor collision for testing
    float floorY = -1e30f;
    float frictionCoeff = 0.5f;
    float damping = 0.99f; // Velocity damping factor per 1/60s frame

    // Self-collision
    bool enableSelfCollision = false;
    float selfCollisionRadius = 0.05f; // Minimum distance between particles

    /// Add a soft body to the system
    int addBody(SoftBody3D&& body) {
        m_softBodies.push_back(std::move(body));
        return static_cast<int>(m_softBodies.size() - 1);
    }

    SoftBody3D& getBody(int index) { return m_softBodies[index]; }

    void step(float dt) {
        if (dt <= 0.0f) return;
        
        for (auto& body : m_softBodies) {
            // 1. Predict positions (Semi-implicit Euler update)
            for (auto& p : body.m_particles) {
                if (p.isStatic) {
                    p.velocity = math::Vector3D::Zero;
                    continue;
                }
                
                // Add external forces (gravity)
                p.velocity += gravity * dt;
                
                p.prevPosition = p.position;
                p.position += p.velocity * dt;
            }

            // Reset constraint multipliers for XPBD
            for (auto& c : body.m_constraints) {
                c->lambda = 0.0f;
            }

            // 2. Solve Constraints (Gauss-Seidel iterations)
            for (int iter = 0; iter < solverIterations; iter++) {
                for (auto& c : body.m_constraints) {
                    if (!c->broken) c->solve(body.m_particles, dt);
                }

                // Self-collision: enforce minimum distance between particles
                if (enableSelfCollision) {
                    float r2 = selfCollisionRadius * selfCollisionRadius;
                    int nParts = static_cast<int>(body.m_particles.size());
                    for (int i = 0; i < nParts; i++) {
                        auto& pi = body.m_particles[i];
                        if (pi.isStatic) continue;
                        for (int j = i + 1; j < nParts; j++) {
                            auto& pj = body.m_particles[j];
                            math::Vector3D diff = pi.position - pj.position;
                            float dist2 = diff.sqrMagnitude();
                            if (dist2 < r2 && dist2 > 1e-12f) {
                                float dist = std::sqrt(dist2);
                                math::Vector3D n = diff * (1.0f / dist);
                                float overlap = selfCollisionRadius - dist;
                                float wi = pi.invMass;
                                float wj = pj.isStatic ? 0.0f : pj.invMass;
                                float wSum = wi + wj;
                                if (wSum > 1e-10f) {
                                    math::Vector3D correction = n * (overlap / wSum);
                                    pi.position += correction * wi;
                                    if (!pj.isStatic) pj.position -= correction * wj;
                                }
                            }
                        }
                    }
                }

                // Simple collision resolution (floor)
                for (auto& p : body.m_particles) {
                    if (p.isStatic) continue;

                    if (p.position.y < floorY) {
                        float depth = floorY - p.position.y;
                        p.position.y = floorY;

                        // Friction approximation during constraint solving
                        math::Vector3D dx = p.position - p.prevPosition;
                        dx.y = 0; // horizontal movement
                        float dxLen = dx.magnitude();
                        if (dxLen > 0) {
                            float maxFriction = depth * frictionCoeff;
                            if (dxLen > maxFriction) {
                                p.position -= dx * (maxFriction / dxLen);
                            } else {
                                p.position -= dx; // stick
                            }
                        }
                    }
                }
            }
            
            // 2.5 Apply Solid Mechanics (Plasticity & Fracture)
            for (auto& c : body.m_constraints) {
                c->applySolidMechanics(dt);
            }

            // 3. Update velocities
            float invDt = 1.0f / dt;
            float dampFactor = std::pow(damping, dt * 60.0f);
            for (auto& p : body.m_particles) {
                if (p.isStatic) continue;
                p.velocity = (p.position - p.prevPosition) * invDt;
                p.velocity *= dampFactor;
            }
        }
    }

private:
    std::vector<SoftBody3D> m_softBodies;
};

} // namespace physics
} // namespace engine
