# Embed with add_subdirectory(src/markdown); no Folia root configuration needed.
if(NOT TARGET folia_tree_sitter_runtime)
    add_subdirectory(../../third_party/tree-sitter ${CMAKE_CURRENT_BINARY_DIR}/tree-sitter)
endif()
add_library(folia_tree_sitter_html STATIC
    ../../third_party/tree-sitter/grammars/html_parser.c
    ../../third_party/tree-sitter/grammars/html_scanner.c)
target_include_directories(folia_tree_sitter_html PRIVATE
    ../../third_party/tree-sitter/include ../../third_party/tree-sitter/runtime)
target_link_libraries(folia_tree_sitter_html PRIVATE folia_tree_sitter_runtime)
set_target_properties(folia_tree_sitter_html PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
if(MSVC)
    target_compile_options(folia_tree_sitter_html PRIVATE /wd4244 /wd4996)
endif()

add_library(folia_markdown STATIC)
add_library(folia::markdown ALIAS folia_markdown)
target_compile_features(folia_markdown PUBLIC cxx_std_23)
set_target_properties(folia_markdown PROPERTIES CXX_MODULE_STD ON)
target_include_directories(folia_markdown PRIVATE ../../third_party/tree-sitter/include)
target_link_libraries(folia_markdown PRIVATE folia_tree_sitter_html)
if(MSVC)
    target_compile_options(folia_markdown PRIVATE /utf-8)
endif()
target_sources(folia_markdown PUBLIC FILE_SET CXX_MODULES FILES
    markdown.ixx
    model/ast.ixx
    model/block_tree.ixx
    model/callout.ixx
    model/container_source.ixx
    model/document.ixx
    model/ids.ixx
    model/image_dimension.ixx
    model/inline_cst.ixx
    model/inline_document.ixx
    model/metadata.ixx
    model/selection.ixx
    model/symbols.ixx
    model/text_edit.ixx
    parsing/block_line_recognizer.ixx
    parsing/block_source.ixx
    parsing/dialect.ixx
    parsing/gfm_autolink.ixx
    parsing/html_cst.ixx
    parsing/image_block_promotion.ixx
    parsing/inline_parser.ixx
    parsing/link_label.ixx
    parsing/markdown_html.ixx
    parsing/parser.ixx
    parsing/parser_code_math_leaf_source.ixx
    parsing/parser_html_source_view.ixx
    parsing/parser_leaf_line_view.ixx
    parsing/parser_source_session.ixx
    parsing/serializer.ixx
    parsing/serializer_visitor.ixx
    parsing/serializer_writer.ixx
    parsing/slug.ixx
    query/document_symbols.ixx
    query/document_text.ixx
    query/outline.ixx
    support/diagnostics.ixx
    support/html_entities.ixx
    support/instrumentation.ixx
    support/utf.ixx
)
