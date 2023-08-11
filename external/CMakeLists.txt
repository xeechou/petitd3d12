# GLM
# message(STATUS "Installing glm via submodule")
add_subdirectory(glm)

# TinyOBJLoader
add_library(tinyobj STATIC tiny_obj_loader.cc)
target_include_directories(tinyobj PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
