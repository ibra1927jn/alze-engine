#include "PhysicsSoA.h"
#include "RigidBody3D.h"

namespace engine {
namespace physics {

void PhysicsSoA::scatter(RigidBody3D* bodies, int totalBodies) {
    // First pass: count dynamic, non-sleeping, non-removed bodies
    int n = 0;
    for (int i = 0; i < totalBodies; i++) {
        if (bodies[i].m_removed) continue;
        if (!bodies[i].isDynamic()) continue;
        if (bodies[i].isSleeping()) continue;
        n++;
    }
    resize(n);

    // Second pass: scatter data
    int idx = 0;
    for (int i = 0; i < totalBodies; i++) {
        if (bodies[i].m_removed) continue;
        if (!bodies[i].isDynamic()) continue;
        if (bodies[i].isSleeping()) continue;

        bodyIndex[idx] = i;
        px[idx] = bodies[i].position.x;
        py[idx] = bodies[i].position.y;
        pz[idx] = bodies[i].position.z;
        vx[idx] = bodies[i].velocity.x;
        vy[idx] = bodies[i].velocity.y;
        vz[idx] = bodies[i].velocity.z;
        wx[idx] = bodies[i].angularVelocity.x;
        wy[idx] = bodies[i].angularVelocity.y;
        wz[idx] = bodies[i].angularVelocity.z;
        invMass[idx] = bodies[i].getInvMass();
        fx[idx] = 0.0f;
        fy[idx] = 0.0f;
        fz[idx] = 0.0f;
        idx++;
    }
}

void PhysicsSoA::gather(RigidBody3D* bodies) const {
    for (int i = 0; i < count; i++) {
        int bi = bodyIndex[i];
        bodies[bi].position.x = px[i];
        bodies[bi].position.y = py[i];
        bodies[bi].position.z = pz[i];
        bodies[bi].velocity.x = vx[i];
        bodies[bi].velocity.y = vy[i];
        bodies[bi].velocity.z = vz[i];
        bodies[bi].angularVelocity.x = wx[i];
        bodies[bi].angularVelocity.y = wy[i];
        bodies[bi].angularVelocity.z = wz[i];
    }
}

} // namespace physics
} // namespace engine
