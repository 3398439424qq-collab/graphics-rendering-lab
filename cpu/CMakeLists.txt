cmake_minimum_required(VERSION 3.20)
project(cpu_path_tracer LANGUAGES CXX)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Threads REQUIRED)

add_executable(rt src/main.cpp)
target_compile_features(rt PRIVATE cxx_std_17)
set_target_properties(rt PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(rt PRIVATE Threads::Threads)

if(MSVC)
  target_compile_options(rt PRIVATE /W4 /utf-8)
else()
  target_compile_options(rt PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
  add_test(NAME cpu_exam COMMAND rt --scene exam)
endif()
