#pragma once

#include "math/Vector3D.h"
#include "PhysicsMath.h"
#include "SpatialHash3D.h"
#include "../core/JobSystem.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <functional>

namespace engine {
namespace physics {

// ═══════════════════════════════════════════════════════════════
// SPH Kernel Functions — Fundamentals of Smoothed Particle Hydrodynamics
// ═══════════════════════════════════════════════════════════════

namespace SPHKernels {

    constexpr float PI = 3.14159265358979f;

    /// Poly6 kernel — used for density estimation
    /// W(r, h) = 315/(64πh⁹) · (h² - |r|²)³
    inline float poly6(float rSq, float h) {
        float hSq = h * h;
        if (rSq >= hSq) return 0.0f;
        float diff = hSq - rSq;
        float h9 = hSq * hSq * hSq * hSq * h; // h^9
        return (315.0f / (64.0f * PI * h9)) * diff * diff * diff;
    }

    /// Poly6 gradient — for surface normal estimation
    inline math::Vector3D poly6Gradient(const math::Vector3D& r, float rSq, float h) {
        float hSq = h * h;
        if (rSq >= hSq) return math::Vector3D::Zero;
        float diff = hSq - rSq;
        float h9 = hSq * hSq * hSq * hSq * h;
        float coeff = -(945.0f / (32.0f * PI * h9)) * diff * diff;
        return r * coeff;
    }

    /// Spiky kernel gradient — used for pressure force
    /// ∇W_spiky(r, h) = -45/(πh⁶) · (h - |r|)² · r̂
    inline math::Vector3D spikyGradient(const math::Vector3D& r, float rLen, float h) {
        if (rLen >= h || rLen < 1e-6f) return math::Vector3D::Zero;
        float diff = h - rLen;
        float h6 = h * h * h * h * h * h;
        float coeff = -(45.0f / (PI * h6)) * diff * diff / rLen;
        return r * coeff;
    }

    /// Viscosity kernel Laplacian — used for viscosity force
    /// ∇²W_viscosity(r, h) = 45/(πh⁶) · (h - |r|)
    inline float viscosityLaplacian(float rLen, float h) {
        if (rLen >= h) return 0.0f;
        float h6 = h * h * h * h * h * h;
        return (45.0f / (PI * h6)) * (h - rLen);
    }

} // namespace SPHKernels

// ═══════════════════════════════════════════════════════════════
// Fluid Material — Real physical properties of fluids
// ═══════════════════════════════════════════════════════════════

struct FluidMaterial {
    float restDensity     = 1000.0f;  // ρ₀ kg/m³ (water = 1000)
    float viscosity       = 0.001f;   // μ Pa·s (water at 20°C = 0.001)
    float gasStiffness    = 2000.0f;  // k — Tait equation stiffness (higher = more incompressible)
    float surfaceTension  = 0.0728f;  // σ N/m (water = 0.0728)
    float colorFieldThreshold = 7.065f; // Threshold for surface detection

    // Thermal coupling
    float specificHeat    = 4186.0f;  // J/(kg·K) water
    float thermalConductivity = 0.606f; // W/(m·K) water

    // ── Presets ──────────────────────────────────────────────
    static FluidMaterial Water() {
        FluidMaterial m;
        m.restDensity = 998.2f; m.viscosity = 0.001002f;
        m.gasStiffness = 2000.0f; m.surfaceTension = 0.0728f;
        m.specificHeat = 4186.0f; m.thermalConductivity = 0.606f;
        return m;
    }
    static FluidMaterial Oil() {
        FluidMaterial m;
        m.restDensity = 900.0f; m.viscosity = 0.03f;
        m.gasStiffness = 1500.0f; m.surfaceTension = 0.032f;
        m.specificHeat = 2000.0f; m.thermalConductivity = 0.15f;
        return m;
    }
    static FluidMaterial Honey() {
        FluidMaterial m;
        m.restDensity = 1400.0f; m.viscosity = 6.0f;
        m.gasStiffness = 3000.0f; m.surfaceTension = 0.05f;
        m.specificHeat = 2500.0f; m.thermalConductivity = 0.5f;
        return m;
    }
    static FluidMaterial Lava() {
        FluidMaterial m;
        m.restDensity = 2700.0f; m.viscosity = 100.0f;
        m.gasStiffness = 5000.0f; m.surfaceTension = 0.4f;
        m.specificHeat = 1600.0f; m.thermalConductivity = 2.0f;
        return m;
    }
    static FluidMaterial Blood() {
        FluidMaterial m;
        m.restDensity = 1060.0f; m.viscosity = 0.004f;
        m.gasStiffness = 2200.0f; m.surfaceTension = 0.058f;
        m.specificHeat = 3617.0f; m.thermalConductivity = 0.52f;
        return m;
    }
    static FluidMaterial Mercury() {
        FluidMaterial m;
        m.restDensity = 13534.0f; m.viscosity = 0.00155f;
        m.gasStiffness = 8000.0f; m.surfaceTension = 0.487f;
        m.specificHeat = 139.0f; m.thermalConductivity = 8.3f;
        return m;
    }
};

// ═══════════════════════════════════════════════════════════════
// FluidParticle — Individual SPH particle
// ═══════════════════════════════════════════════════════════════

struct FluidParticle {
    math::Vector3D position;
    math::Vector3D velocity;
    math::Vector3D acceleration;   // Forces / mass
    float mass         = 0.02f;    // kg (particle mass)
    float density      = 0.0f;     // ρ — computed each step
    float pressure     = 0.0f;     // P — from equation of state
    float temperature  = 293.15f;  // K
    float viscosity    = 50.0f;    // Per-particle viscosity (can be modified by thermal coupling)
    bool  isActive     = true;
};

// ═══════════════════════════════════════════════════════════════
// FluidSystem — SPH Navier-Stokes Solver
//
// Solves the incompressible Navier-Stokes equations using SPH:
//   ρ(∂v/∂t) = -∇P + μ∇²v + ρg + f_surface
//
// Pipeline per step:
//   1. Build spatial hash for neighbor search
//   2. Compute density (ρ) via Poly6 kernel
//   3. Compute pressure via Tait equation: P = k(ρ/ρ₀ - 1)
//   4. Compute pressure force via Spiky kernel gradient
//   5. Compute viscosity force via Viscosity kernel Laplacian
//   6. Compute surface tension via color field gradient
//   7. Apply external forces (gravity, rigid body interaction)
//   8. Integrate (Velocity-Verlet)
//   9. Enforce boundary conditions
// ═══════════════════════════════════════════════════════════════

class FluidSystem {
public:
    // ── Configuration ───────────────────────────────────────────
    FluidMaterial material = FluidMaterial::Water();
    math::Vector3D gravity = math::Vector3D(0, -9.80665f, 0);
    float smoothingRadius = 0.1f;    // h — SPH smoothing length (meters)
    float timeScale = 1.0f;

    /// Attach a JobSystem to parallelize density and force computations
    void setJobSystem(engine::core::JobSystem* jobs) { m_jobs = jobs; }

    // Boundary (AABB container)
    math::Vector3D boundsMin = math::Vector3D(-5, 0, -5);
    math::Vector3D boundsMax = math::Vector3D(5, 10, 5);
    float boundaryDamping = 0.3f;    // Energy lost on boundary collision

    // ── Particle Management ─────────────────────────────────────

    /// Add a single particle at position with velocity
    int addParticle(const math::Vector3D& pos,
                    const math::Vector3D& vel = math::Vector3D::Zero,
                    float mass = 0.02f) {
        FluidParticle p;
        p.position = pos;
        p.velocity = vel;
        p.mass = mass;
        int idx = static_cast<int>(m_particles.size());
        m_particles.push_back(p);
        return idx;
    }

    /// Add a block of particles filling an AABB region
    void addBlock(const math::Vector3D& minCorner,
                  const math::Vector3D& maxCorner,
                  float spacing = 0.0f,
                  const math::Vector3D& initialVel = math::Vector3D::Zero) {
        if (spacing <= 0.0f) spacing = smoothingRadius * 0.5f;
        float particleMass = material.restDensity * spacing * spacing * spacing;

        for (float x = minCorner.x; x <= maxCorner.x; x += spacing) {
            for (float y = minCorner.y; y <= maxCorner.y; y += spacing) {
                for (float z = minCorner.z; z <= maxCorner.z; z += spacing) {
                    addParticle(math::Vector3D(x, y, z), initialVel, particleMass);
                }
            }
        }
    }

    // ── Simulation Step ─────────────────────────────────────────

    void step(float dt) {
        if (m_particles.empty()) return;
        float scaledDt = dt * timeScale;
        int n = static_cast<int>(m_particles.size());

        // 1. Build spatial hash for O(1) neighbor lookups
        buildSpatialHash();

        // 2. Compute density and pressure for each particle
        computeDensityPressure();

        // 3. Compute forces (pressure + viscosity + surface tension + gravity)
        computeForces();

        // 4. Integrate (Symplectic Euler for stability) — parallelizable
        if (m_jobs && n > 128) {
            m_jobs->parallel_for(0, n, 64, [this, scaledDt](int start, int end) {
                for (int i = start; i < end; i++) {
                    auto& p = m_particles[i];
                    if (!p.isActive) return;
                    p.velocity += p.acceleration * scaledDt;
                    p.position += p.velocity * scaledDt;
                }
            });
        } else {
            for (int i = 0; i < n; i++) {
                auto& p = m_particles[i];
                if (!p.isActive) continue;
                p.velocity += p.acceleration * scaledDt;
                p.position += p.velocity * scaledDt;
            }
        }

        // 5. Enforce boundary conditions
        enforceBoundary();

        m_stepCount++;
    }

    // ── Accessors ───────────────────────────────────────────────
    int getParticleCount() const { return static_cast<int>(m_particles.size()); }
    FluidParticle& getParticle(int i) { return m_particles[i]; }
    const FluidParticle& getParticle(int i) const { return m_particles[i]; }
    const std::vector<FluidParticle>& getParticles() const { return m_particles; }
    int getStepCount() const { return m_stepCount; }

    /// Average density of all active particles
    float getAverageDensity() const {
        float sum = 0; int count = 0;
        for (const auto& p : m_particles) {
            if (p.isActive) { sum += p.density; count++; }
        }
        return count > 0 ? sum / count : 0.0f;
    }

    /// Max velocity magnitude
    float getMaxSpeed() const {
        float maxSq = 0;
        for (const auto& p : m_particles) {
            if (p.isActive) {
                float sq = p.velocity.sqrMagnitude();
                if (sq > maxSq) maxSq = sq;
            }
        }
        return std::sqrt(maxSq);
    }

    /// Total kinetic energy: Σ 0.5·m·v²
    float getKineticEnergy() const {
        float E = 0;
        for (const auto& p : m_particles) {
            if (p.isActive) E += 0.5f * p.mass * p.velocity.sqrMagnitude();
        }
        return E;
    }

    /// Total potential energy: Σ m·g·h
    float getPotentialEnergy() const {
        float E = 0;
        float g = gravity.magnitude();
        for (const auto& p : m_particles) {
            if (p.isActive) E += p.mass * g * (p.position.y - boundsMin.y);
        }
        return E;
    }

    // ── Callbacks ───────────────────────────────────────────────
    /// Called when particle contacts boundary (idx, normal, position)
    std::function<void(int, const math::Vector3D&, const math::Vector3D&)> onBoundaryContact;

private:
    std::vector<FluidParticle> m_particles;
    int m_stepCount = 0;
    engine::core::JobSystem* m_jobs = nullptr;

    // Spatial hash for neighbor search
    struct NeighborEntry { int index; float distSq; };
    std::vector<std::vector<int>> m_grid;
    math::Vector3D m_gridMin;
    int m_gridDimX = 0, m_gridDimY = 0, m_gridDimZ = 0;
    float m_cellSize = 0.1f;

    inline int gridIndex(int gx, int gy, int gz) const {
        return gx + gy * m_gridDimX + gz * m_gridDimX * m_gridDimY;
    }

    // ── 1. Build Spatial Hash ───────────────────────────────────
    void buildSpatialHash() {
        m_cellSize = smoothingRadius;
        m_gridMin = boundsMin - math::Vector3D(m_cellSize, m_cellSize, m_cellSize);
        math::Vector3D range = boundsMax - boundsMin + math::Vector3D(m_cellSize * 2, m_cellSize * 2, m_cellSize * 2);
        m_gridDimX = std::max(1, static_cast<int>(range.x / m_cellSize) + 1);
        m_gridDimY = std::max(1, static_cast<int>(range.y / m_cellSize) + 1);
        m_gridDimZ = std::max(1, static_cast<int>(range.z / m_cellSize) + 1);

        int totalCells = m_gridDimX * m_gridDimY * m_gridDimZ;
        // Cap to avoid insane memory usage
        if (totalCells > 500000) {
            m_cellSize *= 2.0f;
            m_gridDimX = std::max(1, static_cast<int>(range.x / m_cellSize) + 1);
            m_gridDimY = std::max(1, static_cast<int>(range.y / m_cellSize) + 1);
            m_gridDimZ = std::max(1, static_cast<int>(range.z / m_cellSize) + 1);
            totalCells = m_gridDimX * m_gridDimY * m_gridDimZ;
        }

        m_grid.resize(totalCells);
        for (auto& cell : m_grid) cell.clear();

        for (int i = 0; i < static_cast<int>(m_particles.size()); i++) {
            if (!m_particles[i].isActive) continue;
            math::Vector3D rel = m_particles[i].position - m_gridMin;
            int gx = std::max(0, std::min(m_gridDimX - 1, static_cast<int>(rel.x / m_cellSize)));
            int gy = std::max(0, std::min(m_gridDimY - 1, static_cast<int>(rel.y / m_cellSize)));
            int gz = std::max(0, std::min(m_gridDimZ - 1, static_cast<int>(rel.z / m_cellSize)));
            m_grid[gridIndex(gx, gy, gz)].push_back(i);
        }
    }

    // Helper: iterate over neighbors of particle i
    template<typename Func>
    void forEachNeighbor(int i, Func&& func) const {
        const auto& pi = m_particles[i];
        math::Vector3D rel = pi.position - m_gridMin;
        int cx = static_cast<int>(rel.x / m_cellSize);
        int cy = static_cast<int>(rel.y / m_cellSize);
        int cz = static_cast<int>(rel.z / m_cellSize);
        float hSq = smoothingRadius * smoothingRadius;

        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                for (int dz = -1; dz <= 1; dz++) {
                    int nx = cx + dx, ny = cy + dy, nz = cz + dz;
                    if (nx < 0 || nx >= m_gridDimX) continue;
                    if (ny < 0 || ny >= m_gridDimY) continue;
                    if (nz < 0 || nz >= m_gridDimZ) continue;

                    const auto& cell = m_grid[gridIndex(nx, ny, nz)];
                    for (int j : cell) {
                        if (j == i || !m_particles[j].isActive) continue;
                        math::Vector3D diff = pi.position - m_particles[j].position;
                        float dSq = diff.sqrMagnitude();
                        if (dSq < hSq) {
                            func(j, diff, dSq);
                        }
                    }
                }
            }
        }
    }

    // ── 2. Compute Density & Pressure ───────────────────────────
    void computeDensityPressure() {
        float h = smoothingRadius;
        int n = static_cast<int>(m_particles.size());

        auto compute = [&](int start, int end) {
            for (int i = start; i < end; i++) {
                auto& pi = m_particles[i];
                if (!pi.isActive) continue;

                // Self-contribution
                float density = pi.mass * SPHKernels::poly6(0.0f, h);

                forEachNeighbor(i, [&](int j, const math::Vector3D&, float dSq) {
                    density += m_particles[j].mass * SPHKernels::poly6(dSq, h);
                });

                pi.density = std::max(density, 1e-6f);
                pi.pressure = material.gasStiffness * (pi.density / material.restDensity - 1.0f);
                if (pi.pressure < 0.0f) pi.pressure = 0.0f;
            }
        };

        if (m_jobs && n > 128)
            m_jobs->parallel_for(0, n, 64, compute);
        else
            compute(0, n);
    }

    // ── 3. Compute Forces ───────────────────────────────────────
    void computeForces() {
        float h = smoothingRadius;
        int n = static_cast<int>(m_particles.size());

        auto compute = [&](int start, int end) {
            for (int i = start; i < end; i++) {
                auto& pi = m_particles[i];
                if (!pi.isActive) continue;

                math::Vector3D fPressure = math::Vector3D::Zero;
                math::Vector3D fViscosity = math::Vector3D::Zero;
                math::Vector3D fSurface = math::Vector3D::Zero;
                math::Vector3D colorFieldGrad = math::Vector3D::Zero;
                float colorFieldLaplacian = 0.0f;

                forEachNeighbor(i, [&](int j, const math::Vector3D& diff, float dSq) {
                    const auto& pj = m_particles[j];
                    float dist = std::sqrt(dSq);

                    math::Vector3D gradW = SPHKernels::spikyGradient(diff, dist, h);
                    fPressure += gradW * (-pj.mass * (pi.pressure + pj.pressure) / (2.0f * pj.density));

                    float lapW = SPHKernels::viscosityLaplacian(dist, h);
                    fViscosity += (pj.velocity - pi.velocity) * (pj.mass / pj.density * lapW);

                    math::Vector3D polyGrad = SPHKernels::poly6Gradient(diff, dSq, h);
                    colorFieldGrad += polyGrad * (pj.mass / pj.density);
                    float poly6Val = SPHKernels::poly6(dSq, h);
                    colorFieldLaplacian += pj.mass / pj.density * poly6Val;
                });

                fViscosity = fViscosity * material.viscosity;

                float colorGradMag = colorFieldGrad.magnitude();
                if (colorGradMag > material.colorFieldThreshold) {
                    math::Vector3D surfaceNormal = colorFieldGrad * (1.0f / colorGradMag);
                    fSurface = surfaceNormal * (-material.surfaceTension * colorFieldLaplacian);
                }

                math::Vector3D totalForce = fPressure + fViscosity + fSurface;
                pi.acceleration = totalForce * (1.0f / pi.density) + gravity;
            }
        };

        if (m_jobs && n > 128)
            m_jobs->parallel_for(0, n, 64, compute);
        else
            compute(0, n);
    }

    // ── 5. Enforce Boundary ─────────────────────────────────────
    void enforceBoundary() {
        float damping = 1.0f - boundaryDamping;
        for (int i = 0; i < static_cast<int>(m_particles.size()); i++) {
            auto& p = m_particles[i];
            if (!p.isActive) continue;

            // Floor
            if (p.position.y < boundsMin.y) {
                p.position.y = boundsMin.y;
                p.velocity.y *= -damping;
                if (onBoundaryContact) onBoundaryContact(i, math::Vector3D(0,1,0), p.position);
            }
            // Ceiling
            if (p.position.y > boundsMax.y) {
                p.position.y = boundsMax.y;
                p.velocity.y *= -damping;
            }
            // Walls X
            if (p.position.x < boundsMin.x) {
                p.position.x = boundsMin.x;
                p.velocity.x *= -damping;
            }
            if (p.position.x > boundsMax.x) {
                p.position.x = boundsMax.x;
                p.velocity.x *= -damping;
            }
            // Walls Z
            if (p.position.z < boundsMin.z) {
                p.position.z = boundsMin.z;
                p.velocity.z *= -damping;
            }
            if (p.position.z > boundsMax.z) {
                p.position.z = boundsMax.z;
                p.velocity.z *= -damping;
            }
        }
    }
};

} // namespace physics
} // namespace engine
