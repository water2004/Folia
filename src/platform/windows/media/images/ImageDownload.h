#pragma once

#include <winrt/base.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace folia::platform
{
    enum class ImageFetchKind { Content, Dimensions };
    using ImageDownloadCompletion = std::function<void(std::vector<std::uint8_t>)>;

    // Calls completion on the worker with an empty body on download failure.
    winrt::fire_and_forget DownloadImage(std::string source, ImageFetchKind kind, ImageDownloadCompletion complete);
}
