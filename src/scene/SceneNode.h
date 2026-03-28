#pragma once

#include "math/Transform3D.h"
#include "math/Matrix4x4.h"
#include "renderer/MeshPrimitives.h"
#include "renderer/Material.h"
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include <functional>

namespace engine {
namespace scene {

/// SceneNode — Nodo jerárquico de una escena 3D.
///
/// Cada nodo tiene:
///   - Transform3D local (posición/rotación/escala relativa al padre)
///   - World matrix (calculada automáticamente por propagación)
///   - Mesh + Material opcionales (si tiene, se dibuja)
///   - Hijos (ownership via unique_ptr)
///
/// Patrón de uso:
///   auto& cube = root.createChild("MyCube");
///   cube.transform.position = {2, 0, 0};
///   cube.setMesh(cubeMesh);
///   cube.setMaterial(goldMat);
///
///   auto& arm = cube.createChild("Arm");  // hijo del cubo
///   arm.transform.position = {1, 0, 0};   // relativo al cubo
///
class SceneNode {
public:
    // ── Identidad ──────────────────────────────────────────────
    std::string name;
    bool        active = true;    // Desactivar = no se dibuja ni se propaga

    // ── Transform local ────────────────────────────────────────
    math::Transform3D transform;

    // ── Datos de rendering (opcionales) ────────────────────────
    const renderer::Mesh3D*  mesh     = nullptr;
    renderer::Material       material;
    bool                     hasMesh  = false;

    // ── Constructor ────────────────────────────────────────────
    explicit SceneNode(const std::string& nodeName = "Node")
        : name(nodeName) {}

    // No copiable (ownership de hijos)
    SceneNode(const SceneNode&) = delete;
    SceneNode& operator=(const SceneNode&) = delete;

    // ── Hierarchy ──────────────────────────────────────────────

    /// Crear un hijo y devolverlo por referencia
    SceneNode& createChild(const std::string& childName = "Child") {
        auto child = std::make_unique<SceneNode>(childName);
        child->m_parent = this;
        auto& ref = *child;
        m_children.push_back(std::move(child));
        markDirty();
        return ref;
    }

    /// Eliminar un hijo por nombre (primera coincidencia)
    bool removeChild(const std::string& childName) {
        auto it = std::find_if(m_children.begin(), m_children.end(),
            [&](const auto& c) { return c->name == childName; });
        if (it != m_children.end()) {
            m_children.erase(it);
            return true;
        }
        return false;
    }

    /// Buscar nodo por nombre (recursivo, BFS)
    SceneNode* findByName(const std::string& targetName) {
        if (name == targetName) return this;
        for (auto& child : m_children) {
            SceneNode* found = child->findByName(targetName);
            if (found) return found;
        }
        return nullptr;
    }

    /// Número de hijos directos
    int getChildCount() const { return static_cast<int>(m_children.size()); }

    /// Acceso a hijo por índice
    SceneNode& getChild(int index) { return *m_children[index]; }
    const SceneNode& getChild(int index) const { return *m_children[index]; }

    /// Padre (nullptr si es root)
    SceneNode* getParent() const { return m_parent; }

    // ── Rendering Helpers ──────────────────────────────────────

    void setMesh(const renderer::Mesh3D& m) { mesh = &m; hasMesh = true; }
    void setMaterial(const renderer::Material& mat) { material = mat; }

    // ── World Matrix ───────────────────────────────────────────

    /// Obtener la world matrix (cacheada, recalcula si dirty)
    const math::Matrix4x4& getWorldMatrix() const {
        if (m_dirty) {
            recalcWorldMatrix();
        }
        return m_worldMatrix;
    }

    /// Marcar como dirty (se propagará a los hijos)
    void markDirty() {
        if (!m_dirty) {
            m_dirty = true;
            for (auto& child : m_children) {
                child->markDirty();
            }
        }
    }

    /// Actualizar world matrices recursivamente
    void updateWorldMatrix() {
        math::Matrix4x4 localMatrix = transform.toMatrix();

        if (m_parent) {
            m_worldMatrix = m_parent->getWorldMatrix() * localMatrix;
        } else {
            m_worldMatrix = localMatrix;
        }

        m_dirty = false;

        for (auto& child : m_children) {
            child->m_dirty = true;
            child->updateWorldMatrix();
        }
    }

    // ── Traversal ──────────────────────────────────────────────

    /// Recorrer el árbol (pre-order) con callback
    void traverse(const std::function<void(SceneNode&)>& callback) {
        if (!active) return;
        callback(*this);
        for (auto& child : m_children) {
            child->traverse(callback);
        }
    }

    /// Contar total de nodos (incluyendo este)
    int countNodes() const {
        int count = 1;
        for (const auto& child : m_children)
            count += child->countNodes();
        return count;
    }

    /// Contar nodos visibles con mesh
    int countRenderables() const {
        int count = (active && hasMesh) ? 1 : 0;
        for (const auto& child : m_children)
            count += child->countRenderables();
        return count;
    }

private:
    SceneNode*                              m_parent = nullptr;
    std::vector<std::unique_ptr<SceneNode>> m_children;
    mutable math::Matrix4x4                 m_worldMatrix = math::Matrix4x4::identity();
    mutable bool                            m_dirty = true;

    void recalcWorldMatrix() const {
        math::Matrix4x4 localMatrix = transform.toMatrix();
        if (m_parent) {
            m_worldMatrix = m_parent->getWorldMatrix() * localMatrix;
        } else {
            m_worldMatrix = localMatrix;
        }
        m_dirty = false;
    }
};

} // namespace scene
} // namespace engine
