#include "Vector2D.h"
#include "Matrix3x3.h"
#include "MathUtils.h"
#include "AABB.h"
#include <iostream>
#include <cassert>
#include <cmath>

using namespace engine::math;

// ── Utilidad para los tests ────────────────────────────────────
int passed = 0;
int failed = 0;

#define TEST(name, expr) \
    if (expr) { passed++; std::cout << "  [OK] " << name << std::endl; } \
    else { failed++; std::cerr << "  [FAIL] " << name << std::endl; }

// ═══════════════════════════════════════════════════════════════
// NIVEL 1: Tests básicos de Vector2D
// ═══════════════════════════════════════════════════════════════
void testVector2D_Nivel1() {
    std::cout << "\n=== Nivel 1: Vector2D Basico ===" << std::endl;

    // Constructores
    Vector2D v1;
    TEST("Constructor por defecto (0,0)", v1.x == 0.0f && v1.y == 0.0f);
    Vector2D v2(3.0f, 4.0f);
    TEST("Constructor con valores", v2.x == 3.0f && v2.y == 4.0f);

    // Suma y resta
    TEST("Suma", (v2 + Vector2D(1.0f, 2.0f)) == Vector2D(4.0f, 6.0f));
    TEST("Resta", (Vector2D(4.0f, 6.0f) - v2) == Vector2D(1.0f, 2.0f));

    // Multiplicación y división escalar
    TEST("Mul escalar", (v2 * 2.0f) == Vector2D(6.0f, 8.0f));
    TEST("Mul escalar izq", (2.0f * v2) == Vector2D(6.0f, 8.0f));
    TEST("Div escalar", (Vector2D(6.0f, 8.0f) / 2.0f) == v2);

    // Negación y operadores compuestos
    TEST("Negacion", (-v2) == Vector2D(-3.0f, -4.0f));
    Vector2D v8(1.0f, 1.0f);
    v8 += Vector2D(2.0f, 3.0f);
    TEST("+= operador", v8 == Vector2D(3.0f, 4.0f));
    v8 -= Vector2D(1.0f, 1.0f);
    TEST("-= operador", v8 == Vector2D(2.0f, 3.0f));

    // Magnitud (Pitágoras)
    TEST("Magnitud (3,4) = 5", MathUtils::approxEqual(v2.magnitude(), 5.0f));
    TEST("SqrMagnitud = 25", MathUtils::approxEqual(v2.sqrMagnitude(), 25.0f));
    TEST("Magnitud (0,0) = 0", MathUtils::approxEqual(Vector2D::Zero.magnitude(), 0.0f));

    // Normalización
    Vector2D norm = v2.normalized();
    TEST("Normalizado mag = 1", MathUtils::approxEqual(norm.magnitude(), 1.0f));
    TEST("Normalizado dir", MathUtils::approxEqual(norm.x, 0.6f) && MathUtils::approxEqual(norm.y, 0.8f));
    TEST("Normalizar Zero = Zero", Vector2D::Zero.normalized() == Vector2D::Zero);

    // Distancia
    TEST("Distancia (0,0)-(3,4) = 5", MathUtils::approxEqual(Vector2D::distance(Vector2D::Zero, v2), 5.0f));

    // Constantes
    TEST("Zero", Vector2D::Zero == Vector2D(0.0f, 0.0f));
    TEST("One", Vector2D::One == Vector2D(1.0f, 1.0f));
    TEST("Up", Vector2D::Up == Vector2D(0.0f, -1.0f));
    TEST("Right", Vector2D::Right == Vector2D(1.0f, 0.0f));
}

// ═══════════════════════════════════════════════════════════════
// NIVEL 2: Tests intermedios de Vector2D
// ═══════════════════════════════════════════════════════════════
void testVector2D_Nivel2() {
    std::cout << "\n=== Nivel 2: Vector2D Geometria ===" << std::endl;

    // ── Dot Product ────────────────────────────────────────────
    TEST("Dot perpendicular = 0",
        MathUtils::approxEqual(Vector2D(1, 0).dot(Vector2D(0, 1)), 0.0f));
    TEST("Dot paralelo = 1",
        MathUtils::approxEqual(Vector2D(1, 0).dot(Vector2D(1, 0)), 1.0f));
    TEST("Dot opuesto = -1",
        MathUtils::approxEqual(Vector2D(1, 0).dot(Vector2D(-1, 0)), -1.0f));
    TEST("Dot general (3,4).(4,-3) = 0",
        MathUtils::approxEqual(Vector2D(3, 4).dot(Vector2D(4, -3)), 0.0f));

    // ── Cross Product 2D ───────────────────────────────────────
    TEST("Cross (1,0)x(0,1) = 1",
        MathUtils::approxEqual(Vector2D(1, 0).cross(Vector2D(0, 1)), 1.0f));
    TEST("Cross (0,1)x(1,0) = -1",
        MathUtils::approxEqual(Vector2D(0, 1).cross(Vector2D(1, 0)), -1.0f));
    TEST("Cross paralelos = 0",
        MathUtils::approxEqual(Vector2D(2, 3).cross(Vector2D(4, 6)), 0.0f));

    // ── Perpendicular ──────────────────────────────────────────
    TEST("Perpendicular (1,0) = (0,1)",
        Vector2D(1, 0).perpendicular() == Vector2D(0, 1));
    TEST("Perpendicular (0,1) = (-1,0)",
        Vector2D(0, 1).perpendicular() == Vector2D(-1, 0));
    TEST("Perpendicular (3,4) es perpendicular",
        MathUtils::approxEqual(Vector2D(3, 4).dot(Vector2D(3, 4).perpendicular()), 0.0f));

    // ── Reflect (Rebote) ───────────────────────────────────────
    // Bola cayendo verticalmente rebota contra suelo horizontal
    Vector2D falling(0.0f, 1.0f);
    Vector2D floorNormal(0.0f, -1.0f);
    Vector2D bounce = falling.reflect(floorNormal);
    TEST("Reflect vertical vs suelo",
        bounce == Vector2D(0.0f, -1.0f));

    // Rebote en 45 grados contra pared vertical
    Vector2D diagonal(1.0f, -1.0f);
    Vector2D wallNormal(-1.0f, 0.0f);
    Vector2D wallBounce = diagonal.reflect(wallNormal);
    TEST("Reflect diagonal vs pared",
        wallBounce == Vector2D(-1.0f, -1.0f));

    // Rebote contra superficie en 45°
    Vector2D vel(1.0f, 0.0f);
    Vector2D surfNormal = Vector2D(-1.0f, -1.0f).normalized();
    Vector2D surfBounce = vel.reflect(surfNormal);
    TEST("Reflect horizontal vs 45°",
        MathUtils::approxEqual(surfBounce.x, 0.0f) && MathUtils::approxEqual(surfBounce.y, -1.0f));

    // ── Project (Proyección) ───────────────────────────────────
    Vector2D proj1 = Vector2D(3.0f, 4.0f).project(Vector2D(1.0f, 0.0f));
    TEST("Proyeccion sobre eje X",
        proj1 == Vector2D(3.0f, 0.0f));

    Vector2D proj2 = Vector2D(3.0f, 4.0f).project(Vector2D(0.0f, 1.0f));
    TEST("Proyeccion sobre eje Y",
        proj2 == Vector2D(0.0f, 4.0f));

    // Proyección sobre vector diagonal
    Vector2D diagProj = Vector2D(4.0f, 0.0f).project(Vector2D(1.0f, 1.0f));
    TEST("Proyeccion sobre diagonal",
        diagProj == Vector2D(2.0f, 2.0f));

    // Proyectar sobre vector cero → resultado cero
    TEST("Proyeccion sobre Zero",
        Vector2D(5, 5).project(Vector2D::Zero) == Vector2D::Zero);

    // ── Rotate (Rotación) ──────────────────────────────────────
    Vector2D r90 = Vector2D(1, 0).rotated(MathUtils::HALF_PI);
    TEST("Rotar (1,0) 90° = (0,1)",
        MathUtils::approxEqual(r90.x, 0.0f) && MathUtils::approxEqual(r90.y, 1.0f));

    Vector2D r180 = Vector2D(1, 0).rotated(MathUtils::PI);
    TEST("Rotar (1,0) 180° = (-1,0)",
        MathUtils::approxEqual(r180.x, -1.0f) && MathUtils::approxEqual(r180.y, 0.0f));

    Vector2D r360 = Vector2D(3, 4).rotated(MathUtils::TWO_PI);
    TEST("Rotar 360° = original",
        MathUtils::approxEqual(r360.x, 3.0f) && MathUtils::approxEqual(r360.y, 4.0f));

    // Confirmar que rotar preserva la magnitud
    Vector2D rAny = Vector2D(5, 7).rotated(1.234f);
    TEST("Rotar preserva magnitud",
        MathUtils::approxEqual(rAny.magnitude(), Vector2D(5, 7).magnitude()));

    // ── Angulos ────────────────────────────────────────────────
    TEST("Angulo (1,0) = 0", MathUtils::approxEqual(Vector2D(1, 0).angle(), 0.0f));
    TEST("Angulo (0,1) = PI/2", MathUtils::approxEqual(Vector2D(0, 1).angle(), MathUtils::HALF_PI));
    TEST("AngleBetween perpendiculares = PI/2",
        MathUtils::approxEqual(Vector2D::angleBetween(Vector2D(1, 0), Vector2D(0, 1)), MathUtils::HALF_PI));

    // ── Lerp ───────────────────────────────────────────────────
    TEST("Lerp 50%", Vector2D::lerp(Vector2D::Zero, Vector2D(10, 10), 0.5f) == Vector2D(5, 5));
    TEST("Lerp clamped t>1", Vector2D::lerp(Vector2D::Zero, Vector2D(10, 10), 1.5f) == Vector2D(10, 10));
}

// ═══════════════════════════════════════════════════════════════
// Tests de Matrix3x3
// ═══════════════════════════════════════════════════════════════
void testMatrix3x3() {
    std::cout << "\n=== Nivel 2: Matrix3x3 ===" << std::endl;

    Matrix3x3 id = Matrix3x3::identity();
    TEST("Identidad diagonal", MathUtils::approxEqual(id.get(0,0), 1.0f) &&
        MathUtils::approxEqual(id.get(1,1), 1.0f) && MathUtils::approxEqual(id.get(2,2), 1.0f));
    TEST("Identidad fuera-diagonal = 0", MathUtils::approxEqual(id.get(0,1), 0.0f));

    Vector2D p(5.0f, 3.0f);
    TEST("Identidad * punto = punto", id.transformPoint(p) == p);

    // Traslación
    Matrix3x3 trans = Matrix3x3::translation(10.0f, 20.0f);
    TEST("Traslacion punto", trans.transformPoint(Vector2D(5, 5)) == Vector2D(15, 25));
    TEST("Traslacion NO afecta vectores", trans.transformVector(Vector2D(5, 5)) == Vector2D(5, 5));

    // Escala
    TEST("Escala punto", Matrix3x3::scale(2, 3).transformPoint(Vector2D(4, 5)) == Vector2D(8, 15));
    TEST("Escala uniforme", Matrix3x3::scale(2.0f).transformPoint(Vector2D(3, 4)) == Vector2D(6, 8));

    // Rotación 90°
    Vector2D rot = Matrix3x3::rotation(MathUtils::HALF_PI).transformPoint(Vector2D(1, 0));
    TEST("Rotacion 90°", MathUtils::approxEqual(rot.x, 0.0f) && MathUtils::approxEqual(rot.y, 1.0f));

    // Composición: traslación * escala
    Matrix3x3 combined = trans * Matrix3x3::scale(2, 3);
    Vector2D combResult = combined.transformPoint(Vector2D(1, 1));
    TEST("Composicion T*S", combResult == Vector2D(12, 23));

    // Determinante
    TEST("Det identidad = 1", MathUtils::approxEqual(id.determinant(), 1.0f));
    TEST("Det escala(2,3) = 6", MathUtils::approxEqual(Matrix3x3::scale(2, 3).determinant(), 6.0f));

    // Transpuesta
    Matrix3x3 t = trans.transposed();
    TEST("Transpuesta simetria", MathUtils::approxEqual(t.get(2,0), trans.get(0,2)));
}

// ═══════════════════════════════════════════════════════════════
// Tests de MathUtils
// ═══════════════════════════════════════════════════════════════
void testMathUtils() {
    std::cout << "\n=== MathUtils ===" << std::endl;

    TEST("degToRad(180) = PI", MathUtils::approxEqual(MathUtils::degToRad(180), MathUtils::PI));
    TEST("radToDeg(PI) = 180", MathUtils::approxEqual(MathUtils::radToDeg(MathUtils::PI), 180.0f));
    TEST("lerp(0,10,0.5) = 5", MathUtils::approxEqual(MathUtils::lerp(0, 10, 0.5f), 5.0f));
    TEST("clamp(5,0,10) = 5", MathUtils::approxEqual(MathUtils::clamp(5, 0, 10), 5.0f));
    TEST("clamp(-1,0,10) = 0", MathUtils::approxEqual(MathUtils::clamp(-1, 0, 10), 0.0f));
    TEST("clamp(15,0,10) = 10", MathUtils::approxEqual(MathUtils::clamp(15, 0, 10), 10.0f));
    TEST("remap(5,0,10,0,100) = 50", MathUtils::approxEqual(MathUtils::remap(5, 0, 10, 0, 100), 50.0f));
    TEST("approxEqual(1,1)", MathUtils::approxEqual(1.0f, 1.0f));
    TEST("!approxEqual(1,2)", !MathUtils::approxEqual(1.0f, 2.0f));
    TEST("sign(5)=1", MathUtils::approxEqual(MathUtils::sign(5), 1.0f));
    TEST("sign(-3)=-1", MathUtils::approxEqual(MathUtils::sign(-3), -1.0f));
    TEST("sign(0)=0", MathUtils::approxEqual(MathUtils::sign(0), 0.0f));
}

// ═══════════════════════════════════════════════════════════════
// Tests de AABB (Nivel 2)
// ═══════════════════════════════════════════════════════════════
void testAABB() {
    std::cout << "\n=== Nivel 2: AABB ===" << std::endl;

    // ── Constructores ──────────────────────────────────────────
    AABB empty;
    TEST("Constructor vacio", empty.min == Vector2D::Zero && empty.max == Vector2D::Zero);

    AABB box(Vector2D(10, 20), Vector2D(50, 60));
    TEST("Constructor min/max", box.min == Vector2D(10, 20) && box.max == Vector2D(50, 60));

    AABB centered = AABB::fromCenter(Vector2D(100, 100), Vector2D(25, 25));
    TEST("fromCenter", centered.min == Vector2D(75, 75) && centered.max == Vector2D(125, 125));

    AABB rect = AABB::fromRect(10, 20, 40, 30);
    TEST("fromRect", rect.min == Vector2D(10, 20) && rect.max == Vector2D(50, 50));

    // ── Propiedades ────────────────────────────────────────────
    TEST("Center", box.center() == Vector2D(30, 40));
    TEST("Size", box.size() == Vector2D(40, 40));
    TEST("HalfSize", box.halfSize() == Vector2D(20, 20));
    TEST("Width", MathUtils::approxEqual(box.width(), 40.0f));
    TEST("Height", MathUtils::approxEqual(box.height(), 40.0f));
    TEST("Area", MathUtils::approxEqual(box.area(), 1600.0f));
    TEST("isValid", box.isValid());

    // ── Contains (punto dentro) ────────────────────────────────
    TEST("Contains centro", box.contains(Vector2D(30, 40)));
    TEST("Contains esquina min", box.contains(Vector2D(10, 20)));
    TEST("Contains esquina max", box.contains(Vector2D(50, 60)));
    TEST("NO contains fuera", !box.contains(Vector2D(5, 5)));
    TEST("NO contains fuera X", !box.contains(Vector2D(55, 40)));

    // ── Overlaps (AABB vs AABB) ────────────────────────────────
    AABB boxA(Vector2D(0, 0), Vector2D(10, 10));
    AABB boxB(Vector2D(5, 5), Vector2D(15, 15));
    AABB boxC(Vector2D(20, 20), Vector2D(30, 30));
    AABB boxD(Vector2D(10, 0), Vector2D(20, 10));  // Tocándose en borde

    TEST("Overlaps: solapados", boxA.overlaps(boxB));
    TEST("Overlaps: simetrico", boxB.overlaps(boxA));
    TEST("NO overlaps: separados", !boxA.overlaps(boxC));
    TEST("Overlaps: tocandose borde", boxA.overlaps(boxD));

    // ── Overlap con uno dentro del otro ────────────────────────
    AABB big(Vector2D(0, 0), Vector2D(100, 100));
    AABB small(Vector2D(40, 40), Vector2D(60, 60));
    TEST("Overlaps: contenido", big.overlaps(small));
    TEST("Overlaps: contenido simetrico", small.overlaps(big));

    // ── Intersection ───────────────────────────────────────────
    AABB inter = boxA.intersection(boxB);
    TEST("Intersection min", inter.min == Vector2D(5, 5));
    TEST("Intersection max", inter.max == Vector2D(10, 10));

    AABB noInter = boxA.intersection(boxC);
    TEST("No intersection = vacia", noInter.min == Vector2D::Zero && noInter.max == Vector2D::Zero);

    // ── Merge ──────────────────────────────────────────────────
    AABB merged = boxA.merge(boxB);
    TEST("Merge min", merged.min == Vector2D(0, 0));
    TEST("Merge max", merged.max == Vector2D(15, 15));

    // ── getOverlap (MTV) ───────────────────────────────────────
    Vector2D overlap = boxA.getOverlap(boxB);
    TEST("Overlap X = 5", MathUtils::approxEqual(overlap.x, 5.0f));
    TEST("Overlap Y = 5", MathUtils::approxEqual(overlap.y, 5.0f));
    TEST("No overlap = Zero", boxA.getOverlap(boxC) == Vector2D::Zero);

    // ── getCollisionNormal ─────────────────────────────────────
    // boxA(0-10) vs boxB(5-15): B está abajo-derecha de A
    Vector2D normal = boxA.getCollisionNormal(boxB);
    TEST("Normal de colision", (normal == Vector2D(1, 0) || normal == Vector2D(0, 1)));

    // Colisión principalmente horizontal
    AABB hBox(Vector2D(0, 0), Vector2D(10, 10));
    AABB hRight(Vector2D(8, 2), Vector2D(18, 8));  // Poco overlap en X
    Vector2D hNormal = hBox.getCollisionNormal(hRight);
    TEST("Normal horizontal", hNormal == Vector2D(1, 0));

    // ── Expanded ───────────────────────────────────────────────
    AABB exp = boxA.expanded(2.0f);
    TEST("Expanded min", exp.min == Vector2D(-2, -2));
    TEST("Expanded max", exp.max == Vector2D(12, 12));

    // ── Translated ─────────────────────────────────────────────
    AABB moved = boxA.translated(Vector2D(5, 10));
    TEST("Translated min", moved.min == Vector2D(5, 10));
    TEST("Translated max", moved.max == Vector2D(15, 20));
}

// ═══════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════
int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "  PhysicsEngine2D — Tests Matematicos" << std::endl;
    std::cout << "  Nivel 1 + Nivel 2 Completo" << std::endl;
    std::cout << "============================================" << std::endl;

    testVector2D_Nivel1();
    testVector2D_Nivel2();
    testMatrix3x3();
    testMathUtils();
    testAABB();

    std::cout << "\n============================================" << std::endl;
    std::cout << "  Resultados: " << passed << " pasados, " << failed << " fallidos" << std::endl;
    std::cout << "============================================" << std::endl;

    return (failed > 0) ? 1 : 0;
}
