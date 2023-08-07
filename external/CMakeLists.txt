# Dependencies
set(SDL2_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SDL2")
find_package(SDL2 REQUIRED)

# D3D12
find_package(D3D12 REQUIRED)

# GLM
# message(STATUS "Installing glm via submodule")
add_subdirectory(glm)

# TinyOBJLoader
add_library(tinyobj STATIC tiny_obj_loader.cc)
target_include_directories(tinyobj PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
