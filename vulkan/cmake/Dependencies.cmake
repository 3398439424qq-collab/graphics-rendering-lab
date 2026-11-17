# Public build wiring. Application and shader sources retain the source snapshot.
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "This snapshot supports native Windows MSVC x64 only.")
endif()

# An empty root lets FindVulkan use VULKAN_SDK and its normal search paths.
set(VK01_SDK_ROOT "" CACHE PATH "Optional Vulkan SDK root (contains Include, Lib and Bin)")
if(VK01_SDK_ROOT)
  set(Vulkan_INCLUDE_DIR "${VK01_SDK_ROOT}/Include" CACHE PATH "Vulkan headers" FORCE)
  set(Vulkan_LIBRARY "${VK01_SDK_ROOT}/Lib/vulkan-1.lib" CACHE FILEPATH "Vulkan loader import library" FORCE)
  set(Vulkan_GLSLC_EXECUTABLE "${VK01_SDK_ROOT}/Bin/glslc.exe" CACHE FILEPATH "GLSL compiler" FORCE)
  foreach(required IN ITEMS
      "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.h"
      "${Vulkan_LIBRARY}"
      "${Vulkan_GLSLC_EXECUTABLE}")
    if(NOT EXISTS "${required}")
      message(FATAL_ERROR "Vulkan SDK file not found: ${required}")
    endif()
  endforeach()
endif()
find_package(Vulkan REQUIRED COMPONENTS glslc)

# Accept a Windows binary development package or a CMake config package.
set(VK01_GLFW_ROOT "" CACHE PATH "GLFW root containing include/ and lib-vc2022/ or lib/")
if(VK01_GLFW_ROOT)
  find_path(VK01_GLFW_INCLUDE_DIR GLFW/glfw3.h
    PATHS "${VK01_GLFW_ROOT}/include" NO_DEFAULT_PATH REQUIRED)
  find_library(VK01_GLFW_LIBRARY NAMES glfw3
    PATHS "${VK01_GLFW_ROOT}/lib-vc2022" "${VK01_GLFW_ROOT}/lib"
    NO_DEFAULT_PATH REQUIRED)
  add_library(vk01_glfw STATIC IMPORTED)
  set_target_properties(vk01_glfw PROPERTIES
    IMPORTED_LOCATION "${VK01_GLFW_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${VK01_GLFW_INCLUDE_DIR}")
else()
  find_package(glfw3 CONFIG QUIET)
  add_library(vk01_glfw INTERFACE)
  if(TARGET glfw)
    target_link_libraries(vk01_glfw INTERFACE glfw)
  elseif(TARGET glfw3::glfw)
    target_link_libraries(vk01_glfw INTERFACE glfw3::glfw)
  else()
    message(FATAL_ERROR
      "GLFW was not found. Set VK01_GLFW_ROOT to an x64 MSVC GLFW package, "
      "or make its CMake package available through CMAKE_PREFIX_PATH/glfw3_DIR.")
  endif()
endif()
