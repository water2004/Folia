#pragma once

#include "providers/ModelTypes.h"
import std;

namespace folia::platform::models {
    enum class Route { Model, Catalog };
    std::string Endpoint(std::string const& address, ai::ApiProtocol, Route = Route::Model, bool fullUrl = false);
    void ValidateUrl(std::string const&);
    std::string ReadCatalogPage(ai::ApiProtocol, std::string const& endpoint, std::wstring key, std::stop_token);
    using TextDelta = std::function<void(std::string_view)>;
    using StreamObserver = std::function<void(ai::StreamEvent const&)>;
    ai::ModelResult Request(std::string const& baseUrl, ai::ApiProtocol, ai::Json const& prepared,
                            std::wstring key, std::stop_token, TextDelta const&, std::chrono::milliseconds inactivity, bool fullUrl = false, StreamObserver const& = {});
    ai::ModelResult RequestChat(std::string const& address, ai::ApiProtocol, ai::ModelRequest const&,
        std::wstring key, std::stop_token, bool fullUrl, ai::ProviderRequestOptions, StreamObserver const&, std::function<void(unsigned)> const& onRetry = {});
    std::wstring LoadKey(std::wstring const& target);
    void StoreKey(std::wstring const& target, std::wstring_view key);
}
