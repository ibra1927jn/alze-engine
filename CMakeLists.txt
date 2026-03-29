cmake_minimum_required(VERSION 3.20)
project(ALZE VERSION 0.1.0 LANGUAGES CXX)

# ── C++17 estándar ──────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── SDL2: usar instalación local (MSYS2) o descargar ───────────
find_package(SDL2 QUIET)

if(NOT SDL2_FOUND)
    # Fallback: descargar SDL2 con FetchContent
    include(FetchContent)
    FetchContent_Declare(
        SDL2
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-2.30.12
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(SDL2)
endif()

# ── Warnings de compilación estrictos ─────────────────────────────
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
    # Cero overhead: sin excepciones ni RTTI (regla de oro en game dev)
    add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions> $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>)
    # GCC 15 introduce -Wtemplate-body que rompe std::forward válido
    if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 15)
        add_compile_options(-Wno-template-body)
    endif()
endif()

# ── Módulos del motor ───────────────────────────────────────────
add_subdirectory(src/math)
add_subdirectory(src/physics)
add_subdirectory(src/renderer)
add_subdirectory(src/core)
add_subdirectory(src/ecs)
add_subdirectory(src/editor)

# ── Ejecutable principal ────────────────────────────────────────
add_executable(ALZE src/main.cpp src/game/Play3DState.cpp src/game/PlayState.cpp src/game/WorldScene3D.cpp)

# SDL_MAIN_HANDLED: Usamos nuestro propio main() estándar
target_compile_definitions(ALZE PRIVATE SDL_MAIN_HANDLED)

target_include_directories(ALZE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(ALZE
    PRIVATE
        engine_editor
        engine_ecs
        engine_core
        engine_renderer
        engine_physics
        engine_math
)

# ── Tests ───────────────────────────────────────────────────────
add_executable(test_math tests/test_math.cpp)
target_link_libraries(test_math PRIVATE engine_math)

add_executable(test_physics tests/test_physics.cpp)
target_include_directories(test_physics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_physics PRIVATE engine_ecs engine_physics engine_math)

add_executable(test_physics3d tests/test_physics3d.cpp)
target_include_directories(test_physics3d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_physics3d PRIVATE engine_physics engine_math)


add_executable(test_ecs tests/test_ecs.cpp)
target_link_libraries(test_ecs PRIVATE engine_ecs engine_math)

add_executable(test_subsystems tests/test_subsystems.cpp)
target_link_libraries(test_subsystems PRIVATE engine_core engine_math)

add_executable(test_phase25 tests/test_phase25.cpp)
target_link_libraries(test_phase25 PRIVATE engine_core engine_physics engine_math)

add_executable(test_phase3 tests/test_phase3.cpp src/game/Play3DState.cpp src/game/WorldScene3D.cpp)
target_compile_definitions(test_phase3 PRIVATE SDL_MAIN_HANDLED)
target_include_directories(test_phase3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_phase3 PRIVATE engine_ecs engine_core engine_physics engine_math SDL2::SDL2)

add_executable(test_stress tests/test_stress.cpp)
target_include_directories(test_stress PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(test_stress PRIVATE engine_ecs engine_physics engine_math)

add_executable(test_math3d tests/test_math3d.cpp)
target_link_libraries(test_math3d PRIVATE engine_math)

# ── Demo 3D ─────────────────────────────────────────────────────
add_executable(demo_3d src/demo_3d.cpp)
target_compile_definitions(demo_3d PRIVATE SDL_MAIN_HANDLED)
target_include_directories(demo_3d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(demo_3d PRIVATE engine_editor engine_ecs engine_core engine_renderer engine_physics engine_math SDL2::SDL2)

# ── Benchmark 3D ────────────────────────────────────────────────
# Escena de stress test 250 objetos físicos — arranca sin juego principal
add_executable(benchmark_3d src/benchmark.cpp src/game/BenchmarkScene3D.cpp src/game/WorldScene3D.cpp)
target_compile_definitions(benchmark_3d PRIVATE SDL_MAIN_HANDLED)
target_include_directories(benchmark_3d PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(benchmark_3d
    PRIVATE
        engine_ecs
        engine_core
        engine_renderer
        engine_physics
        engine_math
        SDL2::SDL2
)
