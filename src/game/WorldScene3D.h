#pragma once

#include "ecs/ECSCoordinator.h"
#include "ecs/Components3D.h"
#include "renderer/Material.h"
#include "renderer/ModelLoader.h"
#include "renderer/MeshPrimitives.h"
#include "scene/SceneGraph.h"
#include "renderer/Texture2D.h"

namespace engine {
namespace game {

namespace WorldScene3D {

void registerComponents(ecs::ECSCoordinator& ecs);

ecs::Entity createFloor(ecs::ECSCoordinator& ecs, renderer::Mesh3D& plane, renderer::Texture2D* normalMapTex = nullptr);

ecs::Entity createHelmet(ecs::ECSCoordinator& ecs, renderer::LoadedModel& model, bool modelLoaded);

void createPhysicsSpheres(ecs::ECSCoordinator& ecs, renderer::Mesh3D& sphereHigh);

void createPhysicsCubes(ecs::ECSCoordinator& ecs, renderer::Mesh3D& cube);

void createOrbitingLights(ecs::ECSCoordinator& ecs, renderer::Mesh3D& particleMesh, ecs::Entity outLights[3]);

void setupPillarsSceneGraph(scene::SceneGraph& sceneGraph, renderer::Mesh3D& cube);

struct SceneHandles {
    ecs::Entity helmetEntity = 0;
    ecs::Entity lightEntities[3] = {};
};

SceneHandles createFullScene(ecs::ECSCoordinator& ecs,
                                     scene::SceneGraph& sceneGraph,
                                     renderer::Mesh3D& sphereHigh,
                                     renderer::Mesh3D& cube,
                                     renderer::Mesh3D& plane,
                                     renderer::Mesh3D& particleMesh,
                                     renderer::LoadedModel& helmet,
                                     bool helmetLoaded,
                                     renderer::Texture2D* floorNormalTex = nullptr);

} // namespace WorldScene3D
} // namespace game
} // namespace engine
