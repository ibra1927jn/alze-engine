#include "Matrix4x4.h"
#include <cstdio>

namespace engine {
namespace math {

// ── Determinante ───────────────────────────────────────────────
float Matrix4x4::determinant() const {
    // Expansión por cofactores usando subdeterminantes 2x2
    float a00 = get(0,0), a01 = get(0,1), a02 = get(0,2), a03 = get(0,3);
    float a10 = get(1,0), a11 = get(1,1), a12 = get(1,2), a13 = get(1,3);
    float a20 = get(2,0), a21 = get(2,1), a22 = get(2,2), a23 = get(2,3);
    float a30 = get(3,0), a31 = get(3,1), a32 = get(3,2), a33 = get(3,3);

    float s0 = a00 * a11 - a10 * a01;
    float s1 = a00 * a12 - a10 * a02;
    float s2 = a00 * a13 - a10 * a03;
    float s3 = a01 * a12 - a11 * a02;
    float s4 = a01 * a13 - a11 * a03;
    float s5 = a02 * a13 - a12 * a03;

    float c5 = a22 * a33 - a32 * a23;
    float c4 = a21 * a33 - a31 * a23;
    float c3 = a21 * a32 - a31 * a22;
    float c2 = a20 * a33 - a30 * a23;
    float c1 = a20 * a32 - a30 * a22;
    float c0 = a20 * a31 - a30 * a21;

    return s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
}

// ── Inversa ────────────────────────────────────────────────────
Matrix4x4 Matrix4x4::inverse() const {
    float a00 = get(0,0), a01 = get(0,1), a02 = get(0,2), a03 = get(0,3);
    float a10 = get(1,0), a11 = get(1,1), a12 = get(1,2), a13 = get(1,3);
    float a20 = get(2,0), a21 = get(2,1), a22 = get(2,2), a23 = get(2,3);
    float a30 = get(3,0), a31 = get(3,1), a32 = get(3,2), a33 = get(3,3);

    float s0 = a00 * a11 - a10 * a01;
    float s1 = a00 * a12 - a10 * a02;
    float s2 = a00 * a13 - a10 * a03;
    float s3 = a01 * a12 - a11 * a02;
    float s4 = a01 * a13 - a11 * a03;
    float s5 = a02 * a13 - a12 * a03;

    float c5 = a22 * a33 - a32 * a23;
    float c4 = a21 * a33 - a31 * a23;
    float c3 = a21 * a32 - a31 * a22;
    float c2 = a20 * a33 - a30 * a23;
    float c1 = a20 * a32 - a30 * a22;
    float c0 = a20 * a31 - a30 * a21;

    float det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;

    if (MathUtils::approxEqual(det, 0.0f)) {
        return identity();  // Singular — devolvemos identidad como safety
    }

    float invDet = 1.0f / det;

    Matrix4x4 inv;

    inv.set(0, 0, ( a11 * c5 - a12 * c4 + a13 * c3) * invDet);
    inv.set(0, 1, (-a01 * c5 + a02 * c4 - a03 * c3) * invDet);
    inv.set(0, 2, ( a31 * s5 - a32 * s4 + a33 * s3) * invDet);
    inv.set(0, 3, (-a21 * s5 + a22 * s4 - a23 * s3) * invDet);

    inv.set(1, 0, (-a10 * c5 + a12 * c2 - a13 * c1) * invDet);
    inv.set(1, 1, ( a00 * c5 - a02 * c2 + a03 * c1) * invDet);
    inv.set(1, 2, (-a30 * s5 + a32 * s2 - a33 * s1) * invDet);
    inv.set(1, 3, ( a20 * s5 - a22 * s2 + a23 * s1) * invDet);

    inv.set(2, 0, ( a10 * c4 - a11 * c2 + a13 * c0) * invDet);
    inv.set(2, 1, (-a00 * c4 + a01 * c2 - a03 * c0) * invDet);
    inv.set(2, 2, ( a30 * s4 - a31 * s2 + a33 * s0) * invDet);
    inv.set(2, 3, (-a20 * s4 + a21 * s2 - a23 * s0) * invDet);

    inv.set(3, 0, (-a10 * c3 + a11 * c1 - a12 * c0) * invDet);
    inv.set(3, 1, ( a00 * c3 - a01 * c1 + a02 * c0) * invDet);
    inv.set(3, 2, (-a30 * s3 + a31 * s1 - a32 * s0) * invDet);
    inv.set(3, 3, ( a20 * s3 - a21 * s1 + a22 * s0) * invDet);

    return inv;
}

// ── Debug ──────────────────────────────────────────────────────
std::string Matrix4x4::toString() const {
    char buf[512];
    int n = 0;
    n += std::snprintf(buf + n, sizeof(buf) - n, "Matrix4x4(\n");
    for (int r = 0; r < 4; r++) {
        n += std::snprintf(buf + n, sizeof(buf) - n, "  [%g, %g, %g, %g]\n",
            get(r, 0), get(r, 1), get(r, 2), get(r, 3));
    }
    std::snprintf(buf + n, sizeof(buf) - n, ")");
    return buf;
}

} // namespace math
} // namespace engine
