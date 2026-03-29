# ── Módulo de Físicas ────────────────────────────────────────────────────
# Convertido de INTERFACE a STATIC: separación .h / .cpp
add_library(engine_physics STATIC
    PhysicsWorld3D.cpp
    CollisionSolver3D.cpp
    DynamicBVH3D.cpp
    Constraints3D.cpp
    Collider3D.cpp
    RigidBody3D.cpp
    GJK.cpp
    PhysicsSoA.cpp
)

target_include_directories(engine_physics
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(engine_physics
    PUBLIC engine_math
)
