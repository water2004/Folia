#include "pch.h"
#include "support/folia_test.hpp"
#include "theme/ThemeConfig.h"
#include "editor/rendering/EditorStyleSheet.h"

using namespace boost::ut;
using namespace winrt::Folia;

namespace
{
    using winrt::Windows::Data::Json::JsonObject;
    using winrt::Windows::Data::Json::JsonValue;

    JsonObject ReadTheme(std::filesystem::path const& path)
    {
        std::ifstream file(path, std::ios::binary);
        return JsonObject::Parse(winrt::to_hstring(std::string{
            std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}}));
    }

    struct ThemeFiles
    {
        std::filesystem::path source, imported;
        ThemeFiles()
        {
            GUID id{};
            winrt::check_hresult(CoCreateGuid(&id));
            auto directory = std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path() / "build/tests";
            std::filesystem::create_directories(directory);
            source = directory / (L"marker-theme-" + winrt::to_hstring(id) + L".json").c_str();
            imported = source;
            imported += L".imported.json";
        }
        void Write(JsonObject const& value) const
        {
            std::ofstream file(source, std::ios::binary);
            file << winrt::to_string(value.Stringify());
            if (!file) throw std::runtime_error("cannot write theme test fixture");
        }
        ~ThemeFiles()
        {
            std::error_code ignored;
            std::filesystem::remove(source, ignored);
            std::filesystem::remove(imported, ignored);
        }
    };

    suite syntax_marker_theme_tests = [] {
        "theme marker assets travel in the single imported file and determine geometry"_test = [] {
            for (auto name : {L"light", L"dark", L"high-contrast", L"inkstone-paper", L"moss-linen", L"arctic-night", L"ember-night"}) {
                auto loaded = LoadThemeFile(BuiltinThemeDirectory() / (std::wstring{name} + L".json"));
                expect(loaded.profile.has_value()) << winrt::to_string(loaded.diagnostic);
                if (!loaded.profile) continue;
                expect(loaded.profile->schema_version == 2_u);
                expect(loaded.profile->editor.bracket_border_width == 1.f);
                expect(loaded.profile->editor.bracket_match_background.a <= 26);
                expect(loaded.profile->editor.bracket_match_border != loaded.profile->colors.accent_fg);
                expect(loaded.profile->syntax_markers != nullptr);
                if (!loaded.profile->syntax_markers) continue;
                auto const& assets = *loaded.profile->syntax_markers;
                for (std::size_t i = 1; i < folia::syntax_marker_names.size(); ++i)
                    expect(!assets.svg[i].empty());
            }

            ThemeFiles files;
            auto root = ReadTheme(BuiltinThemeDirectory() / L"light.json");
            auto markers = root.GetNamedObject(L"syntaxMarkers");
            markers.SetNamedValue(L"sizeEm", JsonValue::CreateNumberValue(1.2));
            markers.SetNamedValue(L"gapEm", JsonValue::CreateNumberValue(.4));
            auto replacement = L"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\"><circle cx=\"12\" cy=\"12\" r=\"5\" fill=\"currentColor\"/></svg>";
            markers.GetNamedObject(L"icons").SetNamedValue(L"star", JsonValue::CreateStringValue(replacement));
            files.Write(root);
            // ThemeCatalog imports the complete JSON with copy_file. No adjacent
            // SVG files or access to the original directory may be necessary.
            std::filesystem::copy_file(files.source, files.imported);
            std::filesystem::remove(files.source);
            auto loaded = LoadThemeFile(files.imported);
            expect(loaded.profile.has_value()) << winrt::to_string(loaded.diagnostic);
            if (!loaded.profile) return;
            auto sheet = CreateEditorStyleSheet(*loaded.profile);
            expect(sheet.markerAssets == loaded.profile->syntax_markers);
            expect(std::abs(sheet.marker.sizeEm - 1.2f) < .001f);
            expect(std::abs(sheet.marker.gapEm - .4f) < .001f);
            expect(static_cast<bool>(sheet.markerAssets->svg[static_cast<std::size_t>(folia::SyntaxMarker::Star)] == winrt::to_string(replacement)));
        };

        "invalid marker themes are rejected as complete profiles"_test = [] {
            ThemeFiles files;
            for (auto invalid : {L"", L"<svg", L"<svg viewBox=\"0 0 32 32\"/>",
                L"<svg viewBox=\"0 0 24 24\"><text>*</text></svg>"}) {
                auto root = ReadTheme(BuiltinThemeDirectory() / L"light.json");
                root.GetNamedObject(L"syntaxMarkers").GetNamedObject(L"icons")
                    .SetNamedValue(L"star", JsonValue::CreateStringValue(invalid));
                files.Write(root);
                auto loaded = LoadThemeFile(files.source);
                expect(!loaded.profile.has_value());
                expect(!loaded.diagnostic.empty());
            }
            auto root = ReadTheme(BuiltinThemeDirectory() / L"light.json");
            root.GetNamedObject(L"syntaxMarkers").GetNamedObject(L"icons").Remove(L"doubleDollar");
            files.Write(root);
            expect(!LoadThemeFile(files.source).profile.has_value());
            auto fallback = CreateEditorStyleSheet(folia::default_theme_profile(folia::Theme::Light));
            expect(fallback.markerAssets == DefaultSyntaxMarkerTheme());
        };
    };
}
