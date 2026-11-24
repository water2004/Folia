# Standalone, platform-independent core gate. Deliberately do not configure the
# root project: editor/model tests do not need MathJax font generation or WinUI.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
cmake_minimum_required(VERSION 4.3)
project(FoliaCoreTests LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_GENERATOR MATCHES "Ninja")
    message(FATAL_ERROR "Use -G Ninja for C++ module dependency scanning.")
endif()
if(NOT "23" IN_LIST CMAKE_CXX_COMPILER_IMPORT_STD)
    message(FATAL_ERROR "This compiler/standard library cannot provide C++23 import std. Use a supported Clang/libc++ or MSVC toolchain; no header-only fallback is used.")
endif()

get_filename_component(FOLIA_DEFAULT_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(FOLIA_SOURCE_ROOT "${FOLIA_DEFAULT_SOURCE_ROOT}" CACHE PATH
    "Production source checkout (override for same-toolchain baseline measurements)")
find_package(Python3 REQUIRED COMPONENTS Interpreter)

include("${FOLIA_DEFAULT_SOURCE_ROOT}/cmake/FoliaCore.cmake")

enable_testing()
include("${FOLIA_DEFAULT_SOURCE_ROOT}/tests/cmake/Support.cmake")
include("${FOLIA_DEFAULT_SOURCE_ROOT}/tests/cmake/Core.cmake")
include("${FOLIA_DEFAULT_SOURCE_ROOT}/tests/cmake/Benchmarks.cmake")
