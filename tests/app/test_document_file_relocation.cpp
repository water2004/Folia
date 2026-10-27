#include "pch.h"
#include "support/folia_test.hpp"
#include "support/DocumentControllerFixture.h"
#include "workspace/WorkspaceFileOperations.h"

import folia.markdown.instrumentation;

namespace {
    using namespace boost::ut;
    using namespace document_tests;
    using namespace winrt::Folia;
    suite document_file_relocation_tests = [] {
        "document file relocation preserves edited source history selection and save state in both modes"_test = [] {
            for (bool sourceMode : {false, true}) {
                Fixture f;
                if (sourceMode) expect(f.session.EnterSourceMode());
                f.Type(U"edited");
                auto text = f.session.TextUtf8();
                auto selection = f.session.Selection();
                auto state = f.session.ContentState();
                auto oldPath = std::filesystem::path(f.session.Path().c_str());
                auto newPath = f.directory.path / L"moved.md";
                auto token = f.controller.BeginFileRelocation();
                expect(token.has_value());
                expect(!f.controller.BeginFileRelocation());
                expect(!Await(f.controller.SaveAsync()));
                expect(!Await(f.controller.ConfirmCloseAsync()));
                winrt::Windows::ApplicationModel::DataTransfer::DataPackage package;
                package.SetText(L"must not insert");
                expect(!Await(f.controller.PasteAsync(package.GetView())));
                Await(ApplyWorkspaceFileActionAsync({WorkspaceFileActionKind::Move, oldPath, newPath, false}));
                auto file = Await(StorageFile::GetFileFromPathAsync(newPath.wstring()));
                folia::reset_core_operation_counters();
                f.controller.FinishFileRelocation(*token, file);
                auto counters = folia::read_core_operation_counters();
                expect(counters.full_document_parses == 0_u);
                expect(counters.full_document_serializations == 0_u);
                expect(winrt::to_string(f.session.Path()) == winrt::to_string(file.Path()));
                expect(f.session.BaseDirectory() == std::filesystem::path(file.Path().c_str()).parent_path().wstring());
                expect(f.session.ContentState() == state);
                expect(f.session.Selection() == selection);
                expect(f.session.TextUtf8() == text);
                expect(f.session.IsSourceMode() == sourceMode);
                expect(f.controller.IsModified());
                expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
                expect(!f.controller.IsModified());
                expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Redo}));
                expect(f.session.TextUtf8() == text);
                expect(f.session.Selection() == selection);
            }
        };
        "document file relocation abort and obsolete tokens do not alter file ownership"_test = [] {
            Fixture f;
            auto original = f.session.File();
            auto first = f.controller.BeginFileRelocation();
            expect(first.has_value());
            f.controller.FinishFileRelocation(*first);
            expect(f.session.File() == original);
            expect(!f.controller.IsModified());
            auto second = f.controller.BeginFileRelocation();
            expect(second.has_value());
            auto different = f.File(L"different.md", "different");
            f.controller.FinishFileRelocation(*first, different);
            expect(f.session.File() == original);
            expect(!Await(f.controller.SaveAsync()));
            f.controller.FinishFileRelocation(*second);
            expect(Await(f.controller.SaveAsync()));
            expect(f.session.File() == original);
        };
        "document file relocation updates an inactive document without resetting its dirty baseline"_test = [] {
            Fixture f;
            f.Type(U"pending");
            auto state = f.session.ContentState();
            auto token = f.controller.BeginFileRelocation();
            expect(token.has_value());
            f.controller.SuspendPresentation();
            auto target = f.File(L"inactive.md", "initial\n");
            f.controller.FinishFileRelocation(*token, target);
            expect(f.session.File() == target);
            expect(f.session.ContentState() == state);
            expect(f.controller.IsModified());
            f.controller.ResumePresentation(f.renderer, f.input);
            expect(f.controller.IsModified());
            expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(!f.controller.IsModified());
        };
    };
}
