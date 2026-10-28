cmake_minimum_required(VERSION 4.3)

# This gate is pinned to CMake 4.3 and must precede CXX toolchain discovery.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(folia_mermaid_cpp LANGUAGES CXX)
    enable_testing()
endif()

if(NOT CMAKE_GENERATOR MATCHES "^Ninja")
    message(FATAL_ERROR "The Mermaid C++ module build requires Ninja.")
endif()
if(NOT "23" IN_LIST CMAKE_CXX_COMPILER_IMPORT_STD)
    message(FATAL_ERROR "The selected C++ toolchain does not support import std for C++23.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
get_filename_component(FOLIA_MERMAID_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(FOLIA_MERMAID_SOURCE_DIR "${FOLIA_MERMAID_REPO_ROOT}/third_party/mermaid-cpp/src")

if(MSVC)
    add_compile_options(/utf-8 /permissive- /W3 /Zc:preprocessor /Zc:__cplusplus)
endif()

# The same target is available from this standalone project and add_subdirectory.
# Source discovery is confined to this library; no MathJax generator is invoked.
file(GLOB_RECURSE FOLIA_MERMAID_MODULES CONFIGURE_DEPENDS
    "${FOLIA_MERMAID_SOURCE_DIR}/*.ixx")
file(GLOB_RECURSE FOLIA_MERMAID_SOURCES CONFIGURE_DEPENDS
    "${FOLIA_MERMAID_SOURCE_DIR}/*.cpp")
if(NOT FOLIA_MERMAID_MODULES)
    message(FATAL_ERROR "No Mermaid C++ module interfaces were found in ${FOLIA_MERMAID_SOURCE_DIR}.")
endif()
add_library(folia_mermaid STATIC)
target_compile_features(folia_mermaid PUBLIC cxx_std_23)
set_target_properties(folia_mermaid PROPERTIES
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON)
target_sources(folia_mermaid PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS "${FOLIA_MERMAID_SOURCE_DIR}"
    FILES ${FOLIA_MERMAID_MODULES})
target_sources(folia_mermaid PRIVATE ${FOLIA_MERMAID_SOURCES})

# Grammar only: schema resolution is owned by folia.mermaid.frontmatter.
# Keep this native dependency pinned independently of the JS test oracle.
include(FetchContent)
find_package(Git REQUIRED)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
set(YAML_CPP_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(folia_yaml_cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 56e3bb550c91fd7005566f19c079cb7a503223cf
    PATCH_COMMAND "${GIT_EXECUTABLE}" apply --ignore-space-change
        "${CMAKE_CURRENT_LIST_DIR}/yaml-cpp-json-escapes.patch")
FetchContent_MakeAvailable(folia_yaml_cpp)
set_target_properties(yaml-cpp PROPERTIES CXX_MODULE_STD OFF CXX_SCAN_FOR_MODULES OFF)
target_link_libraries(folia_mermaid PRIVATE yaml-cpp::yaml-cpp)

if(NOT TARGET Boost::ut_module)
    set(BOOST_UT_DISABLE_MODULE OFF CACHE BOOL "Build Boost.UT's C++ module" FORCE)
    set(BOOST_UT_BUILD_EXAMPLES OFF CACHE BOOL "Do not build Boost.UT examples" FORCE)
    set(BOOST_UT_BUILD_TESTS OFF CACHE BOOL "Do not build Boost.UT self-tests" FORCE)
    set(BOOST_UT_BUILD_BENCHMARKS OFF CACHE BOOL "Do not build Boost.UT benchmarks" FORCE)
    set(BOOST_UT_ENABLE_INSTALL OFF CACHE BOOL "Do not install vendored Boost.UT" FORCE)
    add_subdirectory("${FOLIA_MERMAID_REPO_ROOT}/third_party/ut"
        "${CMAKE_CURRENT_BINARY_DIR}/boost-ut" EXCLUDE_FROM_ALL)
endif()

if(WIN32 AND MSVC AND NOT TARGET folia_console_test_runtime)
    add_library(folia_console_test_runtime OBJECT
        "${FOLIA_MERMAID_REPO_ROOT}/tools/test-support/windows_console_runtime.cpp")
    target_compile_features(folia_console_test_runtime PRIVATE cxx_std_23)
    set_target_properties(folia_console_test_runtime PROPERTIES
        CXX_MODULE_STD OFF
        CXX_SCAN_FOR_MODULES OFF)
endif()

file(GLOB_RECURSE FOLIA_MERMAID_TEST_SOURCES CONFIGURE_DEPENDS
    "${FOLIA_MERMAID_REPO_ROOT}/tests/mermaid_cpp/*.cpp")
if(NOT FOLIA_MERMAID_TEST_SOURCES)
    message(FATAL_ERROR "No native Mermaid tests were found; refusing to build an empty test runner.")
endif()
add_executable(FoliaMermaidTests
    "${FOLIA_MERMAID_REPO_ROOT}/tests/main.cpp"
    ${FOLIA_MERMAID_TEST_SOURCES})
target_compile_features(FoliaMermaidTests PRIVATE cxx_std_23)
set_target_properties(FoliaMermaidTests PROPERTIES
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON)
target_include_directories(FoliaMermaidTests PRIVATE "${FOLIA_MERMAID_REPO_ROOT}/tests")
target_compile_definitions(FoliaMermaidTests PRIVATE
    FOLIA_MERMAID_FIXTURE_DIR="${FOLIA_MERMAID_REPO_ROOT}/tests/mermaid_cpp/fixtures")
target_link_libraries(FoliaMermaidTests PRIVATE folia_mermaid Boost::ut_module)
if(WIN32 AND MSVC)
    # The parent directory's executable enumeration does not cover this target.
    target_link_libraries(FoliaMermaidTests PRIVATE folia_console_test_runtime)
endif()
add_test(NAME FoliaMermaidTests COMMAND FoliaMermaidTests)

if(WIN32 AND MSVC)
    if(TARGET folia_platform)
        # The root project's platform glob already owns this module. Reuse it
        # instead of providing the same named module from two static libraries.
        add_library(folia_mermaid_measurement INTERFACE)
        target_link_libraries(folia_mermaid_measurement INTERFACE folia_platform)
        target_link_libraries(folia_platform PUBLIC folia_mermaid)
    else()
        add_library(folia_mermaid_measurement STATIC)
        set_target_properties(folia_mermaid_measurement PROPERTIES CXX_MODULE_STD ON)
        target_sources(folia_mermaid_measurement PUBLIC FILE_SET CXX_MODULES
            BASE_DIRS "${FOLIA_MERMAID_REPO_ROOT}/src/platform/text" FILES
            "${FOLIA_MERMAID_REPO_ROOT}/src/platform/text/mermaid_measurer.ixx"
            "${FOLIA_MERMAID_REPO_ROOT}/src/platform/text/mermaid_renderer.ixx"
            "${FOLIA_MERMAID_REPO_ROOT}/src/platform/text/mermaid_host.ixx"
            "${FOLIA_MERMAID_REPO_ROOT}/src/platform/text/mermaid_pie.ixx")
        target_link_libraries(folia_mermaid_measurement PUBLIC folia_mermaid PRIVATE dwrite user32)
    endif()
    add_executable(FoliaMermaidTextProbe native-text-probe.cpp)
    set_target_properties(FoliaMermaidTextProbe PROPERTIES CXX_MODULE_STD ON)
    target_link_libraries(FoliaMermaidTextProbe PRIVATE
        folia_mermaid_measurement folia_console_test_runtime)
    add_executable(FoliaMermaidPlatformTests
        "${FOLIA_MERMAID_REPO_ROOT}/tests/main.cpp"
        "${FOLIA_MERMAID_REPO_ROOT}/tests/platform/mermaid_measurement.cpp"
        "${FOLIA_MERMAID_REPO_ROOT}/tests/platform/mermaid_xy.cpp"
        "${FOLIA_MERMAID_REPO_ROOT}/tests/platform/mermaid_sankey.cpp"
        "${FOLIA_MERMAID_REPO_ROOT}/tests/platform/mermaid_treemap.cpp"
        "${FOLIA_MERMAID_REPO_ROOT}/tests/platform/mermaid_journey.cpp"
        "${FOLIA_MERMAID_REPO_ROOT}/tests/platform/mermaid_timeline.cpp"
        "${FOLIA_MERMAID_REPO_ROOT}/tests/platform/mermaid_gantt.cpp"
        "${FOLIA_MERMAID_REPO_ROOT}/tests/platform/mermaid_git_graph.cpp"
        "${FOLIA_MERMAID_REPO_ROOT}/tests/platform/mermaid_mindmap.cpp"
        "${FOLIA_MERMAID_REPO_ROOT}/tests/platform/mermaid_block.cpp"
        "${FOLIA_MERMAID_REPO_ROOT}/tests/platform/mermaid_kanban.cpp"
        "${FOLIA_MERMAID_REPO_ROOT}/tests/platform/mermaid_state.cpp"
        "${FOLIA_MERMAID_REPO_ROOT}/tests/platform/mermaid_zenuml.cpp"
        "${FOLIA_MERMAID_REPO_ROOT}/tests/platform/mermaid_c4.cpp"
        "${FOLIA_MERMAID_REPO_ROOT}/tests/platform/mermaid_elk_flowchart.cpp"
        "${FOLIA_MERMAID_REPO_ROOT}/tests/platform/mermaid_architecture.cpp")
    set_target_properties(FoliaMermaidPlatformTests PROPERTIES CXX_MODULE_STD ON)
    target_include_directories(FoliaMermaidPlatformTests PRIVATE "${FOLIA_MERMAID_REPO_ROOT}/tests")
    target_link_libraries(FoliaMermaidPlatformTests PRIVATE
        folia_mermaid_measurement Boost::ut_module folia_console_test_runtime)
    add_test(NAME FoliaMermaidPlatformTests COMMAND FoliaMermaidPlatformTests)
    add_library(FoliaNativeMermaid SHARED native-renderer.cpp)
    set_target_properties(FoliaNativeMermaid PROPERTIES CXX_MODULE_STD ON)
    target_link_libraries(FoliaNativeMermaid PRIVATE folia_mermaid_measurement)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/FoliaNativeMermaid.def" CONTENT
"LIBRARY FoliaNativeMermaid
EXPORTS
FoliaNativeMermaidRender
FoliaNativeMermaidFree
")
    target_sources(FoliaNativeMermaid PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/FoliaNativeMermaid.def")
    add_executable(FoliaMermaidHostTests
        "${FOLIA_MERMAID_REPO_ROOT}/tests/main.cpp" native-renderer-test.cpp)
    set_target_properties(FoliaMermaidHostTests PROPERTIES CXX_MODULE_STD ON)
    target_include_directories(FoliaMermaidHostTests PRIVATE "${FOLIA_MERMAID_REPO_ROOT}/tests")
    target_link_libraries(FoliaMermaidHostTests PRIVATE
        FoliaNativeMermaid Boost::ut_module folia_console_test_runtime)
    add_test(NAME FoliaMermaidHostTests COMMAND FoliaMermaidHostTests)
endif()
