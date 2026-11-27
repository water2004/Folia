# Shared model protocols. Initial MIT source recorded in source-lock.json.
add_library(folia_model_providers STATIC)
target_compile_features(folia_model_providers PUBLIC cxx_std_23)
set_target_properties(folia_model_providers PROPERTIES CXX_SCAN_FOR_MODULES OFF)
file(GLOB_RECURSE provider_sources CONFIGURE_DEPENDS providers/*.cpp)
target_sources(folia_model_providers PRIVATE ${provider_sources})
target_include_directories(folia_model_providers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/nlohmann-json/include)

add_library(folia_agent STATIC agent/Agent.cpp)
add_library(folia::agent ALIAS folia_agent)
target_compile_features(folia_agent PUBLIC cxx_std_23)
set_target_properties(folia_agent PROPERTIES CXX_MODULE_STD ON)
target_link_libraries(folia_agent PUBLIC folia_model_providers)
