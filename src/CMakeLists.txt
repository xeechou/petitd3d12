# =============================================================
# d3d12helper

add_library(d3d12helper STATIC
  window.cpp
  application.cpp
  commandqueue.cpp
  clock.cpp)

target_link_libraries(d3d12helper PUBLIC
  ${D3D12_LIBRARIES}
  ${SDL2_LIBRARY})

target_include_directories(d3d12helper PUBLIC
  ${SDL2_INCLUDE_DIR}
  ${D3D12_INCLUDE_DIRS})

# =============================================================
####### Shaders

set(SHADER_FILES
  shaders/VertexShader.hlsl
  shaders/PixelShader.hlsl)

set_source_files_properties( shaders/VertexShader.hlsl PROPERTIES
  ShaderType "vs" #they also use ShaderType, ShaderModel
  ShaderModel "5_1")

set_source_files_properties( shaders/PixelShader.hlsl PROPERTIES
  ShaderType "ps"
  ShaderModel "5_1")

#you can also use

foreach(FILE ${SHADER_FILES})
  get_filename_component(FILE_WE ${FILE} NAME_WE)
  set(SHADER_OUTPUT_PATH "${CMAKE_BINARY_DIR}/${FILE_WE}.cso")
  set(SHADER_PDB_PATH    "${CMAKE_BINARY_DIR}/${FILE_WE}.pdb")
  list(APPEND CSO_SHADER_FILES ${SHADER_OUTPUT_PATH})
  get_source_file_property(shadertype  ${FILE} ShaderType)
  get_source_file_property(shadermodel ${FILE} ShaderModel)
  add_custom_command(OUTPUT ${SHADER_OUTPUT_PATH}
    COMMAND ${FXC_EXE} /nologo /Emain /T${shadertype}_${shadermodel} $<IF:$<CONFIG:DEBUG>,/Od,/O1> /Zi /Fo ${SHADER_OUTPUT_PATH} /Fd ${SHADER_PDB_PATH} ${FILE}
    MAIN_DEPENDENCY ${FILE}
    COMMENT "HLSL ${FILE}"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM)
endforeach(FILE)


# =============================================================
# cube app
add_executable(cube
  main.cpp
  cube.cpp
  ${CSO_SHADER_FILES})

target_link_libraries(cube
  ${SDL2_LIBRARY}
  ${SDL2MAIN_LIBRARY}
  ${D3D12_LIBRARIES}
  glm::glm
  d3d12helper)

target_include_directories(cube PUBLIC
  ${SDL2_INCLUDE_DIR}
  ${D3D12_INCLUDE_DIRS})

target_compile_definitions(cube PUBLIC
  -Dno_init_all)

set_target_properties(cube PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set_property(TARGET cube PROPERTY VS_DEBUGGER_WORKING_DIRECTORY
  ${CMAKE_BINARY_DIR}/bin)

# copy shaders to output dir of cube
add_custom_command(TARGET cube POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy ${CSO_SHADER_FILES} $<TARGET_FILE_DIR:cube>
  COMMAND_EXPAND_LISTS)
