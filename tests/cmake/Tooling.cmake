add_test(NAME FoliaMermaidComparatorTests
    COMMAND "${Python3_EXECUTABLE}"
        "${FOLIA_SOURCE_ROOT}/tools/mermaid-cpp/regression/compare-svg.py" --self-test)
set_tests_properties(FoliaMermaidComparatorTests PROPERTIES LABELS "tooling;portable")
add_test(NAME FoliaMarkdownToolTests
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${FOLIA_SOURCE_ROOT}/tools/markdown-conformance" -p "test_*.py")
set_tests_properties(FoliaMarkdownToolTests PROPERTIES LABELS "tooling;portable")
