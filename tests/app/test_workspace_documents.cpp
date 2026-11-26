#include "pch.h"
#include "support/folia_test.hpp"
#include "support/DocumentControllerFixture.h"
#include "workspace/WorkspaceDocuments.h"
#include "workspace/RecentWorkspace.h"
#include "workspace/WorkspaceIcons.h"

import folia.markdown.instrumentation;

namespace {
    using namespace boost::ut;
    using namespace winrt::Folia;
    using namespace document_tests;

    suite workspace_documents = [] {
        "workspace recent files and folders persist deduplicated in separate lists"_test = [] {
            TestDirectory fixture;
            auto historyFile = fixture.path / L"recent.json";
            RecentWorkspace firstWindow(historyFile), secondWindow(historyFile);
            using Kind = RecentWorkspaceKind;
            auto note = fixture.path / L"笔记.md";
            auto folder = fixture.path / L"资料";
            firstWindow.Remember(Kind::File, note);
            secondWindow.Remember(Kind::Folder, folder / L".");
            for (int i = 0; i < 14; ++i)
                firstWindow.Remember(Kind::File, fixture.path / (std::to_wstring(i) + L".png"));
            secondWindow.Remember(Kind::File, note);
            firstWindow.Remember(Kind::File, fixture.path / L"笔记.MD");
            auto persisted = RecentWorkspace(historyFile).Read();
            expect(persisted.files.size() == 12_u);
            expect(fatal(!persisted.files.empty() && persisted.folders.size() == 1_u));
            expect(persisted.files.front() == note && persisted.folders.front() == folder);
            expect(persisted.files.at(1) == fixture.path / L"13.png");
            firstWindow.ForgetMissing(Kind::Folder, folder, HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED));
            expect(secondWindow.Read().folders.size() == 1_u);
            firstWindow.ForgetMissing(Kind::File, fixture.path / L"笔记.MD", HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
            expect(secondWindow.Read().files.size() == 11_u);
            firstWindow.ForgetMissing(Kind::Folder, folder, HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND));
            expect(secondWindow.Read().folders.empty());
        };
        "workspace classifies files and shares the preview slot with images"_test = [] {
            expect(WorkspaceFileIcon(ClassifyWorkspaceFile(L"note.MARKDOWN")) == WorkspaceIcon::Markdown);
            expect(WorkspaceFileIcon(ClassifyWorkspaceFile(L"photo.JPEG")) == WorkspaceIcon::Image);
            expect(WorkspaceFileIcon(ClassifyWorkspaceFile(L"vector.SvG")) == WorkspaceIcon::Image);
            expect(WorkspaceFileIcon(ClassifyWorkspaceFile(L"plain.TXT")) == WorkspaceIcon::Document);
            expect(WorkspaceFileIcon(ClassifyWorkspaceFile(L"archive.zip")) == WorkspaceIcon::Document);
            WorkspaceDocuments documents;
            auto first = documents.Open(L"D:/files/note.md", WorkspaceOpenMode::Preview);
            documents.Activate(first.document->id);
            auto photo = documents.Open(L"D:/files/PHOTO.PNG", WorkspaceOpenMode::Preview);
            expect(photo.replaced == first.document->id);
            expect(!photo.document->editor && !photo.document->IsModified());
            documents.Activate(photo.document->id);
            expect(documents.Open(L"d:/FILES/photo.png", WorkspaceOpenMode::Keep).document == photo.document);
            expect(photo.document->kept);
            auto second = documents.Open(L"D:/files/note.md", WorkspaceOpenMode::Preview);
            expect(!second.replaced && static_cast<bool>(second.document->editor));
            documents.Activate(second.document->id);
            WorkspaceDocuments receiving;
            documents.TransferTo(photo.document->id, receiving, 0);
            expect(receiving.Find(photo.document->id) == photo.document && !documents.Find(photo.document->id));
        };
        "workspace transfer moves the original session and preserves source history"_test = [] {
            Fixture fixture(false, L"");
            WorkspaceDocuments source, target;
            auto document = source.Active();
            auto successor = source.New();
            auto targetActive = target.Active();
            expect(document->id != targetActive->id && document->id != successor->id);
            document->editor->controller.Attach(document->editor->session, {fixture.renderer, fixture.input, {}, {}, {}, {}, {}, {}, {}});
            expect(document->editor->session.ExecuteCommand(folia::Command::InsertText(U"**数学** $x+1$")));
            source.Keep(document->id);
            expect(document->editor->session.EnterSourceMode());
            expect(document->editor->session.ExecuteCommand(folia::Command::InsertText(U"prefix ")));
            auto text = document->editor->session.TextUtf8();
            auto selection = document->editor->session.Selection();
            auto state = document->editor->session.ContentState();
            document->editor->math.emplace();
            auto cache = &*document->editor->math;
            document->editor->controller.SuspendPresentation();
            source.Activate(successor->id);
            folia::reset_core_operation_counters();
            source.TransferTo(document->id, target, 0);
            expect(!source.Find(document->id));
            expect(target.Find(document->id).get() == document.get());
            expect(target.Items().front() == document && target.Active() == targetActive);
            expect(document->editor->controller.IsModified() && document->kept && document->editor->session.IsSourceMode());
            expect(&*document->editor->math == cache);
            expect(document->editor->session.Selection() == selection && document->editor->session.ContentState() == state);
            expect(document->editor->session.TextUtf8() == text);
            expect(document->editor->session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(document->editor->session.ExecuteCommand(folia::Command{folia::CommandKind::Redo}));
            auto counters = folia::read_core_operation_counters();
            expect(counters.full_document_parses == 0_u && counters.full_document_serializations == 0_u);
            expect(document->editor->session.TextUtf8() == text && document->editor->session.Selection() == selection);
            target.TransferTo(document->id, source, 1);
            expect(source.Items().back() == document && !target.Find(document->id));
        };
        "workspace transfer rejects active missing same-owner and duplicate-path requests without changes"_test = [] {
            WorkspaceDocuments source, target;
            auto document = source.Open(L"D:/notes/same.md", WorkspaceOpenMode::Keep).document;
            // Open may replace the implicit preview; activate an owned successor.
            auto successor = source.New(); source.Activate(successor->id);
            auto duplicate = target.Open(L"d:/NOTES/same.md", WorkspaceOpenMode::Keep).document;
            auto fromSize = source.Items().size(), toSize = target.Items().size();
            expect(throws([&] { source.TransferTo(document->id, target, 0); }));
            expect(throws([&] { source.TransferTo(successor->id, target, 0); }));
            expect(throws([&] { source.TransferTo(document->id, source, 0); }));
            expect(throws([&] { source.TransferTo(0, target, 0); }));
            expect(source.Items().size() == fromSize && target.Items().size() == toSize);
            expect(source.Find(document->id) == document && target.Find(duplicate->id) == duplicate);
        };
        "workspace documents preview replacement and path identity preserve kept tabs"_test = [] {
            WorkspaceDocuments documents;
            const auto initial = documents.Active();
            auto first = documents.Open(L"D:/notes/first.md", WorkspaceOpenMode::Preview);
            expect(first.replaced == initial->id);
            documents.Activate(first.document->id);
            auto again = documents.Open(L"d:/NOTES/sub/../first.md", WorkspaceOpenMode::Preview);
            expect(again.document == first.document);
            expect(!again.created);
            auto kept = documents.Open(L"D:/notes/first.md", WorkspaceOpenMode::Keep);
            expect(kept.document->kept);
            auto second = documents.Open(L"D:/other/first.md", WorkspaceOpenMode::Preview);
            expect(!second.replaced);
            documents.Activate(second.document->id);
            auto third = documents.Open(L"D:/notes/third.md", WorkspaceOpenMode::Preview);
            expect(third.replaced == second.document->id);
            expect(documents.Items().size() == 2_u);
            expect(documents.Find(first.document->id) == first.document);
        };

        "workspace documents edits pin previews and retain independent session history"_test = [] {
            Fixture fixture(false, L"");
            WorkspaceDocuments documents;
            auto first = documents.Active();
            first->editor->controller.Attach(first->editor->session, {fixture.renderer, fixture.input,
                {}, {}, {}, {}, {}, {}, {}});
            folia::Command insert{folia::CommandKind::InsertText};
            insert.text = U"first";
            expect(first->editor->session.ExecuteCommand(insert));
            const auto selection = first->editor->session.Selection();
            auto second = documents.Open(L"D:/notes/second.md", WorkspaceOpenMode::Preview).document;
            expect(first->kept);
            expect(!second->kept);
            documents.Activate(second->id);
            insert.text = U"second";
            expect(second->editor->session.ExecuteCommand(insert));
            folia::reset_core_operation_counters();
            documents.Activate(first->id);
            expect(first->editor->session.Selection() == selection);
            expect(first->editor->session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(first->editor->session.ExecuteCommand(folia::Command{folia::CommandKind::Redo}));
            auto counters = folia::read_core_operation_counters();
            expect(counters.full_document_parses == 0_u);
            expect(counters.full_document_serializations == 0_u);
            expect(first->editor->session.TextUtf8() == "first");
            expect(second->editor->session.TextUtf8() == "second");
        };

        "workspace documents explicit activation reorder and close preserve owners"_test = [] {
            WorkspaceDocuments documents;
            auto a = documents.Active();
            auto b = documents.New();
            auto c = documents.New();
            expect(documents.Active() == a);
            std::array order{c->id, a->id, b->id};
            documents.Reorder(order);
            expect(documents.Items()[0] == c);
            expect(documents.Items()[1] == a);
            expect(documents.Items()[2] == b);
            expect(throws([&] { documents.Remove(a->id); }));
            documents.Activate(c->id);
            expect(documents.Remove(a->id));
            expect(!documents.Find(a->id));
            expect(documents.Active() == c);
            expect(!documents.Remove(a->id));
            std::array invalid{b->id, b->id};
            expect(throws([&] { documents.Reorder(invalid); }));
            expect(documents.Items()[0] == c);
        };

        "workspace documents retain source view selection and exact undo across switches"_test = [] {
            WorkspaceDocuments documents;
            auto first = documents.Active();
            auto second = documents.New();
            first->editor->session.SetText(L"**first** and $x+1$");
            second->editor->session.SetText(L"_second_");
            expect(first->editor->session.ToggleSourceMode());
            expect(first->editor->session.ExecuteCommand(folia::Command::InsertText(U"prefix ")));
            auto source = first->editor->session.TextUtf8();
            auto selection = first->editor->session.Selection();
            auto state = first->editor->session.ContentState();
            folia::reset_core_operation_counters();
            for (int i = 0; i < 10; ++i) {
                documents.Activate(second->id);
                expect(!second->editor->session.IsSourceMode());
                documents.Activate(first->id);
                expect(first->editor->session.IsSourceMode());
                expect(first->editor->session.ContentState() == state);
                expect(first->editor->session.Selection() == selection);
            }
            expect(first->editor->session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(first->editor->session.ExecuteCommand(folia::Command{folia::CommandKind::Redo}));
            auto counters = folia::read_core_operation_counters();
            expect(counters.full_document_parses == 0_u);
            expect(counters.full_document_serializations == 0_u);
            expect(first->editor->session.TextUtf8() == source);
            expect(first->editor->session.Selection() == selection);
            expect(second->editor->session.TextUtf8() == "_second_");
        };
    };
}
