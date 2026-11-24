include_guard(GLOBAL)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(NOT TARGET Boost::ut_module)
    set(BOOST_UT_DISABLE_MODULE OFF CACHE BOOL "Use the forked Boost.UT module" FORCE)
    set(BOOST_UT_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BOOST_UT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(BOOST_UT_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(BOOST_UT_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory("${FOLIA_SOURCE_ROOT}/third_party/ut"
        "${CMAKE_CURRENT_BINARY_DIR}/boost-ut" EXCLUDE_FROM_ALL)
endif()

add_library(folia_test_support STATIC)
target_sources(folia_test_support PUBLIC FILE_SET CXX_MODULES
    BASE_DIRS "${FOLIA_SOURCE_ROOT}/tests/support"
    FILES "${FOLIA_SOURCE_ROOT}/tests/support/fixture_data.ixx")
target_compile_features(folia_test_support PUBLIC cxx_std_23)
set_target_properties(folia_test_support PROPERTIES CXX_MODULE_STD ON)
target_compile_definitions(folia_test_support PRIVATE
    FOLIA_TEST_DATA_ROOT="${FOLIA_SOURCE_ROOT}/tests")
if(MSVC)
    target_compile_options(folia_test_support PRIVATE /utf-8)
endif()
if(WIN32 AND MSVC AND NOT TARGET folia_console_test_runtime)
    add_library(folia_console_test_runtime OBJECT
        "${FOLIA_SOURCE_ROOT}/tests/support/windows_console_runtime.cpp")
    target_compile_features(folia_console_test_runtime PRIVATE cxx_std_23)
    set_target_properties(folia_console_test_runtime PROPERTIES
        CXX_MODULE_STD OFF CXX_SCAN_FOR_MODULES OFF)
endif()

function(folia_test_defaults target)
    target_compile_features(${target} PRIVATE cxx_std_23)
    set_target_properties(${target} PROPERTIES
        CXX_MODULE_STD ON CXX_SCAN_FOR_MODULES ON)
    target_include_directories(${target} PRIVATE "${FOLIA_SOURCE_ROOT}/tests")
    target_compile_definitions(${target} PRIVATE
        FOLIA_TEST_DATA_ROOT="${FOLIA_SOURCE_ROOT}/tests")
    target_link_libraries(${target} PRIVATE folia_test_support)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8 /permissive- /Zc:preprocessor)
        target_link_libraries(${target} PRIVATE folia_console_test_runtime)
    endif()
endfunction()
