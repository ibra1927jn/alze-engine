// Physics3DSystem.cpp — Implementación de todos los métodos de Physics3DSystem
#include "Physics3DSystem.h"
#include <algorithm>
#include <cmath>

namespace engine {
namespace ecs {

void Physics3DSystem::update(float dt) {
    if (dt <= 0.0f) return;
    float subDt = dt / static_cast<float>(m_subSteps);
    for (int step = 0; step < m_subSteps; step++) {
        collectEntities();
        integrate(subDt);
        broadphase();
        narrowphaseAndSolve(subDt);
        solveConstraints(subDt);
        applyBounds();
        if (m_sleepEnabled) updateSleep(subDt);
    }
}

void Physics3DSystem::collectEntities() {
    m_entities.clear();
    m_sleepingCount = 0;
    m_ecs.forEach<Transform3DComponent, Physics3DComponent, Collider3DComponent>(
        [this](Entity e, Transform3DComponent& t, Physics3DComponent& p, Collider3DComponent& c) {
            m_entities.push_back({e, &t, &p, &c});
            if (p.sleeping) m_sleepingCount++;
        }
    );
}

void Physics3DSystem::integrate(float dt) {
    for (auto& ent : m_entities) {
        auto& p = *ent.p;
        auto& t = *ent.t;
        if (p.isStatic || p.sleeping) continue;
        if (p.useGravity) p.velocity += m_gravity * dt;
        p.velocity += p.acceleration * dt;
        p.acceleration = math::Vector3D::Zero;
        float linDamp = 1.0f / (1.0f + p.drag * dt * 60.0f);
        float angDamp = 1.0f / (1.0f + p.angularDrag * dt * 60.0f);
        p.velocity = p.velocity * linDamp;
        p.angularVelocity = p.angularVelocity * angDamp;
        float linSpeedSq = p.velocity.sqrMagnitude();
        if (linSpeedSq > m_maxLinVelSq)
            p.velocity = p.velocity * (m_maxLinVel / std::sqrt(linSpeedSq));
        float angSpeedSq = p.angularVelocity.sqrMagnitude();
        if (angSpeedSq > m_maxAngVelSq)
            p.angularVelocity = p.angularVelocity * (m_maxAngVel / std::sqrt(angSpeedSq));
        t.transform.position += p.velocity * dt;
        if (p.angularVelocity.sqrMagnitude() > 1e-10f) {
            math::Quaternion omega(p.angularVelocity.x, p.angularVelocity.y, p.angularVelocity.z, 0.0f);
            math::Quaternion spin = (omega * t.transform.rotation).scale(0.5f);
            t.transform.rotation.x += spin.x * dt;
            t.transform.rotation.y += spin.y * dt;
            t.transform.rotation.z += spin.z * dt;
            t.transform.rotation.w += spin.w * dt;
            t.transform.rotation.normalize();
        }
        t.dirty = true;
    }
}

physics::AABB3D Physics3DSystem::computeAABB(const Transform3DComponent& t,
                                               const Collider3DComponent& c) const {
    math::Vector3D pos = t.transform.position + c.offset;
    if (c.shape == Collider3DComponent::SPHERE) {
        return physics::AABB3D(pos - math::Vector3D(c.radius, c.radius, c.radius),
                                pos + math::Vector3D(c.radius, c.radius, c.radius));
    } else if (c.shape == Collider3DComponent::CAPSULE) {
        math::Vector3D worldAxis = t.transform.rotation.rotateVector(c.capsuleAxis).normalized();
        float halfH = c.capsuleHeight * 0.5f;
        math::Vector3D extent(std::abs(worldAxis.x)*halfH + c.radius,
                              std::abs(worldAxis.y)*halfH + c.radius,
                              std::abs(worldAxis.z)*halfH + c.radius);
        return physics::AABB3D(pos - extent, pos + extent);
    } else {
        math::Vector3D he = c.halfExtents * t.transform.scale;
        math::Matrix4x4 rotMat = t.transform.rotation.toMatrix();
        float ex = std::abs(rotMat.get(0,0))*he.x + std::abs(rotMat.get(0,1))*he.y + std::abs(rotMat.get(0,2))*he.z;
        float ey = std::abs(rotMat.get(1,0))*he.x + std::abs(rotMat.get(1,1))*he.y + std::abs(rotMat.get(1,2))*he.z;
        float ez = std::abs(rotMat.get(2,0))*he.x + std::abs(rotMat.get(2,1))*he.y + std::abs(rotMat.get(2,2))*he.z;
        return physics::AABB3D(pos - math::Vector3D(ex, ey, ez), pos + math::Vector3D(ex, ey, ez));
    }
}

physics::OBB3D Physics3DSystem::buildOBB(const Transform3DComponent& t,
                                          const Collider3DComponent& c) const {
    math::Vector3D pos = t.transform.position + c.offset;
    physics::OBB3D obb;
    obb.center = pos;
    obb.halfExtents = c.halfExtents * t.transform.scale;
    obb.axes[0] = t.transform.rotation.rotateVector(math::Vector3D(1,0,0));
    obb.axes[1] = t.transform.rotation.rotateVector(math::Vector3D(0,1,0));
    obb.axes[2] = t.transform.rotation.rotateVector(math::Vector3D(0,0,1));
    return obb;
}

physics::ContactInfo Physics3DSystem::doNarrowphase(const PhysEntity& a, const PhysEntity& b) const {
    math::Vector3D posA = a.t->transform.position + a.c->offset;
    math::Vector3D posB = b.t->transform.position + b.c->offset;
    using S = Collider3DComponent::Shape;

    if (a.c->shape == S::SPHERE && b.c->shape == S::SPHERE)
        return physics::sphereVsSphere(physics::SphereCollider(posA, a.c->radius),
                                        physics::SphereCollider(posB, b.c->radius));
    if (a.c->shape == S::SPHERE && b.c->shape == S::BOX) {
        auto info = physics::obbVsSphere(buildOBB(*b.t, *b.c), physics::SphereCollider(posA, a.c->radius));
        if (info.hasContact) info.normal = info.normal * -1.0f;
        return info;
    }
    if (a.c->shape == S::BOX && b.c->shape == S::SPHERE)
        return physics::obbVsSphere(buildOBB(*a.t, *a.c), physics::SphereCollider(posB, b.c->radius));
    if (a.c->shape == S::BOX && b.c->shape == S::BOX)
        return physics::obbVsOBB(buildOBB(*a.t, *a.c), buildOBB(*b.t, *b.c));

    auto buildCapsule = [](const Transform3DComponent& t, const Collider3DComponent& c) {
        math::Vector3D pos = t.transform.position + c.offset;
        math::Vector3D worldAxis = t.transform.rotation.rotateVector(c.capsuleAxis).normalized();
        return physics::CapsuleCollider(pos, c.capsuleHeight, c.radius, worldAxis);
    };
    if (a.c->shape == S::CAPSULE && b.c->shape == S::SPHERE)
        return physics::capsuleVsSphere(buildCapsule(*a.t, *a.c), physics::SphereCollider(posB, b.c->radius));
    if (a.c->shape == S::SPHERE && b.c->shape == S::CAPSULE) {
        auto info = physics::capsuleVsSphere(buildCapsule(*b.t, *b.c), physics::SphereCollider(posA, a.c->radius));
        if (info.hasContact) info.normal = info.normal * -1.0f;
        return info;
    }
    if (a.c->shape == S::CAPSULE && b.c->shape == S::CAPSULE)
        return physics::capsuleVsCapsule(buildCapsule(*a.t, *a.c), buildCapsule(*b.t, *b.c));
    if (a.c->shape == S::CAPSULE && b.c->shape == S::BOX)
        return physics::capsuleVsOBB(buildCapsule(*a.t, *a.c), buildOBB(*b.t, *b.c));
    if (a.c->shape == S::BOX && b.c->shape == S::CAPSULE) {
        auto info = physics::capsuleVsOBB(buildCapsule(*b.t, *b.c), buildOBB(*a.t, *a.c));
        if (info.hasContact) info.normal = info.normal * -1.0f;
        return info;
    }
    return physics::ContactInfo{};
}

void Physics3DSystem::broadphase() {
    m_spatialHash.clear();
    for (uint32_t i = 0; i < static_cast<uint32_t>(m_entities.size()); i++) {
        if (!m_entities[i].p->sleeping || m_entities[i].p->isStatic) {
            physics::AABB3D aabb = computeAABB(*m_entities[i].t, *m_entities[i].c);
            m_spatialHash.insert(i, aabb);
        }
    }
}

uint32_t Physics3DSystem::computeContactHash(uint32_t a, uint32_t b, const math::Vector3D& p) const {
    int32_t qx = static_cast<int32_t>(std::floor(p.x * 50.0f));
    int32_t qy = static_cast<int32_t>(std::floor(p.y * 50.0f));
    int32_t qz = static_cast<int32_t>(std::floor(p.z * 50.0f));
    uint32_t h = a * 73856093u ^ b * 19349663u;
    h ^= static_cast<uint32_t>(qx) * 83492791u;
    h ^= static_cast<uint32_t>(qy) * 53471161u;
    h ^= static_cast<uint32_t>(qz) * 27644437u;
    return h;
}

float Physics3DSystem::computeEffectiveMass(const Physics3DComponent& pA, const Physics3DComponent& pB,
    const math::Vector3D& rA, const math::Vector3D& rB, const math::Vector3D& dir) const
{
    float invMass = pA.invMass + pB.invMass;
    math::Vector3D raCross = rA.cross(dir);
    math::Vector3D rbCross = rB.cross(dir);
    float angA = pA.invMass > 0.0f ? raCross.sqrMagnitude() * pA.invMass * 2.5f : 0.0f;
    float angB = pB.invMass > 0.0f ? rbCross.sqrMagnitude() * pB.invMass * 2.5f : 0.0f;
    float total = invMass + angA + angB;
    return total > 1e-8f ? 1.0f / total : 0.0f;
}

void Physics3DSystem::narrowphaseAndSolve(float dt) {
    m_collisionCount = 0;
    m_narrowphaseTests = 0;
    std::swap(m_contacts, m_prevContacts);
    m_contacts.clear();
    std::sort(m_prevContacts.begin(), m_prevContacts.end(),
        [](const SolverContact& a, const SolverContact& b) { return a.contactHash < b.contactHash; });

    const auto& pairs = m_spatialHash.getPotentialPairs();
    m_broadphasePairs = static_cast<int>(pairs.size());

    for (const auto& [idxA, idxB] : pairs) {
        auto& a = m_entities[idxA];
        auto& b = m_entities[idxB];
        if (!(a.c->layer & b.c->mask) || !(b.c->layer & a.c->mask)) continue;
        if (a.p->isStatic && b.p->isStatic) continue;
        m_narrowphaseTests++;
        physics::ContactInfo info = doNarrowphase(a, b);
        if (!info.hasContact) continue;
        m_collisionCount++;
        if (a.p->sleeping) { a.p->sleeping = false; a.p->sleepTimer = 0; }
        if (b.p->sleeping) { b.p->sleeping = false; b.p->sleepTimer = 0; }
        if (a.c->isTrigger || b.c->isTrigger) continue;

        SolverContact sc;
        sc.idxA = idxA; sc.idxB = idxB;
        sc.normal = info.normal;
        sc.contactPoint = info.contactPoint;
        sc.penetration = info.penetration;
        math::Vector3D posA = a.t->transform.position + a.c->offset;
        math::Vector3D posB = b.t->transform.position + b.c->offset;
        sc.rA = sc.contactPoint - posA;
        sc.rB = sc.contactPoint - posB;
        if (std::abs(sc.normal.x) >= 0.57735f)
            sc.tangent1 = math::Vector3D(sc.normal.y, -sc.normal.x, 0).normalized();
        else
            sc.tangent1 = math::Vector3D(0, sc.normal.z, -sc.normal.y).normalized();
        sc.tangent2 = sc.normal.cross(sc.tangent1);
        sc.normalMass   = computeEffectiveMass(*a.p, *b.p, sc.rA, sc.rB, sc.normal);
        sc.tangentMass1 = computeEffectiveMass(*a.p, *b.p, sc.rA, sc.rB, sc.tangent1);
        sc.tangentMass2 = computeEffectiveMass(*a.p, *b.p, sc.rA, sc.rB, sc.tangent2);
        sc.restitution = std::max(a.p->restitution, b.p->restitution);
        sc.friction = std::sqrt(a.p->friction * b.p->friction);
        float invDt = dt > 0.0f ? 1.0f / dt : 0.0f;
        sc.bias = sc.penetration > 0.005f ? -0.2f * invDt * (sc.penetration - 0.005f) : 0.0f;
        math::Vector3D relVel = b.p->velocity - a.p->velocity;
        float closingVel = relVel.dot(sc.normal);
        if (-closingVel > 1.0f) sc.bias += sc.restitution * closingVel;

        sc.contactHash = computeContactHash(idxA, idxB, sc.contactPoint);
        auto it = std::lower_bound(m_prevContacts.begin(), m_prevContacts.end(),
            sc.contactHash, [](const SolverContact& c, uint32_t h) { return c.contactHash < h; });
        if (it != m_prevContacts.end() && it->contactHash == sc.contactHash) {
            sc.normalImpulse   = it->normalImpulse;
            sc.tangentImpulse1 = it->tangentImpulse1;
            sc.tangentImpulse2 = it->tangentImpulse2;
        }
        m_contacts.push_back(sc);
    }

    // Warm-start
    for (auto& c : m_contacts) {
        if (c.normalImpulse == 0.0f && c.tangentImpulse1 == 0.0f && c.tangentImpulse2 == 0.0f) continue;
        auto& a = m_entities[c.idxA]; auto& b = m_entities[c.idxB];
        math::Vector3D P = c.normal * c.normalImpulse + c.tangent1 * c.tangentImpulse1 + c.tangent2 * c.tangentImpulse2;
        if (!a.p->isStatic) a.p->velocity -= P * a.p->invMass;
        if (!b.p->isStatic) b.p->velocity += P * b.p->invMass;
    }

    // Iterative solve
    for (int iter = 0; iter < m_solverIterations; iter++) {
        for (auto& c : m_contacts) {
            auto& a = m_entities[c.idxA]; auto& b = m_entities[c.idxB];
            math::Vector3D relVel = b.p->velocity - a.p->velocity;
            float vn = relVel.dot(c.normal);
            float lambda = c.normalMass * (-(vn + c.bias));
            float newImpulse = std::max(c.normalImpulse + lambda, 0.0f);
            lambda = newImpulse - c.normalImpulse;
            c.normalImpulse = newImpulse;
            math::Vector3D P = c.normal * lambda;
            if (!a.p->isStatic) a.p->velocity -= P * a.p->invMass;
            if (!b.p->isStatic) b.p->velocity += P * b.p->invMass;
            relVel = b.p->velocity - a.p->velocity;
            float vt1 = relVel.dot(c.tangent1), vt2 = relVel.dot(c.tangent2);
            float lt1 = c.tangentMass1 * (-vt1), lt2 = c.tangentMass2 * (-vt2);
            float newT1 = c.tangentImpulse1 + lt1, newT2 = c.tangentImpulse2 + lt2;
            float maxF = c.friction * c.normalImpulse;
            float magSq = newT1 * newT1 + newT2 * newT2;
            if (magSq > maxF * maxF && magSq > 1e-16f) { float s = maxF / std::sqrt(magSq); newT1 *= s; newT2 *= s; }
            lt1 = newT1 - c.tangentImpulse1; lt2 = newT2 - c.tangentImpulse2;
            c.tangentImpulse1 = newT1; c.tangentImpulse2 = newT2;
            math::Vector3D fP = c.tangent1 * lt1 + c.tangent2 * lt2;
            if (!a.p->isStatic) a.p->velocity -= fP * a.p->invMass;
            if (!b.p->isStatic) b.p->velocity += fP * b.p->invMass;
        }
    }

    // Position correction
    for (auto& c : m_contacts) {
        if (c.penetration <= 0.005f) continue;
        auto& a = m_entities[c.idxA]; auto& b = m_entities[c.idxB];
        float totalInvMass = a.p->invMass + b.p->invMass;
        if (totalInvMass < 1e-8f) continue;
        float correction = 0.4f * (c.penetration - 0.005f) / totalInvMass;
        math::Vector3D corr = c.normal * correction;
        if (!a.p->isStatic) { a.t->transform.position -= corr * a.p->invMass; a.t->dirty = true; }
        if (!b.p->isStatic) { b.t->transform.position += corr * b.p->invMass; b.t->dirty = true; }
    }
}

void Physics3DSystem::solveConstraints(float dt) {
    if (dt <= 0.0f) return;
    float invDt = 1.0f / dt;
    m_ecs.forEach<Transform3DComponent, Physics3DComponent, ConstraintComponent>(
        [this, dt, invDt](Entity e, Transform3DComponent& tA, Physics3DComponent& pA, ConstraintComponent& con) {
            if (!con.enabled) return;
            if (!m_ecs.isAlive(con.targetEntity)) return;
            if (!m_ecs.hasComponent<Transform3DComponent>(con.targetEntity)) return;
            if (!m_ecs.hasComponent<Physics3DComponent>(con.targetEntity)) return;
            auto& tB = m_ecs.getComponent<Transform3DComponent>(con.targetEntity);
            auto& pB = m_ecs.getComponent<Physics3DComponent>(con.targetEntity);
            pA.wake(); pB.wake();
            math::Vector3D wA = tA.transform.position + tA.transform.rotation.rotateVector(con.localAnchorA);
            math::Vector3D wB = tB.transform.position + tB.transform.rotation.rotateVector(con.localAnchorB);
            for (int iter = 0; iter < m_solverIterations; iter++) {
                if (con.type == ConstraintComponent::DISTANCE)
                    solveDistance(tA, pA, tB, pB, con, wA, wB, invDt);
                else if (con.type == ConstraintComponent::BALL_SOCKET)
                    solveBallSocket(tA, pA, tB, pB, con, wA, wB, invDt);
                else if (con.type == ConstraintComponent::HINGE) {
                    solveBallSocket(tA, pA, tB, pB, con, wA, wB, invDt);
                    solveHingeAngular(tA, pA, tB, pB, con);
                }
            }
        }
    );
}

void Physics3DSystem::solveDistance(Transform3DComponent& tA, Physics3DComponent& pA,
    Transform3DComponent& tB, Physics3DComponent& pB,
    ConstraintComponent& con, const math::Vector3D& wA, const math::Vector3D& wB, float invDt)
{
    math::Vector3D delta = wB - wA;
    float dist = delta.magnitude();
    if (dist < 1e-6f) return;
    math::Vector3D n = delta * (1.0f / dist);
    float error = dist - con.restLength;
    float totalInvMass = pA.invMass + pB.invMass;
    if (totalInvMass < 1e-8f) return;
    float effMass = 1.0f / totalInvMass;
    float bias = -0.2f * invDt * error * con.stiffness;
    float vn = (pB.velocity - pA.velocity).dot(n);
    float lambda = effMass * (-(vn + bias));
    con.accImpulse[0] += lambda;
    math::Vector3D P = n * lambda;
    if (!pA.isStatic) pA.velocity -= P * pA.invMass;
    if (!pB.isStatic) pB.velocity += P * pB.invMass;
}

void Physics3DSystem::solveBallSocket(Transform3DComponent& tA, Physics3DComponent& pA,
    Transform3DComponent& tB, Physics3DComponent& pB,
    ConstraintComponent& con, const math::Vector3D& wA, const math::Vector3D& wB, float invDt)
{
    math::Vector3D error = wB - wA;
    float totalInvMass = pA.invMass + pB.invMass;
    if (totalInvMass < 1e-8f) return;
    float effMass = 1.0f / totalInvMass;
    const math::Vector3D axes[3] = {{1,0,0}, {0,1,0}, {0,0,1}};
    for (int i = 0; i < 3; i++) {
        float errAxis = (i==0) ? error.x : (i==1) ? error.y : error.z;
        float bias = -0.3f * invDt * errAxis;
        float vn = (pB.velocity - pA.velocity).dot(axes[i]);
        float lambda = effMass * (-(vn + bias));
        con.accImpulse[i] += lambda;
        math::Vector3D P = axes[i] * lambda;
        if (!pA.isStatic) pA.velocity -= P * pA.invMass;
        if (!pB.isStatic) pB.velocity += P * pB.invMass;
    }
}

void Physics3DSystem::solveHingeAngular(Transform3DComponent& tA, Physics3DComponent& pA,
    Transform3DComponent& tB, Physics3DComponent& pB, ConstraintComponent& con)
{
    math::Vector3D worldAxis = tA.transform.rotation.rotateVector(con.hingeAxis).normalized();
    math::Vector3D u, v;
    if (std::abs(worldAxis.x) >= 0.57735f)
        u = math::Vector3D(worldAxis.y, -worldAxis.x, 0).normalized();
    else
        u = math::Vector3D(0, worldAxis.z, -worldAxis.y).normalized();
    v = worldAxis.cross(u);
    math::Vector3D relW = pB.angularVelocity - pA.angularVelocity;
    float totalInvMass = pA.invMass + pB.invMass;
    if (totalInvMass < 1e-8f) return;
    float angEffMass = 1.0f / (totalInvMass * 2.5f);
    for (int i = 0; i < 2; i++) {
        math::Vector3D axis = (i == 0) ? u : v;
        float wn = relW.dot(axis);
        float lambda = angEffMass * (-wn);
        math::Vector3D angP = axis * lambda;
        if (!pA.isStatic) pA.angularVelocity -= angP * pA.invMass * 2.5f;
        if (!pB.isStatic) pB.angularVelocity += angP * pB.invMass * 2.5f;
    }
}

void Physics3DSystem::updateSleep(float dt) {
    for (auto& ent : m_entities) {
        auto& p = *ent.p;
        if (p.isStatic) continue;
        float energy = p.velocity.sqrMagnitude() + p.angularVelocity.sqrMagnitude();
        p.filteredEnergy = p.filteredEnergy * 0.8f + energy * 0.2f;
        if (p.filteredEnergy < m_sleepThreshold) {
            p.sleepTimer += dt;
            if (p.sleepTimer > m_sleepTime) {
                p.sleeping = true;
                p.velocity = math::Vector3D::Zero;
                p.angularVelocity = math::Vector3D::Zero;
            }
        } else {
            p.sleepTimer = 0.0f;
            p.sleeping = false;
        }
    }
}

void Physics3DSystem::applyBounds() {
    for (auto& ent : m_entities) {
        auto& p = *ent.p; auto& t = *ent.t; auto& c = *ent.c;
        if (p.isStatic || p.sleeping) continue;
        float r;
        if (c.shape == Collider3DComponent::SPHERE) r = c.radius;
        else if (c.shape == Collider3DComponent::CAPSULE) r = c.capsuleHeight * 0.5f + c.radius;
        else r = std::max({c.halfExtents.x, c.halfExtents.y, c.halfExtents.z})
                 * std::max({t.transform.scale.x, t.transform.scale.y, t.transform.scale.z});

        if (m_hasFloor && t.transform.position.y - r < m_floorY) {
            t.transform.position.y = m_floorY + r;
            if (std::abs(p.velocity.y) > 0.15f) p.velocity.y = -p.velocity.y * p.restitution;
            else p.velocity.y = 0;
            float ff = 1.0f - p.friction * 0.3f;
            p.velocity.x *= ff; p.velocity.z *= ff;
            t.dirty = true;
        }
        if (m_hasBounds) {
            auto bounce = [](float& pos, float& vel, float limit, float rad, float rest, bool lower) {
                if (lower && pos - rad < limit) { pos = limit + rad; vel = std::abs(vel) * rest; }
                else if (!lower && pos + rad > limit) { pos = limit - rad; vel = -std::abs(vel) * rest; }
            };
            bounce(t.transform.position.x, p.velocity.x, m_boundsMin.x, r, p.restitution, true);
            bounce(t.transform.position.x, p.velocity.x, m_boundsMax.x, r, p.restitution, false);
            bounce(t.transform.position.z, p.velocity.z, m_boundsMin.z, r, p.restitution, true);
            bounce(t.transform.position.z, p.velocity.z, m_boundsMax.z, r, p.restitution, false);
            t.dirty = true;
        }
    }
}

} // namespace ecs
} // namespace engine
