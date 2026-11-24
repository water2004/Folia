# The complete native MathJax suite plus the focused embedding boundary.
file(GLOB_RECURSE FOLIA_MATH_TEST_SOURCES CONFIGURE_DEPENDS
    "${FOLIA_SOURCE_ROOT}/tests/math/*.cpp")
list(FILTER FOLIA_MATH_TEST_SOURCES EXCLUDE REGEX "/editor_syntax[.]cpp$")
if(NOT FOLIA_MATH_TEST_SOURCES)
    message(FATAL_ERROR "The math suite has no sources")
endif()
add_executable(FoliaMathTests EXCLUDE_FROM_ALL
    "${FOLIA_SOURCE_ROOT}/tests/main.cpp" ${FOLIA_MATH_TEST_SOURCES})
target_compile_features(FoliaMathTests PRIVATE cxx_std_23)
set_target_properties(FoliaMathTests PROPERTIES CXX_MODULE_STD ON CXX_SCAN_FOR_MODULES ON)

# Replay the retained numeric Source corpus against the original typed utility.
# This unused application utility is compiled only for its mathematical tests.
target_sources(FoliaMathTests PRIVATE ${FOLIA_SOURCE_ROOT}/src/math/util/numeric.cpp)
target_sources(FoliaMathTests PRIVATE
    FILE_SET CXX_MODULES BASE_DIRS "${FOLIA_SOURCE_ROOT}" FILES
    ${FOLIA_SOURCE_ROOT}/src/math/util/numeric.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/ordered_map_storage.ixx)
target_include_directories(FoliaMathTests PRIVATE ${FOLIA_SOURCE_ROOT}/tests)
target_compile_definitions(FoliaMathTests PRIVATE
    FOLIA_TEST_DATA_ROOT="${FOLIA_SOURCE_ROOT}/tests"
    FOLIA_MATH_FIXTURE_DIR="${FOLIA_SOURCE_ROOT}/tests/math/fixtures")
target_link_libraries(FoliaMathTests PRIVATE folia_math folia::math_editor FoliaNativeMath Boost::ut_module folia_test_support)
add_test(NAME FoliaMathTests COMMAND FoliaMathTests)
# Focused native SVG/C ABI gate for embedders such as Mermaid. The same tests
# remain in FoliaMathTests; this explicit target avoids compiling unrelated
# TeX parser test translation units for an output-boundary change.
file(GLOB FOLIA_MATH_SVG_WRITER_TEST_SOURCES CONFIGURE_DEPENDS
    "${FOLIA_SOURCE_ROOT}/tests/math/output/svg/native_svg_writer*.cpp")
add_executable(FoliaMathSvgBridgeTests EXCLUDE_FROM_ALL
    ${FOLIA_SOURCE_ROOT}/tests/main.cpp ${FOLIA_MATH_SVG_WRITER_TEST_SOURCES}
    ${FOLIA_SOURCE_ROOT}/tests/math/integration/native_renderer_bridge.cpp
    ${FOLIA_SOURCE_ROOT}/tests/math/output/svg/native_source_svg_paint.cpp ${FOLIA_SOURCE_ROOT}/tests/math/output/common/native_common_wrapper.cpp
    ${FOLIA_SOURCE_ROOT}/tests/math/output/svg/native_svg_merror.cpp ${FOLIA_SOURCE_ROOT}/tests/math/output/common/native_table_profile_source.cpp
    ${FOLIA_SOURCE_ROOT}/tests/math/output/common/native_common_table.cpp ${FOLIA_SOURCE_ROOT}/tests/math/output/common/native_common_text.cpp)
target_compile_features(FoliaMathSvgBridgeTests PRIVATE cxx_std_23)
set_target_properties(FoliaMathSvgBridgeTests PROPERTIES CXX_MODULE_STD ON CXX_SCAN_FOR_MODULES ON)
target_include_directories(FoliaMathSvgBridgeTests PRIVATE ${FOLIA_SOURCE_ROOT}/tests)
target_compile_definitions(FoliaMathSvgBridgeTests PRIVATE
    FOLIA_TEST_DATA_ROOT="${FOLIA_SOURCE_ROOT}/tests")
target_link_libraries(FoliaMathSvgBridgeTests PRIVATE folia_math FoliaNativeMath Boost::ut_module folia_test_support)
add_executable(FoliaMathFontDump EXCLUDE_FROM_ALL
    ${FOLIA_SOURCE_ROOT}/tools/math-port/font-data-dump.cpp)
target_compile_features(FoliaMathFontDump PRIVATE cxx_std_23)
set_target_properties(FoliaMathFontDump PROPERTIES CXX_MODULE_STD ON CXX_SCAN_FOR_MODULES ON)
target_link_libraries(FoliaMathFontDump PRIVATE folia_math)
if(MSVC)
    target_link_options(FoliaMathTests PRIVATE /DEBUG:NONE /INCREMENTAL:NO)
    target_link_options(FoliaMathTests PRIVATE /STACK:16777216)
    target_link_options(FoliaMathFontDump PRIVATE /DEBUG:NONE /INCREMENTAL:NO)
endif()

set_tests_properties(FoliaMathTests PROPERTIES LABELS "math;portable")
add_test(NAME FoliaMathSvgBridgeTests COMMAND FoliaMathSvgBridgeTests)
set_tests_properties(FoliaMathSvgBridgeTests PROPERTIES LABELS "math-bridge;portable")
