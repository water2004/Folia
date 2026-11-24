add_test(NAME FoliaMermaidComparatorTests
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${FOLIA_SOURCE_ROOT}/tools/mermaid-cpp/regression/tests" -p "test_*.py")
set_tests_properties(FoliaMermaidComparatorTests PROPERTIES LABELS "tooling;portable")
add_test(NAME FoliaMarkdownToolTests
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${FOLIA_SOURCE_ROOT}/tools/markdown-conformance" -p "test_*.py")
set_tests_properties(FoliaMarkdownToolTests PROPERTIES LABELS "tooling;portable")
add_test(NAME FoliaMathToolTests
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${FOLIA_SOURCE_ROOT}/tools/math-port/tests" -p "test_*.py")
set_tests_properties(FoliaMathToolTests PROPERTIES LABELS "tooling;portable")
add_test(NAME FoliaTestRunnerTests
    COMMAND "${Python3_EXECUTABLE}" -m unittest discover
        -s "${FOLIA_SOURCE_ROOT}/tests/tools" -p "test_*.py")
set_tests_properties(FoliaTestRunnerTests PROPERTIES LABELS "tooling;portable")
