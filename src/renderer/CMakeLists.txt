# ── Renderer OpenGL (engine_renderer) ───────────────────────────
enable_language(C)
add_library(engine_renderer STATIC
    glad/src/gl.c
    Material.cpp
    cgltf_impl.cpp
    ForwardRenderer.cpp
    DeferredRenderer.cpp
    EnvironmentMap.cpp
    PostProcess.cpp
    SSAO.cpp
    ScreenEffects.cpp
    SpriteBatch2D.cpp
    TextRenderer.cpp
    DecalRenderer.cpp
    SkeletalAnimation.cpp
    ModelLoader.cpp
    InstancedRenderer.cpp
    ImageDecoder.cpp
    ProceduralTextures.cpp
)
set_target_properties(engine_renderer PROPERTIES LINKER_LANGUAGE CXX)

target_include_directories(engine_renderer
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/glad/include
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..
)

# Link OpenGL (comes with windows/linux by default)
find_package(OpenGL REQUIRED)
target_link_libraries(engine_renderer PUBLIC OpenGL::GL)
