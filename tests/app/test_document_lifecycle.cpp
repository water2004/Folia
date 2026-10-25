#include "pch.h"
#include "support/folia_test.hpp"
#include "localization/Localization.h"
#include "support/DocumentControllerFixture.h"

using namespace boost::ut;
using namespace document_tests;

namespace {
    suite document_lifecycle_tests = [] {
        "document lifecycle closes clean documents without prompting"_test = [] {
            Fixture f;
            expect(!f.controller.HasUnsavedChanges());
            expect(Await(f.controller.ConfirmCloseAsync()));
            expect(f.prompts == 0);
            Fixture empty(false, L"");
            expect(!empty.controller.HasUnsavedChanges());
            expect(Await(empty.controller.ConfirmCloseAsync()));
            expect(empty.prompts == 0);
        };
        "document lifecycle cancel and discard preserve local changes until close"_test = [] {
            Fixture f;
            auto disk = f.DiskText();
            f.Type(U"edit");
            auto local = f.session.TextUtf8();
            expect(f.controller.HasUnsavedChanges());
            expect(!Await(f.controller.ConfirmCloseAsync()));
            expect(f.session.TextUtf8() == local);
            expect(f.DiskText() == disk);
            f.choice = ContentDialogResult::Secondary;
            expect(Await(f.controller.ConfirmCloseAsync()));
            expect(f.session.TextUtf8() == local);
            expect(f.DiskText() == disk);
            expect(f.controller.HasUnsavedChanges());
        };
        "document lifecycle awaits save and tracks exact undo redo content"_test = [] {
            Fixture f;
            f.Type(U"changed");
            auto saved = f.session.TextUtf8();
            f.choice = ContentDialogResult::Primary;
            expect(Await(f.controller.ConfirmCloseAsync()));
            expect(f.DiskText() == saved);
            expect(!f.controller.HasUnsavedChanges());
            expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(f.controller.HasUnsavedChanges());
            expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Redo}));
            expect(!f.controller.HasUnsavedChanges());
            expect(!f.wrongThread);
        };
        "document lifecycle cancelled first save and failed writes keep the document open"_test = [] {
            Fixture f(false);
            f.choice = ContentDialogResult::Primary;
            expect(!Await(f.controller.ConfirmCloseAsync()));
            expect(!f.session.HasFile());
            expect(f.controller.HasUnsavedChanges());
            f.saveTarget = f.File(L"target.md");
            Await(f.saveTarget.DeleteAsync(winrt::Windows::Storage::StorageDeleteOption::PermanentDelete));
            expect(!Await(f.controller.ConfirmCloseAsync()));
            expect(!f.session.HasFile());
            expect(f.controller.HasUnsavedChanges());
        };
        "document lifecycle rechecks changes made as a save completes"_test = [] {
            Fixture f;
            f.Type(U"first");
            auto snapshot = f.session.TextUtf8();
            f.choice = ContentDialogResult::Primary;
            f.saved = [&] { f.saved = {}; f.Type(U"later"); f.choice = ContentDialogResult::None; };
            expect(!Await(f.controller.ConfirmCloseAsync()));
            expect(f.DiskText() == snapshot);
            expect(f.controller.HasUnsavedChanges());
            expect(f.prompts == 2);
        };
        "document lifecycle repeated close requests do not overlap prompts"_test = [] {
            Fixture f;
            f.Type(U"changed");
            int prompts = 0;
            f.controller.SetSaveDialogs([&]() -> IAsyncOperation<ContentDialogResult> {
                ++prompts;
                co_await winrt::resume_after(std::chrono::milliseconds(30));
                co_return ContentDialogResult::None;
            });
            auto first = f.controller.ConfirmCloseAsync();
            auto repeated = f.controller.ConfirmCloseAsync();
            expect(!Await(repeated));
            expect(!Await(first));
            expect(prompts == 1);
            auto detached = f.controller.ConfirmCloseAsync();
            f.controller.Detach();
            expect(!Await(detached));
        };
        "document lifecycle protects opening another document from discarding changes"_test = [] {
            Fixture f;
            auto other = f.File(L"other.md", "other\n");
            f.Type(U"changed");
            auto before = f.session.TextUtf8();
            f.controller.OpenDocumentPath(other.Path());
            Pump([&] { return f.prompts > 0; });
            expect(f.session.TextUtf8() == before);
            f.choice = ContentDialogResult::Secondary;
            f.controller.OpenDocumentPath(other.Path());
            Pump([&] { return f.session.Path() == other.Path(); });
            expect(f.session.TextUtf8() == "other\n");
            expect(!f.controller.HasUnsavedChanges());
        };
        "document lifecycle does not treat loaded CRLF spacing or view switches as edits"_test = [] {
            Fixture f;
            std::string bytes = "# Header\r\n\r\n\r\nText with two spaces.  \r\n\r\n";
            auto other = f.File(L"spacing.md", bytes);
            f.controller.OpenDocumentPath(other.Path());
            Pump([&] { return f.session.Path() == other.Path(); });
            expect(!f.controller.HasUnsavedChanges());
            expect(Await(f.controller.ConfirmCloseAsync()));
            expect(f.prompts == 0);
            expect(f.session.EnterSourceMode());
            expect(!f.controller.HasUnsavedChanges());
            expect(f.session.ExitSourceMode());
            expect(!f.controller.HasUnsavedChanges());
            f.Type(U"edit");
            expect(f.controller.HasUnsavedChanges());
            expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(!f.controller.HasUnsavedChanges());
            expect(f.DiskText() == bytes);
        };
        "document lifecycle retains a pending dialog callback until detach cancellation completes"_test = [] {
            Fixture f;
            f.Type(U"edit");
            auto context = std::make_shared<int>(0);
            std::weak_ptr<int> lifetime = context;
            f.controller.SetSaveDialogs([context]() -> IAsyncOperation<ContentDialogResult> {
                co_await winrt::resume_after(std::chrono::milliseconds(30));
                ++*context;
                co_return ContentDialogResult::None;
            });
            context.reset();
            auto pending = f.controller.ConfirmCloseAsync();
            f.controller.Detach();
            expect(!lifetime.expired());
            expect(!Await(pending));
            expect(lifetime.expired());
        };
        "document lifecycle cannot save or paste into a document being replaced"_test = [] {
            Fixture f(true, L"initial\n", false);
            auto next = f.File(L"next.md", "next\n");
            f.controller.OpenDocumentPath(next.Path());
            Pump([&] { return f.loading; });
            auto before = f.session.TextUtf8();
            f.Type(U"edit while loading");
            expect(!Await(f.controller.SaveAsync()));
            auto package = Package(Await(Image()));
            expect(!Await(f.controller.PasteAsync(package.GetView())));
            expect(f.Images().empty());
            expect(f.DiskText() == before);
            f.controller.CancelOperation();
        };
    };
}
