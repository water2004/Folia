# Shared production core target used by the app and portable test projects.
include_guard(GLOBAL)

if(NOT TARGET folia::markdown)
    add_subdirectory("${FOLIA_SOURCE_ROOT}/src/markdown" "${CMAKE_CURRENT_BINARY_DIR}/markdown")
endif()

add_library(folia_core STATIC)
target_compile_features(folia_core PUBLIC cxx_std_23)
set_target_properties(folia_core PROPERTIES CXX_MODULE_STD ON)
target_include_directories(folia_core PRIVATE
    ${FOLIA_SOURCE_ROOT}/third_party/srell)
target_link_libraries(folia_core PUBLIC folia::markdown)
if(MSVC)
    target_compile_options(folia_core PRIVATE /bigobj)
endif()

file(GLOB_RECURSE FOLIA_CORE_MODULE CONFIGURE_DEPENDS
    ${FOLIA_SOURCE_ROOT}/src/core/*.ixx)
file(GLOB_RECURSE FOLIA_CORE_SOURCE CONFIGURE_DEPENDS
    ${FOLIA_SOURCE_ROOT}/src/core/*.cpp)
target_sources(folia_core PUBLIC
    FILE_SET CXX_MODULES BASE_DIRS "${FOLIA_SOURCE_ROOT}" FILES ${FOLIA_CORE_MODULE})
target_sources(folia_core PRIVATE ${FOLIA_CORE_SOURCE})

