# MIT protocol codecs vendored from the revision in source-lock.json.
add_library(folia_model_providers STATIC)
target_compile_features(folia_model_providers PUBLIC cxx_std_23)
set_target_properties(folia_model_providers PROPERTIES CXX_SCAN_FOR_MODULES OFF)
file(GLOB_RECURSE provider_sources CONFIGURE_DEPENDS providers/*.cpp)
target_sources(folia_model_providers PRIVATE ${provider_sources})
target_include_directories(folia_model_providers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../nlohmann-json/include)
