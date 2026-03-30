#pragma once

#include "math/Quaternion.h"
#include "math/Matrix4x4.h"
#include <vector>
#include <string>
#include <cstdint>

namespace engine {
namespace renderer {

struct Joint {
    std::string name;
    int32_t parentIndex = -1;
    math::Vector3D   bindPosition;
    math::Quaternion bindRotation;
    math::Vector3D   bindScale = math::Vector3D(1, 1, 1);
    math::Matrix4x4  inverseBindMatrix = math::Matrix4x4::identity();
};

class Skeleton {
public:
    static constexpr int MAX_JOINTS = 128;

    int addJoint(const std::string& name, int parentIndex,
                 const math::Vector3D& position   = math::Vector3D::Zero,
                 const math::Quaternion& rotation  = math::Quaternion{},
                 const math::Vector3D& scale       = math::Vector3D(1, 1, 1));
    void build();
    int  getJointCount() const { return m_jointCount; }
    const Joint& getJoint(int index) const { return m_joints[index]; }
    int  findJoint(const std::string& name) const;
    const Joint* joints() const { return m_joints; }

private:
    math::Matrix4x4 computeLocalMatrix(int index) const;
    Joint m_joints[MAX_JOINTS];
    int   m_jointCount = 0;
};

struct Keyframe {
    float time = 0;
    math::Vector3D   position;
    math::Quaternion rotation;
    math::Vector3D   scale = math::Vector3D(1, 1, 1);
};

struct AnimChannel {
    int jointIndex = -1;
    std::vector<Keyframe> keyframes;
    Keyframe sample(float time) const;
};

struct AnimClip3D {
    std::string name;
    float duration = 0;
    bool  loop     = true;
    std::vector<AnimChannel> channels;
};

class AnimPlayer3D {
public:
    explicit AnimPlayer3D(const Skeleton& skeleton) : m_skeleton(skeleton) {}

    void  play(const AnimClip3D& clip, float blendTime = 0.2f);
    void  stop() { m_playing = false; }
    void  setSpeed(float speed) { m_speed = speed; }
    bool  isPlaying() const { return m_playing; }
    float getTime() const { return m_time; }
    void  update(float dt);

    const float* getSkinningMatrices() const {
        return reinterpret_cast<const float*>(m_skinMatrices);
    }
    int getMatrixCount() const { return m_skeleton.getJointCount(); }

private:
    void computeSkinning();
    math::Matrix4x4 getJointLocalMatrix(int jointIdx) const;
    math::Matrix4x4 getJointLocalMatrixFromClip(const AnimClip3D* clip, int jointIdx, float time) const;

    const Skeleton&   m_skeleton;
    const AnimClip3D* m_currentClip = nullptr;
    const AnimClip3D* m_prevClip    = nullptr;
    float m_time      = 0;
    float m_speed     = 1.0f;
    float m_blendTime = 0.2f;
    float m_blendTimer = 0;
    bool  m_playing   = false;
    math::Matrix4x4 m_skinMatrices[Skeleton::MAX_JOINTS];
};

} // namespace renderer
} // namespace engine
