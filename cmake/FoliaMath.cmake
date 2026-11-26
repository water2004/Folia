# Shared production math targets. Test entry points do not copy this source list.
include_guard(GLOBAL)

# Native math reconstruction: only reviewed typed C++ owners are active.
# Parser/packages and shared Common placement use the accepted typed owners.
# The application-facing Renderer composes them without a Context runtime.
add_library(folia_math STATIC)
target_compile_features(folia_math PUBLIC cxx_std_23)
set_target_properties(folia_math PROPERTIES CXX_MODULE_STD ON POSITION_INDEPENDENT_CODE ON)
target_include_directories(folia_math PRIVATE ${FOLIA_SOURCE_ROOT}/third_party/srell)
# Font tables are generated offline from the pinned Source package data. The
# generator verifies every locked input and preserves unchanged output mtimes.
find_program(FOLIA_NODE_EXECUTABLE NAMES node REQUIRED)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${FOLIA_SOURCE_ROOT}/tools/math-port/prepare-source.py"
    RESULT_VARIABLE FOLIA_SOURCE_PREPARE_RESULT
    ERROR_VARIABLE FOLIA_SOURCE_PREPARE_ERROR)
if(NOT FOLIA_SOURCE_PREPARE_RESULT EQUAL 0)
    message(FATAL_ERROR "MathJax source preparation failed: ${FOLIA_SOURCE_PREPARE_ERROR}")
endif()
execute_process(
    COMMAND ${FOLIA_NODE_EXECUTABLE} tools/math-port/font-data-generate-native.cjs
    WORKING_DIRECTORY ${FOLIA_SOURCE_ROOT}
    RESULT_VARIABLE FOLIA_FONT_GENERATE_RESULT
    OUTPUT_VARIABLE FOLIA_FONT_GENERATE_OUTPUT
    ERROR_VARIABLE FOLIA_FONT_GENERATE_ERROR)
if(NOT FOLIA_FONT_GENERATE_RESULT EQUAL 0)
    message(FATAL_ERROR "Native font data generation failed: ${FOLIA_FONT_GENERATE_ERROR}")
endif()
set(FOLIA_FONT_GENERATED_DIR "${FOLIA_SOURCE_ROOT}/build/math-port/font-generated-native-raii")
file(READ "${FOLIA_FONT_GENERATED_DIR}/font-data-source-dependencies.json" FOLIA_FONT_DEPENDENCIES)
string(JSON FOLIA_FONT_DEPENDENCY_COUNT LENGTH "${FOLIA_FONT_DEPENDENCIES}" source_dependencies)
math(EXPR FOLIA_FONT_DEPENDENCY_LAST "${FOLIA_FONT_DEPENDENCY_COUNT} - 1")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${FOLIA_SOURCE_ROOT}/tools/math-port/font-data-generate-native.cjs"
    "${FOLIA_SOURCE_ROOT}/docs/math-port/evidence/font-data-source-lock.json")
foreach(FOLIA_FONT_INDEX RANGE 0 ${FOLIA_FONT_DEPENDENCY_LAST})
    string(JSON FOLIA_FONT_DEPENDENCY GET "${FOLIA_FONT_DEPENDENCIES}" source_dependencies ${FOLIA_FONT_INDEX} path)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${FOLIA_SOURCE_ROOT}/${FOLIA_FONT_DEPENDENCY}")
endforeach()
string(JSON FOLIA_FONT_GENERATED_COUNT LENGTH "${FOLIA_FONT_DEPENDENCIES}" generated_cpp)
math(EXPR FOLIA_FONT_GENERATED_LAST "${FOLIA_FONT_GENERATED_COUNT} - 1")
set(FOLIA_FONT_GENERATED_CPP)
foreach(FOLIA_FONT_INDEX RANGE 0 ${FOLIA_FONT_GENERATED_LAST})
    string(JSON FOLIA_FONT_GENERATED_FILE GET "${FOLIA_FONT_DEPENDENCIES}" generated_cpp ${FOLIA_FONT_INDEX})
    if(FOLIA_FONT_GENERATED_FILE MATCHES "[.]cpp$")
        list(APPEND FOLIA_FONT_GENERATED_CPP "${FOLIA_SOURCE_ROOT}/${FOLIA_FONT_GENERATED_FILE}")
    endif()
endforeach()
target_include_directories(folia_math PRIVATE ${FOLIA_FONT_GENERATED_DIR})
# Entity resources are local data, generated from all locked Source add() calls.
set(FOLIA_ENTITIES_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/entities")
execute_process(
    COMMAND ${FOLIA_NODE_EXECUTABLE} --max-old-space-size=512
        "${FOLIA_SOURCE_ROOT}/tools/math-port/entities-data-generate.cjs"
        --output "${FOLIA_ENTITIES_GENERATED_DIR}/entities_data.inc"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    RESULT_VARIABLE FOLIA_ENTITIES_GENERATE_RESULT
    OUTPUT_VARIABLE FOLIA_ENTITIES_GENERATE_OUTPUT
    ERROR_VARIABLE FOLIA_ENTITIES_GENERATE_ERROR)
if(NOT FOLIA_ENTITIES_GENERATE_RESULT EQUAL 0)
    message(FATAL_ERROR "Native entity data generation failed: ${FOLIA_ENTITIES_GENERATE_ERROR}")
endif()
set(FOLIA_ENTITIES_LOCK "${FOLIA_SOURCE_ROOT}/docs/math-port/evidence/entities-source-lock.json")
file(READ "${FOLIA_ENTITIES_LOCK}" FOLIA_ENTITIES_DEPENDENCIES)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${FOLIA_SOURCE_ROOT}/tools/math-port/entities-data-generate.cjs"
    "${FOLIA_ENTITIES_LOCK}")
foreach(FOLIA_ENTITIES_KIND IN ITEMS ts cjs)
    string(JSON FOLIA_ENTITIES_COUNT LENGTH "${FOLIA_ENTITIES_DEPENDENCIES}" source ${FOLIA_ENTITIES_KIND})
    math(EXPR FOLIA_ENTITIES_LAST "${FOLIA_ENTITIES_COUNT} - 1")
    if(FOLIA_ENTITIES_KIND STREQUAL "ts")
        set(FOLIA_ENTITIES_ROOT "${FOLIA_SOURCE_ROOT}/.reference/MathJax-src/ts/util")
    else()
        set(FOLIA_ENTITIES_ROOT "${FOLIA_SOURCE_ROOT}/tools/mathjax-bundle/node_modules/@mathjax/src/cjs/util")
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
target_sources(folia_math PUBLIC FILE_SET CXX_MODULES BASE_DIRS "${FOLIA_SOURCE_ROOT}" FILES
    ${FOLIA_SOURCE_ROOT}/src/math/math.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/tree/factory_types.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/tree/factory.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/tree/wrapper_types.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/tree/wrapper.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/tree/wrapper_factory.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/tree/node_types.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/tree/node.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/tree/visitor.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/math_item.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/node_kind.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/attributes.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/node_properties.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mml_node.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mml_document.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/node_traits.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/node_defaults.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mml_factory.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/operator_dictionary.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mo_patterns.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/session.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/token.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/parser_types.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/macro.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/token_map.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/map_handler.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/parse_options.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/tags.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/stack_item.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/stack_item_factory.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/base/base_items.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/stack.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/tex_parser.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/text_parser.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/column_parser.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/parser_family.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/begingroup/stack.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/begingroup/methods.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/begingroup/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/environment.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/tex_error.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/tex_scanner.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/unit_util.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/parse_util.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/parse_methods.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/base/base_methods.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/base/base_mappings.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/base/base_configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/tex_jax.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/filter_util.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/input_jax.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/node_factory.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/node_util.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/boldsymbol/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/newcommand/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/newcommand/items.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/newcommand/mappings.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/newcommand/methods.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/newcommand/util.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/color/constants.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/color/util.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/color/methods.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/color/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/noundefined/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/configmacros/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/verb/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/upgreek/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/default_packages.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/mediawiki_texvc/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/gensymb/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/ams/methods.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/ams/items.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/ams/mappings.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/ams/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/amscd/methods.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/amscd/mappings.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/amscd/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/extpfeil/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bbox/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/enclose/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/cancel/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/html/methods.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/html/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/action/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/centernot/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/fontsizev3/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/braket/items.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/braket/methods.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/braket/mappings.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/braket/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/units/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/textmacros_methods.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/textmacros_mappings.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/textmacros_configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/textcomp/mappings.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/textcomp/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bbm/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/dsfont/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bboldx/methods.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bboldx/mappings.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bboldx/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/tagformat/tag_format_configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/setoptions/set_options_configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/empheq/util.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/empheq/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/cases/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/colortbl/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/mathtools/tags.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/mathtools/items.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/mathtools/methods.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/mathtools/util.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/mathtools/mappings.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/mathtools/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bussproofs/items.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bussproofs/util.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bussproofs/methods.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bussproofs/mappings.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bussproofs/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/physics/items.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/physics/methods.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/physics/mappings.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/physics/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/unicode/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/mhchem/configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/dependencies/mhchem_parser/parser.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/scene/types.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/scene/html_types.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/scene/html_snapshot.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/scene/scene.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/scene/builder.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/writer.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/font_cache.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/menclose_notations.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/merror.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/maction.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/mglyph.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/wrapper_factory.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/xml_wrapper.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/serializer.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/font_data.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/direction.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/font_data.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/font_path.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/line_bbox.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/notation.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/common.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/wrapper.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/wrapper_factory.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/output_jax.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/linebreak_visitor.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/wrappers/mfrac.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/wrappers/msqrt.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/wrappers/mroot.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/fonts/newcm_font_data.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/fonts/newcm_font_module_provider.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/constants.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/instance_count.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/prioritized_list.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/function_list.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/bbox.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/lengths.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/js_number.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/styles.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/style_json.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/unicode_normalization.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/unicode_normalization_data.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/types.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/regexp_host.ixx)
target_sources(folia_math PRIVATE
    ${FOLIA_SOURCE_ROOT}/src/math/math.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/default_packages.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/mediawiki_texvc/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/math_item.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/math_item_state.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/attributes.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/node_properties.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mml_node.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mml_document.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mml_factory.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/node_defaults.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/operator_dictionary.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/operator_dictionary_data.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mo_patterns.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mo_pattern_data.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/session.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/token.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/parse_options.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/stack_item.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/stack_item_factory.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/base/base_items.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/stack.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/tex_parser.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/text_parser.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/column_parser.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/tags.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/macro.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/token_map.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/map_handler.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/begingroup/stack.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/begingroup/methods.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/begingroup/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/environment.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/tex_error.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/tex_scanner.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/unit_util.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/parse_util.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/parse_methods.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/base/base_methods.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/base/base_mappings.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/base/base_configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/tex_jax.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/filter_util.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/node_factory.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/node_util.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/boldsymbol/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/newcommand/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/newcommand/items.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/newcommand/mappings.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/newcommand/methods.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/newcommand/util.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/color/constants.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/color/util.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/color/methods.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/color/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/noundefined/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/configmacros/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/verb/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/upgreek/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/gensymb/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/ams/methods.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/ams/items.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/ams/mappings.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/ams/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/amscd/methods.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/amscd/mappings.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/amscd/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/extpfeil/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bbox/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/enclose/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/cancel/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/html/methods.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/html/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/action/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/centernot/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/fontsizev3/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/braket/items.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/braket/methods.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/braket/mappings.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/braket/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/units/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/textmacros_methods.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/textmacros_mappings.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/textmacros_configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/textcomp/mappings.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/textcomp/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bbm/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/dsfont/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bboldx/methods.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bboldx/mappings.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bboldx/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/tagformat/tag_format_configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/setoptions/set_options_configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/empheq/util.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/empheq/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/cases/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/colortbl/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/mathtools/tags.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/mathtools/items.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/mathtools/methods.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/mathtools/util.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/mathtools/mappings.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/mathtools/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bussproofs/items.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bussproofs/util.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bussproofs/methods.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bussproofs/mappings.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/bussproofs/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/physics/items.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/physics/methods.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/physics/mappings.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/physics/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/unicode/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/mhchem/configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/dependencies/mhchem_parser/parser.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/dependencies/mhchem_parser/patterns.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/dependencies/mhchem_parser/actions.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/dependencies/mhchem_parser/texify.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/scene/scene.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/scene/builder.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/scene/html_snapshot.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/writer.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/font_cache.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/menclose_notations.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/merror.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/maction.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/mglyph.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/wrapper_factory.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/xml_wrapper.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/serializer.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/node_metadata.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/svg/font_data.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/font_data.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/font_path.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/line_bbox.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/notation.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/wrapper.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/wrapper_factory.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/output_jax.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/linebreak_visitor.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/wrappers/mfrac.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/wrappers/msqrt.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/output/common/wrappers/mroot.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/fonts/newcm_font_data.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/fonts/newcm_font_module_provider.cpp
    ${FOLIA_FONT_GENERATED_CPP}
    ${FOLIA_SOURCE_ROOT}/src/math/util/lengths.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/util/styles.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/util/style_json.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/util/regexp_host.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/util/unicode_normalization.cpp)

# Native Common classes retain their original family partitions; the concrete
# LinebreakVisitor uses these same classes for recursive mathematical breaks.
set(FOLIA_COMMON_NATIVE_FAMILIES
    mo mrow inferred_mrow text_node mi mn ms mspace mglyph mtext math
    scriptbase msubsup mmultiscripts munderover mfenced maction
    mtable mtr mtd mpadded mphantom semantics tex_atom menclose xml_node)
foreach(family IN LISTS FOLIA_COMMON_NATIVE_FAMILIES)
    target_sources(folia_math PUBLIC FILE_SET CXX_MODULES BASE_DIRS "${FOLIA_SOURCE_ROOT}" FILES
        ${FOLIA_SOURCE_ROOT}/src/math/output/common/wrappers/${family}.ixx)
    target_sources(folia_math PRIVATE ${FOLIA_SOURCE_ROOT}/src/math/output/common/wrappers/${family}.cpp)
endforeach()

# Actual MathJax MML families share the document-owned native arena. Keep the
# original per-family modules; only maction and mo have split implementation TUs.
set(FOLIA_MML_FAMILIES
    math mi mn mo mtext mspace ms mrow mfrac msqrt mroot mstyle merror mpadded
    mphantom mfenced menclose maction msubsup munderover mmultiscripts mtable
    mtr mtd maligngroup malignmark mglyph semantics tex_atom mathchoice html_node)
foreach(family IN LISTS FOLIA_MML_FAMILIES)
    target_sources(folia_math PUBLIC FILE_SET CXX_MODULES BASE_DIRS "${FOLIA_SOURCE_ROOT}" FILES
        ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mml_nodes/${family}.ixx)
    if(NOT family STREQUAL "maction" AND NOT family STREQUAL "mo")
        target_sources(folia_math PRIVATE ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mml_nodes/${family}.cpp)
    endif()
endforeach()
target_sources(folia_math PUBLIC FILE_SET CXX_MODULES BASE_DIRS "${FOLIA_SOURCE_ROOT}" FILES
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/html_adaptor.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/xml_dom_adaptor.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/dom_node.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/dom_adaptor.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/find_math.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/find_tex.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/adaptors/lite_adaptor.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/adaptors/lite/element.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/adaptors/lite/text.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/adaptors/lite/document.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/adaptors/lite/window.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/adaptors/lite/list.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/adaptors/lite/parser.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/handlers/html/html_dom_strings.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/texhtml/tex_html_configuration.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mo_types.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/operator_class.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/js_numeric_input.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/js_case.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/utf16_types.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/utf16.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/string.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/util/entities.ixx
    ${FOLIA_SOURCE_ROOT}/src/math/cancellation.ixx)
target_sources(folia_math PRIVATE
    ${FOLIA_SOURCE_ROOT}/src/math/core/dom_adaptor.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/find_math.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/find_tex.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/xml_dom_adaptor.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/adaptors/lite_adaptor.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/adaptors/lite/element.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/adaptors/lite/text.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/adaptors/lite/document.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/adaptors/lite/window.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/adaptors/lite/list.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/adaptors/lite/parser.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/handlers/html/html_dom_strings.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/input/tex/texhtml/tex_html_configuration.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mml_branches.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mml_empty.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mml_nodes/maction_selection.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mml_nodes/maction_behavior.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mml_nodes/mo_lookup.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mml_nodes/mo_classes.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/mml_nodes/mo_scripts.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/core/mml_tree/operator_class.cpp
    ${FOLIA_SOURCE_ROOT}/src/math/cancellation.cpp)
target_sources(folia_math PRIVATE ${FOLIA_SOURCE_ROOT}/src/math/util/string.cpp)
target_sources(folia_math PRIVATE ${FOLIA_SOURCE_ROOT}/src/math/util/entities.cpp)
target_sources(folia_math PRIVATE ${FOLIA_SOURCE_ROOT}/src/math/util/js_case.cpp ${FOLIA_SOURCE_ROOT}/src/math/util/js_case_data.cpp)

# Opaque application boundary. Only the C functions cross the DLL boundary;
# the implementation and its C++ module dependencies remain private.
add_library(FoliaNativeMath SHARED ${FOLIA_SOURCE_ROOT}/src/math/native_renderer_bridge.cpp)
target_include_directories(FoliaNativeMath PUBLIC ${FOLIA_SOURCE_ROOT}/src/math/include)
target_compile_features(FoliaNativeMath PRIVATE cxx_std_23)
set_target_properties(FoliaNativeMath PROPERTIES CXX_MODULE_STD ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(FoliaNativeMath PRIVATE folia_math)
# Match the Windows .def boundary on ELF: archive-owned C++ module
# initializers must not interpose with a test executable's static Math engine.
target_link_options(FoliaNativeMath PRIVATE
    "$<$<PLATFORM_ID:Linux>:LINKER:--exclude-libs,ALL>")
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
FoliaNativeMathRenderSvgFragment
FoliaNativeMathResultSvg
FoliaNativeMathResultFragmentMetrics
FoliaNativeMathResultReusableWidthFrom
FoliaNativeMathResultError
FoliaNativeMathResultDestroy
")
    target_sources(FoliaNativeMath PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/FoliaNativeMath.def")
endif()

