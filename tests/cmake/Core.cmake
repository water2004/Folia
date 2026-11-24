# Ordinary portable editor/model coverage and platform-free key policies.
if(NOT FOLIA_CORE_TEST_TARGET)
    set(FOLIA_CORE_TEST_TARGET FoliaCoreTests)
endif()

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

add_executable(${FOLIA_CORE_TEST_TARGET} "${FOLIA_SOURCE_ROOT}/tests/main.cpp"
    ${FOLIA_CORE_TEST_SOURCE})
target_compile_features(${FOLIA_CORE_TEST_TARGET} PRIVATE cxx_std_23)
set_target_properties(${FOLIA_CORE_TEST_TARGET} PROPERTIES
    CXX_MODULE_STD ON CXX_SCAN_FOR_MODULES ON)
target_include_directories(${FOLIA_CORE_TEST_TARGET} PRIVATE
    "${FOLIA_SOURCE_ROOT}/tests")
target_compile_definitions(${FOLIA_CORE_TEST_TARGET} PRIVATE
    FOLIA_TEST_DATA_ROOT="${FOLIA_SOURCE_ROOT}/tests")
target_link_libraries(${FOLIA_CORE_TEST_TARGET} PRIVATE folia_core Boost::ut_module folia_test_support)

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
target_sources(${FOLIA_CORE_TEST_TARGET} PRIVATE
    "${FOLIA_SOURCE_ROOT}/tests/platform/test_editor_input_command.cpp"
    "${FOLIA_SOURCE_ROOT}/tests/platform/test_editor_shortcuts.cpp"
    "${FOLIA_SOURCE_ROOT}/tests/platform/test_editor_shortcut_settings.cpp")
target_link_libraries(${FOLIA_CORE_TEST_TARGET} PRIVATE folia_test_editor_input)

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
target_link_libraries(${FOLIA_CORE_TEST_TARGET} PRIVATE folia_test_math_regexp)


if(MSVC)
    foreach(folia_target IN ITEMS folia_core folia_test_math_regexp folia_test_editor_input ${FOLIA_CORE_TEST_TARGET})
        target_compile_options(${folia_target} PRIVATE /utf-8 /permissive- /Zc:preprocessor)
    endforeach()
    target_link_libraries(${FOLIA_CORE_TEST_TARGET} PRIVATE folia_console_test_runtime)
    target_link_options(${FOLIA_CORE_TEST_TARGET} PRIVATE /STACK:16777216)
endif()

add_test(NAME ${FOLIA_CORE_TEST_TARGET} COMMAND ${FOLIA_CORE_TEST_TARGET})
set_tests_properties(${FOLIA_CORE_TEST_TARGET} PROPERTIES LABELS "core;portable")

include("${CMAKE_CURRENT_LIST_DIR}/PortablePolicies.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/MarkdownConsumer.cmake")
