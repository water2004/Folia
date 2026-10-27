#include "pch.h"
#include "support/folia_test.hpp"
#include "support/DocumentControllerFixture.h"
#include "workspace/WorkspaceFileCoordinator.h"
import folia.markdown.instrumentation;

namespace {
    using namespace boost::ut;
    using namespace document_tests;
    using namespace winrt::Folia;
    using Kind = WorkspaceFileActionKind;
    suite workspace_file_coordinator_tests = [] {
        "workspace file coordinator moves dirty directory tabs without rebuilding their documents"_test = [] {
            Fixture f;
            WorkspaceDocuments workspace;
            auto parent = std::filesystem::path(f.session.Path().c_str()).parent_path();
            auto folder = parent / L"folder";
            std::filesystem::create_directory(folder);
            std::ofstream(folder / L"a.md", std::ios::binary) << "**first**\n";
            std::ofstream(folder / L"b.md", std::ios::binary) << "_second_\n";
            auto first = workspace.Active();
            auto second = workspace.New();
            int changed = 0;
            for (auto const& doc : {first, second}) {
                auto file = Await(StorageFile::GetFileFromPathAsync((folder / (doc == first ? L"a.md" : L"b.md")).native()));
                doc->session.Open(file, doc == first ? L"**first**\n" : L"_second_\n", {}, {});
                workspace.RefreshPath(doc->id);
                doc->loaded = true;
                doc->controller.Attach(doc->session, {f.renderer, f.input, {}, {}, {}, [&] { ++changed; }, {}, {}, {}});
                if (doc == second) { expect(doc->session.EnterSourceMode()); doc->controller.SuspendPresentation(); }
                expect(doc->session.ExecuteCommand(folia::Command::InsertText(U"edited ")));
            }
            auto firstText = first->session.TextUtf8(), secondText = second->session.TextUtf8();
            auto firstSelection = first->session.Selection(), secondSelection = second->session.Selection();
            auto destination = parent / L"renamed";
            folia::reset_core_operation_counters();
            Await(ApplyWorkspaceDocumentFileActionAsync(workspace, {Kind::Rename, folder, destination, true}));
            auto counters = folia::read_core_operation_counters();
            expect(counters.full_document_parses == 0_u);
            expect(counters.full_document_serializations == 0_u);
            expect(changed == 2_i);
            expect(!std::filesystem::exists(folder));
            expect(first->path == destination / L"a.md");
            expect(second->path == destination / L"b.md");
            expect(workspace.Find(first->path) == first);
            expect(first->session.TextUtf8() == firstText);
            expect(second->session.TextUtf8() == secondText);
            expect(first->session.Selection() == firstSelection);
            expect(second->session.Selection() == secondSelection);
            expect(second->session.IsSourceMode());
            for (auto const& doc : {first, second}) {
                expect(doc->controller.IsModified());
                expect(doc->session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
                expect(!doc->controller.IsModified());
                expect(doc->session.ExecuteCommand(folia::Command{folia::CommandKind::Redo}));
                expect(doc->controller.IsModified());
                expect(Await(doc->controller.SaveAsync()));
                expect(!doc->controller.IsModified());
            }
            expect(!std::filesystem::exists(folder));
        };
        "workspace file coordinator releases relocation leases after conflicts and busy siblings"_test = [] {
            Fixture f;
            WorkspaceDocuments workspace;
            auto doc = workspace.Active();
            doc->session.Open(f.session.File(), L"initial\n", {}, {});
            workspace.RefreshPath(doc->id);
            doc->loaded = true;
            doc->controller.Attach(doc->session, {f.renderer, f.input, {}, {}, {}, {}, {}, {}, {}});
            expect(doc->session.ExecuteCommand(folia::Command::InsertText(U"pending")));
            auto original = doc->path;
            auto existing = f.File(L"existing.md", "must keep");
            bool rejected = false;
            try { Await(ApplyWorkspaceDocumentFileActionAsync(workspace, {Kind::Rename, original, std::filesystem::path(existing.Path().c_str()), false})); }
            catch (winrt::hresult_error const&) { rejected = true; }
            expect(rejected);
            expect(doc->path == original);
            expect(doc->controller.IsModified());
            auto token = doc->controller.BeginFileRelocation();
            expect(token.has_value());
            doc->controller.FinishFileRelocation(*token);
            expect(Await(doc->controller.SaveAsync()));
            auto sibling = workspace.New();
            sibling->path = original.parent_path() / L"loading.md";
            sibling->opening = true;
            rejected = false;
            try { Await(ApplyWorkspaceDocumentFileActionAsync(workspace, {Kind::Rename, original.parent_path(), original.parent_path().parent_path() / L"unused-relocation", true})); }
            catch (winrt::hresult_error const&) { rejected = true; }
            expect(rejected);
            expect(std::filesystem::exists(original));
            token = doc->controller.BeginFileRelocation();
            expect(token.has_value());
            doc->controller.FinishFileRelocation(*token);
        };
    };
}
