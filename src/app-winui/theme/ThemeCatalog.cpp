#include "pch.h"
#include "theme/ThemeCatalog.h"
#include "localization/Localization.h"
#include "storage/AssetPaths.h"

namespace
{
    std::filesystem::path PreviewPath(std::filesystem::path profile)
    {
        return profile.replace_extension(L".png");
    }

    bool IsPng(std::filesystem::path const& path)
    {
        winrt::com_ptr<IWICImagingFactory> factory;
        winrt::com_ptr<IWICBitmapDecoder> decoder;
        winrt::com_ptr<IWICBitmapFrameDecode> frame;
        GUID format{};
        UINT width = 0, height = 0;
        return SUCCEEDED(::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(factory.put())))
            && SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                WICDecodeMetadataCacheOnDemand, decoder.put()))
            && SUCCEEDED(decoder->GetContainerFormat(&format)) && format == GUID_ContainerFormatPng
            && SUCCEEDED(decoder->GetFrame(0, frame.put()))
            && SUCCEEDED(frame->GetSize(&width, &height)) && width && height;
    }

    winrt::hstring IncompatiblePackage(std::filesystem::path const& path)
    {
        return winrt::Folia::LocalizeFormat(L"IncompatibleThemePackage", {winrt::hstring(path.filename().c_str())});
    }

    std::filesystem::path TransferDirectory(std::filesystem::path const& parent)
    {
        GUID id{};
        winrt::check_hresult(::CoCreateGuid(&id));
        wchar_t name[39]{};
        ::StringFromGUID2(id, name, static_cast<int>(std::size(name)));
        auto directory = parent / L".staging" / name;
        std::filesystem::create_directories(directory);
        return directory;
    }

    void ScanThemeDirectory(
        std::filesystem::path const& directory,
        bool builtIn,
        std::unordered_set<std::string>& ids,
        std::vector<winrt::Folia::ThemeEntry>& entries,
        std::vector<winrt::hstring>& diagnostics)
    {
        std::error_code error;
        if (!std::filesystem::exists(directory, error)) return;
        for (auto const& item : std::filesystem::directory_iterator(directory, error))
        {
            if (error) break;
            auto path = item.path();
            if (!builtIn && item.is_directory() && path.filename() != L".staging") path /= L"theme.json";
            else if (!item.is_regular_file() || path.extension() != L".json") continue;
            auto preview = PreviewPath(path);
            if ((!builtIn && item.is_regular_file()) || !IsPng(preview)) {
                diagnostics.push_back(IncompatiblePackage(item.path()));
                continue;
            }
            auto loaded = winrt::Folia::LoadThemeFile(path);
            if (!loaded.profile)
            {
                diagnostics.push_back(std::move(loaded.diagnostic));
                continue;
            }
            if (!ids.insert(loaded.profile->id).second)
            {
                diagnostics.push_back(winrt::Folia::LocalizeFormat(
                    L"IgnoredDuplicateTheme", { winrt::to_hstring(loaded.profile->id) }));
                continue;
            }
            entries.push_back({ std::move(*loaded.profile), std::move(path), std::move(preview), builtIn });
        }
        if (error) diagnostics.push_back(winrt::Folia::LocalizeFormat(
            L"UnableEnumerateThemeDirectory", { winrt::hstring(directory.c_str()) }));
    }
}

namespace winrt::Folia
{
    std::filesystem::path ThemeCatalog::CustomThemeDirectory()
    {
        return AssetPath(std::filesystem::path(L"themes") / L"custom");
    }

    void ThemeCatalog::Refresh()
    {
        entries_.clear();
        diagnostics_.clear();
        std::unordered_set<std::string> ids;
        ScanThemeDirectory(BuiltinThemeDirectory(), true, ids, entries_, diagnostics_);
        ScanThemeDirectory(CustomThemeDirectory(), false, ids, entries_, diagnostics_);
        std::ranges::sort(entries_, [](ThemeEntry const& left, ThemeEntry const& right)
        {
            if (left.builtIn != right.builtIn) return left.builtIn > right.builtIn;
            return left.profile.name < right.profile.name;
        });
    }

    ThemeLoadResult ThemeCatalog::Resolve(std::string_view id, folia::Theme systemVariant) const
    {
        if (id.empty() || id == "system") return LoadThemeProfile(systemVariant);
        if (auto found = Find(id)) return { found->profile, true, {} };
        auto fallback = LoadThemeProfile(systemVariant);
        fallback.loadedFromFile = false;
        fallback.diagnostic = Localize(L"SelectedThemeUnavailable");
        return fallback;
    }

    ThemeEntry const* ThemeCatalog::Find(std::string_view id) const
    {
        auto found = std::ranges::find(entries_, id, [](ThemeEntry const& entry) { return std::string_view(entry.profile.id); });
        return found != entries_.end() ? &*found : nullptr;
    }

    std::filesystem::path ThemeCatalog::Preview(std::string_view id, folia::Theme systemVariant) const
    {
        if (id == "system") {
            auto profile = BuiltinThemeProfilePath(systemVariant);
            auto found = std::ranges::find(entries_, profile, &ThemeEntry::path);
            return found != entries_.end() ? found->preview : std::filesystem::path{};
        }
        auto found = Find(id);
        return found ? found->preview : std::filesystem::path{};
    }

    std::optional<winrt::hstring> ThemeCatalog::Import(std::filesystem::path const& source, std::string& importedId)
    {
        auto preview = PreviewPath(source);
        if (!IsPng(preview)) return IncompatiblePackage(source);
        auto loaded = LoadThemeFile(source);
        if (!loaded.profile) return loaded.diagnostic;
        const auto& id = loaded.profile->id;
        if (auto existing = Find(id); existing && existing->builtIn) return Localize(L"BuiltInThemeIdConflict");
        auto directory = CustomThemeDirectory();
        auto destination = directory / (L"theme-" + winrt::to_hstring(id) + L".package").c_str();
        std::filesystem::path staging;
        try
        {
            staging = TransferDirectory(directory);
            auto incoming = staging / L"incoming";
            std::filesystem::create_directory(incoming);
            std::filesystem::copy_file(source, incoming / L"theme.json");
            std::filesystem::copy_file(preview, incoming / L"theme.png");
            if (std::filesystem::exists(destination)) std::filesystem::rename(destination, staging / L"previous");
            std::filesystem::rename(incoming, destination);
        }
        catch (std::exception const& error)
        {
            if (!staging.empty()) {
                std::error_code ec;
                auto previous = staging / L"previous";
                if (std::filesystem::exists(previous, ec)) std::filesystem::rename(previous, destination, ec);
                if (!ec) std::filesystem::remove_all(staging, ec);
            }
            return LocalizeFormat(L"UnableImportTheme", { winrt::to_hstring(error.what()) });
        }
        std::error_code cleanup;
        std::filesystem::remove_all(staging, cleanup);
        importedId = id;
        Refresh();
        return std::nullopt;
    }

    std::optional<winrt::hstring> ThemeCatalog::Remove(std::string_view id)
    {
        auto found = Find(id);
        if (!found) return Localize(L"ThemeNotFound");
        if (found->builtIn) return Localize(L"BuiltInThemeCannotRemove");
        try
        {
            auto package = std::filesystem::weakly_canonical(found->path.parent_path());
            auto allowed = std::filesystem::weakly_canonical(CustomThemeDirectory());
            if (package.parent_path() != allowed) return Localize(L"ThemeOutsideManagedDirectory");
            auto staging = TransferDirectory(allowed);
            std::filesystem::rename(package, staging / L"removed");
            std::error_code cleanup;
            std::filesystem::remove_all(staging, cleanup);
            Refresh();
            return std::nullopt;
        }
        catch (std::exception const& error)
        {
            return LocalizeFormat(L"UnableRemoveTheme", { winrt::to_hstring(error.what()) });
        }
    }
}
