# ── Modulo Editor (scene hierarchy, inspector, gizmos) ──────────
add_library(engine_editor STATIC
    Editor.cpp
)

target_include_directories(engine_editor
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
           ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(engine_editor
    PUBLIC
        engine_ecs
        engine_core
        engine_renderer
        engine_math
)
