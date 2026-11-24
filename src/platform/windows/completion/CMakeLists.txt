add_library(folia_windows_completion STATIC CompletionService.cpp CompletionHttpTransport.cpp)
add_library(folia::windows_completion ALIAS folia_windows_completion)
target_compile_features(folia_windows_completion PUBLIC cxx_std_23)
set_target_properties(folia_windows_completion PROPERTIES CXX_MODULE_STD ON)
target_link_libraries(folia_windows_completion PUBLIC folia_completion
    PRIVATE folia_platform folia_markdown folia_model_providers winhttp advapi32)
target_include_directories(folia_windows_completion PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../..")
