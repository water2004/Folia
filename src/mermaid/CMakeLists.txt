file(GLOB_RECURSE FOLIA_MERMAID_MODULES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.ixx")
file(GLOB_RECURSE FOLIA_MERMAID_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
if(NOT FOLIA_MERMAID_MODULES)
    message(FATAL_ERROR "No Mermaid C++ module interfaces were found in ${CMAKE_CURRENT_SOURCE_DIR}.")
endif()
add_library(folia_mermaid STATIC)
add_library(folia::mermaid ALIAS folia_mermaid)
target_compile_features(folia_mermaid PUBLIC cxx_std_23)
set_target_properties(folia_mermaid PROPERTIES
    CXX_MODULE_STD ON
    CXX_SCAN_FOR_MODULES ON)
target_sources(folia_mermaid PUBLIC
    FILE_SET CXX_MODULES
    BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES ${FOLIA_MERMAID_MODULES})
target_sources(folia_mermaid PRIVATE ${FOLIA_MERMAID_SOURCES})

# Grammar only: schema resolution is owned by folia.mermaid.frontmatter.
# Keep this native dependency pinned independently of the JS test oracle.
include(FetchContent)
find_package(Git REQUIRED)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
set(YAML_CPP_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(folia_yaml_cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 56e3bb550c91fd7005566f19c079cb7a503223cf
    PATCH_COMMAND "${GIT_EXECUTABLE}" apply --ignore-space-change
        "${CMAKE_CURRENT_LIST_DIR}/yaml-cpp-json-escapes.patch")
FetchContent_MakeAvailable(folia_yaml_cpp)
set_target_properties(yaml-cpp PROPERTIES CXX_MODULE_STD OFF CXX_SCAN_FOR_MODULES OFF)
target_link_libraries(folia_mermaid PRIVATE yaml-cpp::yaml-cpp)
