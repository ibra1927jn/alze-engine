#include "Vector3D.h"
#include "Matrix4x4.h"
#include "Quaternion.h"
#include "Transform3D.h"
#include "MathUtils.h"
#include <iostream>
#include <cmath>

using namespace engine::math;

// ── Utilidad para los tests ────────────────────────────────────
int passed = 0;
int failed = 0;

#define TEST(name, expr) \
    if (expr) { passed++; std::cout << "  [OK] " << name << std::endl; } \
    else { failed++; std::cerr << "  [FAIL] " << name << std::endl; }

// Tolerancia más amplia para operaciones encadenadas
bool approx(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) < eps;
}

bool v3approx(const Vector3D& a, const Vector3D& b, float eps = 1e-4f) {
    return approx(a.x, b.x, eps) && approx(a.y, b.y, eps) && approx(a.z, b.z, eps);
}

// ═══════════════════════════════════════════════════════════════
// Tests de Vector3D
// ═══════════════════════════════════════════════════════════════
void testVector3D() {
    std::cout << "\n=== Vector3D: Basico ===" << std::endl;

    // Constructores
    Vector3D v0;
    TEST("Constructor defecto (0,0,0)", v0.x == 0 && v0.y == 0 && v0.z == 0);
    Vector3D v1(3, 4, 0);
    TEST("Constructor valores", v1.x == 3 && v1.y == 4 && v1.z == 0);
    Vector3D v2(1, 2, 3);
    TEST("Constructor 3D", v2.x == 1 && v2.y == 2 && v2.z == 3);

    // Constantes
    TEST("Zero",    Vector3D::Zero == Vector3D(0, 0, 0));
    TEST("One",     Vector3D::One == Vector3D(1, 1, 1));
    TEST("Up",      Vector3D::Up == Vector3D(0, 1, 0));
    TEST("Down",    Vector3D::Down == Vector3D(0, -1, 0));
    TEST("Right",   Vector3D::Right == Vector3D(1, 0, 0));
    TEST("Left",    Vector3D::Left == Vector3D(-1, 0, 0));
    TEST("Forward", Vector3D::Forward == Vector3D(0, 0, -1));
    TEST("Back",    Vector3D::Back == Vector3D(0, 0, 1));

    // Aritmética
    TEST("Suma",       (Vector3D(1,2,3) + Vector3D(4,5,6)) == Vector3D(5,7,9));
    TEST("Resta",      (Vector3D(5,7,9) - Vector3D(4,5,6)) == Vector3D(1,2,3));
    TEST("Mul escalar",(Vector3D(1,2,3) * 2.0f) == Vector3D(2,4,6));
    TEST("Mul izq",    (2.0f * Vector3D(1,2,3)) == Vector3D(2,4,6));
    TEST("Div escalar",(Vector3D(4,6,8) / 2.0f) == Vector3D(2,3,4));
    TEST("Negacion",   (-Vector3D(1,2,3)) == Vector3D(-1,-2,-3));

    // Operadores compuestos
    Vector3D vc(1,1,1);
    vc += Vector3D(2, 3, 4);
    TEST("+= operador", vc == Vector3D(3, 4, 5));
    vc -= Vector3D(1, 1, 1);
    TEST("-= operador", vc == Vector3D(2, 3, 4));
    vc *= 2.0f;
    TEST("*= operador", vc == Vector3D(4, 6, 8));
    vc /= 2.0f;
    TEST("/= operador", vc == Vector3D(2, 3, 4));

    // Hadamard (component-wise multiply)
    TEST("Hadamard", (Vector3D(2,3,4) * Vector3D(5,6,7)) == Vector3D(10,18,28));
}

void testVector3D_Geometria() {
    std::cout << "\n=== Vector3D: Geometria ===" << std::endl;

    // Magnitud
    TEST("Magnitud (3,4,0)=5", approx(Vector3D(3,4,0).magnitude(), 5.0f));
    TEST("Magnitud (1,2,2)=3", approx(Vector3D(1,2,2).magnitude(), 3.0f));
    TEST("SqrMag (1,2,2)=9",   approx(Vector3D(1,2,2).sqrMagnitude(), 9.0f));
    TEST("Magnitud Zero=0",    approx(Vector3D::Zero.magnitude(), 0.0f));

    // Normalización
    Vector3D n = Vector3D(0, 3, 4).normalized();
    TEST("Normalizado mag=1", approx(n.magnitude(), 1.0f));
    TEST("Normalizar Zero=Zero", Vector3D::Zero.normalized() == Vector3D::Zero);

    // Dot product
    TEST("Dot perpendicular=0", approx(Vector3D(1,0,0).dot(Vector3D(0,1,0)), 0.0f));
    TEST("Dot paralelo=1",     approx(Vector3D(1,0,0).dot(Vector3D(1,0,0)), 1.0f));
    TEST("Dot opuesto=-1",     approx(Vector3D(1,0,0).dot(Vector3D(-1,0,0)), -1.0f));
    TEST("Dot general",        approx(Vector3D(1,2,3).dot(Vector3D(4,5,6)), 32.0f));

    // Cross product
    TEST("Cross X×Y=Z",    Vector3D(1,0,0).cross(Vector3D(0,1,0)) == Vector3D(0,0,1));
    TEST("Cross Y×X=-Z",   Vector3D(0,1,0).cross(Vector3D(1,0,0)) == Vector3D(0,0,-1));
    TEST("Cross Y×Z=X",    Vector3D(0,1,0).cross(Vector3D(0,0,1)) == Vector3D(1,0,0));
    TEST("Cross paralelo=Zero", Vector3D(1,0,0).cross(Vector3D(2,0,0)) == Vector3D::Zero);

    // Cross anticonmutatividad: a×b = -(b×a)
    Vector3D a(1,2,3), b(4,5,6);
    TEST("Cross anticonmutativo", a.cross(b) == -(b.cross(a)));

    // Cross perpendicular a ambos
    Vector3D c = a.cross(b);
    TEST("Cross perp a A", approx(c.dot(a), 0.0f));
    TEST("Cross perp a B", approx(c.dot(b), 0.0f));

    // Distancia
    TEST("Distancia",     approx(Vector3D::distance(Vector3D::Zero, Vector3D(1,2,2)), 3.0f));
    TEST("SqrDistancia",  approx(Vector3D::sqrDistance(Vector3D::Zero, Vector3D(1,2,2)), 9.0f));

    // Angle between
    TEST("Angulo perp=PI/2", approx(
        Vector3D::angleBetween(Vector3D(1,0,0), Vector3D(0,1,0)), MathUtils::HALF_PI));

    // Reflect
    Vector3D inc(1, -1, 0);
    Vector3D normal(0, 1, 0);
    Vector3D ref = inc.reflect(normal);
    TEST("Reflect", v3approx(ref, Vector3D(1, 1, 0)));

    // Project
    Vector3D proj = Vector3D(3, 4, 0).project(Vector3D(1, 0, 0));
    TEST("Proyeccion X", proj == Vector3D(3, 0, 0));
    TEST("Proyeccion Zero", Vector3D(5,5,5).project(Vector3D::Zero) == Vector3D::Zero);

    // Lerp
    TEST("Lerp 0%",  Vector3D::lerp(Vector3D::Zero, Vector3D(10,10,10), 0.0f) == Vector3D::Zero);
    TEST("Lerp 50%", Vector3D::lerp(Vector3D::Zero, Vector3D(10,10,10), 0.5f) == Vector3D(5,5,5));
    TEST("Lerp 100%",Vector3D::lerp(Vector3D::Zero, Vector3D(10,10,10), 1.0f) == Vector3D(10,10,10));
    TEST("Lerp clamp",Vector3D::lerp(Vector3D::Zero, Vector3D(10,10,10), 1.5f) == Vector3D(10,10,10));

    // Min/Max
    TEST("Min", Vector3D::min(Vector3D(1,5,3), Vector3D(4,2,6)) == Vector3D(1,2,3));
    TEST("Max", Vector3D::max(Vector3D(1,5,3), Vector3D(4,2,6)) == Vector3D(4,5,6));
}

// ═══════════════════════════════════════════════════════════════
// Tests de Matrix4x4
// ═══════════════════════════════════════════════════════════════
void testMatrix4x4() {
    std::cout << "\n=== Matrix4x4: Basico ===" << std::endl;

    Matrix4x4 id = Matrix4x4::identity();
    TEST("Identidad diagonal", approx(id.get(0,0), 1) && approx(id.get(1,1), 1) &&
         approx(id.get(2,2), 1) && approx(id.get(3,3), 1));
    TEST("Identidad off-diagonal=0", approx(id.get(0,1), 0) && approx(id.get(1,0), 0));

    // Identidad * punto = punto
    Vector3D p(5, 3, 7);
    TEST("Identidad*punto=punto", id.transformPoint(p) == p);
    TEST("Identidad*vector=vector", id.transformVector(p) == p);

    // Traslación
    Matrix4x4 trans = Matrix4x4::translation(10, 20, 30);
    TEST("Traslacion punto", trans.transformPoint(Vector3D(5,5,5)) == Vector3D(15,25,35));
    TEST("Traslacion NO afecta vectores", trans.transformVector(Vector3D(5,5,5)) == Vector3D(5,5,5));

    // Escala
    TEST("Escala punto", Matrix4x4::scale(2,3,4).transformPoint(Vector3D(1,1,1)) == Vector3D(2,3,4));
    TEST("Escala uniforme", Matrix4x4::scale(2.0f).transformPoint(Vector3D(3,4,5)) == Vector3D(6,8,10));

    // Composición T*S
    Matrix4x4 combined = trans * Matrix4x4::scale(2, 3, 4);
    Vector3D combResult = combined.transformPoint(Vector3D(1, 1, 1));
    TEST("Composicion T*S", combResult == Vector3D(12, 23, 34));

    // I * I = I
    Matrix4x4 ii = id * id;
    TEST("I*I=I", ii == id);

    // Determinante
    TEST("Det identidad=1", approx(id.determinant(), 1.0f));
    TEST("Det escala(2,3,4)=24", approx(Matrix4x4::scale(2,3,4).determinant(), 24.0f));
    TEST("Det traslacion=1", approx(trans.determinant(), 1.0f));

    // Transpuesta
    Matrix4x4 t = trans.transposed();
    TEST("Transpuesta", approx(t.get(0,3), trans.get(3,0)));
}

void testMatrix4x4_Avanzado() {
    std::cout << "\n=== Matrix4x4: Avanzado ===" << std::endl;

    // Inversa: M * M^-1 = I
    Matrix4x4 trans = Matrix4x4::translation(10, 20, 30);
    Matrix4x4 inv = trans.inverse();
    Matrix4x4 result = trans * inv;
    Matrix4x4 id = Matrix4x4::identity();
    TEST("Inversa traslacion: M*M^-1=I", result == id);

    // Inversa escala
    Matrix4x4 sc = Matrix4x4::scale(2, 3, 4);
    TEST("Inversa escala: M*M^-1=I", (sc * sc.inverse()) == id);

    // Inversa rotación
    Matrix4x4 rot = Matrix4x4::rotationY(MathUtils::HALF_PI);
    TEST("Inversa rotacion: M*M^-1=I", (rot * rot.inverse()) == id);

    // Inversa composición (wider tolerance for chained operations)
    Matrix4x4 composed = trans * rot * sc;
    Matrix4x4 compInv = composed.inverse();
    Matrix4x4 compResult = composed * compInv;
    bool compOk = true;
    for (int r = 0; r < 4 && compOk; r++)
        for (int c = 0; c < 4 && compOk; c++)
            if (!approx(compResult.get(r,c), id.get(r,c), 1e-3f)) compOk = false;
    TEST("Inversa composicion: M*M^-1=I", compOk);

    // Rotación 90° sobre Y: (1,0,0) → (0,0,-1)
    Vector3D rotResult = Matrix4x4::rotationY(MathUtils::HALF_PI).transformPoint(Vector3D(1,0,0));
    TEST("RotY 90°: X→-Z", v3approx(rotResult, Vector3D(0, 0, -1)));

    // Rotación 90° sobre X: (0,1,0) → (0,0,1)
    Vector3D rotXResult = Matrix4x4::rotationX(MathUtils::HALF_PI).transformPoint(Vector3D(0,1,0));
    TEST("RotX 90°: Y→Z", v3approx(rotXResult, Vector3D(0, 0, 1)));

    // Rotación 90° sobre Z: (1,0,0) → (0,1,0)
    Vector3D rotZResult = Matrix4x4::rotationZ(MathUtils::HALF_PI).transformPoint(Vector3D(1,0,0));
    TEST("RotZ 90°: X→Y", v3approx(rotZResult, Vector3D(0, 1, 0)));

    // Column-major: data() devuelve los datos en order correcto
    Matrix4x4 tm = Matrix4x4::translation(5, 10, 15);
    const float* d = tm.data();
    // Column-major: traslación está en columna 3 → indices 12,13,14
    TEST("data() column-major tx", approx(d[12], 5.0f));
    TEST("data() column-major ty", approx(d[13], 10.0f));
    TEST("data() column-major tz", approx(d[14], 15.0f));

    // Perspectiva: verificar propiedades básicas
    Matrix4x4 persp = Matrix4x4::perspective(MathUtils::degToRad(60.0f), 16.0f/9.0f, 0.1f, 100.0f);
    TEST("Perspectiva m33=0", approx(persp.get(3,3), 0.0f));
    TEST("Perspectiva m32=-1", approx(persp.get(3,2), -1.0f));

    // LookAt: cámara en (0,0,5) mirando al origen
    Matrix4x4 view = Matrix4x4::lookAt(Vector3D(0,0,5), Vector3D::Zero, Vector3D::Up);
    // El origen del mundo debería mapearse a (0,0,-5) en espacio de la cámara
    Vector3D viewResult = view.transformPoint(Vector3D::Zero);
    TEST("LookAt: origen→z negativo", viewResult.z < 0);
    TEST("LookAt: el punto esta centrado", approx(viewResult.x, 0) && approx(viewResult.y, 0));

    // Ortho: verificar propiedades
    Matrix4x4 ortho = Matrix4x4::orthographic(-1, 1, -1, 1, 0.1f, 100.0f);
    TEST("Ortho m33=1", approx(ortho.get(3,3), 1.0f));
}

// ═══════════════════════════════════════════════════════════════
// Tests de Quaternion
// ═══════════════════════════════════════════════════════════════
void testQuaternion() {
    std::cout << "\n=== Quaternion: Basico ===" << std::endl;

    // Identidad
    Quaternion qi;
    TEST("Identidad defecto", approx(qi.w, 1) && approx(qi.x, 0) && approx(qi.y, 0) && approx(qi.z, 0));
    TEST("Identidad constante", Quaternion::Identity == qi);
    TEST("Identidad magnitud=1", approx(qi.magnitude(), 1.0f));

    // Identidad no rota nada
    Vector3D vi(1, 2, 3);
    TEST("Identidad no rota", v3approx(qi.rotate(vi), vi));

    // fromAxisAngle: 90° sobre Y → (1,0,0) debería ir a (0,0,-1)
    Quaternion qy90 = Quaternion::fromAxisAngle(Vector3D::Up, MathUtils::HALF_PI);
    Vector3D rotY = qy90.rotate(Vector3D(1, 0, 0));
    TEST("AxisAngle Y90: X→-Z", v3approx(rotY, Vector3D(0, 0, -1)));

    // fromAxisAngle: 90° sobre X → (0,1,0) debería ir a (0,0,1)
    Quaternion qx90 = Quaternion::fromAxisAngle(Vector3D::Right, MathUtils::HALF_PI);
    Vector3D rotX = qx90.rotate(Vector3D(0, 1, 0));
    TEST("AxisAngle X90: Y→Z", v3approx(rotX, Vector3D(0, 0, 1)));

    // fromAxisAngle: 90° sobre Z → (1,0,0) debería ir a (0,1,0)
    Quaternion qz90 = Quaternion::fromAxisAngle(Vector3D(0,0,1), MathUtils::HALF_PI);
    Vector3D rotZ = qz90.rotate(Vector3D(1, 0, 0));
    TEST("AxisAngle Z90: X→Y", v3approx(rotZ, Vector3D(0, 1, 0)));

    // Composición: dos rotaciones de 90° = 180°
    Quaternion q180 = qy90 * qy90;
    Vector3D rot180 = q180.rotate(Vector3D(1, 0, 0));
    TEST("Composicion 90+90=180: X→-X", v3approx(rot180, Vector3D(-1, 0, 0)));

    // Conjugado: invierte la rotación
    Quaternion qConj = qy90.conjugate();
    Vector3D rotConj = qConj.rotate(Vector3D(1, 0, 0));
    TEST("Conjugado invierte: X→Z", v3approx(rotConj, Vector3D(0, 0, 1)));

    // Inversa (para unitarios = conjugado)
    Quaternion qInv = qy90.inverse();
    TEST("Inversa unitario = conjugado", qInv == qConj);

    // q * q^-1 = identidad
    Quaternion qIdentity = qy90 * qInv;
    TEST("q*q^-1=identidad", v3approx(qIdentity.rotate(vi), vi));

    // Normalización
    Quaternion qDenorm(1, 2, 3, 4);
    Quaternion qNorm = qDenorm.normalized();
    TEST("Normalizado mag=1", approx(qNorm.magnitude(), 1.0f));
}

void testQuaternion_Avanzado() {
    std::cout << "\n=== Quaternion: Avanzado ===" << std::endl;

    // Slerp t=0 → a
    Quaternion qa = Quaternion::fromAxisAngle(Vector3D::Up, 0.0f);
    Quaternion qb = Quaternion::fromAxisAngle(Vector3D::Up, MathUtils::HALF_PI);
    Quaternion s0 = Quaternion::slerp(qa, qb, 0.0f);
    TEST("Slerp t=0 → a", v3approx(s0.rotate(Vector3D(1,0,0)), qa.rotate(Vector3D(1,0,0))));

    // Slerp t=1 → b
    Quaternion s1 = Quaternion::slerp(qa, qb, 1.0f);
    TEST("Slerp t=1 → b", v3approx(s1.rotate(Vector3D(1,0,0)), qb.rotate(Vector3D(1,0,0))));

    // Slerp t=0.5 → mitad (45°)
    Quaternion sMid = Quaternion::slerp(qa, qb, 0.5f);
    Vector3D midResult = sMid.rotate(Vector3D(1, 0, 0));
    float expectedAngle = MathUtils::HALF_PI * 0.5f;  // 45°
    TEST("Slerp t=0.5 → 45°", approx(
        Vector3D::angleBetween(Vector3D(1,0,0), midResult), expectedAngle, 0.01f));

    // toMatrix: rotación por cuaternión = rotación por matriz
    Quaternion qRot = Quaternion::fromAxisAngle(Vector3D::Up, MathUtils::HALF_PI);
    Matrix4x4 mRot = qRot.toMatrix();
    Vector3D vFromQuat = qRot.rotate(Vector3D(1, 0, 0));
    Vector3D vFromMat = mRot.transformPoint(Vector3D(1, 0, 0));
    TEST("toMatrix equivalente a rotate", v3approx(vFromQuat, vFromMat));

    // fromQuaternion bridge
    Matrix4x4 mBridge = Matrix4x4::fromQuaternion(qRot);
    TEST("fromQuaternion = toMatrix", mBridge == mRot);

    // Euler roundtrip: fromEuler → toEuler ≈ original
    // Euler roundtrip: verify that fromEuler → rotate → toEuler → rotate gives same result
    float pitch = MathUtils::degToRad(30.0f);
    float yaw = MathUtils::degToRad(45.0f);
    float roll = MathUtils::degToRad(60.0f);
    Quaternion qEuler = Quaternion::fromEuler(pitch, yaw, roll);
    Vector3D testVec(1, 2, 3);
    Vector3D rotatedByOriginal = qEuler.rotate(testVec);
    // Roundtrip through euler conversion
    Vector3D eulerBack = qEuler.toEuler();
    Quaternion qRebuilt = Quaternion::fromEuler(eulerBack.x, eulerBack.y, eulerBack.z);
    Vector3D rotatedByRebuilt = qRebuilt.rotate(testVec);
    TEST("Euler roundtrip rotation X", approx(rotatedByOriginal.x, rotatedByRebuilt.x, 0.01f));
    TEST("Euler roundtrip rotation Y", approx(rotatedByOriginal.y, rotatedByRebuilt.y, 0.01f));
    TEST("Euler roundtrip rotation Z", approx(rotatedByOriginal.z, rotatedByRebuilt.z, 0.01f));

    // Nlerp: producir resultado similar a slerp para ángulos pequeños
    Quaternion n05 = Quaternion::nlerp(qa, qb, 0.5f);
    Vector3D nlerpResult = n05.rotate(Vector3D(1,0,0));
    TEST("Nlerp aproxima slerp", approx(
        Vector3D::angleBetween(Vector3D(1,0,0), nlerpResult), expectedAngle, 0.05f));
}

// ═══════════════════════════════════════════════════════════════
// Tests de Transform3D
// ═══════════════════════════════════════════════════════════════
void testTransform3D() {
    std::cout << "\n=== Transform3D ===" << std::endl;

    // Defecto: posición zero, sin rotación, escala uno
    Transform3D t0;
    TEST("Defecto posicion", t0.position == Vector3D::Zero);
    TEST("Defecto escala", t0.scale == Vector3D::One);

    // Forward, right, up en identidad
    TEST("Forward sin rotacion", v3approx(t0.forward(), Vector3D::Forward));
    TEST("Right sin rotacion",   v3approx(t0.right(), Vector3D::Right));
    TEST("Up sin rotacion",      v3approx(t0.up(), Vector3D::Up));

    // Transform con posición
    Transform3D t1(Vector3D(10, 20, 30));
    Vector3D local = t1.localToWorld(Vector3D::Zero);
    TEST("localToWorld origen+traslacion", local == Vector3D(10, 20, 30));

    // Transform con rotación 90° Y
    // Forward = (0,0,-1). After 90° rotation around Y:
    // Quaternion rotates (0,0,-1) 90° around Y → (x rotates to -z):
    // Result should be (-1,0,0) — but let's just verify via rotate
    Transform3D t2(Vector3D::Zero, Quaternion::fromAxisAngle(Vector3D::Up, MathUtils::HALF_PI));
    Vector3D fwd = t2.forward();
    Vector3D expectedFwd = Quaternion::fromAxisAngle(Vector3D::Up, MathUtils::HALF_PI).rotate(Vector3D::Forward);
    TEST("Forward rotado 90Y", v3approx(fwd, expectedFwd, 0.01f));

    // Transform con escala
    Transform3D t3(Vector3D::Zero, Quaternion::Identity, Vector3D(2, 2, 2));
    Vector3D scaled = t3.localToWorld(Vector3D(1, 1, 1));
    TEST("Escala 2x", scaled == Vector3D(2, 2, 2));

    // TRS completo: traslación + rotación + escala
    Transform3D t4(
        Vector3D(5, 0, 0),
        Quaternion::fromAxisAngle(Vector3D::Up, MathUtils::HALF_PI),
        Vector3D(2, 2, 2)
    );
    Vector3D trsResult = t4.localToWorld(Vector3D(1, 0, 0));
    // Escala: (1,0,0)*2 = (2,0,0)
    // Rotación 90° Y: (2,0,0) → (0,0,-2)
    // Traslación: (0,0,-2) + (5,0,0) = (5,0,-2)
    TEST("TRS completo", v3approx(trsResult, Vector3D(5, 0, -2)));

    // Interpolación
    Transform3D ta(Vector3D::Zero);
    Transform3D tb(Vector3D(10, 10, 10));
    Transform3D mid = Transform3D::lerp(ta, tb, 0.5f);
    TEST("Lerp posicion 50%", mid.position == Vector3D(5, 5, 5));

    // toMatrix genera la misma transformación que localToWorld
    Matrix4x4 mat = t4.toMatrix();
    Vector3D matResult = mat.transformPoint(Vector3D(1, 0, 0));
    TEST("toMatrix = localToWorld", v3approx(matResult, trsResult));
}

// ═══════════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════════
int main() {
    std::cout << "============================================" << std::endl;
    std::cout << "  ALZE — Tests Matematicas 3D (Fase 3.0)" << std::endl;
    std::cout << "  Vector3D + Matrix4x4 + Quaternion + Transform3D" << std::endl;
    std::cout << "============================================" << std::endl;

    testVector3D();
    testVector3D_Geometria();
    testMatrix4x4();
    testMatrix4x4_Avanzado();
    testQuaternion();
    testQuaternion_Avanzado();
    testTransform3D();

    std::cout << "\n============================================" << std::endl;
    std::cout << "  Resultados: " << passed << " pasados, " << failed << " fallidos" << std::endl;
    std::cout << "============================================" << std::endl;

    return (failed > 0) ? 1 : 0;
}
