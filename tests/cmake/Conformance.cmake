# These CMake executables are console ${FOLIA_SOURCE_ROOT}/tests/probes; the WinUI app uses MSBuild.
# Official specification tests run the native source invariants and compare
# projected HTML with the same canonicalizer used for official expected HTML.
add_executable(FoliaCommonMarkTests EXCLUDE_FROM_ALL
    ${FOLIA_SOURCE_ROOT}/tests/main.cpp ${FOLIA_SOURCE_ROOT}/tests/parsing/test_commonmark.cpp)
target_compile_features(FoliaCommonMarkTests PRIVATE cxx_std_23)
set_target_properties(FoliaCommonMarkTests PROPERTIES CXX_MODULE_STD ON)
target_include_directories(FoliaCommonMarkTests PRIVATE "${FOLIA_SOURCE_ROOT}/tests")
target_link_libraries(FoliaCommonMarkTests PRIVATE folia_core Boost::ut_module folia_test_support)
add_test(NAME FoliaCommonMarkTests
    COMMAND "${Python3_EXECUTABLE}" "${FOLIA_SOURCE_ROOT}/tools/markdown-conformance/run.py"
        --executable "$<TARGET_FILE:FoliaCommonMarkTests>"
        --output "${CMAKE_CURRENT_BINARY_DIR}/markdown-conformance")
set_tests_properties(FoliaCommonMarkTests PROPERTIES LABELS "commonmark;conformance")

add_executable(FoliaGfmTests EXCLUDE_FROM_ALL
    ${FOLIA_SOURCE_ROOT}/tests/main.cpp ${FOLIA_SOURCE_ROOT}/tests/parsing/test_gfm.cpp ${FOLIA_SOURCE_ROOT}/tests/parsing/test_gfm_footnotes.cpp)
target_compile_features(FoliaGfmTests PRIVATE cxx_std_23)
set_target_properties(FoliaGfmTests PROPERTIES CXX_MODULE_STD ON)
target_include_directories(FoliaGfmTests PRIVATE "${FOLIA_SOURCE_ROOT}/tests")
target_link_libraries(FoliaGfmTests PRIVATE folia_core Boost::ut_module folia_test_support)
add_test(NAME FoliaGfmTests
    COMMAND "${Python3_EXECUTABLE}" "${FOLIA_SOURCE_ROOT}/tools/markdown-conformance/run.py" --suite gfm
        --executable "$<TARGET_FILE:FoliaGfmTests>"
        --output "${CMAKE_CURRENT_BINARY_DIR}/gfm-conformance")
set_tests_properties(FoliaGfmTests PROPERTIES LABELS "gfm;conformance")

# Full-size generated parser stress cases are isolated in one process per case
# by the runner so a pathological input cannot hold the ordinary console suite.
add_executable(FoliaGfmPathologicalTests EXCLUDE_FROM_ALL
    ${FOLIA_SOURCE_ROOT}/tests/main.cpp ${FOLIA_SOURCE_ROOT}/tests/parsing/test_gfm_pathological.cpp
    ${FOLIA_SOURCE_ROOT}/tests/parsing/test_inline_cst_ownership.cpp
    ${FOLIA_SOURCE_ROOT}/tests/parsing/test_deep_block_ownership.cpp
    ${FOLIA_SOURCE_ROOT}/tests/parsing/test_parser_completion.cpp
    ${FOLIA_SOURCE_ROOT}/tests/parsing/test_parser_container_frames.cpp
    ${FOLIA_SOURCE_ROOT}/tests/parsing/test_block_line_thematic.cpp)
target_compile_features(FoliaGfmPathologicalTests PRIVATE cxx_std_23)
set_target_properties(FoliaGfmPathologicalTests PROPERTIES CXX_MODULE_STD ON)
target_include_directories(FoliaGfmPathologicalTests PRIVATE "${FOLIA_SOURCE_ROOT}/tests")
target_link_libraries(FoliaGfmPathologicalTests PRIVATE folia_core Boost::ut_module folia_test_support)
add_test(NAME FoliaGfmPathologicalTests
    COMMAND "${Python3_EXECUTABLE}" "${FOLIA_SOURCE_ROOT}/tools/markdown-conformance/run_gfm_pathological.py"
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
    ${FOLIA_SOURCE_ROOT}/tests/main.cpp ${FOLIA_SOURCE_ROOT}/tests/parsing/test_serializer_deep.cpp)
target_compile_features(FoliaDeepSerializerTests PRIVATE cxx_std_23)
set_target_properties(FoliaDeepSerializerTests PROPERTIES
    CXX_MODULE_STD ON CXX_SCAN_FOR_MODULES ON)
target_include_directories(FoliaDeepSerializerTests PRIVATE "${FOLIA_SOURCE_ROOT}/tests")
target_link_libraries(FoliaDeepSerializerTests PRIVATE folia_core Boost::ut_module folia_test_support)
add_test(NAME FoliaDeepSerializerQuotes
    COMMAND FoliaDeepSerializerTests "*deep_quote_50000*")
add_test(NAME FoliaDeepSerializerLists
    COMMAND FoliaDeepSerializerTests "*deep_nested_lists_1000*")
set_tests_properties(FoliaDeepSerializerQuotes FoliaDeepSerializerLists
    PROPERTIES LABELS "gfm;generated;bounded" TIMEOUT 60)

add_test(NAME FoliaGfmPathologicalRunnerTests
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${FOLIA_SOURCE_ROOT}/tools/markdown-conformance/tests"
        -p test_run_gfm_pathological.py)
set_tests_properties(FoliaGfmPathologicalRunnerTests PROPERTIES LABELS "gfm;runner")

option(FOLIA_BUILD_MD4C_REFERENCE_TESTS "Build md4c controls for failing official CommonMark cases" OFF)
if(FOLIA_BUILD_MD4C_REFERENCE_TESTS)
    set(FOLIA_MD_REFERENCE "${FOLIA_SOURCE_ROOT}/build/dependencies/markdown-conformance")
    if(NOT EXISTS "${FOLIA_MD_REFERENCE}/md4c.c")
        message(FATAL_ERROR "Run python ${FOLIA_SOURCE_ROOT}/tools/markdown-conformance/prepare.py --fetch --reference first")
    endif()
    add_library(folia_md4c_reference STATIC
        "${FOLIA_MD_REFERENCE}/md4c.c"
        "${FOLIA_MD_REFERENCE}/md4c-html.c"
        "${FOLIA_MD_REFERENCE}/entity.c")
    target_include_directories(folia_md4c_reference PUBLIC "${FOLIA_MD_REFERENCE}")
    target_compile_definitions(folia_md4c_reference PRIVATE _CRT_SECURE_NO_WARNINGS)
    set(FOLIA_MD4C_FAILED_CASES "${CMAKE_CURRENT_BINARY_DIR}/commonmark/md4c_failed_examples.inc")
    add_custom_command(OUTPUT "${FOLIA_MD4C_FAILED_CASES}"
        COMMAND "${Python3_EXECUTABLE}" "${FOLIA_SOURCE_ROOT}/tools/markdown-conformance/reference.py"
            --generate "${FOLIA_MD4C_FAILED_CASES}"
        DEPENDS ${FOLIA_SOURCE_ROOT}/tools/markdown-conformance/reference.py
            "${FOLIA_SOURCE_ROOT}/build/markdown-conformance/results.json"
        VERBATIM)
    add_custom_target(FoliaMd4cFailureFixtures DEPENDS "${FOLIA_MD4C_FAILED_CASES}")
    add_executable(FoliaMd4cReferenceTests EXCLUDE_FROM_ALL
        ${FOLIA_SOURCE_ROOT}/tests/main.cpp ${FOLIA_SOURCE_ROOT}/tests/parsing/test_md4c_reference.cpp)
    add_dependencies(FoliaMd4cReferenceTests FoliaMd4cFailureFixtures)
    target_compile_features(FoliaMd4cReferenceTests PRIVATE cxx_std_23)
    set_target_properties(FoliaMd4cReferenceTests PROPERTIES CXX_MODULE_STD ON)
    target_include_directories(FoliaMd4cReferenceTests PRIVATE "${FOLIA_SOURCE_ROOT}/tests" "${CMAKE_CURRENT_BINARY_DIR}/commonmark")
    target_link_libraries(FoliaMd4cReferenceTests PRIVATE folia_md4c_reference Boost::ut_module folia_test_support)
    add_test(NAME FoliaMd4cReferenceTests COMMAND FoliaMd4cReferenceTests)
    set_tests_properties(FoliaMd4cReferenceTests PROPERTIES LABELS "commonmark;reference")
endif()

