#include "pch.h"
#include "support/folia_test.hpp"
#include "support/DocumentControllerFixture.h"
#include "workspace/WorkspaceFileCoordinator.h"
import folia.markdown.instrumentation;

namespace {
    using namespace boost::ut;
    using namespace document_tests;
    using namespace winrt::Folia;
    using Kind = folia::platform::WorkspaceFileActionKind;
    suite workspace_file_coordinator_tests = [] {
        "workspace renames an open image without creating an editor or save lease"_test = [] {
            TestDirectory directory;
            WorkspaceDocuments workspace;
            auto before = directory.path / L"before.png";
            auto after = directory.path / L"after.PNG";
            std::ofstream(before, std::ios::binary) << "fixture";
            auto image = workspace.Open(before, WorkspaceOpenMode::Keep).document;
            workspace.Activate(image->id);
            expect(!image->editor);
            Await(ApplyWorkspaceDocumentFileActionAsync(workspace, {Kind::Rename, before, after, false}));
            expect(workspace.Find(after) == image && image->path == after);
            expect(!std::filesystem::exists(before) && std::filesystem::exists(after));
        };
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
                doc->editor->session.Open(file, doc == first ? L"**first**\n" : L"_second_\n", {}, {});
                workspace.RefreshPath(doc->id);
                doc->editor->loaded = true;
                doc->editor->controller.Attach(doc->editor->session, {f.renderer, f.input, {}, {}, {}, [&] { ++changed; }, {}, {}, {}});
                if (doc == second) { expect(doc->editor->session.EnterSourceMode()); doc->editor->controller.SuspendPresentation(); }
                expect(doc->editor->session.ExecuteCommand(folia::Command::InsertText(U"edited ")));
            }
            auto firstText = first->editor->session.TextUtf8(), secondText = second->editor->session.TextUtf8();
            auto firstSelection = first->editor->session.Selection(), secondSelection = second->editor->session.Selection();
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
            expect(first->editor->session.TextUtf8() == firstText);
            expect(second->editor->session.TextUtf8() == secondText);
            expect(first->editor->session.Selection() == firstSelection);
            expect(second->editor->session.Selection() == secondSelection);
            expect(second->editor->session.IsSourceMode());
            for (auto const& doc : {first, second}) {
                expect(doc->editor->controller.IsModified());
                expect(doc->editor->session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
                expect(!doc->editor->controller.IsModified());
                expect(doc->editor->session.ExecuteCommand(folia::Command{folia::CommandKind::Redo}));
                expect(doc->editor->controller.IsModified());
                expect(Await(doc->editor->controller.SaveAsync()));
                expect(!doc->editor->controller.IsModified());
            }
            expect(!std::filesystem::exists(folder));
        };
        "workspace file coordinator releases relocation leases after conflicts and busy siblings"_test = [] {
            Fixture f;
            WorkspaceDocuments workspace;
            auto doc = workspace.Active();
            doc->editor->session.Open(f.session.File(), L"initial\n", {}, {});
            workspace.RefreshPath(doc->id);
            doc->editor->loaded = true;
            doc->editor->controller.Attach(doc->editor->session, {f.renderer, f.input, {}, {}, {}, {}, {}, {}, {}});
            expect(doc->editor->session.ExecuteCommand(folia::Command::InsertText(U"pending")));
            auto original = doc->path;
            auto existing = f.File(L"existing.md", "must keep");
            bool rejected = false;
            try { Await(ApplyWorkspaceDocumentFileActionAsync(workspace, {Kind::Rename, original, std::filesystem::path(existing.Path().c_str()), false})); }
            catch (winrt::hresult_error const&) { rejected = true; }
            expect(rejected);
            expect(doc->path == original);
            expect(doc->editor->controller.IsModified());
            auto token = doc->editor->controller.BeginFileRelocation();
            expect(token.has_value());
            doc->editor->controller.FinishFileRelocation(*token);
            expect(Await(doc->editor->controller.SaveAsync()));
            auto sibling = workspace.New();
            sibling->path = original.parent_path() / L"loading.md";
            sibling->editor->opening = true;
            rejected = false;
            try { Await(ApplyWorkspaceDocumentFileActionAsync(workspace, {Kind::Rename, original.parent_path(), original.parent_path().parent_path() / L"unused-relocation", true})); }
            catch (winrt::hresult_error const&) { rejected = true; }
            expect(rejected);
            expect(std::filesystem::exists(original));
            token = doc->editor->controller.BeginFileRelocation();
            expect(token.has_value());
            doc->editor->controller.FinishFileRelocation(*token);
        };
    };
}
