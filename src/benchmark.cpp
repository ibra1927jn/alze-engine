// benchmark.cpp — Arranque dedicado para BenchmarkScene3D
//
// Uso: ./benchmark_3d.exe
// Controles:
//   WASD/Mouse  → FPS camera
//   F5          → Resetear escena (respawn de 250 entidades)
//   Tab         → Toggle debug colliders
//   ESC         → Cerrar
//
// Este ejecutable es 100% independiente del juego principal.
// Usa solo SceneSetup3D + WorldScene3D (sin PlayState, sin SDL_mixer).
#include <cstdlib>
#include <ctime>

#include "core/Engine.h"
#include "core/StateManager.h"
#include "core/AudioEngine.h"
#include "core/Logger.h"
#include "game/BenchmarkScene3D.h"

using namespace engine;

static constexpr int W = 1280;
static constexpr int H = 720;

int main(int, char*[]) {
    srand(static_cast<unsigned>(time(nullptr)));

    core::Logger::info("Benchmark", "==============================================");
    core::Logger::info("Benchmark", "  ALZE Engine — Benchmark Scene 3D");
    core::Logger::info("Benchmark", "  " + std::to_string(game::BenchmarkScene3D::SPHERE_COUNT)
        + " spheres + " + std::to_string(game::BenchmarkScene3D::CUBE_COUNT) + " cubes");
    core::Logger::info("Benchmark", "==============================================");

    core::Engine engine("ALZE Benchmark | " +
        std::to_string(game::BenchmarkScene3D::SPHERE_COUNT + game::BenchmarkScene3D::CUBE_COUNT)
        + " physics bodies", W, H);

    core::StateManager states;
    core::AudioEngine  audio;

    // Audio es opcional en benchmark
    if (!audio.init()) {
        core::Logger::warn("Benchmark", "Audio no disponible (no requerido en benchmark)");
    }

    // Push única escena de benchmark
    states.push(std::make_unique<game::BenchmarkScene3D>(engine));

    // Callbacks del engine
    engine.setInputCallback([&](const core::InputManager& input, float dt) {
        (void)input;
        states.handleInput(dt);
    });

    engine.setUpdateCallback([&](float dt) {
        states.update(dt);
    });

    engine.setRenderCallback([&](core::Window& window, float alpha) {
        (void)window;
        states.render(alpha);
    });

    engine.run();

    states.clear();
    audio.shutdown();
    return 0;
}
