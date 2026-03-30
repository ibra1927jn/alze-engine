#include "BenchmarkScene3D.h"
#include <glad/gl.h>

namespace engine {
namespace game {

    void BenchmarkScene3D::onEnter() {
        core::Logger::info("BenchmarkScene3D", "=== BENCHMARK INICIADO ===");
        m_engine.getGraphics().setMode(core::GraphicsContext::MODE_3D);

        int w = m_engine.getWindow().getWidth();
        int h = m_engine.getWindow().getHeight();

        // ── 2 líneas = renderer completo, vía SceneSetup3D ────────
        SceneSetup3D::initRenderer(m_renderer, m_postProcess, m_ssao, m_skybox,
                                    m_instancer, m_motionBlur, m_decalRenderer, w, h);
        core::DebugDraw3D::init();

        // IBL + Sol
        auto ibl    = SceneSetup3D::initIBL(m_envMap, m_renderer);
        m_hdriLoaded= ibl.hdriLoaded;
        m_skyParams = ibl.skyParams;
        SceneSetup3D::setupSunLight(m_renderer);

        // Mallas
        SceneSetup3D::initMeshes(m_sphereHigh, m_sphereMed, m_sphereLow,
                                  m_cube, m_plane, m_particleMesh);
        SceneSetup3D::initSphereLOD(m_sphereLOD, m_sphereHigh, m_sphereMed, m_sphereLow);

        // Textura suelo
        m_floorNormal = SceneSetup3D::createFloorNormalMap();
        m_floorNormalWrap.wrapHandle(m_floorNormal.getHandle(), 512, 512);

        // ECS + Physics
        WorldScene3D::registerComponents(m_ecs);
        WorldScene3D::createFloor(m_ecs, m_plane, &m_floorNormalWrap);

        m_physics = std::make_unique<ecs::Physics3DSystem>(m_ecs);
        m_physics->setGravity({0, -9.81f, 0});
        m_physics->setFloorY(0.0f);
        m_physics->setWorldBounds({-30, -1, -30}, {30, 80, 30});
        m_physics->setSubSteps(2);
        m_physics->setSolverIterations(8);
        m_physics->setSleepEnabled(true);

        m_renderSystem = std::make_unique<ecs::Render3DSystem>(m_ecs, m_renderer);

        // Spawn de entidades de benchmark
        spawnAll();

        // Cámara lateral para ver la cascada completa
        m_camera.setPosition({20.0f, 15.0f, 20.0f});
        m_camera.setYaw(-135.0f);
        m_camera.setPitch(-20.0f);

        m_startTime = SDL_GetTicks();
        core::Logger::info("BenchmarkScene3D",
            "Lanzadas " + std::to_string(SPHERE_COUNT + CUBE_COUNT) + " entidades físicas");
    }

    void BenchmarkScene3D::handleInput(float dt) {
        const auto& input = m_engine.getInput();

        m_camera.update(input, dt);

        // F5: resetear escena
        if (input.isKeyPressed(SDL_SCANCODE_F5)) {
            reset();
        }
        // Tab: toggle debug colliders
        if (input.isKeyPressed(SDL_SCANCODE_TAB)) {
            m_showDebug = !m_showDebug;
        }
        // ESC: log y cerrar
        if (input.isKeyPressed(SDL_SCANCODE_ESCAPE)) {
            logResults();
        }
    }

    void BenchmarkScene3D::update(float dt) {
        core::Profiler::beginFrame();
        m_time += dt;

        // Física
        {
            core::ScopedTimer _t(core::Profiler::SECTION_PHYSICS);
            m_physics->update(dt);
        }

        // Debug draw colliders
        if (m_showDebug) {
            core::DebugDraw3D::setEnabled(true);
            core::DebugDraw3D::drawGrid(30.0f, 2.0f, core::DebugDraw3D::GRAY);
            m_ecs.forEach<ecs::Transform3DComponent, ecs::Collider3DComponent>(
                [](ecs::Entity, ecs::Transform3DComponent& t, ecs::Collider3DComponent& c) {
                    auto pos = t.transform.position + c.offset;
                    if (c.shape == ecs::Collider3DComponent::SPHERE)
                        core::DebugDraw3D::drawSphere(pos, c.radius, core::DebugDraw3D::GREEN, 8);
                    else
                        core::DebugDraw3D::drawBox(pos, c.halfExtents, core::DebugDraw3D::CYAN);
                }
            );
        } else {
            core::DebugDraw3D::setEnabled(false);
        }

        // Stats cada segundo
        m_frameCount++;
        m_elapsedSec += dt;
        if (m_elapsedSec >= 1.0f) {
            m_currentFPS = static_cast<float>(m_frameCount) / m_elapsedSec;
            m_frameCount = 0;
            m_elapsedSec = 0.0f;
        }

        core::Profiler::endFrame();
    }

    void BenchmarkScene3D::render(float) {
        core::ScopedTimer _render(core::Profiler::SECTION_RENDER);

        int   w    = m_engine.getGraphics().getWidth();
        int   h    = m_engine.getGraphics().getHeight();
        auto  view = m_camera.getViewMatrix();
        auto  proj = m_camera.getProjectionMatrix(static_cast<float>(w) / h);
        auto  vp   = proj * view;

        // ── HDR FBO ───────────────────────────────────────────────
        m_postProcess.beginScene();  // 0 args — correcto

        // ── Skybox ────────────────────────────────────────────────
        if (m_hdriLoaded && m_envMap.isValid())
            m_skybox.drawCubemap(view, proj, m_envMap.getEnvCubemap(), 1.0f);
        else
            m_skybox.draw(view, proj, m_skyParams);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);

        // ── PBR via ECS ───────────────────────────────────────────
        m_renderer.begin(view, proj);  // API correcta
        m_renderer.clearLights();
        m_renderSystem->run();         // API correcta
        m_renderer.end();

        // ── Debug ─────────────────────────────────────────────────
        if (m_showDebug) core::DebugDraw3D::flushWithDepth(vp);

        // ── SSAO ──────────────────────────────────────────────────
        if (m_ssaoEnabled && m_ssao.isValid()) {
            m_ssao.generate(m_postProcess.getDepthTexture(), proj);
            glActiveTexture(GL_TEXTURE7);
            glBindTexture(GL_TEXTURE_2D, m_ssao.getResult());
        }

        // ── PostProcess ───────────────────────────────────────────
        auto pp = SceneSetup3D::makeVisualStyle(0, m_ssaoEnabled, 0.0f);
        m_postProcess.endScene(pp);

        // ── HUD ───────────────────────────────────────────────────
        renderHUD(w, h);

        m_prevVP = vp;
        SDL_GL_SwapWindow(m_engine.getWindow().getSDLWindow());
    }

    void BenchmarkScene3D::onExit() {
        core::Logger::info("BenchmarkScene3D", "Benchmark finalizado");
        logResults();
    }

    void BenchmarkScene3D::spawnAll() {
        const renderer::Material mats[] = {
            renderer::Material::gold(),  renderer::Material::chrome(),
            renderer::Material::copper(),renderer::Material::emerald(),
            renderer::Material::ruby(),  renderer::Material::iron(),
            renderer::Material::rubber(),renderer::Material::ceramic(),
        };
        constexpr int MAT_COUNT = 8;

        srand(42); // seed fija para reproducibilidad
        for (int i = 0; i < SPHERE_COUNT; i++) {
            float x = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 50.0f - 25.0f;
            float z = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 50.0f - 25.0f;
            float r = 0.3f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 0.8f;
            float y = SPAWN_HEIGHT + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 20.0f;

            auto e  = m_ecs.createEntity();
            auto& t = m_ecs.emplaceComponent<ecs::Transform3DComponent>(e, math::Vector3D(x, y, z));
            float d = r * 2.0f;
            t.transform.scale = {d, d, d}; t.dirty = true;

            m_ecs.addComponent<ecs::MeshComponent>(e, {&m_sphereHigh, mats[i % MAT_COUNT]});
            auto& p = m_ecs.emplaceComponent<ecs::Physics3DComponent>(e, 1.5f + r);
            p.restitution = 0.45f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 0.3f;
            p.friction    = 0.2f;
            m_ecs.addComponent<ecs::Collider3DComponent>(e, ecs::Collider3DComponent::sphere(r));
            m_benchEntities.push_back(e);
        }

        for (int i = 0; i < CUBE_COUNT; i++) {
            float x  = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 48.0f - 24.0f;
            float z  = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 48.0f - 24.0f;
            float sc = 0.4f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 1.0f;
            float y  = SPAWN_HEIGHT * 0.5f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 10.0f;

            auto e  = m_ecs.createEntity();
            auto& t = m_ecs.emplaceComponent<ecs::Transform3DComponent>(e, math::Vector3D(x, y, z));
            t.transform.scale = {sc, sc, sc}; t.dirty = true;

            m_ecs.addComponent<ecs::MeshComponent>(e, {&m_cube, mats[(i + 3) % MAT_COUNT]});
            auto& p = m_ecs.emplaceComponent<ecs::Physics3DComponent>(e, 2.0f + sc);
            p.restitution = 0.3f; p.friction = 0.5f;
            m_ecs.addComponent<ecs::Collider3DComponent>(
                e, ecs::Collider3DComponent::box({sc*0.5f, sc*0.5f, sc*0.5f}));
            m_benchEntities.push_back(e);
        }
    }

    void BenchmarkScene3D::reset() {
        for (auto e : m_benchEntities)
            if (m_ecs.isAlive(e)) m_ecs.destroyEntity(e);
        m_benchEntities.clear();
        spawnAll();
        m_resetCount++;
        core::Logger::info("BenchmarkScene3D", "Reset #" + std::to_string(m_resetCount));
    }

    void BenchmarkScene3D::renderHUD(int sw, int sh) {
        auto ortho = renderer::SpriteBatch2D::ortho2D(
            static_cast<float>(sw), static_cast<float>(sh));

        glDisable(GL_DEPTH_TEST);
        m_spriteBatch.begin(ortho, renderer::BlendMode::ALPHA);

        renderer::SpriteColor green  (0.2f, 1.0f, 0.4f, 0.9f);
        renderer::SpriteColor yellow (1.0f, 0.9f, 0.2f, 0.9f);
        renderer::SpriteColor red    (1.0f, 0.3f, 0.2f, 0.9f);
        renderer::SpriteColor dim    (0.7f, 0.8f, 0.7f, 0.7f);

        int   total    = m_ecs.getActiveEntityCount();
        int   sleeping = m_physics->getSleepingCount();
        int   contacts = m_physics->getCollisionCount();
        float elapsed  = static_cast<float>(SDL_GetTicks() - m_startTime) / 1000.0f;

        renderer::SpriteColor fpsColor = (m_currentFPS >= 55.0f) ? green
                                       : (m_currentFPS >= 30.0f) ? yellow : red;

        // FPS + entidades
        m_textRenderer.draw(m_spriteBatch,
            "FPS: " + std::to_string(static_cast<int>(m_currentFPS))
            + "  Entities: " + std::to_string(total),
            10, 10, 2.0f, fpsColor);

        // Physics stats
        m_textRenderer.draw(m_spriteBatch,
            "Contacts: " + std::to_string(contacts)
            + "  Sleeping: "  + std::to_string(sleeping)
            + "  Resets: "    + std::to_string(m_resetCount),
            10, 42, 1.5f, green);

        // Timer + controles
        m_textRenderer.draw(m_spriteBatch,
            "Time: " + std::to_string(static_cast<int>(elapsed)) + "s"
            + "  |  F5=Reset  Tab=Debug  ESC=Salir",
            10, 66, 1.2f, yellow);

        // Info benchmark en la parte inferior
        m_textRenderer.draw(m_spriteBatch,
            "Benchmark: " + std::to_string(SPHERE_COUNT) + " spheres + "
            + std::to_string(CUBE_COUNT) + " cubes — BenchmarkScene3D via SceneSetup3D + WorldScene3D",
            10, static_cast<float>(sh) - 22, 1.1f, dim);

        m_spriteBatch.end();
        glEnable(GL_DEPTH_TEST);
    }

    void BenchmarkScene3D::logResults() {
        float elapsed = static_cast<float>(SDL_GetTicks() - m_startTime) / 1000.0f;
        core::Logger::info("BenchmarkScene3D",
            "=== RESULTADO ==="
            "\n  FPS promedio: " + std::to_string(static_cast<int>(m_currentFPS)) +
            "\n  Entidades:    " + std::to_string(SPHERE_COUNT + CUBE_COUNT) +
            "\n  Tiempo:       " + std::to_string(static_cast<int>(elapsed)) + "s" +
            "\n  Resets:       " + std::to_string(m_resetCount));
    }

} // namespace game
} // namespace engine
