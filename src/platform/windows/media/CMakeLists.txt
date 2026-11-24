add_library(folia_windows_media STATIC
    images/EditorGifDecoder.cpp
    images/ImageDownload.cpp
    images/ImageSource.cpp
    math/MathJaxRenderer.cpp
    math/MathJaxRenderer.Batch.cpp
    math/MathJaxRenderer.Cache.cpp
    math/MathJaxRenderer.Worker.cpp
    math/MathJaxSvgParser.cpp
    math/MathSvgStore.cpp
    math/NativeMathSession.cpp
    mermaid/MermaidRenderer.cpp
    svg/SvgNormalizer.cpp)
add_library(folia::windows_media ALIAS folia_windows_media)
target_compile_features(folia_windows_media PUBLIC cxx_std_23)
set_target_properties(folia_windows_media PROPERTIES CXX_MODULE_STD ON)
target_link_libraries(folia_windows_media PUBLIC folia_platform
    PRIVATE FoliaNativeMath runtimeobject)
