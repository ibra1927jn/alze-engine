/// Tests for SkeletalAnimation — Skeleton, AnimChannel, AnimPlayer3D
#include "renderer/SkeletalAnimation.h"
#include <iostream>
#include <cmath>

using namespace engine::renderer;
using namespace engine::math;

int passed = 0;
int failed = 0;

#define TEST(name, expr) \
    if (expr) { passed++; std::cout << "  [OK] " << name << std::endl; } \
    else { failed++; std::cerr << "  [FAIL] " << name << std::endl; }

#define APPROX(a, b) (std::abs((a) - (b)) < 0.01f)

// ═══════════════════════════════════════════════════════════════
// Skeleton
// ═══════════════════════════════════════════════════════════════
void testSkeleton() {
    std::cout << "\n=== Skeleton ===" << std::endl;

    Skeleton skel;
    TEST("Initial joint count is 0", skel.getJointCount() == 0);

    int root = skel.addJoint("root", -1, Vector3D(0, 0, 0));
    TEST("Root joint index is 0", root == 0);
    TEST("Joint count is 1", skel.getJointCount() == 1);

    int child = skel.addJoint("child", 0, Vector3D(0, 2, 0));
    TEST("Child joint index is 1", child == 1);
    TEST("Joint count is 2", skel.getJointCount() == 2);

    int grandchild = skel.addJoint("grandchild", 1, Vector3D(1, 0, 0));
    TEST("Grandchild joint index is 2", grandchild == 2);

    // Find joint by name
    TEST("findJoint root", skel.findJoint("root") == 0);
    TEST("findJoint child", skel.findJoint("child") == 1);
    TEST("findJoint grandchild", skel.findJoint("grandchild") == 2);
    TEST("findJoint nonexistent returns -1", skel.findJoint("nope") == -1);

    // Joint data
    const Joint& j = skel.getJoint(0);
    TEST("Root joint name", j.name == "root");
    TEST("Root joint parentIndex is -1", j.parentIndex == -1);

    const Joint& jc = skel.getJoint(1);
    TEST("Child joint parentIndex is 0", jc.parentIndex == 0);
    TEST("Child bind position Y", APPROX(jc.bindPosition.y, 2.0f));

    // Build computes inverse bind matrices
    skel.build();
    // After build, inverseBindMatrix should be valid (non-identity for child)
    const Joint& builtChild = skel.getJoint(1);
    // The inverse bind matrix of the child (at Y=2) should translate by -2 in Y
    TEST("Child inverse bind matrix is not identity",
         !APPROX(builtChild.inverseBindMatrix.get(1, 3), 0.0f));

    // Max joints overflow
    Skeleton full;
    for (int i = 0; i < Skeleton::MAX_JOINTS; i++) {
        full.addJoint("j" + std::to_string(i), i > 0 ? i - 1 : -1);
    }
    TEST("Full skeleton has MAX_JOINTS", full.getJointCount() == Skeleton::MAX_JOINTS);
    int overflow = full.addJoint("overflow", 0);
    TEST("Overflow returns -1", overflow == -1);
    TEST("Count unchanged after overflow", full.getJointCount() == Skeleton::MAX_JOINTS);
}

// ═══════════════════════════════════════════════════════════════
// AnimChannel::sample
// ═══════════════════════════════════════════════════════════════
void testAnimChannel() {
    std::cout << "\n=== AnimChannel ===" << std::endl;

    // Empty channel
    AnimChannel empty;
    empty.jointIndex = 0;
    Keyframe kf = empty.sample(0.5f);
    TEST("Empty channel returns default keyframe", APPROX(kf.time, 0.0f));
    TEST("Empty channel returns zero position", APPROX(kf.position.x, 0.0f));

    // Single keyframe
    AnimChannel single;
    single.jointIndex = 0;
    Keyframe k0;
    k0.time = 0.0f;
    k0.position = Vector3D(1, 2, 3);
    single.keyframes.push_back(k0);

    kf = single.sample(0.0f);
    TEST("Single keyframe at t=0", APPROX(kf.position.x, 1.0f));
    kf = single.sample(5.0f);
    TEST("Single keyframe at t=5 (clamped)", APPROX(kf.position.x, 1.0f));

    // Two keyframes — interpolation
    AnimChannel two;
    two.jointIndex = 0;
    Keyframe ka, kb;
    ka.time = 0.0f;
    ka.position = Vector3D(0, 0, 0);
    ka.scale = Vector3D(1, 1, 1);
    kb.time = 1.0f;
    kb.position = Vector3D(10, 0, 0);
    kb.scale = Vector3D(2, 2, 2);
    two.keyframes.push_back(ka);
    two.keyframes.push_back(kb);

    kf = two.sample(0.0f);
    TEST("Interpolation at t=0", APPROX(kf.position.x, 0.0f));

    kf = two.sample(0.5f);
    TEST("Interpolation at t=0.5 position", APPROX(kf.position.x, 5.0f));
    TEST("Interpolation at t=0.5 scale", APPROX(kf.scale.x, 1.5f));

    kf = two.sample(1.0f);
    TEST("Interpolation at t=1.0 (clamped to last)", APPROX(kf.position.x, 10.0f));

    kf = two.sample(-0.5f);
    TEST("Interpolation at t<0 (clamped to first)", APPROX(kf.position.x, 0.0f));

    kf = two.sample(2.0f);
    TEST("Interpolation at t>1 (clamped to last)", APPROX(kf.position.x, 10.0f));

    // Three keyframes
    AnimChannel three;
    three.jointIndex = 0;
    Keyframe kc;
    kc.time = 2.0f;
    kc.position = Vector3D(20, 0, 0);
    kc.scale = Vector3D(1, 1, 1);
    three.keyframes.push_back(ka);
    three.keyframes.push_back(kb);
    three.keyframes.push_back(kc);

    kf = three.sample(0.5f);
    TEST("Three keyframes at t=0.5", APPROX(kf.position.x, 5.0f));

    kf = three.sample(1.5f);
    TEST("Three keyframes at t=1.5", APPROX(kf.position.x, 15.0f));
}

// ═══════════════════════════════════════════════════════════════
// AnimPlayer3D
// ═══════════════════════════════════════════════════════════════
void testAnimPlayer() {
    std::cout << "\n=== AnimPlayer3D ===" << std::endl;

    // Setup a simple skeleton: root -> child (Y+2)
    Skeleton skel;
    skel.addJoint("root", -1, Vector3D(0, 0, 0));
    skel.addJoint("child", 0, Vector3D(0, 2, 0));
    skel.build();

    AnimPlayer3D player(skel);
    TEST("Not playing initially", !player.isPlaying());
    TEST("Time is 0 initially", APPROX(player.getTime(), 0.0f));

    // Create a simple animation: move root from (0,0,0) to (5,0,0) over 1 second
    AnimClip3D clip;
    clip.name = "walk";
    clip.duration = 1.0f;
    clip.loop = false;

    AnimChannel ch;
    ch.jointIndex = 0;
    Keyframe k0, k1;
    k0.time = 0.0f;
    k0.position = Vector3D(0, 0, 0);
    k1.time = 1.0f;
    k1.position = Vector3D(5, 0, 0);
    ch.keyframes.push_back(k0);
    ch.keyframes.push_back(k1);
    clip.channels.push_back(ch);

    player.play(clip, 0.0f); // No blend
    TEST("Playing after play()", player.isPlaying());

    player.update(0.5f);
    TEST("Time after 0.5s update", APPROX(player.getTime(), 0.5f));
    TEST("Still playing at t=0.5", player.isPlaying());

    // Skinning matrices should be valid
    TEST("Matrix count equals joint count", player.getMatrixCount() == 2);
    const float* matrices = player.getSkinningMatrices();
    TEST("Skinning matrices not null", matrices != nullptr);

    // Update past duration (non-looping)
    player.update(0.6f); // total 1.1s > 1.0s duration
    TEST("Stopped after exceeding duration", !player.isPlaying());
    TEST("Time clamped to duration", APPROX(player.getTime(), 1.0f));

    // Looping clip
    AnimClip3D loopClip;
    loopClip.name = "idle";
    loopClip.duration = 1.0f;
    loopClip.loop = true;
    loopClip.channels.push_back(ch);

    player.play(loopClip, 0.0f);
    player.update(1.5f); // Should wrap around
    TEST("Looping: still playing after wrap", player.isPlaying());
    TEST("Looping: time wrapped", player.getTime() < 1.0f);

    // Stop
    player.stop();
    TEST("Stopped after stop()", !player.isPlaying());

    // Speed
    player.play(clip, 0.0f);
    player.setSpeed(2.0f);
    player.update(0.25f); // effective time = 0.5s
    TEST("Speed 2x: time is 0.5", APPROX(player.getTime(), 0.5f));
}

// ═══════════════════════════════════════════════════════════════
// AnimClip3D
// ═══════════════════════════════════════════════════════════════
void testAnimClip() {
    std::cout << "\n=== AnimClip3D ===" << std::endl;

    AnimClip3D clip;
    TEST("Default name is empty", clip.name.empty());
    TEST("Default duration is 0", APPROX(clip.duration, 0.0f));
    TEST("Default loop is true", clip.loop);
    TEST("Default channels empty", clip.channels.empty());
}

int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "  Test: SkeletalAnimation" << std::endl;
    std::cout << "============================================" << std::endl;

    testSkeleton();
    testAnimChannel();
    testAnimPlayer();
    testAnimClip();

    std::cout << "\n============================================" << std::endl;
    std::cout << "  Resultados: " << passed << " pasados, " << failed << " fallidos" << std::endl;
    std::cout << "============================================" << std::endl;

    return failed > 0 ? 1 : 0;
}
