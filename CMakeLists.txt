cmake_minimum_required(VERSION 3.18)
project(ProjectWolf)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(SDL3_DISABLE_SHARED ON)

# ==== GLM Setup ====
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE external/glm)


# ==== SHADERCROSS Setup ====

set(SDLSHADERCROSS_USE_SYSTEM_SDL OFF)
set(SDLSHADERCROSS_VENDORED ON)
set(SDLSHADERCROSS_SPIRVCROSS_SHARED OFF)
set(SDLSHADERCROSS_DXC ON)
set(SDLSHADERCROSS_CLI ON)
set(SDLSHADERCROSS_CLI_STATIC ON)
set(SDLSHADERCROSS_INSTALL OFF)
set(SDLSHADERCROSS_BUILD_TESTS OFF)

add_subdirectory(external/SDL)

# Make sure SDL_shadercross can find the SDL3 targets we just built,
# without falling back to find_package().
if(NOT TARGET SDL3::SDL3-static AND TARGET SDL3-static)
    add_library(SDL3::SDL3-static ALIAS SDL3-static)
endif()
if(NOT TARGET SDL3::SDL3 AND TARGET SDL3-static)
    add_library(SDL3::SDL3 ALIAS SDL3-static)
endif()
if(NOT TARGET SDL3::Headers AND TARGET SDL3-static)
    add_library(SDL3::Headers ALIAS SDL3-static)
endif()

add_subdirectory(external/SDL_shadercross)

set(SOURCES
    src/main.cpp
    src/app.cpp
    src/game.cpp
    src/player.cpp
    src/editor/mapio.cpp
)

set(HEADERS
    src/app.h
    src/game.h
    src/player.h
    src/editor/mapio.h
)

set(EDITOR_SOURCES
    src/editor/editor_main.cpp
    src/editor/editor_app.cpp
    src/editor/mapio.cpp
)

set(EDITOR_HEADERS
    src/editor/editor_app.h
    src/editor/mapio.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3 glm)

add_executable(editor ${EDITOR_SOURCES} ${EDITOR_HEADERS})
target_link_libraries(editor PRIVATE SDL3::SDL3 glm)

# On macOS app bundles SDL_GetBasePath() returns Contents/Resources/;
# on other platforms it returns the directory containing the executable.
if(APPLE)
    set(RUNTIME_DATA_DIR "$<TARGET_BUNDLE_CONTENT_DIR:${PROJECT_NAME}>/Resources")
else()
    set(RUNTIME_DATA_DIR "$<TARGET_FILE_DIR:${PROJECT_NAME}>")
endif()

# Stage runtime data where SDL_GetBasePath() will find them.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "${RUNTIME_DATA_DIR}/assets"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${RUNTIME_DATA_DIR}/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_SOURCE_DIR}/src/shaders/TexturedQuad.vert.spv"
        "${CMAKE_SOURCE_DIR}/src/shaders/TexturedQuad.frag.spv"
        "${CMAKE_SOURCE_DIR}/src/shaders/TexturedQuad.vert.msl"
        "${CMAKE_SOURCE_DIR}/src/shaders/TexturedQuad.frag.msl"
        "${CMAKE_SOURCE_DIR}/src/shaders/TexturedQuad.vert.dxil"
        "${CMAKE_SOURCE_DIR}/src/shaders/TexturedQuad.frag.dxil"
        "${RUNTIME_DATA_DIR}/shaders/")

if (WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE_DIR:SDL3::SDL3>/SDL3.dll"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endif()

if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.${PROJECT_NAME}
    MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}")
endif()
