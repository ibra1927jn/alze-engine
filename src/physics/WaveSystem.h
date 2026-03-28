#pragma once

#include "RigidBody3D.h"
#include "math/Vector3D.h"
#include <vector>
#include <cmath>
#include <memory>

namespace engine {
namespace physics {

// ═══════════════════════════════════════════════════════════════
// Acoustic / Wave Constants
// ═══════════════════════════════════════════════════════════════

namespace WaveConstants {
    constexpr float SPEED_OF_SOUND_AIR   = 343.0f;  // m/s at 20°C
    constexpr float SPEED_OF_SOUND_WATER = 1480.0f; // m/s
    constexpr float PI = 3.14159265359f;
    constexpr float TWO_PI = 6.28318530718f;
    constexpr float REFERENCE_INTENSITY = 1e-12f;   // W/m^2 (0 dB threshold of human hearing)
}

// ═══════════════════════════════════════════════════════════════
// Basic Wave Source
// ═══════════════════════════════════════════════════════════════

struct WaveSource {
    math::Vector3D position;
    math::Vector3D velocity;
    
    float frequency = 440.0f;  // Hz (A4)
    float amplitude = 1.0f;    // Base wave amplitude (Pressure/Displacement)
    float phase = 0.0f;        // Initial phase phi
    
    // An optional link to a RigidBody3D to sync pos/vel
    int rigidBodyIndex = -1;
};

struct WaveReceiver {
    math::Vector3D position;
    math::Vector3D velocity;
    
    int rigidBodyIndex = -1;
};

// ═══════════════════════════════════════════════════════════════
// Wave Math 
// ═══════════════════════════════════════════════════════════════

namespace WaveMath {

    /// Doppler Effect shift
    /// fs: source frequency, c: wave speed in medium
    /// vs: source velocity vector, vr: receiver velocity vector
    /// positionS: source position, positionR: receiver position
    inline float dopplerFrequency(float fs, float c, 
                                  const math::Vector3D& positionS, const math::Vector3D& vs,
                                  const math::Vector3D& positionR, const math::Vector3D& vr) 
    {
        math::Vector3D directionRtoS = positionS - positionR;
        float dist = directionRtoS.magnitude();
        if (dist < 1e-6f) return fs; // Same place

        // Unit vector from Receiver pointing towards Source
        math::Vector3D dir = directionRtoS * (1.0f / dist);

        // Component of velocities along the line of sight
        // vr_radial is positive if receiver moves TOWARDS source
        float vr_radial = vr.dot(dir);
        
        // vs_radial is positive if source moves TOWARDS receiver (meaning velocity is along -dir)
        float vs_radial = vs.dot(-dir);

        // Formula: f' = f * (c + v_r) / (c - v_s)
        // Guard against sonic booms (moving faster than sound towards each other)
        float denom = c - vs_radial;
        if (denom <= 0.0f) return 0.0f; // Shockwave singularity
        
        float num = c + vr_radial;
        if (num <= 0.0f) return 0.0f; // Reversing away faster than speed of sound

        return fs * (num / denom);
    }

    /// Inverse-square law attenuation for Spherical Waves
    /// Intensity I α 1/r^2. Amplitude A α 1/r
    inline float sphericalAttenuation(float baseAmplitude, float distance) {
        if (distance < 1.0f) return baseAmplitude; // Cap at 1m to prevent singularity
        return baseAmplitude / distance;
    }

    /// Calculate sound intensity level in Decibels (dB)
    /// Power P in Watts, dist in meters. I = P / (4*pi*r^2)
    inline float intensityToDecibels(float acousticPower, float distance) {
        if (distance < 1e-4f) distance = 1e-4f;
        float intensity = acousticPower / (4.0f * WaveConstants::PI * distance * distance);
        if (intensity <= WaveConstants::REFERENCE_INTENSITY) return 0.0f;
        return 10.0f * std::log10(intensity / WaveConstants::REFERENCE_INTENSITY);
    }

} // namespace WaveMath

// ═══════════════════════════════════════════════════════════════
// WaveSystem — Manages sources and interference in a medium
// ═══════════════════════════════════════════════════════════════

class WaveSystem {
public:
    float mediumSpeed = WaveConstants::SPEED_OF_SOUND_AIR;
    float currentTime = 0.0f;

    int addSource(const WaveSource& src) {
        m_sources.push_back(src);
        return static_cast<int>(m_sources.size() - 1);
    }

    int addReceiver(const WaveReceiver& rec) {
        m_receivers.push_back(rec);
        return static_cast<int>(m_receivers.size() - 1);
    }

    WaveSource& getSource(int index) { return m_sources[index]; }
    WaveReceiver& getReceiver(int index) { return m_receivers[index]; }

    void step(float dt, const std::vector<RigidBody3D>& worldBodies) {
        currentTime += dt;

        // Sync with rigidbodies if attached
        for (auto& s : m_sources) {
            if (s.rigidBodyIndex >= 0 && static_cast<size_t>(s.rigidBodyIndex) < worldBodies.size()) {
                s.position = worldBodies[s.rigidBodyIndex].position;
                s.velocity = worldBodies[s.rigidBodyIndex].velocity;
            }
        }
        for (auto& r : m_receivers) {
            if (r.rigidBodyIndex >= 0 && static_cast<size_t>(r.rigidBodyIndex) < worldBodies.size()) {
                r.position = worldBodies[r.rigidBodyIndex].position;
                r.velocity = worldBodies[r.rigidBodyIndex].velocity;
            }
        }
    }

    /// Calculate the instantaneous wave amplitude (superposition) at a specific point in space
    /// y(x,t) = Σ A_i * sin(2π*f_i*t - k_i*x + φ_i)
    float sampleAmplitudeAt(const math::Vector3D& targetPosition) const {
        float totalAmplitude = 0.0f;

        for (const auto& s : m_sources) {
            if (s.amplitude <= 0.0f) continue;
            
            float dist = (s.position - targetPosition).magnitude();
            float A = WaveMath::sphericalAttenuation(s.amplitude, dist);
            
            // Wavenumber k = 2*pi/lambda = 2*pi*f/c
            float k = (WaveConstants::TWO_PI * s.frequency) / mediumSpeed;
            float angularFreq = WaveConstants::TWO_PI * s.frequency;

            // Phase: (wt - kr + phi)
            float phaseVal = angularFreq * currentTime - k * dist + s.phase;
            
            totalAmplitude += A * std::sin(phaseVal);
        }
        return totalAmplitude;
    }

    /// Calculate the perceived frequency of a specific source by a specific receiver
    float getPerceivedFrequency(int sourceIndex, int receiverIndex) const {
        if (sourceIndex < 0 || static_cast<size_t>(sourceIndex) >= m_sources.size()) return 0.0f;
        if (receiverIndex < 0 || static_cast<size_t>(receiverIndex) >= m_receivers.size()) return 0.0f;

        const auto& s = m_sources[sourceIndex];
        const auto& r = m_receivers[receiverIndex];

        return WaveMath::dopplerFrequency(
            s.frequency, mediumSpeed,
            s.position, s.velocity,
            r.position, r.velocity
        );
    }

private:
    std::vector<WaveSource> m_sources;
    std::vector<WaveReceiver> m_receivers;
};

} // namespace physics
} // namespace engine
