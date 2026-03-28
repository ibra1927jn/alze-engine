#pragma once

#include "math/Vector3D.h"
#include <cmath>
#include <string>

namespace engine {
namespace physics {

/// High-precision scalar type for Physics. Use `double` to prevent 
/// drift in Relativity/Quantum, or `float` for raw speed.
using PhysicsScalar = double;

// ═══════════════════════════════════════════════════════════════
// PhysicsConfig — Centralized configuration for ALL physics
//
// Single source of truth for thresholds, constants, and tuning.
// Eliminates hardcoded values scattered across subsystems.
// ═══════════════════════════════════════════════════════════════

struct PhysicsConfig {
    // ── World ──────────────────────────────────────────────────
    math::Vector3D gravity = math::Vector3D(0, -9.81f, 0);
    int   subSteps        = 4;
    int   solverIterations = 10;
    float fixedTimestep    = 1.0f / 60.0f;

    // ── CCD ────────────────────────────────────────────────────
    bool  ccdEnabled       = true;
    float ccdSpeedThreshold = 2.0f;    // m/s — bodies faster than this get CCD

    // ── Sleep ──────────────────────────────────────────────────
    bool  sleepEnabled       = true;
    float sleepEnergyThreshold = 0.01f; // J — below this, body sleeps
    float sleepTimerThreshold  = 1.0f;  // s — must be below energy for this long

    // ── Collision ──────────────────────────────────────────────
    float baumgarteCoeff   = 0.2f;     // Penetration correction aggressiveness
    float penetrationSlop  = 0.005f;   // m — allowed overlap before correction
    float restitutionThreshold = 1.0f; // m/s — min closing vel for bounce

    // ── SPH Fluids ─────────────────────────────────────────────
    float sphSmoothingRadius = 0.1f;
    float sphRestDensity     = 1000.0f; // kg/m³
    float sphGasConstant     = 2000.0f;
    float sphViscosity       = 50.0f;
    int   sphParallelThreshold = 128;   // Use JobSystem above this count

    // ── Thermal ────────────────────────────────────────────────
    float thermalDiffusivity    = 1.43e-7f; // m²/s (water)
    float ambientTemperature    = 293.15f;  // K (20°C)
    float freezingPoint         = 273.15f;  // K
    float boilingPoint          = 373.15f;  // K
    float specificHeatCapacity  = 4186.0f;  // J/(kg·K) (water)

    // ── Chemistry ──────────────────────────────────────────────
    int   chemParallelThreshold = 4;

    // ── Gravity N-Body ─────────────────────────────────────────
    float gravitationalConstant = 6.674e-11f;
    float barnesHutTheta = 0.5f;        // Opening angle for BH approximation
    int   barnesHutThreshold = 64;       // Use BH above this body count

    // ── MHD ────────────────────────────────────────────────────
    float mhdConductivity = 1e6f;        // S/m (default: molten metal)
    math::Vector3D mhdExternalField = math::Vector3D(0, 0, 0); // Tesla

    // ── Performance ────────────────────────────────────────────
    int   broadphaseType = 0;            // 0=BVH, 1=SpatialHash
    float spatialHashCellSize = 2.0f;

    // ── Cross-System Coupling ──────────────────────────────────
    bool  thermoFluidCoupling     = true;  // Temperature affects SPH viscosity
    bool  chemFluidCoupling       = true;  // Reactions heat fluid particles
    bool  emFluidCoupling         = true;  // EM deflects charged fluid particles
    bool  fractureCoupling        = true;  // Fracture spawns rigid body fragments

    // ── Presets ─────────────────────────────────────────────────
    static PhysicsConfig Default() { return {}; }

    static PhysicsConfig HighAccuracy() {
        PhysicsConfig c;
        c.subSteps = 8;
        c.solverIterations = 20;
        c.sphParallelThreshold = 64;
        c.barnesHutTheta = 0.3f;
        return c;
    }

    static PhysicsConfig HighPerformance() {
        PhysicsConfig c;
        c.subSteps = 2;
        c.solverIterations = 6;
        c.sphParallelThreshold = 64;
        c.barnesHutTheta = 0.8f;
        c.sleepEnergyThreshold = 0.05f;
        return c;
    }

    static PhysicsConfig ScientificSimulation() {
        PhysicsConfig c;
        c.subSteps = 16;
        c.solverIterations = 30;
        c.thermoFluidCoupling = true;
        c.chemFluidCoupling = true;
        c.emFluidCoupling = true;
        c.fractureCoupling = true;
        c.barnesHutTheta = 0.3f;
        return c;
    }
};

} // namespace physics
} // namespace engine
