#include "Color.h"
#include "Transform2D.h"
#include "MathConstants.h"
#include <iostream>
#include <cmath>

using namespace engine::math;

int passed = 0;
int failed = 0;

#define TEST(name, expr) \
    if (expr) { passed++; std::cout << "  [OK] " << name << std::endl; } \
    else { failed++; std::cerr << "  [FAIL] " << name << std::endl; }

#define APPROX(a, b) (std::abs((a) - (b)) < 0.001f)

// ═══════════════════════════════════════════════════════════════
// Color
// ═══════════════════════════════════════════════════════════════
void testColor() {
    std::cout << "\n=== Color ===" << std::endl;

    // Default constructor = white opaque
    Color c;
    TEST("Default is white", c.r == 255 && c.g == 255 && c.b == 255 && c.a == 255);

    // RGBA constructor
    Color c2(10, 20, 30, 40);
    TEST("RGBA ctor", c2.r == 10 && c2.g == 20 && c2.b == 30 && c2.a == 40);

    // Alpha defaults to 255
    Color c3(1, 2, 3);
    TEST("Alpha default 255", c3.a == 255);

    // Predefined colors
    TEST("black", Color::black().r == 0 && Color::black().g == 0 && Color::black().b == 0);
    TEST("transparent alpha=0", Color::transparent().a == 0);
    TEST("red channel", Color::red().r == 255);

    // withAlpha
    Color ca = Color::white().withAlpha(128);
    TEST("withAlpha", ca.r == 255 && ca.a == 128);

    // brighter clamps at 255
    Color bright = Color(200, 200, 200).brighter(100);
    TEST("brighter clamp", bright.r == 255 && bright.g == 255 && bright.b == 255);

    // brighter normal
    Color bright2 = Color(100, 50, 0).brighter(10);
    TEST("brighter normal", bright2.r == 110 && bright2.g == 60 && bright2.b == 10);

    // darker clamps at 0
    Color dark = Color(10, 5, 0).darker(20);
    TEST("darker clamp", dark.r == 0 && dark.g == 0 && dark.b == 0);

    // darker preserves alpha
    Color dark2 = Color(100, 100, 100, 128).darker(10);
    TEST("darker keeps alpha", dark2.a == 128);

    // Equality
    TEST("equality", Color(1,2,3,4) == Color(1,2,3,4));
    TEST("inequality", Color(1,2,3,4) != Color(4,3,2,1));
}

// ═══════════════════════════════════════════════════════════════
// Transform2D
// ═══════════════════════════════════════════════════════════════
void testTransform2D() {
    std::cout << "\n=== Transform2D ===" << std::endl;

    // Default
    Transform2D t;
    TEST("Default pos zero", t.position.x == 0.0f && t.position.y == 0.0f);
    TEST("Default rot zero", t.rotation == 0.0f);
    TEST("Default scale one", t.scale.x == 1.0f && t.scale.y == 1.0f);

    // Position-only ctor
    Transform2D t2(Vector2D(5, 10));
    TEST("Pos ctor", t2.position.x == 5.0f && t2.position.y == 10.0f);
    TEST("Pos ctor rot=0", t2.rotation == 0.0f);

    // Pos+rot ctor
    Transform2D t3(Vector2D(1, 2), 1.5f);
    TEST("Pos+rot ctor", APPROX(t3.rotation, 1.5f));

    // Full TRS ctor
    Transform2D t4(Vector2D(3, 4), 0.0f, Vector2D(2, 3));
    TEST("TRS ctor scale", t4.scale.x == 2.0f && t4.scale.y == 3.0f);

    // Forward at rotation=0 is (1,0)
    Transform2D tf;
    Vector2D fwd = tf.forward();
    TEST("Forward at 0 rad", APPROX(fwd.x, 1.0f) && APPROX(fwd.y, 0.0f));

    // Forward at PI/2 is (0,1)
    Transform2D tf2(Vector2D::Zero, Constants::HALF_PI);
    Vector2D fwd2 = tf2.forward();
    TEST("Forward at PI/2", APPROX(fwd2.x, 0.0f) && APPROX(fwd2.y, 1.0f));

    // Right is perpendicular to forward
    Vector2D r = tf.right();
    float dot = fwd.x * r.x + fwd.y * r.y;
    TEST("Right perp to forward", APPROX(dot, 0.0f));

    // localToWorld with identity transform = same point
    Transform2D ti;
    Vector2D world = ti.localToWorld(Vector2D(5, 3));
    TEST("Identity localToWorld", APPROX(world.x, 5.0f) && APPROX(world.y, 3.0f));

    // localToWorld with translation
    Transform2D tt(Vector2D(10, 20));
    Vector2D w2 = tt.localToWorld(Vector2D(1, 1));
    TEST("Translated localToWorld", APPROX(w2.x, 11.0f) && APPROX(w2.y, 21.0f));

    // localToWorld with scale
    Transform2D ts(Vector2D(0, 0), 0.0f, Vector2D(2, 3));
    Vector2D w3 = ts.localToWorld(Vector2D(5, 4));
    TEST("Scaled localToWorld", APPROX(w3.x, 10.0f) && APPROX(w3.y, 12.0f));

    // toMatrix produces valid matrix
    Transform2D tm(Vector2D(10, 20), 0.0f, Vector2D(1, 1));
    Matrix3x3 m = tm.toMatrix();
    Vector2D origin = m.transformPoint(Vector2D::Zero);
    TEST("toMatrix translation", APPROX(origin.x, 10.0f) && APPROX(origin.y, 20.0f));
}

// ═══════════════════════════════════════════════════════════════
// MathConstants + utility functions
// ═══════════════════════════════════════════════════════════════
void testMathConstants() {
    std::cout << "\n=== MathConstants ===" << std::endl;

    // Constants values
    TEST("PI", APPROX(Constants::PI, 3.14159f));
    TEST("TAU = 2*PI", APPROX(Constants::TAU, 2.0f * Constants::PI));
    TEST("HALF_PI", APPROX(Constants::HALF_PI, Constants::PI / 2.0f));
    TEST("DEG2RAD", APPROX(180.0f * Constants::DEG2RAD, Constants::PI));
    TEST("RAD2DEG", APPROX(Constants::PI * Constants::RAD2DEG, 180.0f));
    TEST("EPSILON > 0", Constants::EPSILON > 0.0f && Constants::EPSILON < 0.01f);
    TEST("SQRT2", APPROX(Constants::SQRT2, std::sqrt(2.0f)));
    TEST("INV_SQRT2", APPROX(Constants::SQRT2 * Constants::INV_SQRT2, 1.0f));

    // smoothstep
    TEST("smoothstep(0,1,0) = 0", APPROX(smoothstep(0, 1, 0), 0.0f));
    TEST("smoothstep(0,1,1) = 1", APPROX(smoothstep(0, 1, 1), 1.0f));
    TEST("smoothstep(0,1,0.5) = 0.5", APPROX(smoothstep(0, 1, 0.5f), 0.5f));
    TEST("smoothstep clamp below", APPROX(smoothstep(0, 1, -1), 0.0f));
    TEST("smoothstep clamp above", APPROX(smoothstep(0, 1, 2), 1.0f));

    // smootherstep
    TEST("smootherstep(0,1,0) = 0", APPROX(smootherstep(0, 1, 0), 0.0f));
    TEST("smootherstep(0,1,1) = 1", APPROX(smootherstep(0, 1, 1), 1.0f));

    // inverseLerp
    TEST("inverseLerp(0,10,5) = 0.5", APPROX(inverseLerp(0, 10, 5), 0.5f));
    TEST("inverseLerp(0,10,0) = 0", APPROX(inverseLerp(0, 10, 0), 0.0f));
    TEST("inverseLerp degenerate", APPROX(inverseLerp(5, 5, 5), 0.0f));

    // remap
    TEST("remap 0-10 to 0-100", APPROX(remap(5, 0, 10, 0, 100), 50.0f));
    TEST("remap 0-1 to 10-20", APPROX(remap(0.5f, 0, 1, 10, 20), 15.0f));

    // repeat
    TEST("repeat(3.5, 2) = 1.5", APPROX(repeat(3.5f, 2.0f), 1.5f));
    TEST("repeat(4.0, 2) = 0", APPROX(repeat(4.0f, 2.0f), 0.0f));

    // pingPong
    TEST("pingPong(0,1) = 0", APPROX(pingPong(0, 1), 0.0f));
    TEST("pingPong(1,1) = 1", APPROX(pingPong(1, 1), 1.0f));
    TEST("pingPong(1.5,1) = 0.5", APPROX(pingPong(1.5f, 1), 0.5f));

    // deltaAngle
    TEST("deltaAngle same = 0", APPROX(deltaAngle(0, 0), 0.0f));
    TEST("deltaAngle wraps", std::abs(deltaAngle(0.1f, Constants::TAU + 0.1f)) < 0.01f);

    // catmullRom endpoints
    TEST("catmullRom t=0 = p1", APPROX(catmullRom(0, 1, 2, 3, 0), 1.0f));
    TEST("catmullRom t=1 = p2", APPROX(catmullRom(0, 1, 2, 3, 1), 2.0f));

    // cubicBezier endpoints
    TEST("bezier t=0 = p0", APPROX(cubicBezier(0, 1, 2, 3, 0), 0.0f));
    TEST("bezier t=1 = p3", APPROX(cubicBezier(0, 1, 2, 3, 1), 3.0f));

    // Easing boundary conditions
    TEST("easeInQuad(0)=0", APPROX(easeInQuad(0), 0.0f));
    TEST("easeInQuad(1)=1", APPROX(easeInQuad(1), 1.0f));
    TEST("easeOutQuad(0)=0", APPROX(easeOutQuad(0), 0.0f));
    TEST("easeOutQuad(1)=1", APPROX(easeOutQuad(1), 1.0f));
    TEST("easeInOutQuad(0)=0", APPROX(easeInOutQuad(0), 0.0f));
    TEST("easeInOutQuad(1)=1", APPROX(easeInOutQuad(1), 1.0f));
    TEST("easeInCubic(1)=1", APPROX(easeInCubic(1), 1.0f));
    TEST("easeOutCubic(1)=1", APPROX(easeOutCubic(1), 1.0f));
    TEST("easeInOutCubic(0)=0", APPROX(easeInOutCubic(0), 0.0f));
    TEST("easeInElastic(0)=0", APPROX(easeInElastic(0), 0.0f));
    TEST("easeInElastic(1)=1", APPROX(easeInElastic(1), 1.0f));
    TEST("easeOutElastic(0)=0", APPROX(easeOutElastic(0), 0.0f));
    TEST("easeOutElastic(1)=1", APPROX(easeOutElastic(1), 1.0f));
    TEST("easeOutBounce(0)=0", APPROX(easeOutBounce(0), 0.0f));
    TEST("easeOutBounce(1)=1", APPROX(easeOutBounce(1), 1.0f));

    // expDecay
    TEST("expDecay at=target", APPROX(expDecay(5, 5, 10, 1), 5.0f));
    TEST("expDecay approaches target", std::abs(expDecay(0, 10, 100, 1) - 10.0f) < 0.1f);

    // springDamper
    float val = 0.0f, vel = 0.0f;
    for (int i = 0; i < 1000; i++) springDamper(val, vel, 10.0f, 100.0f, 20.0f, 0.01f);
    TEST("springDamper converges", APPROX(val, 10.0f));
}

// ═══════════════════════════════════════════════════════════════
int main() {
    testColor();
    testTransform2D();
    testMathConstants();

    std::cout << "\n============================================" << std::endl;
    std::cout << "  Resultados: " << passed << " pasados, " << failed << " fallidos" << std::endl;
    std::cout << "============================================" << std::endl;
    return failed > 0 ? 1 : 0;
}
