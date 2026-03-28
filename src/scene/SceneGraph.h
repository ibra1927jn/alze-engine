#pragma once

#include "SceneNode.h"
#include "renderer/ForwardRenderer.h"

namespace engine {
namespace scene {

/// SceneGraph — Contenedor raíz del árbol de escena.
///
/// Gestiona la jerarquía completa de nodos, propaga transforms,
/// y envía los nodos visibles al ForwardRenderer.
///
/// Uso:
///   SceneGraph scene;
///   auto& cube = scene.createNode("Cube");
///   cube.setMesh(cubeMesh);
///   cube.transform.position = {0, 1, 0};
///
///   auto& arm = cube.createChild("Arm");
///   arm.transform.position = {2, 0, 0};  // relativo al cubo
///
///   scene.submitToRenderer(renderer);  // Envía todos los visibles
///
class SceneGraph {
public:
    SceneGraph() : m_root("__Root__") {}

    /// Crear un nodo hijo directo de la raíz
    SceneNode& createNode(const std::string& name = "Node") {
        return m_root.createChild(name);
    }

    /// Acceso a la raíz (para traversal manual)
    SceneNode& getRoot() { return m_root; }
    const SceneNode& getRoot() const { return m_root; }

    /// Buscar nodo por nombre en toda la escena
    SceneNode* findNode(const std::string& name) {
        return m_root.findByName(name);
    }

    /// Actualizar todas las world matrices (llamar antes de render)
    void updateTransforms() {
        m_root.updateWorldMatrix();
    }

    /// Enviar todos los nodos visibles al ForwardRenderer
    void submitToRenderer(renderer::ForwardRenderer& fwd) {
        m_root.traverse([&](SceneNode& node) {
            if (node.hasMesh && node.mesh && node.mesh->isValid()) {
                fwd.submit(*node.mesh, node.material, node.getWorldMatrix());
            }
        });
    }

    /// Estadísticas
    int getTotalNodes() const { return m_root.countNodes() - 1; }  // -1 por root
    int getRenderableNodes() const { return m_root.countRenderables(); }

private:
    SceneNode m_root;
};

} // namespace scene
} // namespace engine
