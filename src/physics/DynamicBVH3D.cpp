#include "DynamicBVH3D.h"
#include <algorithm>

namespace engine {
namespace physics {

DynamicBVH3D::DynamicBVH3D(int capacity) : m_nodeCapacity(capacity) {
    m_nodes.resize(m_nodeCapacity);
    // Setup free list
    for (int i = 0; i < m_nodeCapacity - 1; i++) {
        m_nodes[i].nextFree = i + 1;
        m_nodes[i].height = -1;
    }
    m_nodes[m_nodeCapacity - 1].nextFree = -1;
    m_nodes[m_nodeCapacity - 1].height = -1;
    m_freeList = 0;
}

int DynamicBVH3D::allocateNode() {
    if (m_freeList == -1) {
        // Grow pool
        int oldCap = m_nodeCapacity;
        m_nodeCapacity *= 2;
        m_nodes.resize(m_nodeCapacity);
        for (int i = oldCap; i < m_nodeCapacity - 1; i++) {
            m_nodes[i].nextFree = i + 1;
            m_nodes[i].height = -1;
        }
        m_nodes[m_nodeCapacity - 1].nextFree = -1;
        m_nodes[m_nodeCapacity - 1].height = -1;
        m_freeList = oldCap;
    }
    int nodeId = m_freeList;
    m_freeList = m_nodes[nodeId].nextFree;
    m_nodes[nodeId].parent = -1;
    m_nodes[nodeId].left = -1;
    m_nodes[nodeId].right = -1;
    m_nodes[nodeId].height = 0;
    m_nodes[nodeId].userData = -1;
    m_nodeCount++;
    return nodeId;
}

void DynamicBVH3D::freeNode(int nodeId) {
    m_nodes[nodeId].nextFree = m_freeList;
    m_nodes[nodeId].height = -1;
    m_freeList = nodeId;
    m_nodeCount--;
}

void DynamicBVH3D::clear() {
    m_root = -1;
    m_nodeCount = 0;
    m_freeList = 0;
    for (int i = 0; i < m_nodeCapacity - 1; i++) {
        m_nodes[i].nextFree = i + 1;
        m_nodes[i].height = -1;
    }
    m_nodes[m_nodeCapacity - 1].nextFree = -1;
    m_nodes[m_nodeCapacity - 1].height = -1;
}

int DynamicBVH3D::insert(int userData, const AABB3D& aabb) {
    int nodeId = allocateNode();
    
    // Fat AABB
    math::Vector3D fatExtents = aabb.extents() + math::Vector3D(AABB_MARGIN, AABB_MARGIN, AABB_MARGIN);
    m_nodes[nodeId].aabb = AABB3D(aabb.center() - fatExtents, aabb.center() + fatExtents);
    m_nodes[nodeId].userData = userData;
    m_nodes[nodeId].height = 0;

    insertLeaf(nodeId);
    return nodeId;
}

void DynamicBVH3D::remove(int nodeId) {
    if (nodeId < 0 || nodeId >= m_nodeCapacity || m_nodes[nodeId].height == -1) return;
    removeLeaf(nodeId);
    freeNode(nodeId);
}

bool DynamicBVH3D::update(int nodeId, const AABB3D& aabb, const math::Vector3D& displacement) {
    if (m_nodes[nodeId].aabb.contains(aabb)) {
        return false;
    }

    removeLeaf(nodeId);

    // Predict movement
    AABB3D fatted = aabb;
    math::Vector3D d = displacement * 2.0f; // Predict further
    if (d.x < 0.0f) fatted.min.x += d.x; else fatted.max.x += d.x;
    if (d.y < 0.0f) fatted.min.y += d.y; else fatted.max.y += d.y;
    if (d.z < 0.0f) fatted.min.z += d.z; else fatted.max.z += d.z;

    math::Vector3D fatExtents = fatted.extents() + math::Vector3D(AABB_MARGIN, AABB_MARGIN, AABB_MARGIN);
    m_nodes[nodeId].aabb = AABB3D(fatted.center() - fatExtents, fatted.center() + fatExtents);

    insertLeaf(nodeId);
    return true;
}

void DynamicBVH3D::insertLeaf(int leaf) {
    if (m_root == -1) {
        m_root = leaf;
        m_nodes[m_root].parent = -1;
        return;
    }

    AABB3D leafAABB = m_nodes[leaf].aabb;
    int index = m_root;

    // Phase 1: Buscar mejor hermano para el nodo
    while (!m_nodes[index].isLeaf()) {
        int left = m_nodes[index].left;
        int right = m_nodes[index].right;

        float area = m_nodes[index].aabb.surfaceArea();
        
        AABB3D combinedAABB = m_nodes[index].aabb;
        combinedAABB.merge(leafAABB);
        float combinedArea = combinedAABB.surfaceArea();

        float cost = 2.0f * combinedArea;
        float inheritanceCost = 2.0f * (combinedArea - area);

        float cost1, cost2;
        
        // Evaluate left child
        AABB3D aabb1 = leafAABB;
        aabb1.merge(m_nodes[left].aabb);
        if (m_nodes[left].isLeaf()) cost1 = aabb1.surfaceArea() + inheritanceCost;
        else cost1 = (aabb1.surfaceArea() - m_nodes[left].aabb.surfaceArea()) + inheritanceCost;

        // Evaluate right child
        AABB3D aabb2 = leafAABB;
        aabb2.merge(m_nodes[right].aabb);
        if (m_nodes[right].isLeaf()) cost2 = aabb2.surfaceArea() + inheritanceCost;
        else cost2 = (aabb2.surfaceArea() - m_nodes[right].aabb.surfaceArea()) + inheritanceCost;

        if (cost < cost1 && cost < cost2) break;

        index = (cost1 < cost2) ? left : right;
    }

    int sibling = index;
    int oldParent = m_nodes[sibling].parent;
    int newParent = allocateNode();
    m_nodes[newParent].parent = oldParent;
    m_nodes[newParent].userData = -1;
    m_nodes[newParent].aabb = leafAABB;
    m_nodes[newParent].aabb.merge(m_nodes[sibling].aabb);
    m_nodes[newParent].height = m_nodes[sibling].height + 1;

    if (oldParent != -1) {
        if (m_nodes[oldParent].left == sibling) m_nodes[oldParent].left = newParent;
        else m_nodes[oldParent].right = newParent;
    } else {
        m_root = newParent;
    }

    m_nodes[newParent].left = sibling;
    m_nodes[newParent].right = leaf;
    m_nodes[sibling].parent = newParent;
    m_nodes[leaf].parent = newParent;

    // Phase 2: Caminar arriba actualizando AABBs y balanceando
    syncHierarchy(m_nodes[leaf].parent);
}

void DynamicBVH3D::removeLeaf(int leaf) {
    if (leaf == m_root) {
        m_root = -1;
        return;
    }

    int parent = m_nodes[leaf].parent;
    int grandParent = m_nodes[parent].parent;
    int sibling = (m_nodes[parent].left == leaf) ? m_nodes[parent].right : m_nodes[parent].left;

    if (grandParent != -1) {
        if (m_nodes[grandParent].left == parent) m_nodes[grandParent].left = sibling;
        else m_nodes[grandParent].right = sibling;
        m_nodes[sibling].parent = grandParent;
        freeNode(parent);
        syncHierarchy(grandParent);
    } else {
        m_root = sibling;
        m_nodes[sibling].parent = -1;
        freeNode(parent);
    }
}

void DynamicBVH3D::syncHierarchy(int index) {
    while (index != -1) {
        index = balance(index);
        int left = m_nodes[index].left;
        int right = m_nodes[index].right;
        
        m_nodes[index].height = 1 + std::max(m_nodes[left].height, m_nodes[right].height);
        m_nodes[index].aabb = m_nodes[left].aabb;
        m_nodes[index].aabb.merge(m_nodes[right].aabb);
        
        index = m_nodes[index].parent;
    }
}

int DynamicBVH3D::balance(int iA) {
    if (m_nodes[iA].isLeaf() || m_nodes[iA].height < 2) return iA;

    int iB = m_nodes[iA].left;
    int iC = m_nodes[iA].right;

    int balanceFactor = m_nodes[iC].height - m_nodes[iB].height;

    // Rotate C up
    if (balanceFactor > 1) {
        int iF = m_nodes[iC].left;
        int iG = m_nodes[iC].right;

        m_nodes[iC].left = iA;
        m_nodes[iC].parent = m_nodes[iA].parent;
        m_nodes[iA].parent = iC;

        if (m_nodes[iC].parent != -1) {
            if (m_nodes[m_nodes[iC].parent].left == iA) m_nodes[m_nodes[iC].parent].left = iC;
            else m_nodes[m_nodes[iC].parent].right = iC;
        } else {
            m_root = iC;
        }

        if (m_nodes[iF].height > m_nodes[iG].height) {
            m_nodes[iC].right = iF;
            m_nodes[iA].right = iG;
            m_nodes[iG].parent = iA;
            m_nodes[iA].aabb = m_nodes[iB].aabb;
            m_nodes[iA].aabb.merge(m_nodes[iG].aabb);
            m_nodes[iC].aabb = m_nodes[iA].aabb;
            m_nodes[iC].aabb.merge(m_nodes[iF].aabb);
            m_nodes[iA].height = 1 + std::max(m_nodes[iB].height, m_nodes[iG].height);
            m_nodes[iC].height = 1 + std::max(m_nodes[iA].height, m_nodes[iF].height);
        } else {
            m_nodes[iC].right = iG;
            m_nodes[iA].right = iF;
            m_nodes[iF].parent = iA;
            m_nodes[iA].aabb = m_nodes[iB].aabb;
            m_nodes[iA].aabb.merge(m_nodes[iF].aabb);
            m_nodes[iC].aabb = m_nodes[iA].aabb;
            m_nodes[iC].aabb.merge(m_nodes[iG].aabb);
            m_nodes[iA].height = 1 + std::max(m_nodes[iB].height, m_nodes[iF].height);
            m_nodes[iC].height = 1 + std::max(m_nodes[iA].height, m_nodes[iG].height);
        }
        return iC;
    }

    // Rotate B up
    if (balanceFactor < -1) {
        int iD = m_nodes[iB].left;
        int iE = m_nodes[iB].right;

        m_nodes[iB].left = iA;
        m_nodes[iB].parent = m_nodes[iA].parent;
        m_nodes[iA].parent = iB;

        if (m_nodes[iB].parent != -1) {
            if (m_nodes[m_nodes[iB].parent].left == iA) m_nodes[m_nodes[iB].parent].left = iB;
            else m_nodes[m_nodes[iB].parent].right = iB;
        } else {
            m_root = iB;
        }

        if (m_nodes[iD].height > m_nodes[iE].height) {
            m_nodes[iB].right = iD;
            m_nodes[iA].left = iE;
            m_nodes[iE].parent = iA;
            m_nodes[iA].aabb = m_nodes[iC].aabb;
            m_nodes[iA].aabb.merge(m_nodes[iE].aabb);
            m_nodes[iB].aabb = m_nodes[iA].aabb;
            m_nodes[iB].aabb.merge(m_nodes[iD].aabb);
            m_nodes[iA].height = 1 + std::max(m_nodes[iC].height, m_nodes[iE].height);
            m_nodes[iB].height = 1 + std::max(m_nodes[iA].height, m_nodes[iD].height);
        } else {
            m_nodes[iB].right = iE;
            m_nodes[iA].left = iD;
            m_nodes[iD].parent = iA;
            m_nodes[iA].aabb = m_nodes[iC].aabb;
            m_nodes[iA].aabb.merge(m_nodes[iD].aabb);
            m_nodes[iB].aabb = m_nodes[iA].aabb;
            m_nodes[iB].aabb.merge(m_nodes[iE].aabb);
            m_nodes[iA].height = 1 + std::max(m_nodes[iC].height, m_nodes[iD].height);
            m_nodes[iB].height = 1 + std::max(m_nodes[iA].height, m_nodes[iE].height);
        }
        return iB;
    }

    return iA;
}

std::vector<std::pair<int, int>> DynamicBVH3D::getPotentialPairs() const {
    std::vector<std::pair<int, int>> pairs;
    pairs.reserve(m_nodeCount * 4); // Estimación empírica

    if (m_root == -1) return pairs;

    // Fast Tree-Traversal pair overlap using fixed-size D-Stack
    struct Pair { int a, b; };
    Pair stack[128];
    int top = 0;
    
    // Begin by testing root vs itself
    stack[top++] = {m_root, m_root};

    while (top > 0) {
        auto [a, b] = stack[--top];

        if (a == -1 || b == -1) continue;

        const Node& nA = m_nodes[a];
        const Node& nB = m_nodes[b];

        if (!nA.aabb.overlaps(nB.aabb)) continue;

        if (nA.isLeaf() && nB.isLeaf()) {
            if (a != b) {
                // Return original user data pair
                pairs.push_back({std::min(nA.userData, nB.userData), std::max(nA.userData, nB.userData)});
            }
        } else if (nA.isLeaf()) {
            if (top < 126) { stack[top++] = {a, nB.left}; stack[top++] = {a, nB.right}; }
        } else if (nB.isLeaf()) {
            if (top < 126) { stack[top++] = {nA.left, b}; stack[top++] = {nA.right, b}; }
        } else {
            if (top < 124) {
                stack[top++] = {nA.left, nB.left};
                stack[top++] = {nA.right, nB.left};
                stack[top++] = {nA.left, nB.right};
                stack[top++] = {nA.right, nB.right};
            }
        }
    }

    // Remove duplicates
    std::sort(pairs.begin(), pairs.end());
    pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());

    return pairs;
}

} // namespace physics
} // namespace engine
