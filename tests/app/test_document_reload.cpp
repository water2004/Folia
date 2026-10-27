#include "pch.h"
#include "support/folia_test.hpp"
#include "support/DocumentControllerFixture.h"

using namespace boost::ut;
using namespace document_tests;

namespace {
    void CheckFinished(Fixture const& f) {
        expect(!f.controller.IsLoading());
        expect(!f.loading);
        expect(!f.loadingOverlay);
        expect(!f.loadingAtCompletion.empty());
        expect(std::ranges::none_of(f.loadingAtCompletion, [](bool loading) { return loading; }));
        expect(std::ranges::none_of(f.loadingAtDocumentChange, [](bool loading) { return loading; }));
        expect(!f.wrongThread);
    }

    suite document_reload_tests = [] {
        "document reload automatic file changes finish presentation before notifying UI"_test = [] {
            Fixture f;
            auto file = f.File(L"watched.md", "before");
            expect(Await(f.controller.OpenFileAsync(file)));
            for (std::string_view source : {"plain replacement", "inline $x+1$ replacement", "$$\nx^2+1\n$$\n"}) {
                std::ofstream(std::filesystem::path(file.Path().c_str()), std::ios::binary | std::ios::trunc) << source;
                auto expected = LocalizeFormat(L"StatusReloadedFile", {file.Name()});
                Pump([&] { return f.session.TextUtf8() == source && f.status == expected; });
                CheckFinished(f);
                expect(!f.controller.IsModified());
            }
        };

        "document reload manual refresh clears loading without an outer open callback"_test = [] {
            Fixture f;
            auto file = f.File(L"manual.md", "before");
            expect(Await(f.controller.OpenFileAsync(file)));
            std::ofstream(std::filesystem::path(file.Path().c_str()), std::ios::binary | std::ios::trunc) << "manual replacement";
            f.controller.ReloadDocument();
            Pump([&] { return f.status == LocalizeFormat(L"StatusReloadedFile", {file.Name()}); });
            expect(f.session.TextUtf8() == "manual replacement");
            CheckFinished(f);
        };

        "document reload failure clears progress and retains the old document"_test = [] {
            Fixture f;
            auto file = f.File(L"removed-during-read.md", "before");
            expect(Await(f.controller.OpenFileAsync(file)));
            f.progress = [&](bool active) {
                if (active && std::filesystem::exists(std::filesystem::path(file.Path().c_str())))
                    std::filesystem::remove(std::filesystem::path(file.Path().c_str()));
            };
            f.controller.ReloadDocument();
            Pump([&] { return f.status == LocalizeFormat(L"StatusReloadFailed", {L"document no longer exists"}); });
            expect(f.session.TextUtf8() == "before");
            CheckFinished(f);
        };

        "document load cancellation publishes its final loading state"_test = [] {
            Fixture f(true, L"original", false);
            auto file = f.File(L"cancel.md", "replacement $x+1$");
            auto operation = f.controller.OpenFileAsync(file);
            Pump([&] { return f.controller.IsLoading(); });
            f.controller.CancelOperation();
            expect(!Await(operation));
            expect(f.session.TextUtf8() == "original");
            CheckFinished(f);
        };

        "document load async open failure publishes its final loading state"_test = [] {
            Fixture f;
            auto file = f.File(L"failed-open.md", "replacement");
            f.progress = [&](bool active) {
                if (active && std::filesystem::exists(std::filesystem::path(file.Path().c_str())))
                    std::filesystem::remove(std::filesystem::path(file.Path().c_str()));
            };
            expect(!Await(f.controller.OpenFileAsync(file)));
            expect(f.session.TextUtf8() == "initial\n");
            CheckFinished(f);
        };
    };
}
