# Share the same consumer target with the standalone Markdown-only project.
# This target's production dependency is folia::markdown, never folia_core.
add_executable(FoliaMarkdownConsumer
    "${FOLIA_SOURCE_ROOT}/tests/main.cpp"
    "${FOLIA_SOURCE_ROOT}/tests/libraries/markdown/test_consumer.cpp")
target_compile_features(FoliaMarkdownConsumer PRIVATE cxx_std_23)
set_target_properties(FoliaMarkdownConsumer PROPERTIES
    CXX_MODULE_STD ON CXX_SCAN_FOR_MODULES ON)
target_include_directories(FoliaMarkdownConsumer PRIVATE "${FOLIA_SOURCE_ROOT}/tests")
target_link_libraries(FoliaMarkdownConsumer PRIVATE folia::markdown Boost::ut_module)
if(WIN32)
    target_link_libraries(FoliaMarkdownConsumer PRIVATE folia_console_test_runtime)
endif()
if(MSVC)
    target_compile_options(FoliaMarkdownConsumer PRIVATE /utf-8 /permissive- /Zc:preprocessor)
endif()
add_test(NAME markdown_consumer COMMAND FoliaMarkdownConsumer)
set_tests_properties(markdown_consumer PROPERTIES LABELS "core;markdown;portable")
