# These existing editor policies use only std, Markdown and core modules.
# Keep their original Windows-runner sources intact and run them independently
# on every host, without linking the SDK-dependent folia_platform library.
add_library(folia_test_editor_policies STATIC)
target_sources(folia_test_editor_policies PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS "${FOLIA_SOURCE_ROOT}/src/platform/windows/editor"
    FILES
        "${FOLIA_SOURCE_ROOT}/src/platform/windows/editor/layout/editor_display_mapping.ixx"
        "${FOLIA_SOURCE_ROOT}/src/platform/windows/editor/layout/editor_priority_work_queue.ixx"
        "${FOLIA_SOURCE_ROOT}/src/platform/windows/editor/interaction/editor_frame_pacing.ixx"
        "${FOLIA_SOURCE_ROOT}/src/platform/windows/editor/interaction/editor_latex_completion_session.ixx"
        "${FOLIA_SOURCE_ROOT}/src/platform/windows/editor/interaction/editor_scroll_state.ixx"
        "${FOLIA_SOURCE_ROOT}/src/platform/windows/editor/interaction/editor_selection_drag_model.ixx"
        "${FOLIA_SOURCE_ROOT}/src/platform/windows/editor/interaction/editor_snippet_session.ixx")
target_compile_features(folia_test_editor_policies PUBLIC cxx_std_23)
set_target_properties(folia_test_editor_policies PROPERTIES CXX_MODULE_STD ON)
target_link_libraries(folia_test_editor_policies PUBLIC folia_core)
if(MSVC)
    target_compile_options(folia_test_editor_policies PRIVATE /utf-8 /permissive- /Zc:preprocessor)
endif()

add_executable(FoliaPortablePolicyTests
    "${FOLIA_SOURCE_ROOT}/tests/main.cpp"
    "${FOLIA_SOURCE_ROOT}/tests/platform/test_display_mapping.cpp"
    "${FOLIA_SOURCE_ROOT}/tests/platform/test_editor_frame_pacing.cpp"
    "${FOLIA_SOURCE_ROOT}/tests/platform/test_editor_latex_completion_session.cpp"
    "${FOLIA_SOURCE_ROOT}/tests/platform/test_editor_priority_work_queue.cpp"
    "${FOLIA_SOURCE_ROOT}/tests/platform/test_editor_scroll_state.cpp"
    "${FOLIA_SOURCE_ROOT}/tests/platform/test_editor_selection_drag_model.cpp"
    "${FOLIA_SOURCE_ROOT}/tests/platform/test_editor_snippet_session.cpp")
folia_test_defaults(FoliaPortablePolicyTests)
target_link_libraries(FoliaPortablePolicyTests PRIVATE folia_test_editor_policies Boost::ut_module)
add_test(NAME FoliaPortablePolicyTests COMMAND FoliaPortablePolicyTests)
set_tests_properties(FoliaPortablePolicyTests PROPERTIES LABELS "core;policy;portable")
