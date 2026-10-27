#include "pch.h"
#include "support/folia_test.hpp"
#include "support/DocumentControllerFixture.h"

import folia.markdown.instrumentation;

using namespace boost::ut;
using namespace document_tests;

namespace {
    suite document_presentation_tests = [] {
        "document presentation rebinding preserves saved baseline and redirects callbacks to the receiving window"_test = [] {
            Fixture f;
            f.Type(U"edit ");
            auto text = f.session.TextUtf8();
            auto selection = f.session.Selection();
            EditorSurfaceRenderer targetRenderer;
            EditorTextInputController targetInput;
            targetRenderer.InitializeForPdf();
            std::size_t changes = 0;
            f.controller.SuspendPresentation();
            f.controller.RebindPresentation({targetRenderer, targetInput,
                [&](auto const& command) { return f.session.ExecuteCommand(command); }, {}, {}, [&] { ++changes; }, {}, {}, {}});
            expect(f.controller.IsModified());
            expect(f.session.TextUtf8() == text && f.session.Selection() == selection);
            expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(!f.controller.IsModified());
            expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Redo}));
            expect(f.controller.IsModified());
            expect(Await(f.controller.SaveAsync()));
            expect(!f.controller.IsModified() && f.DiskText() == text && changes > 0);
            expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(f.controller.IsModified());
            f.controller.SuspendPresentation();
            f.controller.RebindPresentation({f.renderer, f.input, {}, {}, {}, {}, {}, {}, {}});
            expect(f.controller.IsModified());
            expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Redo}));
            expect(!f.controller.IsModified());
        };
        "document presentation modification state follows save undo redo and new branches"_test = [] {
            Fixture f;
            expect(!f.controller.IsModified());
            folia::reset_core_operation_counters();
            f.Type(U"edited");
            for (int i = 0; i < 64; ++i) expect(f.controller.IsModified());
            expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(!f.controller.IsModified());
            expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Redo}));
            expect(f.controller.IsModified());
            const auto counters = folia::read_core_operation_counters();
            expect(counters.full_document_parses == 0_u);
            expect(counters.full_document_serializations == 0_u);
            expect(Await(f.controller.SaveAsync()));
            expect(!f.controller.IsModified());
            expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(f.controller.IsModified());
            f.Type(U"different");
            expect(f.controller.IsModified());
        };

        "document presentation suspension preserves source selection history and save baseline"_test = [] {
            Fixture f;
            f.Type(U"edit");
            const auto source = f.session.TextUtf8();
            const auto selection = f.session.Selection();
            const auto state = f.session.ContentState();
            folia::reset_core_operation_counters();
            for (int i = 0; i < 8; ++i) {
                f.controller.SuspendPresentation();
                expect(f.controller.IsModified());
                f.controller.ResumePresentation(f.renderer, f.input);
                expect(f.controller.IsModified());
            }
            const auto counters = folia::read_core_operation_counters();
            expect(counters.full_document_parses == 0_u);
            expect(counters.full_document_serializations == 0_u);
            expect(f.session.ContentState() == state);
            expect(f.session.Selection() == selection);
            expect(f.session.TextUtf8() == source);
            expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(!f.controller.IsModified());
            expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Redo}));
            expect(f.controller.IsModified());
        };

        "document presentation save completes for its owner after it becomes inactive"_test = [] {
            Fixture f(false, L"unsaved");
            auto target = f.File(L"saved-inactive.md");
            f.controller.SetSaveDialogs({}, [&]() -> IAsyncOperation<StorageFile> {
                winrt::apartment_context ui;
                co_await winrt::resume_after(std::chrono::milliseconds(10));
                co_await ui;
                f.controller.SuspendPresentation();
                co_return target;
            });
            const auto state = f.session.ContentState();
            expect(Await(f.controller.SaveAsync()));
            expect(f.session.File() == target);
            expect(f.DiskText() == "unsaved");
            expect(!f.controller.IsModified());
            f.controller.ResumePresentation(f.renderer, f.input);
            expect(f.session.ContentState() == state);
            expect(!f.controller.IsModified());
        };

        "document presentation inactive close retains save discard and cancel behavior"_test = [] {
            Fixture f;
            f.Type(U"changed");
            const auto source = f.session.TextUtf8();
            f.controller.SuspendPresentation();
            expect(!Await(f.controller.ConfirmCloseAsync()));
            expect(f.session.TextUtf8() == source);
            f.choice = ContentDialogResult::Secondary;
            expect(Await(f.controller.ConfirmCloseAsync()));
            expect(f.controller.IsModified());
            f.choice = ContentDialogResult::Primary;
            expect(Await(f.controller.ConfirmCloseAsync()));
            expect(f.DiskText() == source);
            expect(!f.controller.IsModified());
        };

        "document presentation stale loading cannot publish after suspension and resumption"_test = [] {
            Fixture f(true, L"original", false);
            auto target = f.File(L"next.md", "next $x+1$");
            auto loading = f.controller.OpenFileAsync(target);
            Pump([&] { return f.controller.IsLoading(); });
            f.controller.SuspendPresentation();
            f.renderer.InitializeForPdf();
            f.controller.ResumePresentation(f.renderer, f.input);
            expect(!Await(loading));
            expect(f.session.TextUtf8() == "original");
            expect(Await(f.controller.OpenFileAsync(target)));
            expect(f.session.TextUtf8() == "next $x+1$");
            expect(!f.controller.IsModified());
        };

        "document presentation resume notices an external edit made while inactive"_test = [] {
            Fixture f;
            auto file = f.File(L"external.md", "before");
            expect(Await(f.controller.OpenFileAsync(file)));
            f.controller.SuspendPresentation();
            std::ofstream(std::filesystem::path(file.Path().c_str()), std::ios::binary | std::ios::trunc) << "after external edit";
            f.controller.ResumePresentation(f.renderer, f.input);
            Pump([&] { return f.session.TextUtf8() == "after external edit"; });
            expect(!f.controller.IsModified());
            expect(!f.wrongThread);
        };

        "document presentation switched paste cannot edit either document"_test = [] {
            Fixture f;
            auto original = f.session.TextUtf8();
            winrt::Windows::ApplicationModel::DataTransfer::DataPackage package;
            winrt::Windows::ApplicationModel::DataTransfer::DataProviderRequest request{nullptr};
            winrt::Windows::ApplicationModel::DataTransfer::DataProviderDeferral deferral{nullptr};
            package.SetDataProvider(winrt::Windows::ApplicationModel::DataTransfer::StandardDataFormats::Text(), [&](auto const& value) {
                request = value;
                deferral = request.GetDeferral();
            });
            auto paste = f.controller.PasteAsync(package.GetView());
            Pump([&] { return request != nullptr; });
            f.controller.SuspendPresentation();
            request.SetData(winrt::box_value(L"paste content"));
            deferral.Complete();
            expect(!Await(paste));
            expect(f.session.TextUtf8() == original);
            f.controller.ResumePresentation(f.renderer, f.input);
            expect(!f.controller.IsModified());
        };
    };
}
