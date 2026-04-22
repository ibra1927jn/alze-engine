#include "PlayState.h"
#include <glad/gl.h>

namespace engine {
namespace game {

    void PlayState::onEnter() {
        // C1 fix: update shared dimension state when entering 2D
        if (m_world) {
            m_world->dimension = 0;
            m_world->visits2D++;
            if (m_world->visits2D > 1)
                m_world->pushMessage("Back in 2D! Score: " + std::to_string(m_world->score)
                    + "  |  Portal: " + std::to_string(static_cast<int>(m_world->portalEnergy * 100)) + "%");
        }

        // ── Register systems ───────────────────────────────────
        m_physics   = m_ecs.registerSystem<ecs::PhysicsSystem>(m_ecs);
        m_collision = m_ecs.registerSystem<ecs::CollisionSystem>(m_ecs);
        m_input     = m_ecs.registerSystem<ecs::InputSystem>(m_ecs, m_engine.getInput());
        m_render    = m_ecs.registerSystem<ecs::RenderSystem>(m_ecs, m_engine.getGraphics().getRenderer());

        m_physics->setGravity(Config::GRAVITY);
        if (m_jobs) m_physics->setJobSystem(m_jobs);
        m_input->moveSpeed   = Config::MOVE_FORCE;
        m_input->jumpImpulse = Config::JUMP_IMPULSE;

        // ── Connect EventBus → CollisionSystem ─────────────────
        m_collision->setEventBus(&m_eventBus);

        // ── Audio: subscribe to collision events (AudioEngine) ──────
        m_eventBus.subscribe<core::CollisionEvent>([this](const core::CollisionEvent& e) {
            if (e.impulse > 50.0f && m_hitSoundsThisFrame < 3) {
                m_hitSoundsThisFrame++;
                m_audio.playSound("hit", core::SoundGroup::SFX);
            }
        });

        // ── Preload game sounds via ProceduralAudio (cero archivos WAV) ──
        if (m_audio.isInitialized()) {
            using namespace engine::core;
            auto jumpBuf = std::make_shared<AudioBuffer>(ProceduralAudio::generateJump());
            auto landBuf = std::make_shared<AudioBuffer>(ProceduralAudio::generateLand());
            auto hitBuf  = std::make_shared<AudioBuffer>(ProceduralAudio::generateHit(0.5f));
            m_audio.loadSoundFromBuffer("jump", jumpBuf);
            m_audio.loadSoundFromBuffer("land", landBuf);
            m_audio.loadSoundFromBuffer("hit",  hitBuf);
        }

        // ── Camera ──────────────────────────────────────────────
        float w = static_cast<float>(m_engine.getWindow().getWidth());
        float h = static_cast<float>(m_engine.getWindow().getHeight());
        m_camera.setViewSize(w, h);
        m_camera.setSmoothSpeed(0.08f);

        m_render->setCamera(&m_camera);
        m_render->setParticlePool(&m_particles);
        core::DebugDraw::setCamera(&m_camera);

        // ── Auto-start ALL diagnostics ────────────────────────────
        // Logger → archivo
        core::Logger::setFile("engine.log");
        core::Logger::info("PlayState", "Session started");

        // FrameLogger → CSV automático
        {
            std::string csvPath = "frames_" + std::to_string(time(nullptr)) + ".csv";
            core::FrameLogger::start(csvPath);
            core::Logger::info("Diag", "FrameLogger auto-started: " + csvPath);
        }

        // InputRecorder → graba inputs automáticamente
        {
            std::string inpPath = "input_" + std::to_string(time(nullptr)) + ".inp";
            core::InputRecorder::startRecording(inpPath);
            core::Logger::info("Diag", "InputRecorder auto-started: " + inpPath);
        }
        core::Logger::flush();  // Ensure startup messages are written

        // ── Create world ───────────────────────────────────────
        m_player = createPlayer({w / 2.0f, 400.0f});

        createPlatform({w * 0.5f, h - 20}, w - 40, 20, math::Color(60, 70, 90));
        createPlatform({200, 550}, 160, 16, math::Color(70, 100, 130));
        createPlatform({550, 480}, 180, 16, math::Color(80, 110, 140));
        createPlatform({820, 520}, 140, 16, math::Color(90, 120, 150));
        createPlatform({350, 380}, 140, 16, math::Color(70, 90, 120));
        createPlatform({700, 350}, 160, 16, math::Color(60, 80, 110));
        createPlatform({150, 280}, 120, 16, math::Color(80, 100, 130));

        // Dynamic platforms (moving)
        m_dynPlats.push_back(createPlatform({400, 300}, 100, 16, math::Color(120, 80, 160)));
        m_dynPlats.push_back(createPlatform({650, 250}, 100, 16, math::Color(100, 60, 140)));

        // ── SpriteBatch2D HUD + TileMap background ──────────────
        m_spriteBatch2D.init();
        m_textRenderer2D.init();
        initTileBackground();
    }

    void PlayState::onExit() {
        // Stop diagnostics
        core::FrameLogger::stop();
        core::InputRecorder::stopRecording();
        core::InputRecorder::stopPlayback();
        core::Logger::info("PlayState", "Session ended");
        core::Logger::closeFile();

        core::DebugDraw::setCamera(nullptr);
    }

    // ── INPUT ──────────────────────────────────────────────────
    void PlayState::handleInput(float dt) {
        auto& input = m_engine.getInput();
        if (!m_ecs.isAlive(m_player)) return;

        auto& phys = m_ecs.getComponent<ecs::PhysicsComponent>(m_player);
        auto& tf   = m_ecs.getComponent<ecs::TransformComponent>(m_player);

        // Movement
        bool movingLeft  = input.isKeyDown(SDL_SCANCODE_A) || input.isKeyDown(SDL_SCANCODE_LEFT);
        bool movingRight = input.isKeyDown(SDL_SCANCODE_D) || input.isKeyDown(SDL_SCANCODE_RIGHT);

        if (movingLeft)  ecs::PhysicsOps::applyForce(phys, {-Config::MOVE_FORCE, 0});
        if (movingRight) ecs::PhysicsOps::applyForce(phys, { Config::MOVE_FORCE, 0});
        phys.velocity.x = math::MathUtils::clamp(phys.velocity.x,
            -Config::MAX_VELOCITY_X, Config::MAX_VELOCITY_X);

        // Jump buffer
        m_jumpBuffer -= dt;
        if (input.isKeyPressed(SDL_SCANCODE_SPACE) || input.isKeyPressed(SDL_SCANCODE_W) ||
            input.isKeyPressed(SDL_SCANCODE_UP))
            m_jumpBuffer = Config::JUMP_BUFFER;

        bool canJump = m_onGround || m_coyote < Config::COYOTE_TIME;
        if (m_jumpBuffer > 0 && canJump) {
            phys.velocity.y = -Config::JUMP_IMPULSE;
            ecs::PhysicsOps::wake(phys);
            m_jumpBuffer = 0;
            m_coyote = Config::COYOTE_TIME;
            m_jumpHeld = true;
            m_audio.playSound("jump", core::SoundGroup::SFX);

            // C1: Gain portal energy + score on every jump
            if (m_world) {
                m_world->addEnergy(0.03f); // 34 jumps to full
                m_world->score++;
            }

            math::Vector2D feet = tf.transform.position +
                math::Vector2D(0, Config::PLAYER_H * 0.5f);
            m_particles.spawnBurst(feet, 8, 120, math::Color(120, 255, 180));
        }

        // Variable jump height
        if (!input.isKeyDown(SDL_SCANCODE_SPACE) && !input.isKeyDown(SDL_SCANCODE_W) &&
            !input.isKeyDown(SDL_SCANCODE_UP)) {
            if (m_jumpHeld && phys.velocity.y < 0) {
                phys.velocity.y *= 0.5f;
            }
            m_jumpHeld = false;
        }
        phys.velocity.y = math::MathUtils::clamp(phys.velocity.y,
            -Config::MAX_VELOCITY_Y, Config::MAX_VELOCITY_Y);

        // Drag
        phys.drag = m_onGround ? Config::GROUND_DRAG : Config::AIR_DRAG;

        // Gravity toggle
        if (input.isKeyPressed(SDL_SCANCODE_G)) {
            m_gravityOn = !m_gravityOn;
            m_physics->setGravity(m_gravityOn ? Config::GRAVITY : 0.0f);
        }

        // Shoot particles
        m_shootCooldown -= dt;
        if ((input.isKeyDown(SDL_SCANCODE_F) || input.isKeyDown(SDL_SCANCODE_J)) && m_shootCooldown <= 0) {
            m_shootCooldown = 0.05f;
            math::Vector2D dir = {movingLeft ? -1.0f : 1.0f, 0.0f};
            m_particles.spawnBurst(tf.transform.position + dir * 15, 3, 250, math::Color(255, 180, 80));
        }

        // Pause
        if (input.isKeyPressed(SDL_SCANCODE_P) || input.isKeyPressed(SDL_SCANCODE_ESCAPE)) {
            if (m_states) m_states->push(std::make_unique<PauseState>(m_engine, *m_states));
        }

        // Tab: Fade to 3D - save player position to shared state
        if (input.isKeyPressed(SDL_SCANCODE_TAB) && !m_fadingTo3D) {
            m_fadingTo3D = true;
            m_fadeAlpha = 0.0f;
            // Save player position for cross-dimension continuity
            if (m_world && m_ecs.isAlive(m_player)) {
                auto& tf2 = m_ecs.getComponent<ecs::TransformComponent>(m_player);
                m_world->lastPlayer2DX = tf2.transform.position.x;
                m_world->lastPlayer2DY = tf2.transform.position.y;
                m_world->dimension = 1;
                m_world->visits3D++;
                m_world->pushMessage("DIMENSION 3D  |  Press TAB to return to 2D");
            }
        }

        // Debug toggle (F3)
        if (input.isKeyPressed(SDL_SCANCODE_F3)) {
            core::DebugDraw::toggle();
        }

        // ── Diagnostics hotkeys ────────────────────────────────
        // F5: Toggle FrameLogger (CSV)
        if (input.isKeyPressed(SDL_SCANCODE_F5)) {
            if (core::FrameLogger::isActive()) {
                core::FrameLogger::stop();
                core::Logger::info("Diag", "FrameLogger stopped: " + std::to_string(core::FrameLogger::getFrameCount()) + " frames");
            } else {
                std::string path = "frames_" + std::to_string(time(nullptr)) + ".csv";
                core::FrameLogger::start(path);
                core::Logger::info("Diag", "FrameLogger started: " + path);
            }
        }

        // F6: Toggle InputRecorder
        if (input.isKeyPressed(SDL_SCANCODE_F6)) {
            if (core::InputRecorder::isRecording()) {
                core::InputRecorder::stopRecording();
                core::Logger::info("Diag", "Input recording stopped: " + std::to_string(core::InputRecorder::getRecordedFrames()) + " frames");
            } else {
                std::string path = "input_" + std::to_string(time(nullptr)) + ".inp";
                core::InputRecorder::startRecording(path);
                core::Logger::info("Diag", "Input recording started: " + path);
            }
        }

        // F7: Start playback of last recorded input
        if (input.isKeyPressed(SDL_SCANCODE_F7)) {
            if (core::InputRecorder::isPlaying()) {
                core::InputRecorder::stopPlayback();
                core::Logger::info("Diag", "Input playback stopped");
            } else {
                std::string path = core::InputRecorder::getRecordPath();
                if (!path.empty() && core::InputRecorder::startPlayback(path)) {
                    core::Logger::info("Diag", "Input playback started: " + path);
                }
            }
        }

        // ── Record current frame inputs ────────────────────────
        if (core::InputRecorder::isRecording()) {
            core::InputRecorder::InputFrame iframe;
            iframe.frameNumber = m_frameNum;
            iframe.mouseX = input.getMousePosition().x;
            iframe.mouseY = input.getMousePosition().y;
            iframe.scrollDelta = input.getScrollDelta();
            // Record key states
            const uint8_t* keyState = SDL_GetKeyboardState(nullptr);
            for (int i = 0; i < 256; i++) {
                if (keyState[i]) iframe.setKey(i, true);
            }
            iframe.mouseButtons = SDL_GetMouseState(nullptr, nullptr);
            core::InputRecorder::recordFrame(iframe);
        }

        // Camera zoom (scroll wheel / trackpad)
        int scroll = input.getScrollDelta();
        if (scroll != 0) {
            float zoom = m_camera.getZoom();
            zoom *= (scroll > 0) ? 1.1f : 0.9f;
            m_camera.setZoom(zoom);
        }

        // Spawn dynamic box (mouse click, with cooldown)
        m_spawnCooldown -= dt;
        if (input.isMouseButtonDown(SDL_BUTTON_LEFT) && m_spawnCooldown <= 0 && m_dynPlats.size() < 200) {
            m_spawnCooldown = 0.15f;  // Max ~7 boxes/sec
            math::Vector2D mouseScreen = input.getMousePosition();
            math::Vector2D mouseWorld = m_camera.screenToWorld(mouseScreen);
            auto e = m_ecs.createEntity();
            m_ecs.addComponent<ecs::TransformComponent>(e, ecs::TransformComponent(mouseWorld));
            ecs::PhysicsComponent p(2.0f);
            p.drag = 0.5f;
            p.restitution = 0.4f;
            p.previousPosition = mouseWorld;
            m_ecs.addComponent<ecs::PhysicsComponent>(e, p);
            float sz = 12.0f + static_cast<float>(rand() % 20);
            m_ecs.addComponent<ecs::ColliderComponent>(e, ecs::ColliderComponent(math::Vector2D(sz, sz)));
            uint8_t r = static_cast<uint8_t>(80 + rand() % 120);
            uint8_t g = static_cast<uint8_t>(80 + rand() % 120);
            uint8_t b = static_cast<uint8_t>(80 + rand() % 120);
            m_ecs.addComponent<ecs::SpriteComponent>(e, ecs::SpriteComponent(math::Color(r, g, b), sz, sz));
            m_particles.spawnBurst(mouseWorld, 5, 100, math::Color(r, g, b));
            m_dynPlats.push_back(e);
        }
    }

    // ── UPDATE ─────────────────────────────────────────────────
    void PlayState::update(float dt) {
        m_hitSoundsThisFrame = 0;
        if (!m_ecs.isAlive(m_player)) return;

        // C1 fix: tick cross-dim messages in update with real dt
        if (m_world) m_world->tickMessages(dt);

        // Systems
        core::Profiler::begin("Physics");
        m_physics->update(dt);
        core::Profiler::end("Physics");

        core::Profiler::begin("Collision");
        m_collision->update(dt);
        core::Profiler::end("Collision");

        auto& phys = m_ecs.getComponent<ecs::PhysicsComponent>(m_player);
        auto& tf   = m_ecs.getComponent<ecs::TransformComponent>(m_player);

        // Ground detection via Raycast
        bool wasOnGround = m_onGround;
        m_onGround = false;

        math::Vector2D feetPos = tf.transform.position + math::Vector2D(0, Config::PLAYER_H * 0.5f);
        math::Vector2D rayDir = {0, 1};
        float rayLen = 4.0f;

        physics::RayHit hitResult;
        bool hitGround = physics::Raycast::castFirst(m_ecs, feetPos, rayDir, rayLen, hitResult, m_player);
        if (hitGround && hitResult.normal.y < -0.5f) {
            m_onGround = true;
        }

        // Coyote time
        if (m_onGround) {
            m_coyote = 0.0f;
        } else {
            if (wasOnGround) m_coyote = 0.0f;
            m_coyote += dt;
        }

        // Landing particles + sound
        if (m_onGround && !wasOnGround) {
            float impactSpeed = math::MathUtils::abs(phys.velocity.y);
            if (impactSpeed > 100.0f) {
                int count = static_cast<int>(math::MathUtils::clamp(impactSpeed / 50.0f, 2, 15));
                math::Vector2D feet = tf.transform.position +
                    math::Vector2D(0, Config::PLAYER_H * 0.5f);
                m_particles.spawnBurst(feet, count, impactSpeed * 0.3f,
                                       math::Color(180, 200, 255));
            }
            m_audio.playSound("land", core::SoundGroup::SFX);
        }

        // Particles
        float grav = m_gravityOn ? Config::GRAVITY * 0.5f : 0.0f;
        m_particles.update(dt, grav);

        // Camera follow
        m_camera.setTarget(tf.transform.position);
        m_camera.update(dt);

        // Fade to 3D transition
        if (m_fadingTo3D) {
            m_fadeAlpha += dt * 2.5f; // 0.4 seconds to fully black
            if (m_fadeAlpha >= 1.0f) {
                m_fadeAlpha = 1.0f;
                m_fadingTo3D = false;
                if (m_states) {
                    auto state3d = std::make_unique<Play3DState>(m_engine, m_states, m_world);
                    m_states->push(std::move(state3d));
                }
            }
        }

        m_frameNum++;

        // ── FrameLogger ────────────────────────────────────────
        if (core::FrameLogger::isActive()) {
            auto pm = core::Profiler::getMetric("Physics");
            auto cm = core::Profiler::getMetric("Collision");
            auto& fm = core::Profiler::getFrameMetric();

            core::FrameLogger::FrameData fd;
            fd.frame = m_frameNum;
            fd.dt = dt;
            fd.fps = m_engine.getFPS();
            fd.entityCount = m_ecs.getActiveEntityCount();
            fd.broadTests = m_collision->getBroadPhaseTests();
            fd.narrowTests = m_collision->getNarrowPhaseTests();
            fd.collisionsResolved = m_collision->getCollisionsResolved();
            fd.particleCount = m_particles.activeCount();
            fd.physicsMs = pm.lastMs;
            fd.collisionMs = cm.lastMs;
            fd.frameMs = fm.lastMs;
            fd.playerX = tf.transform.position.x;
            fd.playerY = tf.transform.position.y;
            fd.playerVx = phys.velocity.x;
            fd.playerVy = phys.velocity.y;
            fd.playerOnGround = m_onGround;
            fd.cameraZoom = m_camera.getZoom();
            core::FrameLogger::logFrame(fd);
        }
    }

    // ── RENDER (delegated to RenderSystem) ─────────────────────
    void PlayState::render(float alpha) {
        // Update HUD/Debug state for RenderSystem
        ecs::RenderSystem::HUDState hud;
        hud.fps = m_engine.getFPS();
        hud.particleCount = m_particles.activeCount();
        hud.maxParticles = Config::MAX_PARTICLES;
        hud.gravityOn = m_gravityOn;
        hud.onGround = m_onGround;
        hud.entityCount = m_ecs.getActiveEntityCount();
        if (m_ecs.isAlive(m_player))
            hud.playerSpeed = m_ecs.getComponent<ecs::PhysicsComponent>(m_player).velocity.magnitude();
        m_render->setHUDState(hud);

        ecs::RenderSystem::DebugState dbg;
        dbg.player = m_player;
        dbg.onGround = m_onGround;
        dbg.coyoteTime = m_coyote;
        dbg.coyoteMax = Config::COYOTE_TIME;
        dbg.broadTests = m_collision->getBroadPhaseTests();
        dbg.narrowTests = m_collision->getNarrowPhaseTests();
        dbg.colResolved = m_collision->getCollisionsResolved();
        dbg.entityCount = m_ecs.getActiveEntityCount();
        dbg.particleCount = m_particles.activeCount();
        m_render->setDebugState(dbg);

        m_render->render(alpha);

        // ── GL overlay: HUD text ────────────────────────────────
        {
            float sw = static_cast<float>(m_engine.getWindow().getWidth());
            float sh = static_cast<float>(m_engine.getWindow().getHeight());
            auto ortho = renderer::SpriteBatch2D::ortho2D(sw, sh);

            glDisable(GL_DEPTH_TEST);
            m_spriteBatch2D.begin(ortho, renderer::BlendMode::ALPHA);

            renderer::SpriteColor hc(0.3f, 1.0f, 0.6f, 0.8f);
            renderer::SpriteColor dc(0.6f, 0.7f, 0.6f, 0.5f);
            renderer::SpriteColor pc(0.3f, 0.7f, 1.0f, 0.9f); // Portal color

            std::string stats = "2D Mode | FPS: " + std::to_string(static_cast<int>(m_engine.getFPS()))
                + "  Entities: " + std::to_string(m_ecs.getActiveEntityCount());
            m_textRenderer2D.draw(m_spriteBatch2D, stats, sw - 320, sh - 18, 1.0f, hc);
            m_textRenderer2D.draw(m_spriteBatch2D, "WASD Move  SPACE Jump  TAB 3D  G Gravity", 10, sh - 18, 1.0f, dc);

            // C1: Portal energy bar + score + cross-dim message
            if (m_world) {
                // Score
                std::string scoreStr = "Score: " + std::to_string(m_world->score)
                    + "  Visits2D:" + std::to_string(m_world->visits2D)
                    + "  Visits3D:" + std::to_string(m_world->visits3D);
                m_textRenderer2D.draw(m_spriteBatch2D, scoreStr, 10, 10, 1.0f, hc);

                // Energy bar label
                float energy = m_world->portalEnergy;
                std::string energyLabel = "Portal Energy: " + std::to_string(static_cast<int>(energy * 100)) + "%";
                if (m_world->portalReady()) energyLabel += "  [PORTAL READY! Press TAB]"; 
                m_textRenderer2D.draw(m_spriteBatch2D, energyLabel, 10, 28, 1.0f, pc);

                // Cross-dim message (if recent)
                if (m_world->msgTimer > 0.0f && !m_world->latestMessage().empty()) {
                    float msgAlpha = std::min(1.0f, m_world->msgTimer);
                    renderer::SpriteColor mc(1.0f, 0.9f, 0.2f, msgAlpha);
                    m_textRenderer2D.draw(m_spriteBatch2D, m_world->latestMessage(),
                        sw * 0.5f - 160, sh * 0.5f - 40, 1.2f, mc);
                }
            }

            m_spriteBatch2D.end();
            glEnable(GL_DEPTH_TEST);
        }
    }

    // ── Entity factories ───────────────────────────────────────
    ecs::Entity PlayState::createPlayer(math::Vector2D pos) {
        ecs::Entity e = m_ecs.createEntity();
        m_ecs.addComponent<ecs::TransformComponent>(e, ecs::TransformComponent(pos));
        ecs::PhysicsComponent p(Config::PLAYER_MASS);
        p.drag = Config::AIR_DRAG;
        p.restitution = Config::RESTITUTION;
        p.previousPosition = pos;
        m_ecs.addComponent<ecs::PhysicsComponent>(e, p);
        m_ecs.addComponent<ecs::ColliderComponent>(e,
            ecs::ColliderComponent(math::Vector2D(Config::PLAYER_W, Config::PLAYER_H)));
        m_ecs.addComponent<ecs::SpriteComponent>(e,
            ecs::SpriteComponent(math::Color::green(), Config::PLAYER_W, Config::PLAYER_H));
        m_ecs.setTag(e, ecs::TAG_PLAYER);
        return e;
    }

    ecs::Entity PlayState::createPlatform(math::Vector2D pos, float w, float h, math::Color color) {
        ecs::Entity e = m_ecs.createEntity();
        m_ecs.addComponent<ecs::TransformComponent>(e, ecs::TransformComponent(pos));
        m_ecs.addComponent<ecs::ColliderComponent>(e, ecs::ColliderComponent(math::Vector2D(w, h), true));
        m_ecs.addComponent<ecs::SpriteComponent>(e, ecs::SpriteComponent(color, w, h));
        m_ecs.setTag(e, ecs::TAG_PLATFORM);
        return e;
    }

    void PlayState::initTileBackground() {
        // Generate a small procedural tileset (4x1 = 4 tile types)
        constexpr int TS = 16; // tile pixel size
        constexpr int COLS = 4, ROWS = 1;
        std::vector<uint8_t> pixels(TS * COLS * TS * ROWS * 4, 0);
        auto setPixel = [&](int tx, int ty, int px, int py, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
            int x = tx * TS + px;
            int y = ty * TS + py;
            int idx = (y * TS * COLS + x) * 4;
            pixels[idx] = r; pixels[idx+1] = g; pixels[idx+2] = b; pixels[idx+3] = a;
        };

        // Tile 0 (id=1): Dark blue fill
        for (int py = 0; py < TS; py++)
            for (int px = 0; px < TS; px++)
                setPixel(0, 0, px, py, 20, 25, 45, 40);

        // Tile 1 (id=2): Slightly lighter
        for (int py = 0; py < TS; py++)
            for (int px = 0; px < TS; px++)
                setPixel(1, 0, px, py, 25, 30, 55, 35);

        // Tile 2 (id=3): Accent dot
        for (int py = 0; py < TS; py++)
            for (int px = 0; px < TS; px++) {
                int dx = px - TS/2, dy = py - TS/2;
                bool dot = (dx*dx + dy*dy) < 9;
                setPixel(2, 0, px, py, dot ? 60 : 18, dot ? 50 : 22, dot ? 80 : 40, dot ? 50 : 30);
            }

        // Tile 3 (id=4): Grid line
        for (int py = 0; py < TS; py++)
            for (int px = 0; px < TS; px++) {
                bool edge = (px == 0 || py == 0);
                setPixel(3, 0, px, py, edge ? 40 : 15, edge ? 40 : 20, edge ? 60 : 35, edge ? 50 : 25);
            }

        // Upload as GL texture via ProceduralTexture's internal upload (use gridPattern hack)
        // Actually, create a Texture2D directly
        // We'll use the glGenTextures path directly
        GLuint texId;
        glGenTextures(1, &texId);
        glBindTexture(GL_TEXTURE_2D, texId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, TS * COLS, TS * ROWS, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);
        m_tileTexId = texId;

        // Create tilemap with 2 layers
        int bgLayer = m_tileMap.addLayer(40, 30, 32); // 40x30 grid, 32px tiles
        m_tileMap.setParallax(bgLayer, 0.3f); // slow parallax

        // Fill with a pattern
        auto& layer = m_tileMap.getLayer(bgLayer);
        for (int y = 0; y < 30; y++)
            for (int x = 0; x < 40; x++) {
                int tile = ((x + y) % 3 == 0) ? 3 : ((x * y) % 7 == 0) ? 2 : 1;
                layer.setTile(x, y, tile);
            }

        core::Logger::info("PlayState", "TileMap background initialized (40x30, parallax 0.3)");
    }

} // namespace game
} // namespace engine
