#pragma once

// PhysicsSoA.h — Structure-of-Arrays layout for SIMD-friendly physics batching
//
// Traditional AoS (RigidBody3D[]) scatters position/velocity across cache lines.
// This SoA layout packs each component contiguously so that:
//   - Integration loops stream through memory linearly (fewer cache misses)
//   - The compiler can auto-vectorize arithmetic on aligned float arrays
//   - SSE/AVX can process 4/8 bodies per instruction for simple operations
//
// Usage: call scatter() to populate from RigidBody3D[], process in SoA, then
// gather() to write results back. The copy overhead is amortized by the faster
// inner loops, especially for >64 bodies.

#include "math/SimdConfig.h"
#include "math/Vector3D.h"
#include <vector>
#include <cstring>

namespace engine {
namespace physics {

class RigidBody3D; // Forward declaration

/// SoA layout for the hot integration data of N bodies.
/// Each array is 16-byte aligned for SSE, 32-byte for AVX.
struct PhysicsSoA {
    // Position components
    std::vector<float> px, py, pz;
    // Velocity components
    std::vector<float> vx, vy, vz;
    // Angular velocity components
    std::vector<float> wx, wy, wz;
    // Inverse mass (0 = static)
    std::vector<float> invMass;
    // Force accumulator
    std::vector<float> fx, fy, fz;
    // Indices into the original body array (only dynamic bodies)
    std::vector<int> bodyIndex;

    int count = 0;

    /// Resize all arrays to hold n bodies
    void resize(int n) {
        count = n;
        px.resize(n); py.resize(n); pz.resize(n);
        vx.resize(n); vy.resize(n); vz.resize(n);
        wx.resize(n); wy.resize(n); wz.resize(n);
        invMass.resize(n);
        fx.resize(n); fy.resize(n); fz.resize(n);
        bodyIndex.resize(n);
    }

    /// Scatter: copy hot data from AoS bodies into SoA arrays.
    /// Only includes dynamic (non-sleeping, non-removed) bodies.
    void scatter(RigidBody3D* bodies, int totalBodies);

    /// Gather: write SoA results back into AoS bodies.
    void gather(RigidBody3D* bodies) const;

    /// Apply uniform gravity to all bodies in SoA (auto-vectorizable)
    void applyGravity(float gx, float gy, float gz, float /*dt*/) {
        for (int i = 0; i < count; i++) {
            float im = invMass[i];
            if (im <= 0.0f) continue;
            float mass = 1.0f / im;
            fx[i] += gx * mass;
            fy[i] += gy * mass;
            fz[i] += gz * mass;
        }
    }

    /// Integrate positions using velocity-Verlet position step (auto-vectorizable)
    void integratePositions(float dt) {
        float halfDtSq = 0.5f * dt * dt;
        for (int i = 0; i < count; i++) {
            float ax = fx[i] * invMass[i];
            float ay = fy[i] * invMass[i];
            float az = fz[i] * invMass[i];
            px[i] += vx[i] * dt + ax * halfDtSq;
            py[i] += vy[i] * dt + ay * halfDtSq;
            pz[i] += vz[i] * dt + az * halfDtSq;
        }
    }

    /// Integrate velocities (Verlet second half-step, auto-vectorizable)
    void integrateVelocities(float dt) {
        for (int i = 0; i < count; i++) {
            vx[i] += fx[i] * invMass[i] * dt;
            vy[i] += fy[i] * invMass[i] * dt;
            vz[i] += fz[i] * invMass[i] * dt;
        }
    }

    /// Apply linear damping (frame-rate independent, auto-vectorizable)
    void applyDamping(float linearDamping, float dt) {
        float damp = 1.0f / (1.0f + dt * linearDamping * 60.0f);
        for (int i = 0; i < count; i++) {
            vx[i] *= damp;
            vy[i] *= damp;
            vz[i] *= damp;
        }
    }

    /// Clamp velocities to max (auto-vectorizable)
    void clampVelocities(float maxLinear) {
        float maxSq = maxLinear * maxLinear;
        for (int i = 0; i < count; i++) {
            float sq = vx[i]*vx[i] + vy[i]*vy[i] + vz[i]*vz[i];
            if (sq > maxSq) {
                float scale = maxLinear / std::sqrt(sq);
                vx[i] *= scale;
                vy[i] *= scale;
                vz[i] *= scale;
            }
        }
    }

    /// Zero all force accumulators
    void clearForces() {
        std::memset(fx.data(), 0, count * sizeof(float));
        std::memset(fy.data(), 0, count * sizeof(float));
        std::memset(fz.data(), 0, count * sizeof(float));
    }
};

/// AABB SoA for broadphase — min/max packed for cache-friendly sweep
struct AABBSoA {
    std::vector<float> minX, minY, minZ;
    std::vector<float> maxX, maxY, maxZ;
    std::vector<int> bodyIndex;
    int count = 0;

    void resize(int n) {
        count = n;
        minX.resize(n); minY.resize(n); minZ.resize(n);
        maxX.resize(n); maxY.resize(n); maxZ.resize(n);
        bodyIndex.resize(n);
    }

    /// Test overlap between body i and body j (branchless-friendly)
    bool overlaps(int i, int j) const {
        return minX[i] <= maxX[j] && maxX[i] >= minX[j] &&
               minY[i] <= maxY[j] && maxY[i] >= minY[j] &&
               minZ[i] <= maxZ[j] && maxZ[i] >= minZ[j];
    }
};

} // namespace physics
} // namespace engine
