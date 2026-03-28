#include "SkeletalAnimation.h"
#include <algorithm>
#include <cmath>

namespace engine {
namespace renderer {

// â”€â”€ Skeleton â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

int Skeleton::addJoint(const std::string& name, int parentIndex,
                       const math::Vector3D& position,
                       const math::Quaternion& rotation,
                       const math::Vector3D& scale)
{
    if (m_jointCount >= MAX_JOINTS) return -1;
    int idx = m_jointCount++;
    m_joints[idx].name        = name;
    m_joints[idx].parentIndex = parentIndex;
    m_joints[idx].bindPosition = position;
    m_joints[idx].bindRotation = rotation;
    m_joints[idx].bindScale    = scale;
    return idx;
}

void Skeleton::build() {
    math::Matrix4x4 worldMatrices[MAX_JOINTS];
    for (int i = 0; i < m_jointCount; i++) {
        math::Matrix4x4 local = computeLocalMatrix(i);
        if (m_joints[i].parentIndex >= 0)
            worldMatrices[i] = worldMatrices[m_joints[i].parentIndex] * local;
        else
            worldMatrices[i] = local;
        m_joints[i].inverseBindMatrix = worldMatrices[i].inverse();
    }
}

int Skeleton::findJoint(const std::string& name) const {
    for (int i = 0; i < m_jointCount; i++) {
        if (m_joints[i].name == name) return i;
    }
    return -1;
}

math::Matrix4x4 Skeleton::computeLocalMatrix(int index) const {
    const auto& j = m_joints[index];
    return math::Matrix4x4::translation(j.bindPosition.x, j.bindPosition.y, j.bindPosition.z) *
           j.bindRotation.toMatrix() *
           math::Matrix4x4::scale(j.bindScale.x, j.bindScale.y, j.bindScale.z);
}

// â”€â”€ AnimChannel â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

Keyframe AnimChannel::sample(float time) const {
    if (keyframes.empty()) return {};
    if (keyframes.size() == 1 || time <= keyframes[0].time) return keyframes[0];
    if (time >= keyframes.back().time) return keyframes.back();

    for (size_t i = 0; i < keyframes.size() - 1; i++) {
        if (time >= keyframes[i].time && time < keyframes[i + 1].time) {
            float t = (time - keyframes[i].time) /
                      (keyframes[i + 1].time - keyframes[i].time);
            Keyframe result;
            result.time     = time;
            result.position = math::Vector3D::lerp(keyframes[i].position, keyframes[i + 1].position, t);
            result.rotation = math::Quaternion::slerp(keyframes[i].rotation, keyframes[i + 1].rotation, t);
            result.scale    = math::Vector3D::lerp(keyframes[i].scale, keyframes[i + 1].scale, t);
            return result;
        }
    }
    return keyframes.back();
}

// â”€â”€ AnimPlayer3D â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€

void AnimPlayer3D::play(const AnimClip3D& clip, float blendTime) {
    m_prevClip    = m_currentClip;
    m_currentClip = &clip;
    m_time        = 0;
    m_blendTime   = blendTime;
    m_blendTimer  = blendTime;
    m_playing     = true;
}

void AnimPlayer3D::update(float dt) {
    if (!m_playing || !m_currentClip) return;

    m_time += dt * m_speed;

    if (m_currentClip->loop) {
        if (m_time >= m_currentClip->duration)
            m_time = std::fmod(m_time, m_currentClip->duration);
    } else {
        if (m_time >= m_currentClip->duration) {
            m_time    = m_currentClip->duration;
            m_playing = false;
        }
    }

    if (m_blendTimer > 0) {
        m_blendTimer -= dt;
        if (m_blendTimer <= 0) { m_blendTimer = 0; m_prevClip = nullptr; }
    }

    computeSkinning();
}

void AnimPlayer3D::computeSkinning() {
    int jointCount = m_skeleton.getJointCount();
    math::Matrix4x4 localMatrices[Skeleton::MAX_JOINTS];
    math::Matrix4x4 worldMatrices[Skeleton::MAX_JOINTS];

    for (int i = 0; i < jointCount; i++)
        localMatrices[i] = getJointLocalMatrix(i);

    if (m_prevClip && m_blendTimer > 0 && m_blendTime > 0) {
        float blendT = 1.0f - (m_blendTimer / m_blendTime);
        for (int i = 0; i < jointCount; i++) {
            math::Matrix4x4 prevLocal = getJointLocalMatrixFromClip(m_prevClip, i, m_time);
            for (int c = 0; c < 4; c++) {
                float prev[4], curr[4];
                prev[0]=prevLocal.get(0,c); prev[1]=prevLocal.get(1,c);
                prev[2]=prevLocal.get(2,c); prev[3]=prevLocal.get(3,c);
                curr[0]=localMatrices[i].get(0,c); curr[1]=localMatrices[i].get(1,c);
                curr[2]=localMatrices[i].get(2,c); curr[3]=localMatrices[i].get(3,c);
                localMatrices[i].set(0,c, prev[0]+(curr[0]-prev[0])*blendT);
                localMatrices[i].set(1,c, prev[1]+(curr[1]-prev[1])*blendT);
                localMatrices[i].set(2,c, prev[2]+(curr[2]-prev[2])*blendT);
                localMatrices[i].set(3,c, prev[3]+(curr[3]-prev[3])*blendT);
            }
        }
    }

    for (int i = 0; i < jointCount; i++) {
        int parent = m_skeleton.getJoint(i).parentIndex;
        if (parent >= 0)
            worldMatrices[i] = worldMatrices[parent] * localMatrices[i];
        else
            worldMatrices[i] = localMatrices[i];
        m_skinMatrices[i] = worldMatrices[i] * m_skeleton.getJoint(i).inverseBindMatrix;
    }
}

math::Matrix4x4 AnimPlayer3D::getJointLocalMatrix(int jointIdx) const {
    if (!m_currentClip) {
        const auto& j = m_skeleton.getJoint(jointIdx);
        return math::Matrix4x4::translation(j.bindPosition.x, j.bindPosition.y, j.bindPosition.z) *
               j.bindRotation.toMatrix() *
               math::Matrix4x4::scale(j.bindScale.x, j.bindScale.y, j.bindScale.z);
    }
    return getJointLocalMatrixFromClip(m_currentClip, jointIdx, m_time);
}

math::Matrix4x4 AnimPlayer3D::getJointLocalMatrixFromClip(const AnimClip3D* clip,
                                                            int jointIdx, float time) const
{
    for (const auto& ch : clip->channels) {
        if (ch.jointIndex == jointIdx) {
            Keyframe kf = ch.sample(time);
            return math::Matrix4x4::translation(kf.position.x, kf.position.y, kf.position.z) *
                   kf.rotation.toMatrix() *
                   math::Matrix4x4::scale(kf.scale.x, kf.scale.y, kf.scale.z);
        }
    }
    const auto& j = m_skeleton.getJoint(jointIdx);
    return math::Matrix4x4::translation(j.bindPosition.x, j.bindPosition.y, j.bindPosition.z) *
           j.bindRotation.toMatrix() *
           math::Matrix4x4::scale(j.bindScale.x, j.bindScale.y, j.bindScale.z);
}

} // namespace renderer
} // namespace engine
