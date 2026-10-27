#include "pch.h"
#include "support/folia_test.hpp"
#include "media/ImageResourceCache.h"
#include "editor/rendering/EditorImageCache.h"

using namespace boost::ut;
using namespace winrt::Folia;

namespace
{
    struct PendingFetch
    {
        std::string source;
        ImageFetchKind kind;
        ImageResourceCache::Completion complete;
    };
    std::vector<std::uint8_t> PngHeader()
    {
        return {137,80,78,71,13,10,26,10, 0,0,0,13, 73,72,68,82,
                0,0,0,3, 0,0,0,2, 8,6,0,0,0};
    }

    suite image_resource_cache_tests = []
    {
        "image resource detach isolates late content metadata and decoder completions"_test = []
        {
            std::vector<PendingFetch> pending;
            ImageResourceCache cache([&](auto source, auto kind, auto complete) {
                pending.push_back({std::move(source), kind, std::move(complete)});
            });
            cache.Attach(nullptr, {});
            cache.Request("image", ImageFetchKind::Content);
            cache.Request("image", ImageFetchKind::Dimensions);
            cache.Request("image", ImageFetchKind::Content);
            expect(pending.size() == 2_u);
            auto oldDecoder = cache.CompletionCallback();
            cache.Detach();
            cache.Attach(nullptr, {});
            cache.Request("image", ImageFetchKind::Content);
            auto generation = cache.Generation();
            pending[0].complete({1,2,3});
            pending[1].complete(PngHeader());
            oldDecoder();
            expect(cache.Generation() == generation);
            expect(cache.HasPendingContent());
            expect(cache.Content("image").status == ImageResourceStatus::Pending);
            expect(!cache.Dimensions("image").has_value());
            pending[2].complete({4,5});
            expect(!cache.HasPendingContent());
            auto content = cache.Content("image");
            expect(content.status == ImageResourceStatus::Ready);
            expect(content.bytes && *content.bytes == std::vector<std::uint8_t>{4,5});
            generation = cache.Generation();
            pending[2].complete({7});
            expect(cache.Generation() == generation) << "duplicate completion is not published";
        };

        "image resource metadata and content have independent completion and retry state"_test = []
        {
            std::vector<PendingFetch> pending;
            ImageResourceCache cache([&](auto source, auto kind, auto complete) {
                pending.push_back({std::move(source), kind, std::move(complete)});
            });
            cache.Attach(nullptr, {});
            cache.Request("image", ImageFetchKind::Dimensions);
            expect(!cache.HasPendingContent()) << "metadata alone must not delay PDF export";
            pending[0].complete(PngHeader());
            auto dimensions = cache.Dimensions("image");
            expect(dimensions.has_value());
            if (dimensions) expect(dimensions->width == 3.0f && dimensions->height == 2.0f);
            cache.Request("image", ImageFetchKind::Content);
            expect(cache.HasPendingContent());
            pending[1].complete({});
            expect(cache.Content("image").status == ImageResourceStatus::Failed);
            cache.Request("image", ImageFetchKind::Content);
            expect(pending.size() == 2_u);
            cache.Attach(nullptr, {});
            cache.Request("image", ImageFetchKind::Content);
            expect(pending.size() == 3_u);
            pending[2].complete({8});
            expect(cache.Content("image").status == ImageResourceStatus::Ready);
        };

        "image resource snapshots outlive bounded cache eviction and destruction"_test = []
        {
            EncodedImage snapshot;
            {
                ImageResourceCache cache([](auto, auto, auto complete) { complete({42}); });
                cache.Attach(nullptr, {});
                cache.Request("first", ImageFetchKind::Content);
                snapshot = cache.Content("first").bytes;
                for (int i = 0; i < 16; ++i) cache.Request(std::to_string(i), ImageFetchKind::Content);
                expect(cache.Content("first").status == ImageResourceStatus::Missing);
                expect(snapshot && snapshot->front() == 42);
                expect(cache.Content("15").status == ImageResourceStatus::Ready);
            }
            expect(snapshot && snapshot->front() == 42);
            ImageResourceCache budget([](auto, auto, auto complete) {
                complete(std::vector<std::uint8_t>(12u * 1024u * 1024u, 1));
            });
            budget.Attach(nullptr, {});
            for (auto name : {"a", "b", "c"}) budget.Request(name, ImageFetchKind::Content);
            expect(budget.Content("a").status == ImageResourceStatus::Missing);
            expect(budget.Content("b").status == ImageResourceStatus::Ready);
            expect(budget.Content("c").status == ImageResourceStatus::Ready);
        };

        "image source decoding and device cache share dimensions and source identity"_test = []
        {
            std::vector<std::uint8_t> bmp(58, 0);
            bmp[0]='B'; bmp[1]='M'; bmp[2]=58; bmp[10]=54; bmp[14]=40;
            bmp[18]=1; bmp[22]=1; bmp[26]=1; bmp[28]=24; bmp[54]=255;
            std::string uri="data:image/bmp,";
            constexpr char hex[]="0123456789ABCDEF";
            for (auto byte : bmp) { uri += '%'; uri += hex[byte >> 4]; uri += hex[byte & 15]; }
            expect(image_source::DecodeImageDataUri(uri) == std::optional(bmp));
            expect(!image_source::DecodeImageDataUri("data:image/png,%ZZ"));
            expect(image_source::DecodeImageDataUri("data:image/png;base64,QUJD") ==
                   std::optional(std::vector<std::uint8_t>{'A','B','C'}));
            auto root = std::filesystem::current_path().wstring();
            expect(image_source::RasterImageKey(root, "a%20b.png?x#y") ==
                   image_source::RasterImageKey(root, "a b.png"));
            auto sheet = CreateEditorStyleSheet(folia::default_theme_profile(folia::Theme::Light));
            EditorRenderResources resources;
            resources.InitializeOffscreen(sheet, 64, 64);
            EditorImageCache cache;
            cache.Attach(nullptr, {});
            auto size = cache.ProbeImageDimensions(resources, {}, uri);
            expect(size && size->width == 1 && size->height == 1);
            auto first = cache.LoadRasterImage(resources, {}, uri);
            auto second = cache.LoadRasterImage(resources, {}, uri);
            expect(first && second);
            if (first && second) expect(first->bitmap.Get() == second->bitmap.Get());
            cache.ClearDeviceResources();
            auto recreated = cache.LoadRasterImage(resources, {}, uri);
            expect(recreated && recreated->width == 1 && recreated->height == 1);
            if (first && recreated) expect(first->bitmap.Get() != recreated->bitmap.Get());
            auto svg = cache.LoadSvgSource({}, "data:image/svg+xml,%3Csvg%20xmlns='http://www.w3.org/2000/svg'/%3E");
            expect(svg.candidate && svg.source && svg.source->starts_with("<svg "));
        };
    };
}
