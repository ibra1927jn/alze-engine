#include "Play3DState.h"

namespace engine {
namespace game {

void Play3DState::onEnter() {
        core::Logger::info("Play3DState", "=== ALL SYSTEMS GO ===");
        m_engine.getGraphics().setMode(core::GraphicsContext::MODE_3D);

        // â”€â”€ Subsystem init (via SceneSetup3D) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        int w = m_engine.getWindow().getWidth();
        int h = m_engine.getWindow().getHeight();
        SceneSetup3D::initRenderer(m_renderer, m_postProcess, m_ssao, m_skybox,
                                    m_instancer, m_motionBlur, m_decalRenderer, w, h);
        core::DebugDraw3D::init();
        m_projectilePool.init(200);

        m_texCache.setLoader([](const std::string& path) -> std::shared_ptr<renderer::Texture2D> {
            auto tex = std::make_shared<renderer::Texture2D>();
            if (tex->loadFromFile(path)) return tex;
            return nullptr;
        });

        // â”€â”€ IBL â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        auto ibl = SceneSetup3D::initIBL(m_envMap, m_renderer);
        m_hdriLoaded = ibl.hdriLoaded;
        m_skyParams  = ibl.skyParams;

        // â”€â”€ Sun Directional Light â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        SceneSetup3D::setupSunLight(m_renderer);

        // â”€â”€ Meshes + LOD â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        SceneSetup3D::initMeshes(m_sphereHigh, m_sphereMed, m_sphereLow,
                                  m_cube, m_plane, m_particleMesh);
        SceneSetup3D::initSphereLOD(m_sphereLOD, m_sphereHigh, m_sphereMed, m_sphereLow);

        // â”€â”€ glTF â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        m_helmetLoaded = renderer::ModelLoader::load("assets/models/DamagedHelmet.glb", m_helmet);

        // â”€â”€ Floor + Decal textures â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        m_floorNormal = SceneSetup3D::createFloorNormalMap();
        m_floorNormalWrap.wrapHandle(m_floorNormal.getHandle(), 512, 512);
        m_decalTex = SceneSetup3D::createDecalTexture();

        // â”€â”€ ECS + Scene (via WorldScene3D) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        setupECS();

        // â”€â”€ Physics â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        m_physics = std::make_unique<ecs::Physics3DSystem>(m_ecs);
        m_physics->setGravity({0, -9.81f, 0});
        m_physics->setFloorY(0.0f);
        m_physics->setWorldBounds({-15, -1, -15}, {15, 50, 15});
        m_physics->setSubSteps(2);
        m_physics->setSolverIterations(10);
        m_physics->setSleepEnabled(true);

        // â”€â”€ Render System â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        m_renderSystem = std::make_unique<ecs::Render3DSystem>(m_ecs, m_renderer);

        // â”€â”€ EventBus: collision â†’ sparks + decals â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        m_eventBus.subscribe<Collision3DEvent>([this](const Collision3DEvent& e) {
            if (e.impulse > 1.0f) {
                // Spark burst
                ParticleEmitter3D spark;
                spark.position = e.contactPoint;
                spark.color = {1, 0.8f, 0.3f};
                spark.colorVariation = {0.2f, 0.2f, 0.1f};
                spark.speed = 4.0f;
                spark.speedVariation = 2.0f;
                spark.size = 0.04f;
                spark.lifetime = 0.4f;
                spark.shape = EmitterShape::SPHERE;
                spark.gravity = {0, -6.0f, 0};
                m_particles.burst(spark, static_cast<int>(e.impulse * 3));

                // Floor decal (impact mark)
                if (e.contactPoint.y < 0.1f) {
                    m_decalRenderer.addDecal(
                        e.contactPoint + math::Vector3D(0, 0.01f, 0),
                        {0, 1, 0}, 0.3f + e.impulse * 0.05f, 8.0f);
                }
            }
        });

        // â”€â”€ SceneGraph (pillars ya creados por WorldScene3D) â”€â”€â”€

        // â”€â”€ Particle emitter â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        m_fireEmitter.color = {1.0f, 0.4f, 0.1f};
        m_fireEmitter.colorVariation = {0.3f, 0.3f, 0.1f};
        m_fireEmitter.speed = 2.0f;
        m_fireEmitter.speedVariation = 1.5f;
        m_fireEmitter.size = 0.08f;
        m_fireEmitter.lifetime = 1.5f;
        m_fireEmitter.emitRate = 60.0f;
        m_fireEmitter.shape = EmitterShape::CONE;
        m_fireEmitter.direction = {0, 1, 0};
        m_fireEmitter.coneAngle = 0.3f;
        m_fireEmitter.gravity = {0, -2.0f, 0};

        // â”€â”€ Camera â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        m_camera.setPosition({0, 3, 8});
        m_camera.setYaw(180.0f);
        m_camera.setPitch(-15.0f);
        m_camera.moveSpeed = 5.0f;
        m_camera.fov = 55.0f;

        // â”€â”€ Motion blur VP tracking â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        auto view0 = m_camera.getViewMatrix();
        auto proj0 = m_camera.getProjectionMatrix(static_cast<float>(w) / h);
        m_prevVP = proj0 * view0;

        SDL_SetRelativeMouseMode(SDL_TRUE);
        m_mouseCaptured = true;

        // â”€â”€ HUD Text â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        m_spriteBatch.init();
        m_textRenderer.init();

        core::Logger::info("Play3DState", "Ready! Entities: " + std::to_string(m_ecs.getActiveEntityCount()) +
            " | SceneGraph: " + std::to_string(m_sceneGraph.getTotalNodes()) +
            " | Pool: " + std::to_string(m_projectilePool.capacity()));
    }

    void Play3DState::onExit() {
        m_ecs.destroyAll();
        m_decalRenderer.destroy();
        core::DebugDraw3D::shutdown();
        SDL_SetRelativeMouseMode(SDL_FALSE);
        m_engine.getGraphics().setMode(core::GraphicsContext::MODE_2D);
    }

    void Play3DState::handleInput(float dt) {
        core::ScopedTimer _t(core::Profiler::SECTION_INPUT);
        const auto& input = m_engine.getInput();
        if (input.isKeyPressed(SDL_SCANCODE_TAB)) {
            // C1: Save camera pos and update visits before leaving
            if (m_world) {
                auto camPos = m_camera.getPosition();
                m_world->last3DCamX = camPos.x;
                m_world->last3DCamZ = camPos.z;
                m_world->dimension = 0;
                m_world->visits2D++;
                m_world->pushMessage("DIMENSION 2D  |  Score: " + std::to_string(m_world->score)
                    + "  |  Press TAB to return to 3D");
            }
            m_states->pop(); return;
        }
        if (input.isKeyPressed(SDL_SCANCODE_ESCAPE)) {
            m_mouseCaptured = !m_mouseCaptured;
            SDL_SetRelativeMouseMode(m_mouseCaptured ? SDL_TRUE : SDL_FALSE);
        }
        if (input.isKeyPressed(SDL_SCANCODE_F1)) m_showDebug = !m_showDebug;
        if (input.isKeyPressed(SDL_SCANCODE_F2)) m_showProfiler = !m_showProfiler;
        if (input.isKeyPressed(SDL_SCANCODE_F3)) m_wireframe = !m_wireframe;
        if (input.isKeyPressed(SDL_SCANCODE_F4)) m_ssaoEnabled = !m_ssaoEnabled;
        if (input.isKeyPressed(SDL_SCANCODE_H)) m_showHUD = !m_showHUD;

        // V: cycle visual style (with real shader modes)
        if (input.isKeyPressed(SDL_SCANCODE_V)) {
            m_visualStyle = (m_visualStyle + 1) % 5;
            const char* names[] = {"PBR Realistic", "Cinematic Warm", "Toon Cel-Shade", "Neon Holographic", "Monochrome"};
            // Set render mode on renderer
            auto& rs = m_renderer.getSettings();
            switch (m_visualStyle) {
                case 0: rs.renderMode = 0; break; // PBR
                case 1: rs.renderMode = 0; break; // PBR (warm PostProcess)
                case 2: rs.renderMode = 1; break; // Toon shader
                case 3: rs.renderMode = 2; break; // Neon shader
                case 4: rs.renderMode = 0; break; // PBR (mono PostProcess)
            }
            std::string title = "ALZE Engine | Style: " + std::string(names[m_visualStyle]);
            SDL_SetWindowTitle(m_engine.getWindow().getSDLWindow(), title.c_str());
        }

        // F8: Save 3D scene
        if (input.isKeyPressed(SDL_SCANCODE_F8)) {
            saveScene3D("scene3d.json");
            SDL_SetWindowTitle(m_engine.getWindow().getSDLWindow(), "ALZE Engine | Scene SAVED");
        }
        // F9: Load 3D scene (reset positions)
        if (input.isKeyPressed(SDL_SCANCODE_F9)) {
            int loaded = loadScene3D("scene3d.json");
            std::string title = "ALZE Engine | Loaded " + std::to_string(loaded) + " entities";
            SDL_SetWindowTitle(m_engine.getWindow().getSDLWindow(), title.c_str());
        }

        m_camera.update(input, dt);

        // Right-click: shoot cube
        if (input.isMouseButtonDown(1) && m_shootCd <= 0) {
            shootCube();
            m_shootCd = 0.12f;
        }

        // E key: grab / throw objects
        if (input.isKeyPressed(SDL_SCANCODE_E)) {
            if (m_grabbedEntity != 0 && m_ecs.isAlive(m_grabbedEntity)) {
                // THROW: release with velocity
                auto& p = m_ecs.getComponent<ecs::Physics3DComponent>(m_grabbedEntity);
                p.velocity = m_camera.getForward() * 12.0f + math::Vector3D(0, 3, 0);
                p.wake();
                m_grabbedEntity = 0;
            } else {
                // GRAB: find nearest entity in front of camera
                m_grabbedEntity = findGrabbable();
            }
        }

        // While holding: move grabbed object to in front of camera
        if (m_grabbedEntity != 0 && m_ecs.isAlive(m_grabbedEntity)) {
            auto& t = m_ecs.getComponent<ecs::Transform3DComponent>(m_grabbedEntity);
            auto& p = m_ecs.getComponent<ecs::Physics3DComponent>(m_grabbedEntity);
            math::Vector3D target = m_camera.getPosition() + m_camera.getForward() * 3.0f;
            // Spring-like follow (smooth)
            math::Vector3D diff = target - t.transform.position;
            p.velocity = diff * 10.0f; // Snappy follow
            p.angularVelocity = math::Vector3D::Zero;
            t.dirty = true;
        }

        // C3: Explosion impulse
        if (input.isKeyPressed(SDL_SCANCODE_X)) {
            triggerExplosion(m_camera.getPosition() + m_camera.getForward() * 5.0f, 8.0f, 100.0f);
        }
    }

    void Play3DState::update(float dt) {
        core::Profiler::beginFrame();
        m_time += dt;
        m_shootCd -= dt;
        m_gcTimer += dt;

        // Fade-in timer
        if (m_fadeIn > 0.0f) {
            m_fadeIn -= dt * 1.7f; // â‰ˆ0.6 seconds
            if (m_fadeIn < 0.0f) m_fadeIn = 0.0f;
        }
        // â”€â”€ C2: Beat-reactive scene animation (120 BPM simulation) â”€â”€
        {
            // 120 BPM = 2 Hz beat
            float beatPhase = std::fmod(m_time * 2.0f, 1.0f); // 0..1 per beat
            float beat = std::max(0.0f, std::sin(beatPhase * math::MathUtils::PI * 2.0f));
            // Beat-pulse: brighten directional light on beat
            auto& light = m_renderer.getSettings();
            // Volumetric intensity pulses with beat
            light.volumetricIntensity = beat * 0.6f;
            // Obelisk emitter rate pulses: slow off-beat, fast on-beat
            for (int i = 0; i < 6; i++)
                m_obeliskEmitters[i].emitRate = 20.0f + beat * 80.0f;
        }

        // â”€â”€ ECS Physics â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        {
            core::ScopedTimer _t(core::Profiler::SECTION_PHYSICS);
            m_physics->update(dt);
        }

        // â”€â”€ Collision events â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        {
            core::ScopedTimer _t(core::Profiler::SECTION_COLLISION);
            emitCollisionEvents();
        }

        // â”€â”€ Entity updates â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        if (m_helmetLoaded) {
            auto& t = m_ecs.getComponent<ecs::Transform3DComponent>(m_helmetEntity);
            t.transform.rotation = math::Quaternion::fromAxisAngle({0,1,0}, m_time * 0.4f);
            t.dirty = true;
        }

        for (int i = 0; i < 3; i++) {
            float angle = m_time * 0.3f + i * (math::MathUtils::PI * 2.0f / 3.0f);
            auto& t = m_ecs.getComponent<ecs::Transform3DComponent>(m_lightEntities[i]);
            t.transform.position = {
                std::cos(angle) * 5.0f,
                3.0f + std::sin(m_time * 0.6f + i * 1.5f) * 0.5f,
                std::sin(angle) * 5.0f
            };
            t.dirty = true;
        }

        // â”€â”€ SceneGraph â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        auto* pillarsRoot = m_sceneGraph.findNode("PillarsRoot");
        if (pillarsRoot) {
            pillarsRoot->transform.rotation = math::Quaternion::fromAxisAngle({0,1,0}, m_time * 0.05f);
        }
        m_sceneGraph.updateTransforms();

        // â”€â”€ Particles â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        {
            core::ScopedTimer _t(core::Profiler::SECTION_PARTICLES);
            // Fire on altar ruby gem (plinth top)
            m_fireEmitter.position = {0, 2.0f, 0};
            m_particles.update(dt, m_fireEmitter);

            // 6 obelisk emitters â€” colored floating sparks
            for (int i = 0; i < 6; i++) {
                // Animate position: slow circular oscillation around obelisk cap
                float a = i * (math::MathUtils::PI * 2.0f / 6.0f) + 0.523f;
                float wave = std::sin(m_time * 1.5f + i * 1.04f) * 0.3f;
                m_obeliskEmitters[i].position = {
                    std::cos(a) * 13.5f,
                    4.2f + wave,
                    std::sin(a) * 13.5f
                };
                m_particles.update(dt, m_obeliskEmitters[i]);
            }
        }

        // â”€â”€ Decals â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        m_decalRenderer.update(dt);

        // â”€â”€ ResourceManager GC (every 5 seconds) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        m_gcTimer += dt;
        if (m_gcTimer > 5.0f) {
            m_texCache.collectGarbage();
            m_gcTimer = 0;
        }

        // â”€â”€ Cleanup dead projectiles (ObjectPool) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        m_projectilePool.forEach([this](uint32_t idx, ecs::Entity& e) {
            if (!m_ecs.isAlive(e)) { m_projectilePool.release(idx); return; }
            auto& t = m_ecs.getComponent<ecs::Transform3DComponent>(e);
            if (t.transform.position.y < -5.0f) {
                m_ecs.destroyEntity(e);
                m_projectilePool.release(idx);
            }
        });

        // â”€â”€ DebugDraw3D â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        if (m_showDebug) {
            core::DebugDraw3D::setEnabled(true);
            core::DebugDraw3D::drawAxes({0, 0.01f, 0}, 3.0f);
            core::DebugDraw3D::drawGrid(15.0f, 2.0f, core::DebugDraw3D::GRAY);
            // Colliders
            m_ecs.forEach<ecs::Transform3DComponent, ecs::Collider3DComponent>(
                [](ecs::Entity, ecs::Transform3DComponent& t, ecs::Collider3DComponent& c) {
                    auto pos = t.transform.position + c.offset;
                    if (c.shape == ecs::Collider3DComponent::SPHERE)
                        core::DebugDraw3D::drawSphere(pos, c.radius, core::DebugDraw3D::GREEN, 12);
                    else
                        core::DebugDraw3D::drawBox(pos, c.halfExtents, core::DebugDraw3D::CYAN);
                }
            );
            // Velocity arrows
            m_ecs.forEach<ecs::Transform3DComponent, ecs::Physics3DComponent>(
                [](ecs::Entity, ecs::Transform3DComponent& t, ecs::Physics3DComponent& p) {
                    if (!p.isStatic && !p.sleeping && p.velocity.length() > 0.5f)
                        core::DebugDraw3D::drawRay(t.transform.position, p.velocity, 1.0f,
                            core::DebugDraw3D::YELLOW);
                }
            );
            // World bounds
            core::DebugDraw3D::drawAABB({-15, 0, -15}, {15, 10, 15}, core::DebugDraw3D::MAGENTA);
        } else {
            core::DebugDraw3D::setEnabled(false);
        }

        // â”€â”€ (LOD spheres are now ECS entities â€” rendered by Render3DSystem) â”€â”€
        // â”€â”€ Profiler report â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        if (m_showProfiler && m_time - m_lastProfileLog > 2.0f) {
            std::string stats = core::Profiler::generateReport();
            stats += "\n  Physics: " + std::to_string(m_physics->getCollisionCount()) + " contacts";
            stats += ", " + std::to_string(m_physics->getBroadphasePairs()) + " broad";
            stats += ", " + std::to_string(m_physics->getSleepingCount()) + " sleeping";
            core::Logger::info("Profiler", stats);
            m_lastProfileLog = m_time;
        }
    }

    void Play3DState::render(float) {
        core::ScopedTimer _render(core::Profiler::SECTION_RENDER);
        int w = m_engine.getGraphics().getWidth();
        int h = m_engine.getGraphics().getHeight();
        auto view = m_camera.getViewMatrix();
        auto proj = m_camera.getProjectionMatrix(static_cast<float>(w) / h);
        auto vp = proj * view;

        // â•â• HDR FBO â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
        m_postProcess.beginScene();

        // â”€â”€ Skybox â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        if (m_hdriLoaded && m_envMap.isValid())
            m_skybox.drawCubemap(view, proj, m_envMap.getEnvCubemap(), 1.0f);
        else
            m_skybox.draw(view, proj, m_skyParams);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        if (m_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        // â”€â”€ PBR via ECS â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        m_renderer.begin(view, proj);
        m_renderer.clearLights();
        m_renderSystem->run();

        // â”€â”€ SceneGraph â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        m_sceneGraph.submitToRenderer(m_renderer);

        // â”€â”€ LOD-selected spheres (showcase) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        for (const auto& sp : m_lodSpheres) {
            int lod = m_sphereLOD.selectLOD(m_camera.getPosition(), sp.position);
            if (lod < 0) continue; // Culled
            const renderer::Mesh3D* mesh = m_sphereLOD.getMesh(lod);
            if (!mesh) continue;
            auto model = math::Matrix4x4::translation(sp.position.x, sp.position.y, sp.position.z)
                       * math::Matrix4x4::scale(sp.scale, sp.scale, sp.scale);
            m_renderer.submit(*mesh, sp.material, model);
        }

        // â”€â”€ Instanced field (background decoration) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        renderInstancedField(view, proj);

        // â”€â”€ Particles â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        for (const auto& p : m_particles.pool()) {
            if (!p.active) continue;
            renderer::Material em;
            em.albedoColor = p.color;
            em.emissiveColor = p.color;
            em.emissiveIntensity = 3.0f * p.alpha;
            em.roughness = 1.0f;
            auto model = math::Matrix4x4::translation(p.position.x, p.position.y, p.position.z)
                       * math::Matrix4x4::scale(p.size, p.size, p.size);
            m_renderer.submit(m_particleMesh, em, model);
        }

        m_renderer.end();

        if (m_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // â”€â”€ Decals â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        m_decalRenderer.render(view, proj, m_postProcess.getDepthTexture(), m_decalTex.getHandle());

        // â”€â”€ DebugDraw3D â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        if (m_showDebug) core::DebugDraw3D::flushWithDepth(vp);

        // â”€â”€ SSAO â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        if (m_ssaoEnabled && m_ssao.isValid()) {
            m_ssao.generate(m_postProcess.getDepthTexture(), proj);
            glActiveTexture(GL_TEXTURE7);
            glBindTexture(GL_TEXTURE_2D, m_ssao.getResult());
        }

        // â•â• PostProcess â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
        renderer::PostProcessSettings pp;
        pp.exposure = 1.2f;
        pp.bloomThreshold = 0.9f;
        pp.bloomIntensity = 0.18f;
        pp.bloomSoftKnee = 0.7f;
        pp.bloomPasses = 5;
        pp.vignetteStrength = 0.22f;
        pp.fxaaEnabled = true;
        pp.halfResBloom = true;
        pp.colorContrast = 1.12f;
        pp.colorSaturation = 1.25f;
        pp.time = m_time;
        pp.filmGrain = 0.006f;
        pp.chromaticAberration = 0.0003f;
        pp.sharpenStrength = 0.18f;

        // â”€â”€ Apply visual style preset â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        switch (m_visualStyle) {
            case 1: // Cinematic Warm
                pp.exposure = 1.0f;
                pp.colorContrast = 1.2f;
                pp.colorSaturation = 1.15f;
                pp.vignetteStrength = 0.35f;
                pp.bloomIntensity = 0.25f;
                pp.filmGrain = 0.015f;
                pp.chromaticAberration = 0.0005f;
                break;
            case 2: // Monochrome
                pp.colorSaturation = 0.0f;  // Zero saturation = B/W
                pp.colorContrast = 1.35f;   // High contrast B/W
                pp.vignetteStrength = 0.3f;
                pp.filmGrain = 0.02f;
                pp.bloomIntensity = 0.1f;
                break;
            case 3: // Neon Vibrant
                pp.exposure = 1.5f;
                pp.colorSaturation = 1.8f;
                pp.colorContrast = 1.25f;
                pp.bloomIntensity = 0.35f;
                pp.bloomThreshold = 0.6f;
                pp.vignetteStrength = 0.15f;
                pp.chromaticAberration = 0.001f;
                break;
            default: break; // 0 = PBR Realistic (already set)
        }

        // Fade-in: darken PostProcess output during transition
        if (m_fadeIn > 0.01f) {
            float brightnessMul = 1.0f - m_fadeIn; // 0â†’1 (blackâ†’normal)
            pp.colorContrast *= brightnessMul;
            pp.bloomIntensity *= brightnessMul;
        }

        m_postProcess.endScene(pp);

        // â”€â”€ HUD Text Overlay â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        if (m_showHUD) {
            float sw = static_cast<float>(m_engine.getWindow().getWidth());
            float sh = static_cast<float>(m_engine.getWindow().getHeight());
            auto ortho = renderer::SpriteBatch2D::ortho2D(sw, sh);

            glDisable(GL_DEPTH_TEST);
            m_spriteBatch.begin(ortho, renderer::BlendMode::ALPHA);

            renderer::SpriteColor hudColor(0.0f, 1.0f, 0.4f, 0.85f);
            renderer::SpriteColor dimColor(0.7f, 0.8f, 0.7f, 0.6f);
            float sc = 1.5f;

            // Top-left: stats
            int fps = static_cast<int>(m_engine.getFPS());
            int entities = m_ecs.getActiveEntityCount();
            const char* styleNames[] = {"PBR", "Cinematic", "Mono", "Neon"};
            std::string stats = "FPS: " + std::to_string(fps)
                + "  Entities: " + std::to_string(entities)
                + "  Style: " + styleNames[m_visualStyle];
            m_textRenderer.draw(m_spriteBatch, stats, 10, 10, sc, hudColor);

            // Bottom-left: controls
            m_textRenderer.draw(m_spriteBatch, "WASD Move  E Grab/Throw  V Style  RClick Shoot  TAB 2D", 10, sh - 20, 1.0f, dimColor);

            m_spriteBatch.end();
            glEnable(GL_DEPTH_TEST);
        }

        m_prevVP = vp;
        core::Profiler::endFrame();

        SDL_GL_SwapWindow(m_engine.getWindow().getSDLWindow());
    }


    // â”€â”€ ECS Setup â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    void Play3DState::setupECS() {
        // Delegamos la creaciÃ³n de toda la escena a WorldScene3D
        auto handles = WorldScene3D::createFullScene(
            m_ecs, m_sceneGraph,
            m_sphereHigh, m_cube, m_plane, m_particleMesh,
            m_helmet, m_helmetLoaded,
            &m_floorNormalWrap
        );
        m_helmetEntity   = handles.helmetEntity;
        for (int i = 0; i < 3; i++) m_lightEntities[i] = handles.lightEntities[i];

        // Obeliscos y LOD spheres siguen siendo responsabilidad de Play3DState
        // (lÃ³gica de juego especÃ­fica, no escena genÃ©rica)
        // â”€â”€ ORIGINAL: Ring of 8 stone pillars â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        auto& root = m_sceneGraph.createNode("PillarsRoot");
        for (int i = 0; i < 8; i++) {
            float angle = i * (math::MathUtils::PI * 2.0f / 8.0f);
            auto& pillar = root.createChild("Pillar" + std::to_string(i));
            pillar.transform.position = {std::cos(angle) * 10.0f, 1.5f, std::sin(angle) * 10.0f};
            pillar.transform.scale = {0.4f, 3.0f, 0.4f};
            pillar.mesh = &m_cube; pillar.hasMesh = true;
            pillar.material.albedoColor = {0.6f, 0.55f, 0.50f};
            pillar.material.roughness = 0.7f; pillar.material.metallic = 0.1f;

            auto& cap = pillar.createChild("Cap" + std::to_string(i));
            cap.transform.position = {0, 0.6f, 0};
            cap.transform.scale = {1.5f, 0.15f, 1.5f};
            cap.mesh = &m_cube; cap.hasMesh = true;
            cap.material = renderer::Material::copper();
        }

        // â”€â”€ NEW: Ramp platforms at 4 corners â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        auto& ramps = m_sceneGraph.createNode("Ramps");
        struct RampInfo { float cx, cz, angle; };
        RampInfo rampDefs[] = {
            { 5.0f,  5.0f, 0.3f},
            {-5.0f,  5.0f, -0.3f},
            { 5.0f, -5.0f, -0.3f},
            {-5.0f, -5.0f, 0.3f},
        };
        for (int i = 0; i < 4; i++) {
            auto& ramp = ramps.createChild("Ramp" + std::to_string(i));
            ramp.transform.position = {rampDefs[i].cx, 0.5f, rampDefs[i].cz};
            ramp.transform.scale = {2.5f, 0.2f, 4.0f};
            // Simple rotation via eulerAngles if available, otherwise tilt via scale asymmetry
            ramp.transform.rotation = math::Quaternion::fromAxisAngle(
                {1, 0, 0}, rampDefs[i].angle);
            ramp.mesh = &m_cube; ramp.hasMesh = true;
            ramp.material.albedoColor = {0.45f, 0.50f, 0.60f};
            ramp.material.roughness = 0.8f; ramp.material.metallic = 0.05f;
        }

        // â”€â”€ NEW: Arch gateway (2 pillars + crossbar) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        auto& arch = m_sceneGraph.createNode("Arch");
        // Left post
        auto& ap1 = arch.createChild("ArchLeft");
        ap1.transform.position = {-14.0f, 2.5f, 0.0f};
        ap1.transform.scale = {0.6f, 5.0f, 0.6f};
        ap1.mesh = &m_cube; ap1.hasMesh = true;
        ap1.material = renderer::Material::gold();
        // Right post
        auto& ap2 = arch.createChild("ArchRight");
        ap2.transform.position = {-11.0f, 2.5f, 0.0f};
        ap2.transform.scale = {0.6f, 5.0f, 0.6f};
        ap2.mesh = &m_cube; ap2.hasMesh = true;
        ap2.material = renderer::Material::gold();
        // Crossbar
        auto& crossbar = arch.createChild("Crossbar");
        crossbar.transform.position = {-12.5f, 5.3f, 0.0f};
        crossbar.transform.scale = {4.0f, 0.5f, 0.6f};
        crossbar.mesh = &m_cube; crossbar.hasMesh = true;
        crossbar.material = renderer::Material::gold();
        // Arch decoration sphere
        auto& archOrb = crossbar.createChild("ArchOrb");
        archOrb.transform.position = {0, 0.8f, 0};
        archOrb.transform.scale = {0.5f, 0.5f, 0.5f};
        archOrb.mesh = &m_sphereHigh; archOrb.hasMesh = true;
        archOrb.material = renderer::Material::emerald();
        archOrb.material.emissiveColor = {0.2f, 1.0f, 0.4f};
        archOrb.material.emissiveIntensity = 3.0f;

        // â”€â”€ NEW: Floating platform ring â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        auto& platforms = m_sceneGraph.createNode("FloatingPlatforms");
        for (int i = 0; i < 6; i++) {
            float a = i * (math::MathUtils::PI * 2.0f / 6.0f);
            float height = 2.0f + std::sin(a * 1.5f) * 1.0f; // varied heights
            auto& plat = platforms.createChild("FPlat" + std::to_string(i));
            plat.transform.position = {std::cos(a) * 7.0f, height, std::sin(a) * 7.0f};
            plat.transform.scale = {2.0f, 0.25f, 2.0f};
            plat.mesh = &m_cube; plat.hasMesh = true;
            // Alternate materials
            if (i % 3 == 0) plat.material = renderer::Material::chrome();
            else if (i % 3 == 1) plat.material = renderer::Material::copper();
            else { plat.material.albedoColor = {0.3f, 0.6f, 0.9f}; plat.material.metallic = 0.9f; plat.material.roughness = 0.1f; }
        }

        // â”€â”€ NEW: Central altar â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        auto& altar = m_sceneGraph.createNode("CentralAltar");
        // Plinth base
        auto& plinth = altar.createChild("Plinth");
        plinth.transform.position = {0.0f, 0.5f, 0.0f};
        plinth.transform.scale = {2.0f, 1.0f, 2.0f};
        plinth.mesh = &m_cube; plinth.hasMesh = true;
        plinth.material.albedoColor = {0.7f, 0.65f, 0.6f};
        plinth.material.roughness = 0.6f; plinth.material.metallic = 0.2f;
        // Gem on top
        auto& gem = plinth.createChild("AlterGem");
        gem.transform.position = {0, 0.8f, 0};
        gem.transform.scale = {0.7f, 0.7f, 0.7f};
        gem.mesh = &m_sphereHigh; gem.hasMesh = true;
        gem.material = renderer::Material::ruby();
        gem.material.emissiveColor = {1.0f, 0.1f, 0.1f};
        gem.material.emissiveIntensity = 4.0f;

        // â”€â”€ NEW: Elevated catwalk/bridge â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        auto& catwalk = m_sceneGraph.createNode("Catwalk");
        auto& bridge = catwalk.createChild("BridgeMain");
        bridge.transform.position = {0.0f, 3.0f, -9.0f};
        bridge.transform.scale = {14.0f, 0.3f, 1.5f};
        bridge.mesh = &m_cube; bridge.hasMesh = true;
        bridge.material.albedoColor = {0.4f, 0.4f, 0.5f};
        bridge.material.roughness = 0.5f; bridge.material.metallic = 0.6f;
        // Railing left
        auto& railL = bridge.createChild("RailL");
        railL.transform.position = {0, 0.3f, -0.55f};
        railL.transform.scale = {1.0f, 0.6f, 0.05f};
        railL.mesh = &m_cube; railL.hasMesh = true;
        railL.material = renderer::Material::iron();
        // Railing right
        auto& railR = bridge.createChild("RailR");
        railR.transform.position = {0, 0.3f, 0.55f};
        railR.transform.scale = {1.0f, 0.6f, 0.05f};
        railR.mesh = &m_cube; railR.hasMesh = true;
        railR.material = renderer::Material::iron();

        // â”€â”€ NEW: 6 glowing energy obelisks â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
        auto& obelisks = m_sceneGraph.createNode("Obelisks");
        math::Vector3D obeliskColors[] = {
            {1.0f, 0.3f, 0.1f}, {0.1f, 0.5f, 1.0f}, {0.2f, 1.0f, 0.3f},
            {1.0f, 0.9f, 0.1f}, {0.8f, 0.1f, 1.0f}, {0.1f, 0.9f, 0.9f}
        };
        for (int i = 0; i < 6; i++) {
            float a = i * (math::MathUtils::PI * 2.0f / 6.0f) + 0.523f;
            auto& ob = obelisks.createChild("Obelisk" + std::to_string(i));
            ob.transform.position = {std::cos(a) * 13.5f, 2.0f, std::sin(a) * 13.5f};
            ob.transform.scale = {0.3f, 4.0f, 0.3f};
            ob.mesh = &m_cube; ob.hasMesh = true;
            ob.material.albedoColor = obeliskColors[i];
            ob.material.emissiveColor = obeliskColors[i];
            ob.material.emissiveIntensity = 5.0f;
            ob.material.roughness = 0.2f; ob.material.metallic = 0.8f;
            // Crystal cap on obelisk
            auto& cap = ob.createChild("ObCap" + std::to_string(i));
            cap.transform.position = {0, 0.6f, 0};
            cap.transform.scale = {2.0f, 0.4f, 2.0f};
            cap.mesh = &m_sphereHigh; cap.hasMesh = true;
            cap.material = ob.material;
            cap.material.emissiveIntensity = 8.0f;
        }
    }

    void Play3DState::shootCube() {
        // ObjectPool recycling
        uint32_t poolIdx = m_projectilePool.acquire();
        if (poolIdx == UINT32_MAX) {
            // Pool full â€” recycle oldest
            m_projectilePool.forEach([this](uint32_t idx, ecs::Entity& e) {
                if (m_ecs.isAlive(e)) m_ecs.destroyEntity(e);
                m_projectilePool.release(idx);
            });
            poolIdx = m_projectilePool.acquire();
            if (poolIdx == UINT32_MAX) return;
        }

        auto e = m_ecs.createEntity();
        m_projectilePool.get(poolIdx) = e;

        math::Vector3D pos = m_camera.getPosition() + m_camera.getForward() * 1.5f;
        auto& t = m_ecs.emplaceComponent<ecs::Transform3DComponent>(e, pos);
        float sz = 0.6f + (rand() % 50) * 0.008f; // Bigger: 0.6â€“1.0
        t.transform.scale = {sz, sz, sz}; t.dirty = true;

        renderer::Material mat;
        switch (rand() % 7) {
            case 0: mat = renderer::Material::gold(); break;
            case 1: mat = renderer::Material::copper(); break;
            case 2: mat = renderer::Material::chrome(); break;
            case 3: mat = renderer::Material::emerald(); break;
            case 4: mat = renderer::Material::ceramic(); break;
            case 5: mat = renderer::Material::ruby(); break;
            default: mat = renderer::Material::iron(); break;
        }
        // Emissive glow on projectiles
        mat.emissiveColor = mat.albedoColor;
        mat.emissiveIntensity = 1.5f;
        m_ecs.addComponent<ecs::MeshComponent>(e, {&m_cube, mat});

        auto& phys = m_ecs.emplaceComponent<ecs::Physics3DComponent>(e, 1.5f);
        phys.velocity = m_camera.getForward() * 20.0f + math::Vector3D(0, 3, 0);
        phys.restitution = 0.5f; phys.friction = 0.4f;

        // BOX collider for cubes (proper OBB SAT collision)
        m_ecs.addComponent<ecs::Collider3DComponent>(e, ecs::Collider3DComponent::box(
            math::Vector3D(sz * 0.5f, sz * 0.5f, sz * 0.5f)));
        m_ecs.addComponent<ecs::TagComponent>(e, ecs::TagComponent(ecs::TagComponent::PROJECTILE));
    }

    void Play3DState::emitCollisionEvents() {
        m_ecs.forEach<ecs::Transform3DComponent, ecs::Physics3DComponent, ecs::Collider3DComponent>(
            [this](ecs::Entity e, ecs::Transform3DComponent& t, ecs::Physics3DComponent& p,
                   ecs::Collider3DComponent& c) {
                if (p.isStatic || p.sleeping) return;
                float r = (c.shape == ecs::Collider3DComponent::SPHERE) ? c.radius
                    : std::max({c.halfExtents.x, c.halfExtents.y, c.halfExtents.z});
                if (t.transform.position.y - r < 0.05f && std::abs(p.velocity.y) > 2.0f) {
                    Collision3DEvent evt;
                    evt.entityA = e; evt.entityB = 0;
                    evt.contactPoint = t.transform.position;
                    evt.contactPoint.y = 0.01f;
                    evt.normal = {0, 1, 0};
                    evt.impulse = std::abs(p.velocity.y) * 0.5f;
                    m_eventBus.emit(evt);
                }
            }
        );
    }

    void Play3DState::renderInstancedField(const math::Matrix4x4& view, const math::Matrix4x4& proj) {
        // Submit 50 tiny decorative spheres via InstancedRenderer
        m_instancer.begin();
        for (int i = 0; i < 50; i++) {
            float angle = i * 0.1256637f;
            float r = 12.0f + std::sin(m_time * 0.2f + i * 0.4f) * 0.5f;
            float y = 0.15f + std::sin(m_time + i * 0.3f) * 0.1f;
            auto model = math::Matrix4x4::translation(
                std::cos(angle) * r, y, std::sin(angle) * r)
                * math::Matrix4x4::scale(0.12f, 0.12f, 0.12f);
            float hue = static_cast<float>(i) / 50.0f;
            m_instancer.addInstance(model, {hue, 0.5f + hue * 0.5f, 1.0f - hue * 0.5f});
        }
        m_instancer.resetStats();
    }

    // â”€â”€ 3D Scene Serialization â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
    void Play3DState::saveScene3D(const std::string& path) {
        core::JsonWriter w;
        w.beginObject();
        w.keyValue("scene", "3DState");
        w.beginArray("entities");

        auto& transforms = m_ecs.getStorage<ecs::Transform3DComponent>();
        for (uint32_t i = 0; i < transforms.size(); i++) {
            ecs::Entity e = transforms.getEntity(i);
            if (!m_ecs.isAlive(e)) continue;
            auto& t = transforms.getDense(i);

            w.beginObject();
            w.key("pos"); w.beginObject();
            w.keyValue("x", t.transform.position.x);
            w.keyValue("y", t.transform.position.y);
            w.keyValue("z", t.transform.position.z);
            w.endObject();
            w.key("scale"); w.beginObject();
            w.keyValue("x", t.transform.scale.x);
            w.keyValue("y", t.transform.scale.y);
            w.keyValue("z", t.transform.scale.z);
            w.endObject();
            if (m_ecs.hasComponent<ecs::Physics3DComponent>(e)) {
                auto& p = m_ecs.getComponent<ecs::Physics3DComponent>(e);
                w.key("vel"); w.beginObject();
                w.keyValue("x", p.velocity.x);
                w.keyValue("y", p.velocity.y);
                w.keyValue("z", p.velocity.z);
                w.endObject();
                w.keyValue("mass", p.mass);
            }
            w.endObject();
        }

        w.endArray();
        w.endObject();
        w.saveToFile(path);
        core::Logger::info("Play3DState", "Scene saved: " + path);
    }

    int Play3DState::loadScene3D(const std::string& path) {
        core::JsonReader r;
        if (!r.loadFromFile(path)) {
            core::Logger::warn("Play3DState", "No scene file: " + path);
            return 0;
        }

        // Read saved positions and apply to existing entities in order
        if (!r.expectChar('{')) return 0;
        int loaded = 0;

        // Collect existing entities
        auto& transforms = m_ecs.getStorage<ecs::Transform3DComponent>();
        std::vector<ecs::Entity> entities;
        for (uint32_t i = 0; i < transforms.size(); i++) {
            ecs::Entity e = transforms.getEntity(i);
            if (m_ecs.isAlive(e)) entities.push_back(e);
        }

        while (!r.peekChar('}')) {
            std::string key = r.readKey();
            if (key == "scene") { r.readString(); }
            else if (key == "entities") {
                if (!r.expectChar('[')) return 0;
                int idx = 0;
                while (!r.peekChar(']')) {
                    if (!r.expectChar('{')) break;
                    float px=0,py=0,pz=0, sx=1,sy=1,sz=1, vx=0,vy=0,vz=0;
                    while (!r.peekChar('}')) {
                        std::string ek = r.readKey();
                        if (ek == "pos") {
                            r.expectChar('{');
                            while (!r.peekChar('}')) {
                                std::string k2 = r.readKey();
                                if (k2=="x") px=r.readFloat(); else if (k2=="y") py=r.readFloat(); else if (k2=="z") pz=r.readFloat(); else r.skipValue();
                                r.skipComma();
                            } r.expectChar('}');
                        } else if (ek == "scale") {
                            r.expectChar('{');
                            while (!r.peekChar('}')) {
                                std::string k2 = r.readKey();
                                if (k2=="x") sx=r.readFloat(); else if (k2=="y") sy=r.readFloat(); else if (k2=="z") sz=r.readFloat(); else r.skipValue();
                                r.skipComma();
                            } r.expectChar('}');
                        } else if (ek == "vel") {
                            r.expectChar('{');
                            while (!r.peekChar('}')) {
                                std::string k2 = r.readKey();
                                if (k2=="x") vx=r.readFloat(); else if (k2=="y") vy=r.readFloat(); else if (k2=="z") vz=r.readFloat(); else r.skipValue();
                                r.skipComma();
                            } r.expectChar('}');
                        } else if (ek == "mass") { r.readFloat(); }
                        else r.skipValue();
                        r.skipComma();
                    }
                    r.expectChar('}');
                    r.skipComma();

                    // Apply to entity if exists
                    if (idx < static_cast<int>(entities.size())) {
                        auto& t = m_ecs.getComponent<ecs::Transform3DComponent>(entities[idx]);
                        t.transform.position = {px, py, pz};
                        t.transform.scale = {sx, sy, sz};
                        t.dirty = true;
                        if (m_ecs.hasComponent<ecs::Physics3DComponent>(entities[idx])) {
                            auto& p = m_ecs.getComponent<ecs::Physics3DComponent>(entities[idx]);
                            p.velocity = {vx, vy, vz};
                            p.wake();
                        }
                        loaded++;
                    }
                    idx++;
                }
                r.expectChar(']');
            } else { r.skipValue(); }
            r.skipComma();
        }
        core::Logger::info("Play3DState", "Scene loaded: " + std::to_string(loaded) + " entities from " + path);
        return loaded;
    }

    // â”€â”€ LOD sphere data â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

    // ── Find nearest grabbable entity in front of camera ───
    ecs::Entity Play3DState::findGrabbable() {
        math::Vector3D camPos = m_camera.getPosition();
        math::Vector3D camFwd = m_camera.getForward();
        float bestDist = 6.0f; // Max grab range
        ecs::Entity best = 0;

        m_ecs.forEach<ecs::Transform3DComponent, ecs::Physics3DComponent>(
            [&](ecs::Entity e, ecs::Transform3DComponent& t, ecs::Physics3DComponent& p) {
                if (p.isStatic) return;
                math::Vector3D toObj = t.transform.position - camPos;
                float dist = toObj.magnitude();
                if (dist > bestDist) return;
                // Check if in front of camera (dot > 0.5 = within ~60° cone)
                if (dist > 0.1f && toObj.normalized().dot(camFwd) > 0.5f) {
                    if (dist < bestDist) {
                bestDist = dist;
                        best = e;
                    }
                }
            }
        );
        return best;
    }

    // ── C3: Physics explosion ──────────────────────────────────
    // Applies outward impulse to all dynamic entities within radius
    void Play3DState::triggerExplosion(math::Vector3D center, float radius, float force) {
        m_ecs.forEach<ecs::Transform3DComponent, ecs::Physics3DComponent>(
            [&](ecs::Entity, ecs::Transform3DComponent& t, ecs::Physics3DComponent& p) {
                if (p.isStatic) return;
                math::Vector3D dir = t.transform.position - center;
                float dist = dir.magnitude();
                if (dist < 0.01f || dist > radius) return;
                // Inverse-square-ish falloff
                float strength = force * (1.0f - dist / radius) * (1.0f - dist / radius);
                math::Vector3D impulse = dir.normalized() * strength;
                impulse.y += strength * 0.5f; // Upward bias
                p.velocity = p.velocity + impulse;
                p.sleeping = false;
            }
        );
        // Particle burst at explosion center
        ParticleEmitter3D blast;
        blast.position = center;
        blast.color = {1.0f, 0.6f, 0.1f};
        blast.colorVariation = {0.2f, 0.3f, 0.1f};
        blast.speed = 8.0f; blast.speedVariation = 4.0f;
        blast.size = 0.18f; blast.lifetime = 0.8f;
        blast.emitRate = 0.0f; // Zero rate — burst mode
        blast.shape = EmitterShape::SPHERE;
        blast.gravity = {0, -5.0f, 0};
        // Fire one batch manually
        for (int i = 0; i < 60; i++)
            m_particles.update(0.016f, blast);

        core::Logger::info("Play3DState", "Explosion at ("
            + std::to_string(static_cast<int>(center.x)) + ","
            + std::to_string(static_cast<int>(center.y)) + ","
            + std::to_string(static_cast<int>(center.z)) + ") r=" + std::to_string(static_cast<int>(radius)));
    }
} // namespace game
} // namespace engine
