# ── Módulo ECS ─── convertido de INTERFACE a STATIC ──────────────────────
add_library(engine_ecs STATIC
    systems/Physics3DSystem.cpp
    systems/RenderSystem.cpp
    systems/CollisionSystem.cpp
)

target_include_directories(engine_ecs
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/systems
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(engine_ecs
    PUBLIC engine_math engine_core engine_physics
)
