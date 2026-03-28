#pragma once

#include "physics/Collider3D.h"
#include <vector>
#include <cstdint>

namespace engine {
namespace physics {

/// Dynamic Bounding Volume Hierarchy (Árbol AABB Dinámico)
/// Provee O(log N) para Raycasts, culling y detección de pares.
/// Optimizado para CPU cache: sin punteros usando una memoria de Pool continua.
class DynamicBVH3D {
public:
    static constexpr float AABB_MARGIN = 0.1f; // Margen para evitar re-insertar a cada frame

    struct Node {
        AABB3D aabb;
        int userData = -1; // Índice del RigidBody, -1 si es interno
        int parent = -1;
        int left = -1;
        int right = -1;
        int nextFree = -1;
        int height = -1; // -1 indica nodo inactivo/libre

        bool isLeaf() const { return right == -1; }
    };

    DynamicBVH3D(int capacity = 1024);

    /// Inserta un AABB y devuelve su Node ID
    int insert(int userData, const AABB3D& aabb);

    /// Actualiza un AABB. Devuelve true si el árbol fue modificado sustancialmente.
    bool update(int nodeId, const AABB3D& aabb, const math::Vector3D& displacement);

    /// Remueve un nodo
    void remove(int nodeId);

    /// Limpia todo el árbol
    void clear();

    /// Devuelve las cabeceras de pares potenciales (Broadphase)
    template<typename T>
    void queryOverlap(const AABB3D& aabb, T callback) const;

    /// Realiza un raycast sobre el árbol
    template<typename T>
    void raycast(const Ray3D& ray, T callback) const;

    /// Obtener todos los pares potenciales usando recorrido D-Stack (O(N log N))
    std::vector<std::pair<int, int>> getPotentialPairs() const;

    int getRoot() const { return m_root; }
    const Node* getNodes() const { return m_nodes.data(); }

private:
    int allocateNode();
    void freeNode(int nodeId);
    void insertLeaf(int leaf);
    void removeLeaf(int leaf);
    int balance(int iA);
    void syncHierarchy(int nodeId);

    std::vector<Node> m_nodes;
    int m_root = -1;
    int m_freeList = 0;
    int m_nodeCapacity = 0;
    int m_nodeCount = 0;
};

// --- IMPLEMENTACIONES TEMPLATES ---

template<typename T>
void DynamicBVH3D::queryOverlap(const AABB3D& aabb, T callback) const {
    if (m_root == -1) return;

    int stack[256];
    int top = 0;
    stack[top++] = m_root;

    while (top > 0) {
        int nodeId = stack[--top];

        const Node& node = m_nodes[nodeId];
        if (node.aabb.overlaps(aabb)) {
            if (node.isLeaf()) {
                if (!callback(node.userData)) return;
            } else {
                if (top < 254) {
                    stack[top++] = node.left;
                    stack[top++] = node.right;
                }
            }
        }
    }
}

template<typename T>
void DynamicBVH3D::raycast(const Ray3D& ray, T callback) const {
    if (m_root == -1) return;

    int stack[256];
    int top = 0;
    stack[top++] = m_root;

    while (top > 0) {
        int nodeId = stack[--top];

        const Node& node = m_nodes[nodeId];
        engine::physics::RayHit3D boxHit = engine::physics::rayVsAABB(ray, node.aabb);
        
        if (boxHit.hit) {
            if (node.isLeaf()) {
                if (!callback(node.userData)) return;
            } else {
                if (top < 254) {
                    stack[top++] = node.left;
                    stack[top++] = node.right;
                }
            }
        }
    }
}

} // namespace physics
} // namespace engine
