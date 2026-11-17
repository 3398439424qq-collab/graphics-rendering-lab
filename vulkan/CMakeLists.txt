# 构建脚本：把全部模块编译为可执行文件 vk01（MSVC /W4 /WX），并在构建时用 glslc 把 shaders/ 编译成 SPIR-V。
cmake_minimum_required(VERSION 3.24)
project(vk01 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
include(cmake/Dependencies.cmake)

set(PROD_SOURCES
  device_info.cpp
  device_selection.cpp
  device.cpp
  enumeration.cpp
  instance.cpp
  main.cpp
  queue_families.cpp
  queue_family_selection.cpp
  surface.cpp
  window.cpp
  surface_support.cpp
  surface_capabilities.cpp
  swapchain_choices.cpp
  surface_options.cpp
  swapchain.cpp
  image_views.cpp
  render_targets.cpp
  shader_modules.cpp
  graphics_pipeline.cpp
  command_recording.cpp
  frame_presentation.cpp
  swapchain_generation.cpp
  buffer_resources.cpp
  buffer_upload.cpp
  image_resources.cpp
  image_upload.cpp
  texture_binding.cpp
  mesh_loading.cpp
  scene_config.cpp
  material_binding.cpp
)
add_executable(vk01 ${PROD_SOURCES})
target_link_libraries(vk01 PRIVATE
  Vulkan::Vulkan
  vk01_glfw
  user32 gdi32 shell32
)
target_compile_options(vk01 PRIVATE /W4 /WX /utf-8)
set_target_properties(vk01 PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

# Fixed AI-authored demo asset; window mode reads it relative to the build cwd.
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/assets/demo_mesh.obj"
  "${CMAKE_CURRENT_BINARY_DIR}/assets/demo_mesh.obj"
  COPYONLY
)
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/assets/example.scene"
  "${CMAKE_CURRENT_BINARY_DIR}/assets/example.scene"
  COPYONLY
)

# ---- 着色器编译：shaders/*.vert|frag → 构建目录 shaders/*.spv，vk01 依赖它们 ----
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")

# bootstrap.vert
set(SHADER_SOURCE      "${CMAKE_CURRENT_SOURCE_DIR}/shaders/bootstrap.vert")
set(SHADER_OUTPUT      "${SHADER_OUTPUT_DIR}/bootstrap.vert.spv")

# triangle.vert
set(SHADER_VERT_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.vert")
set(SHADER_VERT_OUTPUT "${SHADER_OUTPUT_DIR}/triangle.vert.spv")

# triangle.frag
set(SHADER_FRAG_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/triangle.frag")
set(SHADER_FRAG_OUTPUT "${SHADER_OUTPUT_DIR}/triangle.frag.spv")
# vertex_buffer.vert
set(SHADER_VERTEX_BUFFER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/vertex_buffer.vert")
set(SHADER_VERTEX_BUFFER_OUTPUT "${SHADER_OUTPUT_DIR}/vertex_buffer.vert.spv")

# textured.vert
set(SHADER_TEXTURED_VERT_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/textured.vert")
set(SHADER_TEXTURED_VERT_OUTPUT "${SHADER_OUTPUT_DIR}/textured.vert.spv")

# textured_mvp.vert
set(SHADER_TEXTURED_MVP_VERT_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/textured_mvp.vert")
set(SHADER_TEXTURED_MVP_VERT_OUTPUT "${SHADER_OUTPUT_DIR}/textured_mvp.vert.spv")

# textured.frag
set(SHADER_TEXTURED_FRAG_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/textured.frag")
set(SHADER_TEXTURED_FRAG_OUTPUT "${SHADER_OUTPUT_DIR}/textured.frag.spv")

# Codex mechanical build wiring; normal_debug.frag is learner-authored.
set(SHADER_NORMAL_DEBUG_FRAG_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/normal_debug.frag")
set(SHADER_NORMAL_DEBUG_FRAG_OUTPUT "${SHADER_OUTPUT_DIR}/normal_debug.frag.spv")


add_custom_command(
  OUTPUT  "${SHADER_OUTPUT}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
  COMMAND Vulkan::glslc "${SHADER_SOURCE}" --target-env=vulkan1.0 -o "${SHADER_OUTPUT}"
  DEPENDS "${SHADER_SOURCE}"
  COMMENT "Compiling bootstrap.vert"
  VERBATIM
)

add_custom_command(
  OUTPUT  "${SHADER_VERT_OUTPUT}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
  COMMAND Vulkan::glslc "${SHADER_VERT_SOURCE}" --target-env=vulkan1.0 -o "${SHADER_VERT_OUTPUT}"
  DEPENDS "${SHADER_VERT_SOURCE}"
  COMMENT "Compiling triangle.vert"
  VERBATIM
)

add_custom_command(
  OUTPUT  "${SHADER_FRAG_OUTPUT}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
  COMMAND Vulkan::glslc "${SHADER_FRAG_SOURCE}" --target-env=vulkan1.0 -o "${SHADER_FRAG_OUTPUT}"
  DEPENDS "${SHADER_FRAG_SOURCE}"
  COMMENT "Compiling triangle.frag"
  VERBATIM
)

add_custom_command(
  OUTPUT "${SHADER_VERTEX_BUFFER_OUTPUT}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
  COMMAND Vulkan::glslc "${SHADER_VERTEX_BUFFER_SOURCE}"
          --target-env=vulkan1.0 -o "${SHADER_VERTEX_BUFFER_OUTPUT}"
  DEPENDS "${SHADER_VERTEX_BUFFER_SOURCE}"
  COMMENT "Compiling vertex_buffer.vert"
  VERBATIM
)

add_custom_command(
  OUTPUT "${SHADER_TEXTURED_VERT_OUTPUT}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
  COMMAND Vulkan::glslc "${SHADER_TEXTURED_VERT_SOURCE}"
          --target-env=vulkan1.0 -o "${SHADER_TEXTURED_VERT_OUTPUT}"
  DEPENDS "${SHADER_TEXTURED_VERT_SOURCE}"
  COMMENT "Compiling textured.vert"
  VERBATIM
)

# Codex mechanical wiring; textured_mvp.vert main is learner-authored.
add_custom_command(
  OUTPUT "${SHADER_TEXTURED_MVP_VERT_OUTPUT}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
  COMMAND Vulkan::glslc "${SHADER_TEXTURED_MVP_VERT_SOURCE}"
          --target-env=vulkan1.0 -o "${SHADER_TEXTURED_MVP_VERT_OUTPUT}"
  DEPENDS "${SHADER_TEXTURED_MVP_VERT_SOURCE}"
  COMMENT "Compiling textured_mvp.vert"
  VERBATIM
)

add_custom_command(
  OUTPUT "${SHADER_TEXTURED_FRAG_OUTPUT}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
  COMMAND Vulkan::glslc "${SHADER_TEXTURED_FRAG_SOURCE}"
          --target-env=vulkan1.0 -o "${SHADER_TEXTURED_FRAG_OUTPUT}"
  DEPENDS "${SHADER_TEXTURED_FRAG_SOURCE}"
  COMMENT "Compiling textured.frag"
  VERBATIM
)

add_custom_command(
  OUTPUT "${SHADER_NORMAL_DEBUG_FRAG_OUTPUT}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
  COMMAND Vulkan::glslc "${SHADER_NORMAL_DEBUG_FRAG_SOURCE}"
          --target-env=vulkan1.0 -o "${SHADER_NORMAL_DEBUG_FRAG_OUTPUT}"
  DEPENDS "${SHADER_NORMAL_DEBUG_FRAG_SOURCE}"
  COMMENT "Compiling normal_debug.frag"
  VERBATIM
)
# textured_lit.frag
set(SHADER_TEXTURED_LIT_FRAG_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/textured_lit.frag")
set(SHADER_TEXTURED_LIT_FRAG_OUTPUT "${SHADER_OUTPUT_DIR}/textured_lit.frag.spv")

add_custom_command(
  OUTPUT "${SHADER_TEXTURED_LIT_FRAG_OUTPUT}"
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_OUTPUT_DIR}"
  COMMAND Vulkan::glslc "${SHADER_TEXTURED_LIT_FRAG_SOURCE}"
          --target-env=vulkan1.0 -o "${SHADER_TEXTURED_LIT_FRAG_OUTPUT}"
  DEPENDS "${SHADER_TEXTURED_LIT_FRAG_SOURCE}"
  COMMENT "Compiling textured_lit.frag"
  VERBATIM
)

add_custom_target(shaders ALL DEPENDS
    "${SHADER_OUTPUT}"
    "${SHADER_VERT_OUTPUT}"
    "${SHADER_FRAG_OUTPUT}"
    "${SHADER_VERTEX_BUFFER_OUTPUT}"
    "${SHADER_TEXTURED_VERT_OUTPUT}"
    "${SHADER_TEXTURED_MVP_VERT_OUTPUT}"
    "${SHADER_TEXTURED_FRAG_OUTPUT}"
    "${SHADER_NORMAL_DEBUG_FRAG_OUTPUT}"
    "${SHADER_TEXTURED_LIT_FRAG_OUTPUT}"
)
add_dependencies(vk01 shaders)
