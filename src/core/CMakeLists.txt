# ── Módulo Core (Ventana, Input, Engine, Audio, Events) ──────────
add_library(engine_core STATIC
    Window.cpp
    InputManager.cpp
    Engine.cpp
    Engine.cpp
    AudioEngine.cpp
    ProceduralAudio.cpp
)

target_include_directories(engine_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

# AudioEngine propio: usa solo SDL2 raw audio (sin SDL_mixer)
target_link_libraries(engine_core
    PUBLIC
        engine_math
        engine_renderer
        SDL2::SDL2
)

message(STATUS "AudioEngine: motor de audio propio (sin SDL_mixer)")
