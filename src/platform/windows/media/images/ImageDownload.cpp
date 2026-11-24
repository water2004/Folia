#include "windows/media/images/ImageDownload.h"
#include "windows/media/images/ImageSource.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Web.Http.h>
#include <winrt/Windows.Web.Http.Headers.h>

import std;

namespace folia::platform
{
    static constexpr std::uint32_t MetadataBytes = 64u * 1024u;

    winrt::fire_and_forget DownloadImage(std::string source, ImageFetchKind kind,
                                       ImageDownloadCompletion complete)
    {
        std::vector<std::uint8_t> bytes;
        try
        {
            co_await winrt::resume_background();
            auto client = winrt::Windows::Web::Http::HttpClient{};
            auto uri = winrt::Windows::Foundation::Uri(winrt::to_hstring(source));
            if (kind == ImageFetchKind::Content)
            {
                auto buffer = co_await client.GetBufferAsync(uri);
                if (buffer.Length() <= image_source::MaximumEncodedImageBytes)
                {
                    bytes.resize(buffer.Length());
                    auto reader = winrt::Windows::Storage::Streams::DataReader::FromBuffer(buffer);
                    reader.ReadBytes(winrt::array_view<std::uint8_t>(bytes));
                }
            }
            else
            {
                auto request = winrt::Windows::Web::Http::HttpRequestMessage{
                    winrt::Windows::Web::Http::HttpMethod::Get(), uri};
                request.Headers().TryAppendWithoutValidation(L"Range", L"bytes=0-65535");
                auto response = co_await client.SendRequestAsync(request,
                    winrt::Windows::Web::Http::HttpCompletionOption::ResponseHeadersRead);
                if (response.IsSuccessStatusCode())
                {
                    auto reader = winrt::Windows::Storage::Streams::DataReader{
                        co_await response.Content().ReadAsInputStreamAsync()};
                    reader.InputStreamOptions(winrt::Windows::Storage::Streams::InputStreamOptions::Partial);
                    auto count = co_await reader.LoadAsync(MetadataBytes);
                    bytes.resize((std::min)(count, MetadataBytes));
                    reader.ReadBytes(winrt::array_view<std::uint8_t>(bytes));
                }
            }
        }
        catch (...) { bytes.clear(); }
        complete(std::move(bytes));
    }
}
