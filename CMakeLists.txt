# Folia (C++) — native self-rendered WYSIWYG Markdown editor.
#
# Build design:
#   - C++23 with C++ modules (CMake FILE_SET CXX_MODULES) + `import std;`.
#   - Core library `folia_core` is PURE C++: no Windows / DirectX / WinUI deps.
#   - Platform targets own reusable Windows adapters. Windows headers in
#     module units stay in the global module fragment.
#   - App `Folia` is a WinUI 3 shell hosting a self-drawn EditorView surface.
#
# `import std;` is experimental in CMake 4.3; the UUID below is the official
# activation token for the CxxImportStd experimental feature in CMake 4.3.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

cmake_minimum_required(VERSION 4.3)

set(_manifest "${CMAKE_CURRENT_SOURCE_DIR}/src/app-winui/Package.appxmanifest")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_manifest}")
file(STRINGS "${_manifest}" _identity_version REGEX "^[ \t]+Version=\"[0-9]+\\.[0-9]+\\.[0-9]+\\.0\"")
string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" _product_version "${_identity_version}")
project(folia
    VERSION "${_product_version}"
    LANGUAGES C CXX
    DESCRIPTION "Folia: native self-rendered WYSIWYG Markdown editor (C++)")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

# Require Ninja (needed for CMake's C++ module dependency scanning with MSVC).
if(NOT CMAKE_GENERATOR MATCHES "Ninja")
    message(FATAL_ERROR "Folia requires the Ninja generator for C++ module support. Reconfigure with -G Ninja.")
endif()

if(MSVC)
    # /utf-8: source and execution charset UTF-8 (we embed box-drawing chars).
    # /permissive-: strict standards conformance.
    add_compile_options(/utf-8 /permissive- /W3 /Zc:preprocessor /Zc:__cplusplus)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNDEFINED _UNICODE)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        add_compile_options(/O2)
    endif()
endif()


option(FOLIA_BUILD_WINUI "Build the WinUI 3 C++/WinRT application shell" OFF)
option(FOLIA_BUILD_MERMAID "Build the native Mermaid library and its dedicated tests" OFF)

if(FOLIA_BUILD_WINUI)
    message(FATAL_ERROR "The WinUI 3 shell is built with MSBuild at src/app-winui/Folia.vcxproj so XAML/IDL/C++WinRT generation runs through the Windows App SDK targets. Keep CMake for folia_core, folia_platform, and tests.")
endif()

# ---------------------------------------------------------------------------
# Core library — pure C++, portable. Acceptance gate: no Windows dependency.
# ---------------------------------------------------------------------------
add_subdirectory(src/markdown)
add_subdirectory(src/math/editor)
add_subdirectory(src/highlight)
add_subdirectory(src/completion)
add_subdirectory(src/ai)

set(FOLIA_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
include(cmake/FoliaCore.cmake)
include(cmake/FoliaMath.cmake)

# ---------------------------------------------------------------------------
# Platform targets — Windows adapters below the WinUI shell.
# ---------------------------------------------------------------------------
if(WIN32)
    # Locate the Windows 10/11 SDK so platform .ixx files can #include
    # <dwrite.h>/<d2d1.h>/... without relying on a globally-injected env.
    if(NOT FOLIA_WINSDK_INC)
        file(TO_CMAKE_PATH "$ENV{WindowsSdkDir}" _win_sdk_root)
        file(TO_CMAKE_PATH "$ENV{WindowsSDKVersion}" _win_sdk_version)
        if(NOT _win_sdk_root)
            cmake_host_system_information(RESULT _win_sdk_root QUERY WINDOWS_REGISTRY
                "HKLM/SOFTWARE/Microsoft/Windows Kits/Installed Roots" VALUE KitsRoot10 VIEW BOTH)
        endif()
        if(_win_sdk_root AND EXISTS "${_win_sdk_root}/Include/${_win_sdk_version}/um/windows.h")
            set(FOLIA_WINSDK_INC "${_win_sdk_root}/Include/${_win_sdk_version}" CACHE PATH "Windows SDK Include dir")
        elseif(_win_sdk_root)
            file(GLOB _win_sdk_inc_dirs LIST_DIRECTORIES true "${_win_sdk_root}/Include/10.0.*")
            list(SORT _win_sdk_inc_dirs COMPARE NATURAL ORDER DESCENDING)
            foreach(_include IN LISTS _win_sdk_inc_dirs)
                if(EXISTS "${_include}/um/windows.h")
                    set(FOLIA_WINSDK_INC "${_include}" CACHE PATH "Windows SDK Include dir")
                    break()
                endif()
            endforeach()
        endif()
    endif()

    file(GLOB_RECURSE FOLIA_PLATFORM_MODULE CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/*.ixx)
    list(FILTER FOLIA_PLATFORM_MODULE EXCLUDE REGEX "/src/platform/text/mermaid_[^/]+\\.ixx$")
    file(GLOB_RECURSE FOLIA_PLATFORM_SRC CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/*.cpp)
    list(FILTER FOLIA_PLATFORM_SRC EXCLUDE REGEX "/src/platform/windows/(media|completion|ai)/")

    if(FOLIA_PLATFORM_MODULE OR FOLIA_PLATFORM_SRC)
        add_library(folia_platform STATIC)
        target_compile_features(folia_platform PUBLIC cxx_std_23)
        set_target_properties(folia_platform PROPERTIES CXX_MODULE_STD ON)
        target_sources(folia_platform PUBLIC
            FILE_SET CXX_MODULES FILES ${FOLIA_PLATFORM_MODULE})
        target_sources(folia_platform PRIVATE ${FOLIA_PLATFORM_SRC})
        target_link_libraries(folia_platform PUBLIC folia_core folia_highlight)
        target_include_directories(folia_platform PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src/platform)
        if(FOLIA_WINSDK_INC)
            target_include_directories(folia_platform PUBLIC
                ${FOLIA_WINSDK_INC}/um
                ${FOLIA_WINSDK_INC}/shared
                ${FOLIA_WINSDK_INC}/ucrt
                ${FOLIA_WINSDK_INC}/cppwinrt)
        endif()
        target_link_libraries(folia_platform PUBLIC
            dwrite d2d1 d3d11 dxgi windowscodecs dwmapi uxtheme shlwapi
            ole32 oleaut32 uuid user32 gdi32 comctl32
            shell32 advapi32 imm32 propsys runtimeobject winspool)

        add_subdirectory(src/platform/windows/ai)
        add_subdirectory(src/platform/windows/completion)
        add_subdirectory(src/platform/windows/media)
    endif()
endif()

# ---------------------------------------------------------------------------
# Tests — pure core tests, no Windows dependency, portable.
#
# Boost.UT is consumed through its C++ module. The vendored fork fixes MSVC
# module linkage and command-line initialization; main passes argc/argv
# explicitly so no test translation unit depends on MSVC's private CRT globals.
# ---------------------------------------------------------------------------
enable_testing()
include(tests/cmake/Support.cmake)
set(FOLIA_CORE_TEST_TARGET FoliaTests)
include(tests/cmake/Core.cmake)
include(tests/cmake/Completion.cmake)
include(tests/cmake/MathEditor.cmake)
include(tests/cmake/Math.cmake)

if(TARGET folia_platform)
    add_executable(FoliaWindowsEditorTests)
    target_compile_features(FoliaWindowsEditorTests PRIVATE cxx_std_23)
    set_target_properties(FoliaWindowsEditorTests PROPERTIES
        CXX_MODULE_STD ON
        CXX_SCAN_FOR_MODULES ON)
    file(GLOB_RECURSE FOLIA_PLATFORM_TEST_SRC CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/platform/*.cpp)
    list(FILTER FOLIA_PLATFORM_TEST_SRC EXCLUDE REGEX "/tests/platform/mermaid_[^/]+\\.cpp$")
    target_sources(FoliaWindowsEditorTests PRIVATE
        ${FOLIA_PLATFORM_TEST_SRC})
    target_include_directories(FoliaWindowsEditorTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform)
    target_link_libraries(FoliaWindowsEditorTests PRIVATE
        folia_platform Boost::ut_module)
    add_test(NAME FoliaWindowsEditorTests COMMAND FoliaWindowsEditorTests)
endif()

if(WIN32)
    set(FOLIA_ICU_ROOT
        "${CMAKE_CURRENT_SOURCE_DIR}/build/dependencies/icu-78.3/root"
        CACHE PATH "ICU4C root used by the document encoding library")
    if(EXISTS "${FOLIA_ICU_ROOT}/include/unicode/ucsdet.h")
        add_subdirectory(src/encoding)
        add_executable(FoliaEncodingTests
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/encoding/main.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/encoding/test_document_encoding.cpp)
        target_compile_features(FoliaEncodingTests PRIVATE cxx_std_23)
        set_target_properties(FoliaEncodingTests PROPERTIES
            CXX_MODULE_STD ON
            CXX_SCAN_FOR_MODULES ON)
        target_include_directories(FoliaEncodingTests PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tests)
        target_sources(FoliaEncodingTests PRIVATE
            FILE_SET CXX_MODULES FILES
                ${CMAKE_CURRENT_SOURCE_DIR}/tests/encoding/document_encoding_test_support.ixx)
        target_link_libraries(FoliaEncodingTests PRIVATE
            folia::encoding Boost::ut_module)
        add_custom_command(TARGET FoliaEncodingTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${FOLIA_ICU_ROOT}/bin64/icuin78.dll
                ${FOLIA_ICU_ROOT}/bin64/icuuc78.dll
                ${FOLIA_ICU_ROOT}/bin64/icudt78.dll
                $<TARGET_FILE_DIR:FoliaEncodingTests>)
        add_test(NAME FoliaEncodingTests COMMAND FoliaEncodingTests)
    endif()
endif()

include(tests/cmake/Conformance.cmake)

# Keep CRT reports on stderr, including failures before main().
if(WIN32 AND MSVC)
    get_property(folia_console_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(folia_console_target IN LISTS folia_console_targets)
        get_target_property(folia_console_type ${folia_console_target} TYPE)
        if(folia_console_type STREQUAL "EXECUTABLE" AND folia_console_target MATCHES "^Folia")
            get_target_property(folia_console_links ${folia_console_target} LINK_LIBRARIES)
            if(NOT "folia_console_test_runtime" IN_LIST folia_console_links)
                target_link_libraries(${folia_console_target} PRIVATE folia_console_test_runtime)
            endif()
        endif()
    endforeach()
endif()

if(FOLIA_BUILD_MERMAID)
    add_subdirectory(tools/mermaid-cpp)
endif()
