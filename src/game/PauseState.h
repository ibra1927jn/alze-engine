#pragma once

#include "core/StateManager.h"
#include "core/Engine.h"
#include "core/DebugDraw.h"
#include "math/Color.h"
#include <SDL.h>

namespace engine {
namespace game {

/// PauseState — Congela la física, dibuja overlay transparente.
///
/// - shouldUpdatePhysics() → false   (física congelada)
/// - isTransparent() → true          (PlayState se dibuja debajo)
/// - P o ESC → pop (volver al juego)
///
class PauseState : public core::IGameState {
public:
    PauseState(core::Engine& engine, core::StateManager& states)
        : m_engine(engine), m_states(states) {}

    const char* getName() const override { return "Pause"; }
    bool shouldUpdatePhysics() const override { return false; }
    bool isTransparent() const override { return true; }

    void onEnter() override {
        m_pulseTimer = 0;
    }

    void handleInput(float dt) override {
        (void)dt;
        auto& input = m_engine.getInput();
        if (input.isKeyPressed(SDL_SCANCODE_P) ||
            input.isKeyPressed(SDL_SCANCODE_ESCAPE)) {
            m_states.pop();
        }
    }

    void update(float dt) override {
        m_pulseTimer += dt;
    }

    void render(float alpha) override {
        (void)alpha;
        SDL_Renderer* r = m_engine.getGraphics().getRenderer();

        // Overlay semi-transparente
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(r, 0, 0, 0, 140);
        SDL_Rect full = {0, 0, m_engine.getWindow().getWidth(), m_engine.getWindow().getHeight()};
        SDL_RenderFillRect(r, &full);

        // "PAUSED" indicator (pulsating)
        float pulse = 0.5f + 0.5f * std::sin(m_pulseTimer * 3.0f);
        uint8_t a = static_cast<uint8_t>(120 + pulse * 135);

        int cx = m_engine.getWindow().getWidth() / 2;
        int cy = m_engine.getWindow().getHeight() / 2;

        // Box
        SDL_SetRenderDrawColor(r, 20, 20, 40, 200);
        SDL_Rect box = {cx - 80, cy - 20, 160, 40};
        SDL_RenderFillRect(r, &box);
        SDL_SetRenderDrawColor(r, 100, 180, 255, a);
        SDL_RenderDrawRect(r, &box);

        // "II" pause icon
        SDL_SetRenderDrawColor(r, 100, 180, 255, a);
        SDL_Rect bar1 = {cx - 12, cy - 10, 8, 20};
        SDL_Rect bar2 = {cx + 4,  cy - 10, 8, 20};
        SDL_RenderFillRect(r, &bar1);
        SDL_RenderFillRect(r, &bar2);

        // "P to resume" hint
        core::DebugDraw::drawTextBg(
            {static_cast<float>(cx) - 40, static_cast<float>(cy) + 25},
            "P to resume", math::Color(150, 180, 220, a));

        core::DebugDraw::flush(r);
    }

private:
    core::Engine& m_engine;
    core::StateManager& m_states;
    float m_pulseTimer = 0;
};

} // namespace game
} // namespace engine
