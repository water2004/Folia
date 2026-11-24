if(NOT TARGET folia_math_editor)
    add_subdirectory(../math/editor ${CMAKE_CURRENT_BINARY_DIR}/math-editor)
endif()

file(GLOB grammars CONFIGURE_DEPENDS ../../third_party/tree-sitter/grammars/*.c)
add_library(folia_highlight STATIC)
add_library(folia::highlight ALIAS folia_highlight)
target_compile_features(folia_highlight PUBLIC cxx_std_23)
set_target_properties(folia_highlight PROPERTIES CXX_MODULE_STD ON C_STANDARD 11 C_STANDARD_REQUIRED ON)
target_sources(folia_highlight PUBLIC FILE_SET CXX_MODULES FILES highlight.ixx)
target_sources(folia_highlight PRIVATE highlight.cpp ${grammars})
target_link_libraries(folia_highlight PUBLIC folia_math_editor PRIVATE folia_tree_sitter_runtime)
if(MSVC)
    set_source_files_properties(${grammars} PROPERTIES
        COMPILE_OPTIONS "/wd4244;/wd4267;/wd4996"
        COMPILE_DEFINITIONS "_CRT_SECURE_NO_WARNINGS;TREE_SITTER_HIDE_SYMBOLS")
endif()
