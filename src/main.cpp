#include "core/Engine.h"
#include "core/StateManager.h"
#include "core/EventBus.h"
#include "core/AudioEngine.h"
#include "core/Profiler.h"
#include "core/JobSystem.h"
#include "ecs/ECSCoordinator.h"
#include "ecs/Components.h"
#include "game/PlayState.h"
#include "game/PauseState.h"
#include "game/SharedWorldState.h"
#include <cstdlib>
#include <ctime>

using namespace engine;

/// ─────────────────────────────────────────────────────────────────
/// PhysicsEngine2D — Bootstrapper
///
/// main.cpp es solo un arrancador. Toda la lógica del juego
/// vive en game/PlayState.h y game/PauseState.h.
///
/// Controles:
///   A/D o ←/→       → Mover
///   SPACE/W/↑       → Saltar
///   Click izq.      → Disparar partículas
///   Click der.      → Crear plataforma
///   P               → Pausar / Reanudar
///   G               → Toggle gravedad
///   F1              → Debug sensor
///   F5              → Guardar escena (JSON)
///   F9              → Cargar escena (JSON)
///   R               → Reset
///
/// ─────────────────────────────────────────────────────────────────

static constexpr int WINDOW_W = 1024;
static constexpr int WINDOW_H = 768;

int main(int, char*[]) {
    srand(static_cast<unsigned>(time(nullptr)));

    // ── Core systems ───────────────────────────────────────────
    core::Engine engine("PhysicsEngine2D | Fase 2.5", WINDOW_W, WINDOW_H);
    core::StateManager states;
    core::EventBus eventBus;
    core::AudioEngine audio;

    if (!audio.init()) {
        core::Logger::warn("Engine", "Audio init failed -- continuing without sound");
    }

    // ── Job System (multihilo) ──────────────────────────────────
    core::JobSystem jobs;
    jobs.init();
    core::Logger::info("Engine", "JobSystem: " + std::to_string(jobs.getNumWorkers()) + " workers");

    // ── ECS ────────────────────────────────────────────────────
    ecs::ECSCoordinator ecs;
    ecs.registerComponent<ecs::TransformComponent>();
    ecs.registerComponent<ecs::PhysicsComponent>();
    ecs.registerComponent<ecs::ColliderComponent>();
    ecs.registerComponent<ecs::SpriteComponent>();

    // ── Shared cross-dimension world state ────────────────────
    game::SharedWorldState worldState;
    worldState.pushMessage("DIMENSION 2D  |  Press TAB to enter the 3D realm");

    // ── Push PlayState ───────────────────────────────────
    states.push(std::make_unique<game::PlayState>(engine, ecs, eventBus, audio, &states, &jobs, &worldState));

    // ── Engine callbacks ───────────────────────────────────────
    engine.setInputCallback([&](const core::InputManager& input, float dt) {
        (void)input;
        states.handleInput(dt);
    });

    engine.setUpdateCallback([&](float dt) {
        audio.update(dt); // Crossfade tick
        states.update(dt);
    });

    engine.setRenderCallback([&](core::Window& window, float alpha) {
        (void)window;
        states.render(alpha);
    });

    engine.run();
    states.clear();
    jobs.shutdown();
    audio.shutdown();

    return 0;
}
