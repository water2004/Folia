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

add_subdirectory("${FOLIA_SOURCE_ROOT}/src/markdown" markdown)
add_library(folia_core STATIC)
target_compile_features(folia_core PUBLIC cxx_std_23)
set_target_properties(folia_core PROPERTIES CXX_MODULE_STD ON)
target_include_directories(folia_core PRIVATE "${FOLIA_SOURCE_ROOT}/third_party/srell")
target_link_libraries(folia_core PUBLIC folia::markdown)
file(GLOB_RECURSE FOLIA_CORE_MODULE CONFIGURE_DEPENDS
    "${FOLIA_SOURCE_ROOT}/src/core/*.ixx")
file(GLOB_RECURSE FOLIA_CORE_SOURCE CONFIGURE_DEPENDS
    "${FOLIA_SOURCE_ROOT}/src/core/*.cpp")
target_sources(folia_core PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS "${FOLIA_SOURCE_ROOT}/src/core" FILES ${FOLIA_CORE_MODULE})
target_sources(folia_core PRIVATE ${FOLIA_CORE_SOURCE})

set(BOOST_UT_DISABLE_MODULE OFF CACHE BOOL "Use the forked Boost.UT module" FORCE)
set(BOOST_UT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BOOST_UT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BOOST_UT_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${FOLIA_SOURCE_ROOT}/third_party/ut" boost-ut EXCLUDE_FROM_ALL)

file(GLOB_RECURSE FOLIA_CORE_TEST_SOURCE CONFIGURE_DEPENDS
    "${FOLIA_SOURCE_ROOT}/tests/foundation/*.cpp"
    "${FOLIA_SOURCE_ROOT}/tests/parsing/*.cpp"
    "${FOLIA_SOURCE_ROOT}/tests/editing/*.cpp"
    "${FOLIA_SOURCE_ROOT}/tests/rendering/*.cpp"
    "${FOLIA_SOURCE_ROOT}/tests/query/*.cpp")
# The same generated conformance/stress executables are separate from the
# ordinary core runner in the root build. Their fixtures and timeouts differ.
list(FILTER FOLIA_CORE_TEST_SOURCE EXCLUDE REGEX
    "/tests/parsing/test_(commonmark|gfm|gfm_pathological|inline_cst_ownership|deep_block_ownership|serializer_deep|md4c_reference)\\.cpp$")

add_executable(FoliaCoreTests "${FOLIA_SOURCE_ROOT}/tests/main.cpp"
    ${FOLIA_CORE_TEST_SOURCE})
target_compile_features(FoliaCoreTests PRIVATE cxx_std_23)
set_target_properties(FoliaCoreTests PROPERTIES
    CXX_MODULE_STD ON CXX_SCAN_FOR_MODULES ON)
target_include_directories(FoliaCoreTests PRIVATE
    "${FOLIA_SOURCE_ROOT}/tests" "${CMAKE_CURRENT_BINARY_DIR}/gfm-entities")
target_link_libraries(FoliaCoreTests PRIVATE folia_core Boost::ut_module)

# These deterministic Windows-key adapters contain no Windows APIs. Exercise
# the production shortcut catalog and dispatch policy on the portable runner
# as well, so a global binding cannot silently consume an unsupported command.
add_library(folia_test_editor_input STATIC)
target_sources(folia_test_editor_input PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS "${FOLIA_SOURCE_ROOT}/src/platform/windows/editor/interaction"
    FILES
        "${FOLIA_SOURCE_ROOT}/src/platform/windows/editor/interaction/editor_input_command.ixx"
        "${FOLIA_SOURCE_ROOT}/src/platform/windows/editor/interaction/editor_shortcuts.ixx"
        "${FOLIA_SOURCE_ROOT}/src/platform/windows/editor/interaction/editor_shortcut_settings.ixx")
target_compile_features(folia_test_editor_input PUBLIC cxx_std_23)
set_target_properties(folia_test_editor_input PROPERTIES CXX_MODULE_STD ON)
target_link_libraries(folia_test_editor_input PUBLIC folia_core)
target_sources(FoliaCoreTests PRIVATE
    "${FOLIA_SOURCE_ROOT}/tests/platform/test_editor_input_command.cpp"
    "${FOLIA_SOURCE_ROOT}/tests/platform/test_editor_shortcuts.cpp"
    "${FOLIA_SOURCE_ROOT}/tests/platform/test_editor_shortcut_settings.cpp")
target_link_libraries(FoliaCoreTests PRIVATE folia_test_editor_input)

# One search regression deliberately loads the math regex host in the same
# process to verify the two regex policies remain independent. This narrow
# utility has no renderer/font dependency, and is compiled unchanged.
add_library(folia_test_math_regexp STATIC
    "${FOLIA_SOURCE_ROOT}/src/math/util/regexp_host.cpp")
target_sources(folia_test_math_regexp PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS "${FOLIA_SOURCE_ROOT}/src/math/util"
    FILES "${FOLIA_SOURCE_ROOT}/src/math/util/regexp_host.ixx")
target_compile_features(folia_test_math_regexp PUBLIC cxx_std_23)
set_target_properties(folia_test_math_regexp PROPERTIES CXX_MODULE_STD ON)
target_include_directories(folia_test_math_regexp PRIVATE
    "${FOLIA_SOURCE_ROOT}/third_party/srell")
target_link_libraries(FoliaCoreTests PRIVATE folia_test_math_regexp)

set(FOLIA_GFM_ENTITY_CASES "${CMAKE_CURRENT_BINARY_DIR}/gfm-entities/gfm_entity_cases.inc")
add_custom_command(OUTPUT "${FOLIA_GFM_ENTITY_CASES}"
    COMMAND "${Python3_EXECUTABLE}"
        "${FOLIA_SOURCE_ROOT}/tools/markdown-conformance/generate_pinned_gfm_entities.py"
        --fetch --output "${FOLIA_GFM_ENTITY_CASES}"
    DEPENDS "${FOLIA_SOURCE_ROOT}/tools/markdown-conformance/generate_pinned_gfm_entities.py"
        "${FOLIA_SOURCE_ROOT}/tools/markdown-conformance/gfm-sources.lock.json"
    VERBATIM)
add_custom_target(FoliaCoreEntityFixtures DEPENDS "${FOLIA_GFM_ENTITY_CASES}")
add_dependencies(FoliaCoreTests FoliaCoreEntityFixtures)

if(MSVC)
    foreach(folia_target IN ITEMS folia_core folia_test_math_regexp folia_test_editor_input FoliaCoreTests)
        target_compile_options(${folia_target} PRIVATE /utf-8 /permissive- /Zc:preprocessor)
    endforeach()
    add_library(folia_console_test_runtime OBJECT
        "${FOLIA_SOURCE_ROOT}/tests/support/windows_console_runtime.cpp")
    target_compile_features(folia_console_test_runtime PRIVATE cxx_std_23)
    set_target_properties(folia_console_test_runtime PROPERTIES
        CXX_MODULE_STD OFF CXX_SCAN_FOR_MODULES OFF)
    target_link_libraries(FoliaCoreTests PRIVATE folia_console_test_runtime)
    target_link_options(FoliaCoreTests PRIVATE /STACK:16777216)
endif()

enable_testing()
add_test(NAME FoliaCoreTests COMMAND FoliaCoreTests)
set_tests_properties(FoliaCoreTests PROPERTIES LABELS "core;portable")

option(FOLIA_BUILD_SCALABILITY_BENCHMARK "Build the synthetic document benchmark" OFF)
if(FOLIA_BUILD_SCALABILITY_BENCHMARK)
    add_executable(FoliaCoreScalability
        "${FOLIA_DEFAULT_SOURCE_ROOT}/tools/document-scalability/core_scalability.cpp")
    target_compile_features(FoliaCoreScalability PRIVATE cxx_std_23)
    set_target_properties(FoliaCoreScalability PROPERTIES CXX_MODULE_STD ON)
    target_link_libraries(FoliaCoreScalability PRIVATE folia_core)
    if(MSVC)
        target_compile_options(FoliaCoreScalability PRIVATE /utf-8)
        target_link_libraries(FoliaCoreScalability PRIVATE folia_console_test_runtime)
    endif()
    add_executable(FoliaMemoryProfile
        "${FOLIA_DEFAULT_SOURCE_ROOT}/tools/document-scalability/memory_profile.cpp")
    target_compile_features(FoliaMemoryProfile PRIVATE cxx_std_23)
    set_target_properties(FoliaMemoryProfile PROPERTIES CXX_MODULE_STD ON)
    target_link_libraries(FoliaMemoryProfile PRIVATE folia_core)
    if(MSVC)
        target_compile_options(FoliaMemoryProfile PRIVATE /utf-8)
        target_link_libraries(FoliaMemoryProfile PRIVATE folia_console_test_runtime)
    endif()
endif()
