# Project Info

cmake_minimum_required(VERSION 3.6 FATAL_ERROR)
cmake_policy(VERSION 3.6)
project(PetitD3D12
        VERSION 1.0.0.0
        LANGUAGES C CXX
)

# =============================================================

# CMake Settings

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(CMAKE_SUPPRESS_REGENERATION true)
set(DCMAKE_GENERATOR_PLATFORM "x64")
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
SET(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
SET(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)
if(NOT CMAKE_DEBUG_POSTFIX)
  set(CMAKE_DEBUG_POSTFIX d)
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

# =============================================================

# Dependencies
set(SDL2_DIR "${PROJECT_SOURCE_DIR}/external/SDL2")
find_package(SDL2 REQUIRED)
find_package(D3D12 REQUIRED)

# GLM
# message(STATUS "Installing glm via submodule")
set(BUILD_STATIC_LIBS ON)
add_subdirectory(external/glm)


# TinyOBJLoader
add_library(tinyobj STATIC external/tiny_obj_loader.cc)
target_include_directories(tinyobj PUBLIC external)

# =============================================================


# set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})


# =============================================================
# Another App

add_executable(sdl2main
  src/sdl2main.cpp
  src/renderer1.cpp)

target_link_libraries(sdl2main
  glm::glm
  tinyobj
  ${D3D12_LIBRARIES}
  ${SDL2_LIBRARY}
  ${SDL2MAIN_LIBRARY}
  )
target_compile_definitions(sdl2main PUBLIC -DGLM_FORCE_LEFT_HANDED)

target_include_directories(sdl2main PUBLIC
  ${SDL2_INCLUDE_DIR}
  ${D3D12_INCLUDE_DIRS}
)

set_property(TARGET sdl2main PROPERTY VS_DEBUGGER_WORKING_DIRECTORY
  ${CMAKE_BINARY_DIR}/..)
set_target_properties(sdl2main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# =============================================================

# Finish Settings

