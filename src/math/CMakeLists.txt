# ── Librería de Matemáticas ──────────────────────────────────────
add_library(engine_math STATIC
    Vector2D.cpp
    Matrix3x3.cpp
    AABB.cpp
    Vector3D.cpp
    Matrix4x4.cpp
    Quaternion.cpp
    ../ai/NavMesh.cpp
)

target_include_directories(engine_math
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)
