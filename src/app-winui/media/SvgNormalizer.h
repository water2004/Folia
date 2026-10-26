#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <stop_token>
#include <vector>


namespace winrt::Folia
{
    struct SvgNormalizationInput
    {
        std::string_view source;
        float fontSize;
        std::stop_token stop;
    };
    struct NormalizedSvg
    {
        std::uint64_t id = 0;
        std::shared_ptr<std::string const> svg;
        float width = 0.0f;
        float height = 0.0f;
        std::string error;

        explicit operator bool() const
        {
            return id != 0 && error.empty() && svg && !svg->empty()
                && width > 0.0f && height > 0.0f;
        }
    };

    class SvgNormalizer
    {
    public:
        SvgNormalizer();
        ~SvgNormalizer();
        SvgNormalizer(SvgNormalizer const&) = delete;
        SvgNormalizer& operator=(SvgNormalizer const&) = delete;

        // Complete normalization on the caller's preparation thread. Math
        // layout never waits for an asynchronous SVG completion notification.
        NormalizedSvg NormalizeNow(std::string_view source, float fontSize);
        std::vector<NormalizedSvg> NormalizeBatchNow(std::span<SvgNormalizationInput const> inputs);

        std::optional<NormalizedSvg> GetOrQueue(
            std::string_view source,
            float fontSize,
            bool allowQueue = true,
            bool highPriority = false);
        void SetCompletionCallback(std::function<void()> callback);

    private:
        struct State;
        std::unique_ptr<State> state;
    };
}
