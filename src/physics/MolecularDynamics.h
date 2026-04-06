#pragma once

#include "math/Vector3D.h"
#include <cmath>
#include <vector>

namespace engine {
namespace physics {

// ═══════════════════════════════════════════════════════════════
// Molecular Dynamics — Atomic-scale particle simulation
//
// Simulates atoms/molecules interacting via:
//   - Lennard-Jones potential (van der Waals + Pauli repulsion)
//   - Coulomb (charged atoms/ions)
//   - Morse potential (covalent bonds)
//
// Uses Velocity-Verlet integration for symplectic stability.
// ═══════════════════════════════════════════════════════════════

namespace MDConstants {
    constexpr double BOLTZMANN    = 1.380649e-23;    // J/K
    constexpr double AVOGADRO     = 6.02214076e23;
    constexpr double ELECTRON_CHARGE = 1.602176634e-19; // C
    constexpr double ANGSTROM     = 1e-10;            // m
    constexpr double AMU          = 1.66053906660e-27; // kg (atomic mass unit)
    constexpr double COULOMB_K    = 8.9875517873681764e9; // N·m²/C²
}

// ═══════════════════════════════════════════════════════════════
// Lennard-Jones Potential — V(r) = 4ε[(σ/r)¹² - (σ/r)⁶]
// Models van der Waals attraction + Pauli short-range repulsion
// ═══════════════════════════════════════════════════════════════

struct LennardJonesParams {
    double epsilon = 0.0;  // Well depth (J)
    double sigma = 0.0;    // Distance at V=0 (m)

    // Equilibrium distance: r_min = 2^(1/6) · σ
    double equilibriumDistance() const { return sigma * 1.122462048309373; }

    // ── Presets (ε in eV, σ in Å, stored in SI) ──────────────
    static LennardJonesParams Argon() {
        return {1.654e-21, 3.405 * MDConstants::ANGSTROM}; // ε=0.0103eV, σ=3.405Å
    }
    static LennardJonesParams Neon() {
        return {4.92e-22, 2.749 * MDConstants::ANGSTROM};
    }
    static LennardJonesParams Xenon() {
        return {3.20e-21, 3.961 * MDConstants::ANGSTROM};
    }
    static LennardJonesParams Nitrogen() {
        return {1.24e-21, 3.681 * MDConstants::ANGSTROM};
    }
    static LennardJonesParams Water_TIP3P() {
        return {1.075e-21, 3.1506 * MDConstants::ANGSTROM}; // O-O LJ
    }
};

namespace LJMath {

    /// LJ potential energy V(r) = 4ε[(σ/r)¹² - (σ/r)⁶]
    inline double potential(double r, const LennardJonesParams& p) {
        if (r <= 0) return 0;
        double sr6 = p.sigma / r;
        sr6 = sr6 * sr6 * sr6; sr6 = sr6 * sr6; // (σ/r)⁶
        double sr12 = sr6 * sr6;
        return 4.0 * p.epsilon * (sr12 - sr6);
    }

    /// LJ force magnitude: F(r) = 24ε/r · [2(σ/r)¹² - (σ/r)⁶]
    /// Positive = repulsive, Negative = attractive
    inline double forceMagnitude(double r, const LennardJonesParams& p) {
        if (r <= 0) return 0;
        double sr = p.sigma / r;
        double sr6 = sr * sr * sr; sr6 = sr6 * sr6;
        double sr12 = sr6 * sr6;
        return 24.0 * p.epsilon / r * (2.0 * sr12 - sr6);
    }

    /// LJ force vector between two atoms (from a to b)
    /// Returns force acting on atom a (towards b if attractive)
    inline math::Vector3D forceVector(const math::Vector3D& posA,
                                       const math::Vector3D& posB,
                                       const LennardJonesParams& p,
                                       double cutoffRadius = 0) {
        math::Vector3D diff = posB - posA;
        double r = static_cast<double>(diff.magnitude());
        if (r < 1e-12 || (cutoffRadius > 0 && r > cutoffRadius)) return math::Vector3D::Zero;

        double F = forceMagnitude(r, p);
        float factor = static_cast<float>(F / r);
        return diff * factor;
    }

} // namespace LJMath

// ═══════════════════════════════════════════════════════════════
// Morse Potential — V(r) = D[1 - e^(-a(r-r₀))]²
// Models covalent bond stretching more accurately than harmonic
// ═══════════════════════════════════════════════════════════════

struct MorseParams {
    double D = 0.0;    // Dissociation energy (J)
    double a = 0.0;    // Width parameter (1/m)
    double r0 = 0.0;   // Equilibrium bond length (m)

    static MorseParams HydrogenBond() {
        return {7.4e-19, 1.94e10, 0.74 * MDConstants::ANGSTROM}; // H-H
    }
    static MorseParams OxygenBond() {
        return {8.28e-19, 2.68e10, 1.21 * MDConstants::ANGSTROM}; // O=O
    }
    static MorseParams CarbonCarbon() {
        return {6.03e-19, 1.87e10, 1.54 * MDConstants::ANGSTROM}; // C-C single
    }
};

namespace MorseMath {

    inline double potential(double r, const MorseParams& p) {
        double dr = std::exp(-p.a * (r - p.r0));
        double term = 1.0 - dr;
        return p.D * term * term;
    }

    inline double forceMagnitude(double r, const MorseParams& p) {
        double dr = std::exp(-p.a * (r - p.r0));
        return 2.0 * p.D * p.a * dr * (1.0 - dr);
    }

} // namespace MorseMath

// ═══════════════════════════════════════════════════════════════
// MDAtom — A single atom in the simulation
// ═══════════════════════════════════════════════════════════════

struct MDAtom {
    math::Vector3D position;
    math::Vector3D velocity;
    math::Vector3D force;
    double mass = 1.0 * MDConstants::AMU;  // kg
    double charge = 0.0;                    // Coulombs
    int atomicNumber = 0;
    bool isActive = true;
};

// ═══════════════════════════════════════════════════════════════
// MolecularSystem — N-body molecular dynamics
// ═══════════════════════════════════════════════════════════════

class MolecularSystem {
public:
    LennardJonesParams ljParams = LennardJonesParams::Argon();
    double cutoffRadius = 10.0 * MDConstants::ANGSTROM; // LJ cutoff for performance
    double temperature = 300.0; // K  target temperature

    int addAtom(const math::Vector3D& pos, double mass_amu,
                const math::Vector3D& vel = math::Vector3D::Zero) {
        MDAtom a;
        a.position = pos;
        a.velocity = vel;
        a.mass = mass_amu * MDConstants::AMU;
        int idx = static_cast<int>(m_atoms.size());
        m_atoms.push_back(a);
        return idx;
    }

    /// Velocity-Verlet integration step
    void step(double dt) {
        int n = static_cast<int>(m_atoms.size());

        // 1. Half-step velocity: v(t+dt/2) = v(t) + F(t)/(2m) · dt
        for (int i = 0; i < n; i++) {
            if (!m_atoms[i].isActive || m_atoms[i].mass <= 0.0) continue;
            float halfDtOverM = static_cast<float>(0.5 * dt / m_atoms[i].mass);
            m_atoms[i].velocity += m_atoms[i].force * halfDtOverM;
            m_atoms[i].position += m_atoms[i].velocity * static_cast<float>(dt);
        }

        // 2. Compute new forces
        computeForces();

        // 3. Complete velocity: v(t+dt) = v(t+dt/2) + F(t+dt)/(2m) · dt
        for (int i = 0; i < n; i++) {
            if (!m_atoms[i].isActive || m_atoms[i].mass <= 0.0) continue;
            float halfDtOverM = static_cast<float>(0.5 * dt / m_atoms[i].mass);
            m_atoms[i].velocity += m_atoms[i].force * halfDtOverM;
        }
    }

    /// Total kinetic energy
    double kineticEnergy() const {
        double KE = 0;
        for (const auto& a : m_atoms) {
            if (!a.isActive) continue;
            KE += 0.5 * a.mass * static_cast<double>(a.velocity.sqrMagnitude());
        }
        return KE;
    }

    /// Instantaneous temperature: T = 2·KE / (3·N·k_B)
    double instantaneousTemperature() const {
        int N = 0;
        for (const auto& a : m_atoms) if (a.isActive) N++;
        if (N <= 0) return 0;
        return 2.0 * kineticEnergy() / (3.0 * N * MDConstants::BOLTZMANN);
    }

    /// Total LJ potential energy
    double potentialEnergy() const {
        double PE = 0;
        int n = static_cast<int>(m_atoms.size());
        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                if (!m_atoms[i].isActive || !m_atoms[j].isActive) continue;
                double r = static_cast<double>((m_atoms[j].position - m_atoms[i].position).magnitude());
                if (r < cutoffRadius) PE += LJMath::potential(r, ljParams);
            }
        }
        return PE;
    }

    int getAtomCount() const { return static_cast<int>(m_atoms.size()); }
    MDAtom& getAtom(int i) { return m_atoms[i]; }
    const MDAtom& getAtom(int i) const { return m_atoms[i]; }

private:
    std::vector<MDAtom> m_atoms;

    void computeForces() {
        int n = static_cast<int>(m_atoms.size());
        for (auto& a : m_atoms) a.force = math::Vector3D::Zero;

        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                if (!m_atoms[i].isActive || !m_atoms[j].isActive) continue;
                math::Vector3D fij = LJMath::forceVector(
                    m_atoms[i].position, m_atoms[j].position,
                    ljParams, static_cast<double>(cutoffRadius));
                m_atoms[i].force += fij;
                m_atoms[j].force -= fij;  // Newton's 3rd law
            }
        }
    }
};

} // namespace physics
} // namespace engine
