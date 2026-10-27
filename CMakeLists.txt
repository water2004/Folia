# Folia (C++) — native self-rendered WYSIWYG Markdown editor.
#
# Build design:
#   - C++23 with C++ modules (CMake FILE_SET CXX_MODULES) + `import std;`.
#   - Core library `folia_core` is PURE C++: no Windows / DirectX / WinUI deps.
#   - Platform library `folia_platform` is the ONLY layer touching Win32 /
#     Direct2D / DirectWrite / DXGI / TSF / Clipboard (Windows headers stay
#     in that translation unit's global module fragment so no macros leak).
#   - App `Folia` is a WinUI 3 shell hosting a self-drawn EditorView surface.
#
# `import std;` is experimental in CMake 4.3; the UUID below is the official
# activation token for the CxxImportStd experimental feature in CMake 4.3.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

cmake_minimum_required(VERSION 4.0)

project(folia
    VERSION 0.5.1
    LANGUAGES C CXX
    DESCRIPTION "Folia: native self-rendered WYSIWYG Markdown editor (C++)")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# Require Ninja (needed for CMake's C++ module dependency scanning with MSVC).
if(NOT CMAKE_GENERATOR MATCHES "Ninja")
    message(FATAL_ERROR "Folia requires the Ninja generator for C++ module support. Reconfigure with -G Ninja.")
endif()

if(MSVC)
    # /utf-8: source and execution charset UTF-8 (we embed box-drawing chars).
    # /permissive-: strict standards conformance.
    add_compile_options(/utf-8 /permissive- /W3 /Zc:preprocessor /Zc:__cplusplus)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNDEFINED _UNICODE)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(/O2)
    endif()
endif()


option(FOLIA_BUILD_WINUI "Build the WinUI 3 C++/WinRT application shell" OFF)

if(FOLIA_BUILD_WINUI)
    message(FATAL_ERROR "The WinUI 3 shell is built with MSBuild at src/app-winui/Folia.vcxproj so XAML/IDL/C++WinRT generation runs through the Windows App SDK targets. Keep CMake for folia_core, folia_platform, and tests.")
endif()

# ---------------------------------------------------------------------------
# Core library — pure C++, portable. Acceptance gate: no Windows dependency.
# ---------------------------------------------------------------------------
add_subdirectory(src/markdown)

add_library(folia_core STATIC)
target_compile_features(folia_core PUBLIC cxx_std_23)
set_target_properties(folia_core PROPERTIES CXX_MODULE_STD ON)
target_include_directories(folia_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/srell)
target_link_libraries(folia_core PUBLIC folia::markdown)

file(GLOB_RECURSE FOLIA_CORE_MODULE CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.ixx)
file(GLOB_RECURSE FOLIA_CORE_SOURCE CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp)
target_sources(folia_core PUBLIC
    FILE_SET CXX_MODULES FILES ${FOLIA_CORE_MODULE})
target_sources(folia_core PRIVATE ${FOLIA_CORE_SOURCE})

# Native math reconstruction: only reviewed typed C++ owners are active.
# Parser/packages and shared Common placement use the accepted typed owners.
# The application-facing Renderer composes them without a Context runtime.
add_library(folia_math STATIC)
target_compile_features(folia_math PUBLIC cxx_std_23)
set_target_properties(folia_math PROPERTIES CXX_MODULE_STD ON)
target_include_directories(folia_math PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/srell)
# Font tables are generated offline from the pinned Source package data. The
# generator verifies every locked input and preserves unchanged output mtimes.
find_program(FOLIA_NODE_EXECUTABLE NAMES node REQUIRED)
execute_process(
    COMMAND ${FOLIA_NODE_EXECUTABLE} tools/math-port/font-data-generate-native.cjs
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE FOLIA_FONT_GENERATE_RESULT
    OUTPUT_VARIABLE FOLIA_FONT_GENERATE_OUTPUT
    ERROR_VARIABLE FOLIA_FONT_GENERATE_ERROR)
if(NOT FOLIA_FONT_GENERATE_RESULT EQUAL 0)
    message(FATAL_ERROR "Native font data generation failed: ${FOLIA_FONT_GENERATE_ERROR}")
endif()
set(FOLIA_FONT_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/math-port/font-generated-native-raii")
file(READ "${FOLIA_FONT_GENERATED_DIR}/font-data-source-dependencies.json" FOLIA_FONT_DEPENDENCIES)
string(JSON FOLIA_FONT_DEPENDENCY_COUNT LENGTH "${FOLIA_FONT_DEPENDENCIES}" source_dependencies)
math(EXPR FOLIA_FONT_DEPENDENCY_LAST "${FOLIA_FONT_DEPENDENCY_COUNT} - 1")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/math-port/font-data-generate-native.cjs"
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/math-port/evidence/font-data-source-lock.json")
foreach(FOLIA_FONT_INDEX RANGE 0 ${FOLIA_FONT_DEPENDENCY_LAST})
    string(JSON FOLIA_FONT_DEPENDENCY GET "${FOLIA_FONT_DEPENDENCIES}" source_dependencies ${FOLIA_FONT_INDEX} path)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${FOLIA_FONT_DEPENDENCY}")
endforeach()
string(JSON FOLIA_FONT_GENERATED_COUNT LENGTH "${FOLIA_FONT_DEPENDENCIES}" generated_cpp)
math(EXPR FOLIA_FONT_GENERATED_LAST "${FOLIA_FONT_GENERATED_COUNT} - 1")
set(FOLIA_FONT_GENERATED_CPP)
foreach(FOLIA_FONT_INDEX RANGE 0 ${FOLIA_FONT_GENERATED_LAST})
    string(JSON FOLIA_FONT_GENERATED_FILE GET "${FOLIA_FONT_DEPENDENCIES}" generated_cpp ${FOLIA_FONT_INDEX})
    if(FOLIA_FONT_GENERATED_FILE MATCHES "[.]cpp$")
        list(APPEND FOLIA_FONT_GENERATED_CPP "${CMAKE_CURRENT_SOURCE_DIR}/${FOLIA_FONT_GENERATED_FILE}")
    endif()
endforeach()
target_include_directories(folia_math PRIVATE ${FOLIA_FONT_GENERATED_DIR})
# Entity resources are local data, generated from all locked Source add() calls.
set(FOLIA_ENTITIES_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/entities")
execute_process(
    COMMAND ${FOLIA_NODE_EXECUTABLE} --max-old-space-size=512
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/math-port/entities-data-generate.cjs"
        --output "${FOLIA_ENTITIES_GENERATED_DIR}/entities_data.inc"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    RESULT_VARIABLE FOLIA_ENTITIES_GENERATE_RESULT
    OUTPUT_VARIABLE FOLIA_ENTITIES_GENERATE_OUTPUT
    ERROR_VARIABLE FOLIA_ENTITIES_GENERATE_ERROR)
if(NOT FOLIA_ENTITIES_GENERATE_RESULT EQUAL 0)
    message(FATAL_ERROR "Native entity data generation failed: ${FOLIA_ENTITIES_GENERATE_ERROR}")
endif()
set(FOLIA_ENTITIES_LOCK "${CMAKE_CURRENT_SOURCE_DIR}/docs/math-port/evidence/entities-source-lock.json")
file(READ "${FOLIA_ENTITIES_LOCK}" FOLIA_ENTITIES_DEPENDENCIES)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/math-port/entities-data-generate.cjs"
    "${FOLIA_ENTITIES_LOCK}")
foreach(FOLIA_ENTITIES_KIND IN ITEMS ts cjs)
    string(JSON FOLIA_ENTITIES_COUNT LENGTH "${FOLIA_ENTITIES_DEPENDENCIES}" source ${FOLIA_ENTITIES_KIND})
    math(EXPR FOLIA_ENTITIES_LAST "${FOLIA_ENTITIES_COUNT} - 1")
    if(FOLIA_ENTITIES_KIND STREQUAL "ts")
        set(FOLIA_ENTITIES_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.reference/MathJax-src/ts/util")
    else()
        set(FOLIA_ENTITIES_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/tools/mathjax-bundle/node_modules/@mathjax/src/cjs/util")
    endif()
    foreach(FOLIA_ENTITIES_INDEX RANGE 0 ${FOLIA_ENTITIES_LAST})
        string(JSON FOLIA_ENTITIES_FILE MEMBER "${FOLIA_ENTITIES_DEPENDENCIES}" source ${FOLIA_ENTITIES_KIND} ${FOLIA_ENTITIES_INDEX})
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${FOLIA_ENTITIES_ROOT}/${FOLIA_ENTITIES_FILE}")
    endforeach()
endforeach()
target_include_directories(folia_math PRIVATE ${FOLIA_ENTITIES_GENERATED_DIR})
set_property(GLOBAL APPEND PROPERTY JOB_POOLS folia_math_compile=2)
set_property(TARGET folia_math PROPERTY JOB_POOL_COMPILE folia_math_compile)
target_sources(folia_math PUBLIC FILE_SET CXX_MODULES FILES
    src/math/math.ixx
    src/math/core/tree/factory_types.ixx
    src/math/core/tree/factory.ixx
    src/math/core/tree/wrapper_types.ixx
    src/math/core/tree/wrapper.ixx
    src/math/core/tree/wrapper_factory.ixx
    src/math/core/tree/node_types.ixx
    src/math/core/tree/node.ixx
    src/math/core/tree/visitor.ixx
    src/math/core/math_item.ixx
    src/math/core/mml_tree/node_kind.ixx
    src/math/core/mml_tree/attributes.ixx
    src/math/core/mml_tree/node_properties.ixx
    src/math/core/mml_tree/mml_node.ixx
    src/math/core/mml_tree/mml_document.ixx
    src/math/core/mml_tree/node_traits.ixx
    src/math/core/mml_tree/node_defaults.ixx
    src/math/core/mml_tree/mml_factory.ixx
    src/math/core/mml_tree/operator_dictionary.ixx
    src/math/core/mml_tree/mo_patterns.ixx
    src/math/input/tex/session.ixx
    src/math/input/tex/token.ixx
    src/math/input/tex/parser_types.ixx
    src/math/input/tex/macro.ixx
    src/math/input/tex/token_map.ixx
    src/math/input/tex/map_handler.ixx
    src/math/input/tex/parse_options.ixx
    src/math/input/tex/tags.ixx
    src/math/input/tex/configuration.ixx
    src/math/input/tex/stack_item.ixx
    src/math/input/tex/stack_item_factory.ixx
    src/math/input/tex/base/base_items.ixx
    src/math/input/tex/stack.ixx
    src/math/input/tex/tex_parser.ixx
    src/math/input/tex/text_parser.ixx
    src/math/input/tex/column_parser.ixx
    src/math/input/tex/parser_family.ixx
    src/math/input/tex/begingroup/stack.ixx
    src/math/input/tex/begingroup/methods.ixx
    src/math/input/tex/begingroup/configuration.ixx
    src/math/input/tex/environment.ixx
    src/math/input/tex/tex_error.ixx
    src/math/input/tex/tex_scanner.ixx
    src/math/input/tex/unit_util.ixx
    src/math/input/tex/parse_util.ixx
    src/math/input/tex/parse_methods.ixx
    src/math/input/tex/base/base_methods.ixx
    src/math/input/tex/base/base_mappings.ixx
    src/math/input/tex/base/base_configuration.ixx
    src/math/input/tex/tex_jax.ixx
    src/math/input/tex/filter_util.ixx
    src/math/core/input_jax.ixx
    src/math/input/tex/node_factory.ixx
    src/math/input/tex/node_util.ixx
    src/math/input/tex/boldsymbol/configuration.ixx
    src/math/input/tex/newcommand/configuration.ixx
    src/math/input/tex/newcommand/items.ixx
    src/math/input/tex/newcommand/mappings.ixx
    src/math/input/tex/newcommand/methods.ixx
    src/math/input/tex/newcommand/util.ixx
    src/math/input/tex/color/constants.ixx
    src/math/input/tex/color/util.ixx
    src/math/input/tex/color/methods.ixx
    src/math/input/tex/color/configuration.ixx
    src/math/input/tex/noundefined/configuration.ixx
    src/math/input/tex/configmacros/configuration.ixx
    src/math/input/tex/verb/configuration.ixx
    src/math/input/tex/upgreek/configuration.ixx
    src/math/input/tex/default_packages.ixx
    src/math/input/tex/mediawiki_texvc/configuration.ixx
    src/math/input/tex/gensymb/configuration.ixx
    src/math/input/tex/ams/methods.ixx
    src/math/input/tex/ams/items.ixx
    src/math/input/tex/ams/mappings.ixx
    src/math/input/tex/ams/configuration.ixx
    src/math/input/tex/amscd/methods.ixx
    src/math/input/tex/amscd/mappings.ixx
    src/math/input/tex/amscd/configuration.ixx
    src/math/input/tex/extpfeil/configuration.ixx
    src/math/input/tex/bbox/configuration.ixx
    src/math/input/tex/enclose/configuration.ixx
    src/math/input/tex/cancel/configuration.ixx
    src/math/input/tex/html/methods.ixx
    src/math/input/tex/html/configuration.ixx
    src/math/input/tex/action/configuration.ixx
    src/math/input/tex/centernot/configuration.ixx
    src/math/input/tex/fontsizev3/configuration.ixx
    src/math/input/tex/braket/items.ixx
    src/math/input/tex/braket/methods.ixx
    src/math/input/tex/braket/mappings.ixx
    src/math/input/tex/braket/configuration.ixx
    src/math/input/tex/units/configuration.ixx
    src/math/input/tex/textmacros_methods.ixx
    src/math/input/tex/textmacros_mappings.ixx
    src/math/input/tex/textmacros_configuration.ixx
    src/math/input/tex/textcomp/mappings.ixx
    src/math/input/tex/textcomp/configuration.ixx
    src/math/input/tex/bbm/configuration.ixx
    src/math/input/tex/dsfont/configuration.ixx
    src/math/input/tex/bboldx/methods.ixx
    src/math/input/tex/bboldx/mappings.ixx
    src/math/input/tex/bboldx/configuration.ixx
    src/math/input/tex/tagformat/tag_format_configuration.ixx
    src/math/input/tex/setoptions/set_options_configuration.ixx
    src/math/input/tex/empheq/util.ixx
    src/math/input/tex/empheq/configuration.ixx
    src/math/input/tex/cases/configuration.ixx
    src/math/input/tex/colortbl/configuration.ixx
    src/math/input/tex/mathtools/tags.ixx
    src/math/input/tex/mathtools/items.ixx
    src/math/input/tex/mathtools/methods.ixx
    src/math/input/tex/mathtools/util.ixx
    src/math/input/tex/mathtools/mappings.ixx
    src/math/input/tex/mathtools/configuration.ixx
    src/math/input/tex/bussproofs/items.ixx
    src/math/input/tex/bussproofs/util.ixx
    src/math/input/tex/bussproofs/methods.ixx
    src/math/input/tex/bussproofs/mappings.ixx
    src/math/input/tex/bussproofs/configuration.ixx
    src/math/input/tex/physics/items.ixx
    src/math/input/tex/physics/methods.ixx
    src/math/input/tex/physics/mappings.ixx
    src/math/input/tex/physics/configuration.ixx
    src/math/input/tex/unicode/configuration.ixx
    src/math/input/tex/mhchem/configuration.ixx
    src/math/dependencies/mhchem_parser/parser.ixx
    src/math/output/scene/types.ixx
    src/math/output/scene/html_types.ixx
    src/math/output/scene/html_snapshot.ixx
    src/math/output/scene/scene.ixx
    src/math/output/scene/builder.ixx
    src/math/output/svg/writer.ixx
    src/math/output/svg/font_cache.ixx
    src/math/output/svg/menclose_notations.ixx
    src/math/output/svg/merror.ixx
    src/math/output/svg/maction.ixx
    src/math/output/svg/mglyph.ixx
    src/math/output/svg/wrapper_factory.ixx
    src/math/output/svg/xml_wrapper.ixx
    src/math/output/svg/serializer.ixx
    src/math/output/svg/font_data.ixx
    src/math/output/common/direction.ixx
    src/math/output/common/font_data.ixx
    src/math/output/common/font_path.ixx
    src/math/output/common/line_bbox.ixx
    src/math/output/common/notation.ixx
    src/math/output/common/common.ixx
    src/math/output/common/wrapper.ixx
    src/math/output/common/wrapper_factory.ixx
    src/math/output/common/output_jax.ixx
    src/math/output/common/linebreak_visitor.ixx
    src/math/output/common/wrappers/mfrac.ixx
    src/math/output/common/wrappers/msqrt.ixx
    src/math/output/common/wrappers/mroot.ixx
    src/math/fonts/newcm_font_data.ixx
    src/math/fonts/newcm_font_module_provider.ixx
    src/math/util/constants.ixx
    src/math/util/instance_count.ixx
    src/math/util/prioritized_list.ixx
    src/math/util/function_list.ixx
    src/math/util/bbox.ixx
    src/math/util/lengths.ixx
    src/math/util/js_number.ixx
    src/math/util/styles.ixx
    src/math/util/style_json.ixx
    src/math/util/unicode_normalization.ixx
    src/math/util/unicode_normalization_data.ixx
    src/math/output/common/types.ixx
    src/math/util/regexp_host.ixx)
target_sources(folia_math PRIVATE
    src/math/math.cpp
    src/math/input/tex/default_packages.cpp
    src/math/input/tex/mediawiki_texvc/configuration.cpp
    src/math/core/math_item.cpp
    src/math/core/math_item_state.cpp
    src/math/core/mml_tree/attributes.cpp
    src/math/core/mml_tree/node_properties.cpp
    src/math/core/mml_tree/mml_node.cpp
    src/math/core/mml_tree/mml_document.cpp
    src/math/core/mml_tree/mml_factory.cpp
    src/math/core/mml_tree/node_defaults.cpp
    src/math/core/mml_tree/operator_dictionary.cpp
    src/math/core/mml_tree/operator_dictionary_data.cpp
    src/math/core/mml_tree/mo_patterns.cpp
    src/math/core/mml_tree/mo_pattern_data.cpp
    src/math/input/tex/session.cpp
    src/math/input/tex/token.cpp
    src/math/input/tex/parse_options.cpp
    src/math/input/tex/configuration.cpp
    src/math/input/tex/stack_item.cpp
    src/math/input/tex/stack_item_factory.cpp
    src/math/input/tex/base/base_items.cpp
    src/math/input/tex/stack.cpp
    src/math/input/tex/tex_parser.cpp
    src/math/input/tex/text_parser.cpp
    src/math/input/tex/column_parser.cpp
    src/math/input/tex/tags.cpp
    src/math/input/tex/macro.cpp
    src/math/input/tex/token_map.cpp
    src/math/input/tex/map_handler.cpp
    src/math/input/tex/begingroup/stack.cpp
    src/math/input/tex/begingroup/methods.cpp
    src/math/input/tex/begingroup/configuration.cpp
    src/math/input/tex/environment.cpp
    src/math/input/tex/tex_error.cpp
    src/math/input/tex/tex_scanner.cpp
    src/math/input/tex/unit_util.cpp
    src/math/input/tex/parse_util.cpp
    src/math/input/tex/parse_methods.cpp
    src/math/input/tex/base/base_methods.cpp
    src/math/input/tex/base/base_mappings.cpp
    src/math/input/tex/base/base_configuration.cpp
    src/math/input/tex/tex_jax.cpp
    src/math/input/tex/filter_util.cpp
    src/math/input/tex/node_factory.cpp
    src/math/input/tex/node_util.cpp
    src/math/input/tex/boldsymbol/configuration.cpp
    src/math/input/tex/newcommand/configuration.cpp
    src/math/input/tex/newcommand/items.cpp
    src/math/input/tex/newcommand/mappings.cpp
    src/math/input/tex/newcommand/methods.cpp
    src/math/input/tex/newcommand/util.cpp
    src/math/input/tex/color/constants.cpp
    src/math/input/tex/color/util.cpp
    src/math/input/tex/color/methods.cpp
    src/math/input/tex/color/configuration.cpp
    src/math/input/tex/noundefined/configuration.cpp
    src/math/input/tex/configmacros/configuration.cpp
    src/math/input/tex/verb/configuration.cpp
    src/math/input/tex/upgreek/configuration.cpp
    src/math/input/tex/gensymb/configuration.cpp
    src/math/input/tex/ams/methods.cpp
    src/math/input/tex/ams/items.cpp
    src/math/input/tex/ams/mappings.cpp
    src/math/input/tex/ams/configuration.cpp
    src/math/input/tex/amscd/methods.cpp
    src/math/input/tex/amscd/mappings.cpp
    src/math/input/tex/amscd/configuration.cpp
    src/math/input/tex/extpfeil/configuration.cpp
    src/math/input/tex/bbox/configuration.cpp
    src/math/input/tex/enclose/configuration.cpp
    src/math/input/tex/cancel/configuration.cpp
    src/math/input/tex/html/methods.cpp
    src/math/input/tex/html/configuration.cpp
    src/math/input/tex/action/configuration.cpp
    src/math/input/tex/centernot/configuration.cpp
    src/math/input/tex/fontsizev3/configuration.cpp
    src/math/input/tex/braket/items.cpp
    src/math/input/tex/braket/methods.cpp
    src/math/input/tex/braket/mappings.cpp
    src/math/input/tex/braket/configuration.cpp
    src/math/input/tex/units/configuration.cpp
    src/math/input/tex/textmacros_methods.cpp
    src/math/input/tex/textmacros_mappings.cpp
    src/math/input/tex/textmacros_configuration.cpp
    src/math/input/tex/textcomp/mappings.cpp
    src/math/input/tex/textcomp/configuration.cpp
    src/math/input/tex/bbm/configuration.cpp
    src/math/input/tex/dsfont/configuration.cpp
    src/math/input/tex/bboldx/methods.cpp
    src/math/input/tex/bboldx/mappings.cpp
    src/math/input/tex/bboldx/configuration.cpp
    src/math/input/tex/tagformat/tag_format_configuration.cpp
    src/math/input/tex/setoptions/set_options_configuration.cpp
    src/math/input/tex/empheq/util.cpp
    src/math/input/tex/empheq/configuration.cpp
    src/math/input/tex/cases/configuration.cpp
    src/math/input/tex/colortbl/configuration.cpp
    src/math/input/tex/mathtools/tags.cpp
    src/math/input/tex/mathtools/items.cpp
    src/math/input/tex/mathtools/methods.cpp
    src/math/input/tex/mathtools/util.cpp
    src/math/input/tex/mathtools/mappings.cpp
    src/math/input/tex/mathtools/configuration.cpp
    src/math/input/tex/bussproofs/items.cpp
    src/math/input/tex/bussproofs/util.cpp
    src/math/input/tex/bussproofs/methods.cpp
    src/math/input/tex/bussproofs/mappings.cpp
    src/math/input/tex/bussproofs/configuration.cpp
    src/math/input/tex/physics/items.cpp
    src/math/input/tex/physics/methods.cpp
    src/math/input/tex/physics/mappings.cpp
    src/math/input/tex/physics/configuration.cpp
    src/math/input/tex/unicode/configuration.cpp
    src/math/input/tex/mhchem/configuration.cpp
    src/math/dependencies/mhchem_parser/parser.cpp
    src/math/dependencies/mhchem_parser/patterns.cpp
    src/math/dependencies/mhchem_parser/actions.cpp
    src/math/dependencies/mhchem_parser/texify.cpp
    src/math/output/scene/scene.cpp
    src/math/output/scene/builder.cpp
    src/math/output/scene/html_snapshot.cpp
    src/math/output/svg/writer.cpp
    src/math/output/svg/font_cache.cpp
    src/math/output/svg/menclose_notations.cpp
    src/math/output/svg/merror.cpp
    src/math/output/svg/maction.cpp
    src/math/output/svg/mglyph.cpp
    src/math/output/svg/wrapper_factory.cpp
    src/math/output/svg/xml_wrapper.cpp
    src/math/output/svg/serializer.cpp
    src/math/output/svg/node_metadata.cpp
    src/math/output/svg/font_data.cpp
    src/math/output/common/font_data.cpp
    src/math/output/common/font_path.cpp
    src/math/output/common/line_bbox.cpp
    src/math/output/common/notation.cpp
    src/math/output/common/wrapper.cpp
    src/math/output/common/wrapper_factory.cpp
    src/math/output/common/output_jax.cpp
    src/math/output/common/linebreak_visitor.cpp
    src/math/output/common/wrappers/mfrac.cpp
    src/math/output/common/wrappers/msqrt.cpp
    src/math/output/common/wrappers/mroot.cpp
    src/math/fonts/newcm_font_data.cpp
    src/math/fonts/newcm_font_module_provider.cpp
    ${FOLIA_FONT_GENERATED_CPP}
    src/math/util/lengths.cpp
    src/math/util/styles.cpp
    src/math/util/style_json.cpp
    src/math/util/regexp_host.cpp
    src/math/util/unicode_normalization.cpp)

# Native Common classes retain their original family partitions; the concrete
# LinebreakVisitor uses these same classes for recursive mathematical breaks.
set(FOLIA_COMMON_NATIVE_FAMILIES
    mo mrow inferred_mrow text_node mi mn ms mspace mglyph mtext math
    scriptbase msubsup mmultiscripts munderover mfenced maction
    mtable mtr mtd mpadded mphantom semantics tex_atom menclose xml_node)
foreach(family IN LISTS FOLIA_COMMON_NATIVE_FAMILIES)
    target_sources(folia_math PUBLIC FILE_SET CXX_MODULES FILES
        src/math/output/common/wrappers/${family}.ixx)
    target_sources(folia_math PRIVATE src/math/output/common/wrappers/${family}.cpp)
endforeach()

# Actual MathJax MML families share the document-owned native arena. Keep the
# original per-family modules; only maction and mo have split implementation TUs.
set(FOLIA_MML_FAMILIES
    math mi mn mo mtext mspace ms mrow mfrac msqrt mroot mstyle merror mpadded
    mphantom mfenced menclose maction msubsup munderover mmultiscripts mtable
    mtr mtd maligngroup malignmark mglyph semantics tex_atom mathchoice html_node)
foreach(family IN LISTS FOLIA_MML_FAMILIES)
    target_sources(folia_math PUBLIC FILE_SET CXX_MODULES FILES
        src/math/core/mml_tree/mml_nodes/${family}.ixx)
    if(NOT family STREQUAL "maction" AND NOT family STREQUAL "mo")
        target_sources(folia_math PRIVATE src/math/core/mml_tree/mml_nodes/${family}.cpp)
    endif()
endforeach()
target_sources(folia_math PUBLIC FILE_SET CXX_MODULES FILES
    src/math/core/mml_tree/html_adaptor.ixx
    src/math/core/mml_tree/xml_dom_adaptor.ixx
    src/math/core/dom_node.ixx
    src/math/core/dom_adaptor.ixx
    src/math/core/find_math.ixx
    src/math/input/tex/find_tex.ixx
    src/math/adaptors/lite_adaptor.ixx
    src/math/adaptors/lite/element.ixx
    src/math/adaptors/lite/text.ixx
    src/math/adaptors/lite/document.ixx
    src/math/adaptors/lite/window.ixx
    src/math/adaptors/lite/list.ixx
    src/math/adaptors/lite/parser.ixx
    src/math/handlers/html/html_dom_strings.ixx
    src/math/input/tex/texhtml/tex_html_configuration.ixx
    src/math/core/mml_tree/mo_types.ixx
    src/math/core/mml_tree/operator_class.ixx
    src/math/util/js_numeric_input.ixx
    src/math/util/js_case.ixx
    src/math/util/utf16_types.ixx
    src/math/util/utf16.ixx
    src/math/util/string.ixx
    src/math/util/entities.ixx
    src/math/cancellation.ixx)
target_sources(folia_math PRIVATE
    src/math/core/dom_adaptor.cpp
    src/math/core/find_math.cpp
    src/math/input/tex/find_tex.cpp
    src/math/core/mml_tree/xml_dom_adaptor.cpp
    src/math/adaptors/lite_adaptor.cpp
    src/math/adaptors/lite/element.cpp
    src/math/adaptors/lite/text.cpp
    src/math/adaptors/lite/document.cpp
    src/math/adaptors/lite/window.cpp
    src/math/adaptors/lite/list.cpp
    src/math/adaptors/lite/parser.cpp
    src/math/handlers/html/html_dom_strings.cpp
    src/math/input/tex/texhtml/tex_html_configuration.cpp
    src/math/core/mml_tree/mml_branches.cpp
    src/math/core/mml_tree/mml_empty.cpp
    src/math/core/mml_tree/mml_nodes/maction_selection.cpp
    src/math/core/mml_tree/mml_nodes/maction_behavior.cpp
    src/math/core/mml_tree/mml_nodes/mo_lookup.cpp
    src/math/core/mml_tree/mml_nodes/mo_classes.cpp
    src/math/core/mml_tree/mml_nodes/mo_scripts.cpp
    src/math/core/mml_tree/operator_class.cpp
    src/math/cancellation.cpp)
target_sources(folia_math PRIVATE src/math/util/string.cpp)
target_sources(folia_math PRIVATE src/math/util/entities.cpp)
target_sources(folia_math PRIVATE src/math/util/js_case.cpp src/math/util/js_case_data.cpp)

# Opaque application boundary. Only the C functions cross the DLL boundary;
# the implementation and its C++ module dependencies remain private.
add_library(FoliaNativeMath SHARED src/math/native_renderer_bridge.cpp)
target_include_directories(FoliaNativeMath PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/math/include)
target_compile_features(FoliaNativeMath PRIVATE cxx_std_23)
set_target_properties(FoliaNativeMath PROPERTIES CXX_MODULE_STD ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(FoliaNativeMath PRIVATE folia_math)
if(WIN32)
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/FoliaNativeMath.def" CONTENT
"LIBRARY FoliaNativeMath
EXPORTS
FoliaNativeMathRendererCreate
FoliaNativeMathRendererDestroy
FoliaNativeMathCancellationCreate
FoliaNativeMathCancellationRequestStop
FoliaNativeMathCancellationDestroy
FoliaNativeMathRenderSvg
FoliaNativeMathRenderSvgBatch
FoliaNativeMathResultSvg
FoliaNativeMathResultError
FoliaNativeMathResultDestroy
")
    target_sources(FoliaNativeMath PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/FoliaNativeMath.def")
endif()

# ---------------------------------------------------------------------------
# Platform library — the ONLY Windows-touching layer.
# ---------------------------------------------------------------------------
if(WIN32)
    # Locate the Windows 10/11 SDK so platform .ixx files can #include
    # <dwrite.h>/<d2d1.h>/... without relying on a globally-injected env.
    set(_win_sdk_root "C:/Program Files (x86)/Windows Kits/10")
    file(GLOB _win_sdk_inc_dirs LIST_DIRECTORIES true "${_win_sdk_root}/Include/10.0.*")
    if(_win_sdk_inc_dirs)
        list(SORT _win_sdk_inc_dirs ORDER DESCENDING)
        list(GET _win_sdk_inc_dirs 0 _win_sdk_inc)
        set(FOLIA_WINSDK_INC "${_win_sdk_inc}" CACHE PATH "Windows SDK Include dir")
    endif()

    file(GLOB_RECURSE FOLIA_PLATFORM_MODULE CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/*.ixx)
    file(GLOB_RECURSE FOLIA_PLATFORM_SRC CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/*.cpp)

    if(FOLIA_PLATFORM_MODULE OR FOLIA_PLATFORM_SRC)
        add_library(folia_platform STATIC)
        target_compile_features(folia_platform PUBLIC cxx_std_23)
        set_target_properties(folia_platform PROPERTIES CXX_MODULE_STD ON)
        target_sources(folia_platform PUBLIC
            FILE_SET CXX_MODULES FILES ${FOLIA_PLATFORM_MODULE})
        target_sources(folia_platform PRIVATE ${FOLIA_PLATFORM_SRC})
        target_link_libraries(folia_platform PUBLIC folia_core)
        target_include_directories(folia_platform PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src/platform)
        if(FOLIA_WINSDK_INC)
            target_include_directories(folia_platform PUBLIC
                ${FOLIA_WINSDK_INC}/um
                ${FOLIA_WINSDK_INC}/shared
                ${FOLIA_WINSDK_INC}/ucrt
                ${FOLIA_WINSDK_INC}/cppwinrt)
        endif()
        target_link_libraries(folia_platform PUBLIC
            dwrite d2d1 d3d11 dxgi windowscodecs dwmapi uxtheme shlwapi
            ole32 oleaut32 uuid user32 gdi32 comctl32
            shell32 advapi32 imm32 propsys)

    endif()
endif()

# ---------------------------------------------------------------------------
# Tests — pure core tests, no Windows dependency, portable.
#
# Boost.UT is consumed through its C++ module. The vendored fork fixes MSVC
# module linkage and command-line initialization; main passes argc/argv
# explicitly so no test translation unit depends on MSVC's private CRT globals.
# ---------------------------------------------------------------------------
set(BOOST_UT_DISABLE_MODULE OFF CACHE BOOL "Build Boost.UT's C++ module" FORCE)
set(BOOST_UT_BUILD_EXAMPLES OFF CACHE BOOL "Do not build Boost.UT examples" FORCE)
set(BOOST_UT_BUILD_TESTS OFF CACHE BOOL "Do not build Boost.UT self-tests" FORCE)
set(BOOST_UT_ENABLE_INSTALL OFF CACHE BOOL "Do not install vendored Boost.UT" FORCE)
add_subdirectory(third_party/ut EXCLUDE_FROM_ALL)

enable_testing()
add_executable(FoliaTests)
target_compile_features(FoliaTests PRIVATE cxx_std_23)
set_target_properties(FoliaTests PROPERTIES
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON)
file(GLOB_RECURSE FOLIA_TEST_SRC CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
list(FILTER FOLIA_TEST_SRC EXCLUDE REGEX "/tests/support/")
list(FILTER FOLIA_TEST_SRC EXCLUDE REGEX "/tests/platform/")
list(FILTER FOLIA_TEST_SRC EXCLUDE REGEX "/tests/app/")
list(FILTER FOLIA_TEST_SRC EXCLUDE REGEX "/tests/libraries/")
list(FILTER FOLIA_TEST_SRC EXCLUDE REGEX "/tests/encoding/")
list(FILTER FOLIA_TEST_SRC EXCLUDE REGEX "/tests/math/")
list(FILTER FOLIA_TEST_SRC EXCLUDE REGEX "/tests/parsing/test_commonmark\\.cpp$")
list(FILTER FOLIA_TEST_SRC EXCLUDE REGEX "/tests/parsing/test_gfm\\.cpp$")
list(FILTER FOLIA_TEST_SRC EXCLUDE REGEX "/tests/parsing/test_gfm_pathological\\.cpp$")
list(FILTER FOLIA_TEST_SRC EXCLUDE REGEX "/tests/parsing/test_inline_cst_ownership\\.cpp$")
list(FILTER FOLIA_TEST_SRC EXCLUDE REGEX "/tests/parsing/test_deep_block_ownership\\.cpp$")
list(FILTER FOLIA_TEST_SRC EXCLUDE REGEX "/tests/parsing/test_serializer_deep\\.cpp$")
list(FILTER FOLIA_TEST_SRC EXCLUDE REGEX "/tests/parsing/test_md4c_reference\\.cpp$")
target_sources(FoliaTests PRIVATE
    ${FOLIA_TEST_SRC})
target_include_directories(FoliaTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests
    "${CMAKE_CURRENT_BINARY_DIR}/gfm-entities")
set(FOLIA_GFM_ENTITY_CASES "${CMAKE_CURRENT_BINARY_DIR}/gfm-entities/gfm_entity_cases.inc")
add_custom_command(OUTPUT "${FOLIA_GFM_ENTITY_CASES}"
    COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/tools/markdown-conformance/generate_pinned_gfm_entities.py"
        --fetch --output "${FOLIA_GFM_ENTITY_CASES}"
    DEPENDS tools/markdown-conformance/generate_pinned_gfm_entities.py
        tools/markdown-conformance/gfm-sources.lock.json
    VERBATIM)
add_custom_target(FoliaGfmEntityFixtures DEPENDS "${FOLIA_GFM_ENTITY_CASES}")
add_dependencies(FoliaTests FoliaGfmEntityFixtures)
target_link_libraries(FoliaTests PRIVATE folia_core folia_math Boost::ut_module)
if(MSVC)
    # Native math recursion tests must reach the explicit 512-call resource
    # guard even with Debug variant/callback frames. App math threads must
    # satisfy the same stack requirement before the native integration gate.
    target_link_options(FoliaTests PRIVATE /STACK:16777216)
endif()
# Boost.UT parses positional test-name patterns and CLI options. For example,
# `FoliaTests.exe "*name*"` filters by name; `-t` lists tags.
add_test(NAME FoliaTests COMMAND FoliaTests)

# Native foundation tests are explicit. Historical mathematical suites remain
# tracked for typed migration; they are not reported as passing this rewrite.
add_executable(FoliaMathTests EXCLUDE_FROM_ALL
    tests/main.cpp
    tests/math/mathjax_full_profile_package_bootstrap.cpp
    tests/math/tex_package_registry.cpp
    tests/math/math_cancellation_g147.cpp
    tests/math/native_renderer_bridge.cpp
    tests/math/mml_native_raii_foundation.cpp
    tests/math/native_raii_scene.cpp
    tests/math/lengths_native.cpp
    tests/math/js_case.cpp
    tests/math/ordered_map_storage.cpp
    tests/math/native_raii_regexp.cpp
    tests/math/native_tex_session.cpp
    tests/math/native_tex_definitions.cpp
    tests/math/native_tex_begingroup.cpp
    tests/math/prioritized_list.cpp
    tests/math/parse_options.cpp
    tests/math/configuration.cpp
    tests/math/base_items.cpp
    tests/math/column_parser.cpp
    tests/math/tex_parser.cpp
    tests/math/parse_methods.cpp
    tests/math/base_methods.cpp
    tests/math/base_mappings.cpp
    tests/math/base_configuration.cpp
    tests/math/filter_util.cpp
    tests/math/tex_jax.cpp
    tests/math/stack.cpp
    tests/math/tags.cpp
    tests/math/macro.cpp
    tests/math/token_map.cpp
    tests/math/map_handler.cpp
    tests/math/function_list.cpp
    tests/math/native_raii_tex_environment.cpp
    tests/math/native_svg_writer.cpp
    tests/math/native_svg_font_cache_plan.cpp
    tests/math/native_tex_error_source.cpp
    tests/math/native_tex_error_utf8.cpp
    tests/math/native_tex_scanner.cpp
    tests/math/node_factory_registration.cpp
    tests/math/node_factory.cpp
    tests/math/boldsymbol.cpp
    tests/math/newcommand.cpp
    tests/math/begingroup.cpp
    tests/math/color.cpp
    tests/math/noundefined.cpp
    tests/math/configmacros.cpp
    tests/math/verb.cpp
    tests/math/upgreek.cpp
    tests/math/gensymb.cpp
    tests/math/ams.cpp
    tests/math/ams_items.cpp
    tests/math/amscd.cpp
    tests/math/extpfeil.cpp
    tests/math/bbox.cpp
    tests/math/keyval_options.cpp
    tests/math/enclose.cpp
    tests/math/cancel.cpp
    tests/math/html.cpp
    tests/math/action.cpp
    tests/math/centernot.cpp
    tests/math/fontsizev3.cpp
    tests/math/braket.cpp
    tests/math/units.cpp
    tests/math/textmacros.cpp
    tests/math/textcomp.cpp
    tests/math/bbm.cpp
    tests/math/dsfont.cpp
    tests/math/bboldx.cpp
    tests/math/tagformat.cpp
    tests/math/setoptions.cpp
    tests/math/empheq.cpp
    tests/math/cases.cpp
    tests/math/colortbl.cpp
    tests/math/mathtools_tags.cpp
    tests/math/mathtools.cpp
    tests/math/bussproofs_items.cpp
    tests/math/bussproofs.cpp
    tests/math/physics.cpp
    tests/math/entities.cpp
    tests/math/unicode.cpp
    tests/math/mhchem_parser.cpp
    tests/math/mhchem.cpp
    tests/math/lite.cpp
    tests/math/xml_dom_adaptor.cpp
    tests/math/find_tex.cpp
    tests/math/texhtml.cpp
    tests/math/common_scene.cpp
    tests/math/native_tex_color.cpp
    tests/math/native_tex_column_definitions.cpp
    tests/math/native_tex_unit_util.cpp
    tests/math/native_scene_stroke_dash.cpp
    tests/math/native_math_item.cpp
    tests/math/parse_util.cpp
    tests/math/mml_native_factory.cpp
    tests/math/mml_defaults.cpp
    tests/math/mml_node_state.cpp
    tests/math/operator_dictionary_native.cpp
    tests/math/native_raii_mml_base.cpp
    tests/math/typed_mml_branches.cpp
    tests/math/native_mml_row_tokens.cpp
    tests/math/typed_mml_arithmetic.cpp
    tests/math/typed_mml_scripts.cpp
    tests/math/mml_typed_table.cpp
    tests/math/typed_mml_containers.cpp
    tests/math/native_raii_mo.cpp
    tests/math/mml_exact_clone_document.cpp
    tests/math/mml_exact_clone_mo.cpp
    tests/math/mml_exact_clone_xml.cpp
    tests/math/mml_exact_clone_fenced.cpp
    tests/math/mml_exact_clone_row.cpp
    tests/math/native_mml_families.cpp
    tests/math/native_string_util.cpp
    tests/math/unicode_normalization.cpp
    tests/math/native_raii_fonts.cpp
    tests/math/font_css_metadata.cpp
    tests/math/common_styles_source.cpp
    tests/math/native_style_json.cpp
    tests/math/native_line_bbox.cpp
    tests/math/native_common_factory.cpp
    tests/math/native_common_jax.cpp
    tests/math/native_common_mo.cpp
    tests/math/native_common_mrow.cpp
    tests/math/native_common_text.cpp
    tests/math/native_common_wrapper.cpp
    tests/math/native_common_text_ownership.cpp
    tests/math/native_common_mtext_math.cpp
    tests/math/native_common_inline_policy.cpp
    tests/math/native_common_scripts.cpp
    tests/math/native_common_limits.cpp
    tests/math/native_common_fence_action.cpp
    tests/math/native_common_maction.cpp
    tests/math/native_scene_actions.cpp
    tests/math/native_scene_resource_order.cpp
    tests/math/native_scene_images.cpp
    tests/math/native_common_linebreak.cpp
    tests/math/native_common_table.cpp
    tests/math/native_common_semantics.cpp
    tests/math/native_svg_merror.cpp
    tests/math/native_svg_maction.cpp
    tests/math/native_svg_maction_source.cpp
    tests/math/native_svg_images.cpp
    tests/math/native_svg_mglyph.cpp
    tests/math/native_svg_mglyph_source.cpp
    tests/math/native_table_profile_source.cpp
    tests/math/native_canonical_scene_source.cpp
    tests/math/request_lifetime.cpp
    tests/math/native_svg_actions.cpp
    tests/math/native_xml_dom_export.cpp
    tests/math/native_common_xml.cpp
    tests/math/native_svg_xml_wrapper.cpp
    tests/math/native_svg_xml.cpp
    tests/math/native_common_mfenced.cpp
    tests/math/native_common_containers.cpp
    tests/math/native_common_menclose.cpp
    tests/math/native_scene_element_roles.cpp
    tests/math/native_factory_owned_result.cpp
    tests/math/native_tree_factory.cpp
    tests/math/native_tree_node.cpp
    tests/math/native_tree_visitor.cpp
    tests/math/fraction_native.cpp
    tests/math/radical_native.cpp)
target_compile_features(FoliaMathTests PRIVATE cxx_std_23)
set_target_properties(FoliaMathTests PROPERTIES CXX_MODULE_STD ON CXX_SCAN_FOR_MODULES ON)

# Replay the retained numeric Source corpus against the original typed utility.
# This unused application utility is compiled only for its mathematical tests.
target_sources(FoliaMathTests PRIVATE src/math/util/numeric.cpp)
target_sources(FoliaMathTests PRIVATE
    FILE_SET CXX_MODULES FILES
    src/math/util/numeric.ixx
    src/math/util/ordered_map_storage.ixx)
target_include_directories(FoliaMathTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
target_compile_definitions(FoliaMathTests PRIVATE
    FOLIA_MATH_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/math/fixtures")
target_link_libraries(FoliaMathTests PRIVATE folia_math FoliaNativeMath Boost::ut_module)
add_test(NAME FoliaMathTests COMMAND FoliaMathTests)
add_executable(FoliaMathFontDump EXCLUDE_FROM_ALL
    tools/math-port/font-data-dump.cpp)
target_compile_features(FoliaMathFontDump PRIVATE cxx_std_23)
set_target_properties(FoliaMathFontDump PROPERTIES CXX_MODULE_STD ON CXX_SCAN_FOR_MODULES ON)
target_link_libraries(FoliaMathFontDump PRIVATE folia_math)
if(MSVC)
    target_link_options(FoliaTests PRIVATE /DEBUG:NONE /INCREMENTAL:NO)
    target_link_options(FoliaMathTests PRIVATE /DEBUG:NONE /INCREMENTAL:NO)
    target_link_options(FoliaMathTests PRIVATE /STACK:16777216)
    target_link_options(FoliaMathFontDump PRIVATE /DEBUG:NONE /INCREMENTAL:NO)
endif()

if(TARGET folia_platform)
    add_executable(FoliaWindowsEditorTests)
    target_compile_features(FoliaWindowsEditorTests PRIVATE cxx_std_23)
    set_target_properties(FoliaWindowsEditorTests PROPERTIES
        CXX_MODULE_STD ON
        CXX_SCAN_FOR_MODULES ON)
    file(GLOB_RECURSE FOLIA_PLATFORM_TEST_SRC CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/platform/*.cpp)
    target_sources(FoliaWindowsEditorTests PRIVATE
        ${FOLIA_PLATFORM_TEST_SRC})
    target_include_directories(FoliaWindowsEditorTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform)
    target_link_libraries(FoliaWindowsEditorTests PRIVATE
        folia_platform Boost::ut_module)
    add_test(NAME FoliaWindowsEditorTests COMMAND FoliaWindowsEditorTests)
endif()

if(WIN32)
    set(FOLIA_ICU_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/build/dependencies/icu-78.3/root"
        CACHE PATH "ICU4C root used by document encoding tests")
    if(EXISTS "${FOLIA_ICU_ROOT}/include/unicode/ucsdet.h")
        add_executable(FoliaEncodingTests
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/encoding/main.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/encoding/test_document_encoding.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/app-winui/storage/DocumentEncodingService.cpp)
        target_compile_features(FoliaEncodingTests PRIVATE cxx_std_23)
        set_target_properties(FoliaEncodingTests PROPERTIES
            CXX_MODULE_STD ON
            CXX_SCAN_FOR_MODULES ON)
        target_include_directories(FoliaEncodingTests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
            ${CMAKE_CURRENT_SOURCE_DIR}/src/app-winui
            ${FOLIA_ICU_ROOT}/include)
        target_sources(FoliaEncodingTests PRIVATE
            FILE_SET CXX_MODULES FILES
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/encoding/document_encoding_test_support.ixx)
        target_link_libraries(FoliaEncodingTests PRIVATE
            Boost::ut_module
            ${FOLIA_ICU_ROOT}/lib64/icuin.lib
            ${FOLIA_ICU_ROOT}/lib64/icuuc.lib
            ${FOLIA_ICU_ROOT}/lib64/icudt.lib)
        add_custom_command(TARGET FoliaEncodingTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${FOLIA_ICU_ROOT}/bin64/icuin78.dll
                ${FOLIA_ICU_ROOT}/bin64/icuuc78.dll
                ${FOLIA_ICU_ROOT}/bin64/icudt78.dll
                $<TARGET_FILE_DIR:FoliaEncodingTests>)
        add_test(NAME FoliaEncodingTests COMMAND FoliaEncodingTests)
    endif()
endif()

# These CMake executables are console tests/probes; the WinUI app uses MSBuild.
# Official specification tests run the native source invariants and compare
# projected HTML with the same canonicalizer used for official expected HTML.
set(FOLIA_COMMONMARK_CASES "${CMAKE_CURRENT_BINARY_DIR}/commonmark/commonmark_cases.inc")
add_custom_command(OUTPUT "${FOLIA_COMMONMARK_CASES}"
    COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/tools/markdown-conformance/prepare.py"
        --output "${FOLIA_COMMONMARK_CASES}"
    DEPENDS tools/markdown-conformance/prepare.py
        tools/markdown-conformance/canonical_html.py tools/markdown-conformance/sources.lock.json
    VERBATIM)
add_custom_target(FoliaCommonMarkFixtures DEPENDS "${FOLIA_COMMONMARK_CASES}")
add_executable(FoliaCommonMarkTests EXCLUDE_FROM_ALL
    tests/main.cpp tests/parsing/test_commonmark.cpp)
add_dependencies(FoliaCommonMarkTests FoliaCommonMarkFixtures)
target_compile_features(FoliaCommonMarkTests PRIVATE cxx_std_23)
set_target_properties(FoliaCommonMarkTests PROPERTIES CXX_MODULE_STD ON)
target_include_directories(FoliaCommonMarkTests PRIVATE tests "${CMAKE_CURRENT_BINARY_DIR}/commonmark")
target_link_libraries(FoliaCommonMarkTests PRIVATE folia_core Boost::ut_module)
add_test(NAME FoliaCommonMarkTests
    COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/tools/markdown-conformance/run.py"
        --executable "$<TARGET_FILE:FoliaCommonMarkTests>"
        --output "${CMAKE_CURRENT_BINARY_DIR}/markdown-conformance")
set_tests_properties(FoliaCommonMarkTests PROPERTIES LABELS "commonmark;conformance")

set(FOLIA_GFM_CASES "${CMAKE_CURRENT_BINARY_DIR}/gfm/gfm_cases.inc")
add_custom_command(OUTPUT "${FOLIA_GFM_CASES}"
    COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/tools/markdown-conformance/prepare_gfm.py"
        --output "${FOLIA_GFM_CASES}"
    DEPENDS tools/markdown-conformance/prepare_gfm.py tools/markdown-conformance/prepare.py
        tools/markdown-conformance/gfm-sources.lock.json
    VERBATIM)
add_executable(FoliaGfmTests EXCLUDE_FROM_ALL
    tests/main.cpp tests/parsing/test_gfm.cpp tests/parsing/test_gfm_footnotes.cpp "${FOLIA_GFM_CASES}")
target_compile_features(FoliaGfmTests PRIVATE cxx_std_23)
set_target_properties(FoliaGfmTests PROPERTIES CXX_MODULE_STD ON)
target_include_directories(FoliaGfmTests PRIVATE tests "${CMAKE_CURRENT_BINARY_DIR}/gfm")
target_link_libraries(FoliaGfmTests PRIVATE folia_core Boost::ut_module)
add_test(NAME FoliaGfmTests
    COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/tools/markdown-conformance/run.py" --suite gfm
        --executable "$<TARGET_FILE:FoliaGfmTests>"
        --output "${CMAKE_CURRENT_BINARY_DIR}/gfm-conformance")
set_tests_properties(FoliaGfmTests PROPERTIES LABELS "gfm;conformance")

# Full-size generated parser stress cases are isolated in one process per case
# by the runner so a pathological input cannot hold the ordinary console suite.
add_executable(FoliaGfmPathologicalTests EXCLUDE_FROM_ALL
    tests/main.cpp tests/parsing/test_gfm_pathological.cpp
    tests/parsing/test_inline_cst_ownership.cpp
    tests/parsing/test_deep_block_ownership.cpp
    tests/parsing/test_parser_completion.cpp
    tests/parsing/test_parser_container_frames.cpp
    tests/parsing/test_block_line_thematic.cpp)
target_compile_features(FoliaGfmPathologicalTests PRIVATE cxx_std_23)
set_target_properties(FoliaGfmPathologicalTests PROPERTIES CXX_MODULE_STD ON)
target_include_directories(FoliaGfmPathologicalTests PRIVATE tests)
target_link_libraries(FoliaGfmPathologicalTests PRIVATE folia_core Boost::ut_module)
add_test(NAME FoliaGfmPathologicalTests
    COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/tools/markdown-conformance/run_gfm_pathological.py"
        --executable "$<TARGET_FILE:FoliaGfmPathologicalTests>"
        --report "${CMAKE_CURRENT_BINARY_DIR}/gfm-pathological/results.json"
        --timeout-seconds 30)
set_tests_properties(FoliaGfmPathologicalTests PROPERTIES LABELS "gfm;generated;bounded")
add_test(NAME FoliaInlineCstOwnershipTests
    COMMAND FoliaGfmPathologicalTests "*inline_cst_deep_owner_copy_move_and_clear*")
set_tests_properties(FoliaInlineCstOwnershipTests PROPERTIES LABELS "gfm;generated;bounded" TIMEOUT 30)
add_test(NAME FoliaParserCompletionTests
    COMMAND FoliaGfmPathologicalTests "*parser_completion_matches_serializer_and_exact_save*")
set_tests_properties(FoliaParserCompletionTests PROPERTIES LABELS "gfm;generated;bounded" TIMEOUT 30)
add_test(NAME FoliaParserContainerFrameTests
    COMMAND FoliaGfmPathologicalTests "*container_frame*")
set_tests_properties(FoliaParserContainerFrameTests PROPERTIES LABELS "gfm;parser;bounded" TIMEOUT 30)
add_test(NAME FoliaBlockLineThematicTests
    COMMAND FoliaGfmPathologicalTests "*block_line_thematic*")
set_tests_properties(FoliaBlockLineThematicTests PROPERTIES LABELS "gfm;parser;bounded" TIMEOUT 30)
add_test(NAME FoliaDeepBlockOwnershipTests
    COMMAND FoliaGfmPathologicalTests "*deep_block_ownership_copy_move_clear_and_traversal*")
set_tests_properties(FoliaDeepBlockOwnershipTests PROPERTIES LABELS "gfm;generated;bounded" TIMEOUT 30)
# Full-depth serializer cases have independent bounded processes.
add_executable(FoliaDeepSerializerTests EXCLUDE_FROM_ALL
    tests/main.cpp tests/parsing/test_serializer_deep.cpp)
target_compile_features(FoliaDeepSerializerTests PRIVATE cxx_std_23)
set_target_properties(FoliaDeepSerializerTests PROPERTIES
    CXX_MODULE_STD ON CXX_SCAN_FOR_MODULES ON)
target_include_directories(FoliaDeepSerializerTests PRIVATE tests)
target_link_libraries(FoliaDeepSerializerTests PRIVATE folia_core Boost::ut_module)
add_test(NAME FoliaDeepSerializerQuotes
    COMMAND FoliaDeepSerializerTests "*deep_quote_50000*")
add_test(NAME FoliaDeepSerializerLists
    COMMAND FoliaDeepSerializerTests "*deep_nested_lists_1000*")
set_tests_properties(FoliaDeepSerializerQuotes FoliaDeepSerializerLists
    PROPERTIES LABELS "gfm;generated;bounded" TIMEOUT 60)

add_test(NAME FoliaGfmPathologicalRunnerTests
    COMMAND python -m unittest discover
        -s "${CMAKE_CURRENT_SOURCE_DIR}/tools/markdown-conformance/tests"
        -p test_run_gfm_pathological.py)
set_tests_properties(FoliaGfmPathologicalRunnerTests PROPERTIES LABELS "gfm;runner")

option(FOLIA_BUILD_MD4C_REFERENCE_TESTS "Build md4c controls for failing official CommonMark cases" OFF)
if(FOLIA_BUILD_MD4C_REFERENCE_TESTS)
    set(FOLIA_MD_REFERENCE "${CMAKE_CURRENT_SOURCE_DIR}/build/dependencies/markdown-conformance")
    if(NOT EXISTS "${FOLIA_MD_REFERENCE}/md4c.c")
        message(FATAL_ERROR "Run python tools/markdown-conformance/prepare.py --fetch --reference first")
    endif()
    add_library(folia_md4c_reference STATIC
        "${FOLIA_MD_REFERENCE}/md4c.c"
        "${FOLIA_MD_REFERENCE}/md4c-html.c"
        "${FOLIA_MD_REFERENCE}/entity.c")
    target_include_directories(folia_md4c_reference PUBLIC "${FOLIA_MD_REFERENCE}")
    target_compile_definitions(folia_md4c_reference PRIVATE _CRT_SECURE_NO_WARNINGS)
    set(FOLIA_MD4C_FAILED_CASES "${CMAKE_CURRENT_BINARY_DIR}/commonmark/md4c_failed_examples.inc")
    add_custom_command(OUTPUT "${FOLIA_MD4C_FAILED_CASES}"
        COMMAND python "${CMAKE_CURRENT_SOURCE_DIR}/tools/markdown-conformance/reference.py"
            --generate "${FOLIA_MD4C_FAILED_CASES}"
        DEPENDS tools/markdown-conformance/reference.py
            "${CMAKE_CURRENT_SOURCE_DIR}/build/markdown-conformance/results.json"
        VERBATIM)
    add_custom_target(FoliaMd4cFailureFixtures DEPENDS "${FOLIA_MD4C_FAILED_CASES}")
    add_executable(FoliaMd4cReferenceTests EXCLUDE_FROM_ALL
        tests/main.cpp tests/parsing/test_md4c_reference.cpp)
    add_dependencies(FoliaMd4cReferenceTests FoliaCommonMarkFixtures FoliaMd4cFailureFixtures)
    target_compile_features(FoliaMd4cReferenceTests PRIVATE cxx_std_23)
    set_target_properties(FoliaMd4cReferenceTests PROPERTIES CXX_MODULE_STD ON)
    target_include_directories(FoliaMd4cReferenceTests PRIVATE tests "${CMAKE_CURRENT_BINARY_DIR}/commonmark")
    target_link_libraries(FoliaMd4cReferenceTests PRIVATE folia_md4c_reference Boost::ut_module)
    add_test(NAME FoliaMd4cReferenceTests COMMAND FoliaMd4cReferenceTests)
    set_tests_properties(FoliaMd4cReferenceTests PROPERTIES LABELS "commonmark;reference")
endif()

# Keep CRT reports on stderr, including failures before main().
if(WIN32 AND MSVC)
    add_library(folia_console_test_runtime OBJECT tests/support/windows_console_runtime.cpp)
    target_compile_features(folia_console_test_runtime PRIVATE cxx_std_23)
    set_target_properties(folia_console_test_runtime PROPERTIES
        CXX_MODULE_STD OFF CXX_SCAN_FOR_MODULES OFF)
    get_property(folia_console_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(folia_console_target IN LISTS folia_console_targets)
        get_target_property(folia_console_type ${folia_console_target} TYPE)
        if(folia_console_type STREQUAL "EXECUTABLE" AND folia_console_target MATCHES "^Folia")
            target_link_libraries(${folia_console_target} PRIVATE folia_console_test_runtime)
        endif()
    endforeach()
endif()
