/// ALZE — Demo 3D v13: AAA Visual Showcase
///
/// Features:
///   - FPS camera (WASD + mouse look) — CLICK TO CAPTURE MOUSE
///   - Rich scene: floor, columns, pedestals, walls, emissive orbs
///   - Cinematic post-processing: bloom, FXAA, SSAO, color grading
///   - Volumetric God Rays + atmospheric fog
///   - Contact-hardening soft shadows (Vogel disk)
///   - Press F to spawn a new ball at camera position
///   - Press R to reset all balls

#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include <glad/gl.h>
#include <iostream>
#include <cmath>
#include <vector>

#include "scene/SceneGraph.h"
#include "scene/FPSController.h"
#include "renderer/ForwardRenderer.h"
#include "renderer/PostProcess.h"
#include "renderer/Skybox.h"
#include "renderer/EnvironmentMap.h"
#include "renderer/MeshPrimitives.h"
#include "renderer/Material.h"
#include "renderer/Texture2D.h"
#include "renderer/ProceduralTextures.h"
#include "renderer/SSAO.h"
#include "physics/PhysicsWorld3D.h"
#include "physics/Collider3D.h"
#include "core/InputManager.h"

using namespace engine;

renderer::Material makeBallMat(float r, float g, float b, float metal, float rough) {
    renderer::Material m;
    m.albedoColor = math::Vector3D(r, g, b);
    m.metallic = metal;
    m.roughness = rough;
    return m;
}

int main() {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

    int W = 1280, H = 720;
    SDL_Window* window = SDL_CreateWindow(
        "ALZE Engine v3",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H,
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) return 1;
    SDL_GLContext glCtx = SDL_GL_CreateContext(window);
    if (!glCtx) return 1;
    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) return 1;

    std::cout << "GPU: " << glGetString(GL_RENDERER) << std::endl;
    SDL_GL_SetSwapInterval(1);
    glEnable(GL_MULTISAMPLE);

    // ── Sky (dramatic sunset) ──────────────────────────────────
    renderer::SkyParams sky;
    sky.topColor     = math::Vector3D(0.02f, 0.05f, 0.20f);
    sky.horizonColor = math::Vector3D(0.80f, 0.35f, 0.10f);
    sky.bottomColor  = math::Vector3D(0.08f, 0.06f, 0.04f);
    sky.sunDir       = math::Vector3D(-0.2f, 0.3f, 0.6f);
    sky.sunColor     = math::Vector3D(5.0f, 3.5f, 1.5f);
    sky.sunSize      = 1200.0f;

    // ── IBL ────────────────────────────────────────────────────
    renderer::EnvironmentMap envMap;
    envMap.generate(sky, 256, 32, 128);

    // ── Systems ────────────────────────────────────────────────
    renderer::ForwardRenderer fwd;
    fwd.init(W, H);
    fwd.setEnvironmentMap(&envMap);

    renderer::RenderSettings rs;
    rs.shadows = true;
    rs.shadowDistance = 40.0f;
    rs.frustumCulling = true;
    rs.sortObjects = true;
    rs.iblIntensity = 1.2f;
    rs.volumetricIntensity = 1.5f;
    rs.fogDensity = 0.008f;
    fwd.setSettings(rs);

    renderer::PostProcess postfx;
    postfx.init(W, H);

    renderer::SSAO ssao;
    ssao.init(W, H);
    ssao.getSettings().radius = 0.20f;
    ssao.getSettings().power = 1.5f;

    renderer::Skybox skybox;
    skybox.init();

    core::InputManager input;

    // ── FPS Camera ─────────────────────────────────────────────
    scene::FPSController fps;
    fps.setPosition(math::Vector3D(0, 3.5f, 12));
    fps.setYaw(180.0f);
    fps.setPitch(-8.0f);
    fps.moveSpeed = 6.0f;
    fps.sensitivity = 0.10f;
    fps.fov = 60.0f;

    // ── Meshes ─────────────────────────────────────────────────
    renderer::Mesh3D sphereHQ, cubeMesh, planeMesh;
    renderer::MeshPrimitives::createSphere(sphereHQ, 48, 24);
    renderer::MeshPrimitives::createCube(cubeMesh);
    renderer::MeshPrimitives::createPlane(planeMesh, 20);

    // ── Textures ───────────────────────────────────────────────
    renderer::Texture2D stoneAlbedo, stoneNormal, scratchTex;
    stoneAlbedo.wrapHandle(renderer::ProceduralTextures::createStoneFloorAlbedo(512), 512, 512);
    stoneNormal.wrapHandle(renderer::ProceduralTextures::createStoneFloorNormal(512), 512, 512);
    scratchTex.wrapHandle(renderer::ProceduralTextures::createScratchesNormal(512), 512, 512);

    // ── Scene ──────────────────────────────────────────────────
    scene::SceneGraph sceneGraph;
    const float FLOOR_Y = 0.0f;

    // ── Terrain Heightfield ────────────────────────────────────
    const int TERRAIN_SIZE = 64; // 64x64 nodes
    const float TERRAIN_SPACING = 1.0f; // 1 meter spacing
    std::shared_ptr<physics::HeightfieldCollider> hf = std::make_shared<physics::HeightfieldCollider>();
    hf->numRows = TERRAIN_SIZE;
    hf->numCols = TERRAIN_SIZE;
    hf->rowSpacing = TERRAIN_SPACING;
    hf->colSpacing = TERRAIN_SPACING;
    hf->heights.resize(TERRAIN_SIZE * TERRAIN_SIZE, 0.0f);
    
    // Generate terrain heights (procedural hills)
    std::vector<float> terrainHeights(TERRAIN_SIZE * TERRAIN_SIZE, 0.0f);
    for (int r = 0; r < TERRAIN_SIZE; r++) {
        for (int c = 0; c < TERRAIN_SIZE; c++) {
            float x = c * TERRAIN_SPACING;
            float z = r * TERRAIN_SPACING;
            float y = std::sin(x * 0.2f) * 2.0f + std::cos(z * 0.3f) * 1.5f + std::sin((x+z) * 0.1f) * 4.0f;
            hf->heights[r * TERRAIN_SIZE + c] = y;
            terrainHeights[r * TERRAIN_SIZE + c] = y;
        }
    }
    
    renderer::Mesh3D terrainMesh;
    renderer::MeshPrimitives::createTerrainMesh(terrainMesh, terrainHeights, TERRAIN_SIZE, TERRAIN_SIZE, TERRAIN_SPACING, TERRAIN_SPACING);
    
    auto& terrainNode = sceneGraph.createNode("Terrain");
    terrainNode.setMesh(terrainMesh);
    terrainNode.material.albedoColor = math::Vector3D(0.12f, 0.35f, 0.15f); // Grass base
    terrainNode.material.roughness = 0.9f;
    terrainNode.material.metallic = 0.0f;
    terrainNode.material.normalMap = &stoneNormal;
    terrainNode.transform.position = math::Vector3D(-32.0f, FLOOR_Y, -32.0f); // Center terrain

    // ── Floor ──────────────────────────────────────────────────
    auto& floor = sceneGraph.createNode("Floor");
    floor.setMesh(planeMesh);
    floor.material.albedoColor = math::Vector3D(0.10f, 0.09f, 0.08f);
    floor.material.albedoTexture = &stoneAlbedo;
    floor.material.normalMap = &stoneNormal;
    floor.material.roughness = 0.55f;
    floor.material.metallic = 0.05f;
    floor.transform.position = math::Vector3D(0, FLOOR_Y - 5.0f, 0); // Put original floor below terrain
    floor.transform.scale = math::Vector3D(20, 1, 20);

    // ── Columns (6 tall marble pillars) ────────────────────────
    struct ColDef { float x, z; };
    ColDef columns[] = {{-6, -6}, {6, -6}, {-6, 6}, {6, 6}, {-3, -8}, {3, -8}};
    for (int i = 0; i < 6; i++) {
        auto& col = sceneGraph.createNode("Column");
        col.setMesh(cubeMesh);
        col.material.albedoColor = math::Vector3D(0.90f, 0.87f, 0.82f);
        col.material.roughness = 0.25f;
        col.material.metallic = 0.02f;
        col.transform.position = math::Vector3D(columns[i].x, FLOOR_Y + 3.0f, columns[i].z);
        col.transform.scale = math::Vector3D(0.5f, 6.0f, 0.5f);

        auto& cap = sceneGraph.createNode("Cap");
        cap.setMesh(sphereHQ);
        cap.material = renderer::Material::chrome();
        cap.material.albedoTexture = &scratchTex;
        cap.transform.position = math::Vector3D(columns[i].x, FLOOR_Y + 6.2f, columns[i].z);
        cap.transform.scale = math::Vector3D(0.45f, 0.45f, 0.45f);
    }

    // ── Pedestals ──────────────────────────────────────────────
    struct PedDef { float x, z; renderer::Material mat; };
    PedDef pedestals[] = {
        { 0, -5, renderer::Material::gold() },
        {-3,  0, renderer::Material::copper() },
        { 3,  0, renderer::Material::silver() },
        { 0,  5, renderer::Material::emerald() },
    };
    for (auto& p : pedestals) {
        auto& base = sceneGraph.createNode("PedBase");
        base.setMesh(cubeMesh);
        base.material.albedoColor = math::Vector3D(0.08f, 0.06f, 0.05f);
        base.material.roughness = 0.15f;
        base.material.metallic = 0.9f;
        base.material.normalMap = &scratchTex;
        base.transform.position = math::Vector3D(p.x, FLOOR_Y + 0.5f, p.z);
        base.transform.scale = math::Vector3D(1.0f, 1.0f, 1.0f);

        auto& disp = sceneGraph.createNode("PedSphere");
        disp.setMesh(sphereHQ);
        disp.material = p.mat;
        disp.transform.position = math::Vector3D(p.x, FLOOR_Y + 1.5f, p.z);
        disp.transform.scale = math::Vector3D(0.55f, 0.55f, 0.55f);
    }

    // ── Emissive Orbs ──────────────────────────────────────────
    struct OrbDef { float x, y, z; float r, g, b; float intensity; };
    OrbDef orbs[] = {
        {-4, 1.5f,  3, 1.0f, 0.3f, 0.1f, 3.0f},
        { 4, 1.5f,  3, 0.1f, 0.4f, 1.0f, 3.0f},
        { 0, 2.0f, -3, 0.8f, 0.1f, 0.9f, 2.5f},
        {-5, 1.0f, -5, 0.1f, 0.9f, 0.4f, 2.0f},
    };
    for (auto& orb : orbs) {
        auto& orbNode = sceneGraph.createNode("Orb");
        orbNode.setMesh(sphereHQ);
        orbNode.material.albedoColor = math::Vector3D(0.02f, 0.02f, 0.02f);
        orbNode.material.roughness = 0.1f;
        orbNode.material.metallic = 0.0f;
        orbNode.material.emissiveColor = math::Vector3D(orb.r, orb.g, orb.b);
        orbNode.material.emissiveIntensity = orb.intensity;
        orbNode.transform.position = math::Vector3D(orb.x, orb.y, orb.z);
        orbNode.transform.scale = math::Vector3D(0.2f, 0.2f, 0.2f);
    }

    // ── Walls ──────────────────────────────────────────────────
    auto& wall = sceneGraph.createNode("BackWall");
    wall.setMesh(cubeMesh);
    wall.material.albedoColor = math::Vector3D(0.06f, 0.05f, 0.04f);
    wall.material.roughness = 0.7f;
    wall.material.normalMap = &stoneNormal;
    wall.transform.position = math::Vector3D(0, FLOOR_Y + 3.5f, -9.0f);
    wall.transform.scale = math::Vector3D(20, 7, 0.3f);

    for (float side : {-10.0f, 10.0f}) {
        auto& sw = sceneGraph.createNode("SideWall");
        sw.setMesh(cubeMesh);
        sw.material.albedoColor = math::Vector3D(0.07f, 0.06f, 0.05f);
        sw.material.roughness = 0.75f;
        sw.material.normalMap = &stoneNormal;
        sw.transform.position = math::Vector3D(side, FLOOR_Y + 3.5f, 0);
        sw.transform.scale = math::Vector3D(0.3f, 7, 20);
    }

    // ── Physics ────────────────────────────────────────────────
    physics::PhysicsWorld3D physWorld;
    physWorld.gravity = math::Vector3D(0, -9.81f, 0);
    physWorld.subSteps = 4;
    physWorld.setSolverIterations(8);

    physWorld.addStaticBox(math::Vector3D(0, FLOOR_Y - 5.5f, 0), math::Vector3D(20, 0.5f, 20), 0.6f);
    physWorld.addStaticBox(math::Vector3D(-10.0f, 3, 0), math::Vector3D(0.5f, 5, 20), 0.3f);
    physWorld.addStaticBox(math::Vector3D(10.0f, 3, 0), math::Vector3D(0.5f, 5, 20), 0.3f);
    physWorld.addStaticBox(math::Vector3D(0, 3, -9.0f), math::Vector3D(20, 5, 0.5f), 0.3f);
    physWorld.addStaticBox(math::Vector3D(0, 3, 9.5f), math::Vector3D(20, 5, 0.5f), 0.3f);
    for (auto& p : pedestals)
        physWorld.addStaticBox(math::Vector3D(p.x, FLOOR_Y + 0.5f, p.z), math::Vector3D(0.5f, 0.5f, 0.5f), 0.5f);
    for (int i = 0; i < 6; i++)
        physWorld.addStaticBox(math::Vector3D(columns[i].x, FLOOR_Y + 3.0f, columns[i].z), math::Vector3D(0.25f, 3.0f, 0.25f), 0.4f);

    // Add Terrain to physics
    physWorld.addStaticHeightfield(math::Vector3D(-32.0f, FLOOR_Y, -32.0f), hf, 0.5f);

    // ── Massive Spheres ─────────────────────────────────────────
    std::vector<scene::SceneNode*> ballNodes;
    std::vector<int> ballBodyIds;
    
    // Drop 100 golden / silver spheres across the terrain to showcase DBVH and Heightfield CCD
    for(int j = 0; j < 100; ++j) {
        float bx = -15.0f + (j % 10) * 3.0f;
        float bz = -15.0f + (j / 10) * 3.0f;
        float by = 20.0f + (j % 5) * 2.0f;
        
        auto& ball = sceneGraph.createNode("DropBall" + std::to_string(j));
        ball.setMesh(sphereHQ);
        ball.material = (j % 2 == 0) ? renderer::Material::gold() : renderer::Material::silver();
        ball.transform.scale = math::Vector3D(0.5f, 0.5f, 0.5f);
        ballNodes.push_back(&ball);

        int physId = physWorld.addDynamicSphere(math::Vector3D(bx, by, bz), 0.5f, 5.0f, 0.3f, 0.8f);
        physWorld.getBody(physId).isBullet = true; // Activar SphereVsSphere TOI para mostrar máxima calidad
        ballBodyIds.push_back(physId);
    }

    renderer::Material ballMats[] = {
        makeBallMat(1.0f, 0.15f, 0.1f, 0.95f, 0.15f),
        makeBallMat(0.1f, 0.4f, 1.0f, 0.95f, 0.12f),
        makeBallMat(0.05f, 0.95f, 0.2f, 0.1f, 0.45f),
        makeBallMat(1.0f, 0.85f, 0.1f, 1.0f, 0.1f),
        makeBallMat(0.85f, 0.05f, 0.85f, 0.8f, 0.2f),
        makeBallMat(1.0f, 0.5f, 0.0f, 0.0f, 0.55f),
        makeBallMat(0.05f, 0.85f, 0.85f, 0.9f, 0.15f),
        makeBallMat(0.98f, 0.98f, 0.98f, 1.0f, 0.03f),
    };
    int numBallMats = 8;

    auto spawnBall = [&](float x, float y, float z, int matIdx) {
        float radius = 0.3f + (rand() % 30) * 0.005f;
        float rest = 0.5f + (rand() % 30) * 0.01f;
        int bodyId = physWorld.addDynamicSphere(
            math::Vector3D(x, y, z), radius, 1.0f, rest, 0.4f
        );
        physWorld.getBody(bodyId).velocity = math::Vector3D(
            (rand() % 100 - 50) * 0.04f,
            (rand() % 50) * 0.02f,
            (rand() % 100 - 50) * 0.04f
        );
        auto& node = sceneGraph.createNode("Ball");
        node.setMesh(sphereHQ);
        node.material = ballMats[matIdx % numBallMats];
        ballNodes.push_back(&node);
        ballBodyIds.push_back(bodyId);
    };

    for (int i = 0; i < 8; i++) {
        float x = (i % 4 - 1.5f) * 1.5f;
        float z = (i / 4 - 0.5f) * 1.5f;
        spawnBall(x, 3.0f + i * 0.8f, z, i);
    }

    // ── PostProcess (cinematic) ────────────────────────────────
    renderer::PostProcess::Settings pfx;
    pfx.bloomThreshold = 0.9f;
    pfx.bloomIntensity = 0.15f;
    pfx.bloomSoftKnee  = 0.6f;
    pfx.bloomPasses = 6;
    pfx.vignetteStrength = 0.30f;
    pfx.exposure = 1.2f;
    pfx.fxaaEnabled = true;
    pfx.ssaoEnabled = true;
    pfx.chromaticAberration = 0.002f;
    pfx.filmGrain = 0.015f;
    pfx.sharpenStrength = 0.25f;
    pfx.colorTemp = 0.15f;
    pfx.colorTint = 0.0f;
    pfx.colorContrast = 1.15f;
    pfx.colorSaturation = 1.2f;

    // ── Main loop ──────────────────────────────────────────────
    bool running = true;
    bool mouseCaptured = false;
    Uint32 lastTick = SDL_GetTicks();
    float totalTime = 0.0f;

    std::cout << "\n==============================" << std::endl;
    std::cout << "     ALZE Engine v3 AAA" << std::endl;
    std::cout << "==============================" << std::endl;
    std::cout << " CLICK     : Capture mouse" << std::endl;
    std::cout << " ESCAPE    : Release mouse" << std::endl;
    std::cout << " WASD      : Move" << std::endl;
    std::cout << " Mouse     : Look around" << std::endl;
    std::cout << " Space     : Up | Ctrl : Down" << std::endl;
    std::cout << " Shift     : Sprint" << std::endl;
    std::cout << " F         : Spawn ball" << std::endl;
    std::cout << " R         : Reset all balls" << std::endl;
    std::cout << "==============================\n" << std::endl;

    while (running) {
        input.prepare();

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            input.update(ev);
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) {
                mouseCaptured = false;
                input.setMouseCaptured(false);
            }
            if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT && !mouseCaptured) {
                mouseCaptured = true;
                input.setMouseCaptured(true);
            }
            if (ev.type == SDL_WINDOWEVENT && ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                W = ev.window.data1; H = ev.window.data2;
                fwd.resize(W, H);
                postfx.resize(W, H);
            }
        }

        Uint32 now = SDL_GetTicks();
        float dt = (now - lastTick) / 1000.0f;
        if (dt > 0.05f) dt = 0.05f;
        lastTick = now;
        totalTime += dt;
        pfx.time = totalTime;

        // ── Input ──────────────────────────────────────────────
        if (input.isKeyPressed(SDL_SCANCODE_R)) {
            for (size_t i = 0; i < ballBodyIds.size(); i++) {
                auto& body = physWorld.getBody(ballBodyIds[i]);
                float x = (int(i) % 4 - 1.5f) * 1.5f;
                float z = (int(i) / 4 - 0.5f) * 1.5f;
                body.position = math::Vector3D(x, 3.0f + i * 0.8f, z);
                body.velocity = math::Vector3D(
                    (rand() % 100 - 50) * 0.04f,
                    (rand() % 50) * 0.02f,
                    (rand() % 100 - 50) * 0.04f
                );
                body.angularVelocity = math::Vector3D(0, 0, 0);
                body.wake();
            }
        }
        if (input.isKeyPressed(SDL_SCANCODE_F) && ballBodyIds.size() < 30) {
            math::Vector3D camPos = fps.getPosition();
            math::Vector3D camFwd = fps.getForward();
            int mi = static_cast<int>(ballBodyIds.size()) % numBallMats;
            spawnBall(camPos.x + camFwd.x * 2, camPos.y + camFwd.y * 2, camPos.z + camFwd.z * 2, mi);
            physWorld.getBody(ballBodyIds.back()).velocity = camFwd * 8.0f;
        }

        fps.update(input, dt);
        physWorld.step(dt);

        for (size_t i = 0; i < ballBodyIds.size(); i++) {
            const auto& body = physWorld.getBody(ballBodyIds[i]);
            auto* node = ballNodes[i];
            node->transform.position = body.position;
            float r = body.sphereRadius;
            node->transform.scale = math::Vector3D(r, r, r);
            node->transform.rotation = body.getOrientation();
            node->markDirty();
        }

        // ── Lighting ───────────────────────────────────────────
        fwd.clearLights();

        renderer::DirectionalLight sun;
        sun.direction = math::Vector3D(0.3f, -0.6f, -0.4f).normalized();
        sun.color = math::Vector3D(8.0f, 6.5f, 4.0f);
        sun.skyColor    = math::Vector3D(0.20f, 0.30f, 0.55f);
        sun.groundColor = math::Vector3D(0.05f, 0.03f, 0.02f);
        sun.ambientIntensity = 0.15f;
        fwd.setDirectionalLight(sun);

        for (auto& orb : orbs) {
            renderer::PointLight pl;
            pl.position = math::Vector3D(orb.x, orb.y, orb.z);
            pl.color = math::Vector3D(orb.r, orb.g, orb.b) * orb.intensity;
            pl.linear = 0.07f;
            pl.quadratic = 0.017f;
            fwd.addPointLight(pl);
        }

        // ── RENDER ─────────────────────────────────────────────
        postfx.beginScene();
        sceneGraph.updateTransforms();

        math::Matrix4x4 view = fps.getViewMatrix();
        math::Matrix4x4 proj = fps.getProjectionMatrix((float)W / H);

        fwd.begin(view, proj);
        sceneGraph.submitToRenderer(fwd);
        fwd.end();
        skybox.draw(view, proj, sky);

        ssao.generate(postfx.getDepthTexture(), proj);
        pfx.ssaoTexture = ssao.getResult();

        postfx.endScene(pfx);
        SDL_GL_SwapWindow(window);

        static int fc = 0; static float ft = 0;
        fc++; ft += dt;
        if (ft >= 1.0f) {
            auto stats = fwd.getStats();
            char title[256];
            snprintf(title, sizeof(title),
                "ALZE v3 | FPS: %d | Draw: %d | Culled: %d | Tris: %dk | Bodies: %d | Contacts: %d | %s",
                fc, stats.drawCalls, stats.culledObjects,
                stats.totalTriangles / 1000, (int)ballBodyIds.size(),
                physWorld.getContactCount(),
                mouseCaptured ? "CAPTURED" : "Click to capture");
            SDL_SetWindowTitle(window, title);
            fc = 0; ft = 0;
        }
    }

    SDL_GL_DeleteContext(glCtx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
