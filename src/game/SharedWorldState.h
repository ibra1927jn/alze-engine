#pragma once
// ═══════════════════════════════════════════════════════════════
// SharedWorldState.h  —  Cross-dimension persistent game state
// ═══════════════════════════════════════════════════════════════
//
// This struct is created once in main.cpp and passed by pointer
// to both PlayState (2D) and Play3DState (3D).  It holds light-
// weight data that needs to persist across dimension switches:
//
//   • score         — jumps / collectibles gathered
//   • portalEnergy  — 0.0→1.0 filled by playing; when full, portal activates
//   • dimension     — 0=2D, 1=3D (so each state knows which realm it's in)
//   • lastPlayer2DX/Y — where the 2D player was when entering 3D
//   • visits2D/3D   — how many times each dimension was entered
//   • messages      — ring buffer of up to 4 cross-dim messages for HUD
//
// ───────────────────────────────────────────────────────────────
#include <string>
#include <array>

namespace engine {
namespace game {

struct SharedWorldState {
    // ── Score & Progression ───────────────────────────────────
    int   score        = 0;       // Increments on jumps & portal collects
    float portalEnergy = 0.0f;   // 0→1; fills as you play; drives HUD bar
    int   dimension    = 0;       // 0=2D, 1=3D

    // ── Cross-dimension position ──────────────────────────────
    float lastPlayer2DX = 512.0f; // Last 2D player world-space X
    float lastPlayer2DY = 400.0f; // Last 2D player world-space Y
    float last3DCamX    = 0.0f;   // Last 3D camera position X
    float last3DCamZ    = 8.0f;   // Last 3D camera position Z

    // ── Visit counters ────────────────────────────────────────
    int visits2D = 0;
    int visits3D = 0;

    // ── Cross-dim messaging (small ring buffer) ───────────────
    static constexpr int MSG_COUNT = 4;
    std::array<std::string, MSG_COUNT> messages;
    int msgHead = 0;  // Points to oldest (next to overwrite)
    float msgTimer = 0.0f; // Seconds current message has shown

    void pushMessage(const std::string& msg) {
        messages[msgHead % MSG_COUNT] = msg;
        msgHead = (msgHead + 1) % MSG_COUNT;
        msgTimer = 3.5f; // Show for 3.5 s
    }

    // Returns latest message (empty if expired)
    const std::string& latestMessage() const {
        int idx = ((msgHead - 1) + MSG_COUNT) % MSG_COUNT;
        return messages[idx];
    }

    void tickMessages(float dt) {
        if (msgTimer > 0.0f) msgTimer -= dt;
    }

    // ── Portal energy helpers ─────────────────────────────────
    void addEnergy(float amount) {
        portalEnergy += amount;
        if (portalEnergy > 1.0f) portalEnergy = 1.0f;
    }

    bool portalReady() const { return portalEnergy >= 1.0f; }
};

} // namespace game
} // namespace engine
