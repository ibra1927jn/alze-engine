#include "WorldScene3D.h"
#include "math/MathUtils.h"

namespace engine {
namespace game {

namespace WorldScene3D {

void registerComponents(ecs::ECSCoordinator& ecs) {
    ecs.registerComponent<ecs::Transform3DComponent>();
    ecs.registerComponent<ecs::MeshComponent>();
    ecs.registerComponent<ecs::Physics3DComponent>();
    ecs.registerComponent<ecs::Collider3DComponent>();
    ecs.registerComponent<ecs::PointLightComponent>();
    ecs.registerComponent<ecs::TagComponent>();
    ecs.registerComponent<ecs::ConstraintComponent>();
}

ecs::Entity createFloor(ecs::ECSCoordinator& ecs, renderer::Mesh3D& plane, renderer::Texture2D* normalMapTex) {
    auto e = ecs.createEntity();
    auto& t = ecs.emplaceComponent<ecs::Transform3DComponent>(e);
    t.transform.scale = {30, 1, 30};
    t.dirty = true;

    renderer::Material mat;
    mat.albedoColor = {0.35f, 0.33f, 0.30f};
    mat.roughness   = 0.85f;
    if (normalMapTex) mat.normalMap = normalMapTex;

    ecs.addComponent<ecs::MeshComponent>(e, {&plane, mat});
    ecs.addComponent<ecs::TagComponent>(e, ecs::TagComponent(ecs::TagComponent::GROUND));
    return e;
}

ecs::Entity createHelmet(ecs::ECSCoordinator& ecs, renderer::LoadedModel& model, bool modelLoaded) {
    ecs::Entity lastEntity = 0;
    if (!modelLoaded) return 0;

    for (const auto& entry : model.meshEntries) {
        auto e = ecs.createEntity();
        auto& t = ecs.emplaceComponent<ecs::Transform3DComponent>(e, math::Vector3D(0, 2.0f, 0));
        t.transform.scale = {1.5f, 1.5f, 1.5f};
        t.dirty = true;

        const renderer::Material& mat =
            (entry.materialIndex >= 0 && entry.materialIndex < (int)model.materials.size())
                ? model.materials[entry.materialIndex]
                : renderer::Material::chrome();

        ecs.addComponent<ecs::MeshComponent>(e, {&entry.mesh, mat});
        lastEntity = e;
    }
    return lastEntity;
}

void createPhysicsSpheres(ecs::ECSCoordinator& ecs, renderer::Mesh3D& sphereHigh) {
    struct SphereSetup {
        math::Vector3D pos;
        float          radius;
        renderer::Material mat;
    };

    const SphereSetup spheres[] = {
        {{-4, 1.5f, 0},  1.0f, renderer::Material::gold()},
        {{ 4, 1.5f, 0},  1.0f, renderer::Material::chrome()},
        {{ 0, 1.5f, 4},  0.8f, renderer::Material::copper()},
        {{ 0, 1.5f,-4},  0.8f, renderer::Material::emerald()},
        {{-2, 0.7f, 2},  0.5f, renderer::Material::ruby()},
        {{ 2, 0.7f,-2},  0.5f, renderer::Material::ceramic()},
        {{ 6, 1.2f, 5},  0.9f, renderer::Material::iron()},
        {{-6, 1.2f,-5},  0.9f, renderer::Material::rubber()},
    };

    for (const auto& s : spheres) {
        auto e = ecs.createEntity();
        auto& t = ecs.emplaceComponent<ecs::Transform3DComponent>(e, s.pos);
        float d = s.radius * 2.0f;
        t.transform.scale = {d, d, d};
        t.dirty = true;
        ecs.addComponent<ecs::MeshComponent>(e, {&sphereHigh, s.mat});
        auto& p = ecs.emplaceComponent<ecs::Physics3DComponent>(e, 2.0f);
        p.restitution = 0.6f;
        p.friction    = 0.3f;
        ecs.addComponent<ecs::Collider3DComponent>(e, ecs::Collider3DComponent::sphere(s.radius));
    }
}

void createPhysicsCubes(ecs::ECSCoordinator& ecs, renderer::Mesh3D& cube) {
    struct CubeSetup {
        math::Vector3D pos;
        math::Vector3D scale;
        float mass;
        renderer::Material mat;
    };

    const CubeSetup cubes[] = {
        {{-3, 0.75f, 3}, {1.5f,1.5f,1.5f}, 3.0f, []{ renderer::Material m; m.albedoColor={0.9f,0.15f,0.1f}; m.roughness=0.3f; m.metallic=0.1f; return m; }()},
        {{ 3, 0.75f, 3}, {1.5f,1.5f,1.5f}, 3.0f, renderer::Material::iron()},
        {{ 0, 0.5f,  6}, {1.0f,1.0f,1.0f}, 1.5f, []{ renderer::Material m; m.albedoColor={0.1f,0.4f,0.9f}; m.roughness=0.2f; m.metallic=0.8f; return m; }()},
        {{-5, 1.0f, -3}, {2.0f,2.0f,2.0f}, 5.0f, renderer::Material::gold()},
    };

    for (const auto& c : cubes) {
        auto e = ecs.createEntity();
        auto& t = ecs.emplaceComponent<ecs::Transform3DComponent>(e, c.pos);
        t.transform.scale = c.scale;
        t.dirty = true;
        ecs.addComponent<ecs::MeshComponent>(e, {&cube, c.mat});
        auto& p = ecs.emplaceComponent<ecs::Physics3DComponent>(e, c.mass);
        p.restitution = 0.3f;
        p.friction    = 0.5f;
        ecs.addComponent<ecs::Collider3DComponent>(
            e, ecs::Collider3DComponent::box({c.scale.x * 0.5f, c.scale.y * 0.5f, c.scale.z * 0.5f}));
    }
}

void createOrbitingLights(ecs::ECSCoordinator& ecs, renderer::Mesh3D& particleMesh, ecs::Entity outLights[3]) {
    const math::Vector3D colors[3] = {
        {1.0f, 0.95f, 0.85f},
        {0.85f, 0.92f, 1.0f},
        {1.0f, 0.88f, 0.75f}
    };

    for (int i = 0; i < 3; i++) {
        auto e = ecs.createEntity();
        ecs.emplaceComponent<ecs::Transform3DComponent>(e, math::Vector3D(0, 3, 0));

        ecs::PointLightComponent pl;
        pl.color     = colors[i] * 4.0f;
        pl.linear    = 0.09f;
        pl.quadratic = 0.032f;
        ecs.addComponent(e, pl);

        renderer::Material em;
        em.albedoColor       = colors[i];
        em.emissiveColor     = colors[i];
        em.emissiveIntensity = 5.0f;
        em.roughness         = 1.0f;
        ecs.addComponent<ecs::MeshComponent>(e, {&particleMesh, em});

        outLights[i] = e;
    }
}

void setupPillarsSceneGraph(scene::SceneGraph& sceneGraph, renderer::Mesh3D& cube) {
    auto& root = sceneGraph.createNode("PillarsRoot");
    for (int i = 0; i < 8; i++) {
        float angle = i * (math::MathUtils::PI * 2.0f / 8.0f);
        auto& pillar = root.createChild("Pillar" + std::to_string(i));
        pillar.transform.position = {std::cos(angle) * 10.0f, 1.5f, std::sin(angle) * 10.0f};
        pillar.transform.scale    = {0.4f, 3.0f, 0.4f};
        pillar.mesh    = &cube;
        pillar.hasMesh = true;
        pillar.material.albedoColor = {0.6f, 0.55f, 0.50f};
        pillar.material.roughness   = 0.7f;
        pillar.material.metallic    = 0.1f;

        auto& cap = pillar.createChild("Cap" + std::to_string(i));
        cap.transform.position = {0, 0.6f, 0};
        cap.transform.scale    = {1.5f, 0.15f, 1.5f};
        cap.mesh    = &cube;
        cap.hasMesh = true;
        cap.material = renderer::Material::copper();
    }
}

SceneHandles createFullScene(ecs::ECSCoordinator& ecs,
                                     scene::SceneGraph& sceneGraph,
                                     renderer::Mesh3D& sphereHigh,
                                     renderer::Mesh3D& cube,
                                     renderer::Mesh3D& plane,
                                     renderer::Mesh3D& particleMesh,
                                     renderer::LoadedModel& helmet,
                                     bool helmetLoaded,
                                     renderer::Texture2D* floorNormalTex) {
    SceneHandles out;
    registerComponents(ecs);
    createFloor(ecs, plane, floorNormalTex);
    out.helmetEntity = createHelmet(ecs, helmet, helmetLoaded);
    createPhysicsSpheres(ecs, sphereHigh);
    createPhysicsCubes(ecs, cube);
    createOrbitingLights(ecs, particleMesh, out.lightEntities);
    setupPillarsSceneGraph(sceneGraph, cube);
    return out;
}

} // namespace WorldScene3D
} // namespace game
} // namespace engine
