add_library(folia_encoding STATIC)
add_library(folia::encoding ALIAS folia_encoding)
target_compile_features(folia_encoding PUBLIC cxx_std_23)
set_target_properties(folia_encoding PROPERTIES CXX_MODULE_STD ON)
target_sources(folia_encoding
    PUBLIC FILE_SET CXX_MODULES FILES document_encoding.ixx
    PRIVATE document_encoding.cpp)

if(WIN32 AND FOLIA_ICU_ROOT)
    target_include_directories(folia_encoding PRIVATE "${FOLIA_ICU_ROOT}/include")
    target_link_libraries(folia_encoding PRIVATE
        "${FOLIA_ICU_ROOT}/lib64/icuin.lib"
        "${FOLIA_ICU_ROOT}/lib64/icuuc.lib"
        "${FOLIA_ICU_ROOT}/lib64/icudt.lib")
else()
    find_package(ICU REQUIRED COMPONENTS i18n uc data)
    target_link_libraries(folia_encoding PRIVATE ICU::i18n ICU::uc ICU::data)
endif()
