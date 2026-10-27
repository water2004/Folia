#include "pch.h"
#include "support/folia_test.hpp"
#include "support/DocumentControllerFixture.h"
#include "workspace/WorkspaceDocuments.h"

import folia.markdown.instrumentation;

namespace {
    using namespace boost::ut;
    using namespace winrt::Folia;
    using namespace document_tests;

    suite workspace_documents = [] {
        "workspace transfer moves the original session and preserves source history and viewport"_test = [] {
            Fixture fixture(false, L"");
            WorkspaceDocuments source, target;
            auto document = source.Active();
            auto successor = source.New();
            auto targetActive = target.Active();
            expect(document->id != targetActive->id && document->id != successor->id);
            document->controller.Attach(document->session, {fixture.renderer, fixture.input, {}, {}, {}, {}, {}, {}, {}});
            expect(document->session.ExecuteCommand(folia::Command::InsertText(U"**数学** $x+1$")));
            source.Keep(document->id);
            expect(document->session.EnterSourceMode());
            expect(document->session.ExecuteCommand(folia::Command::InsertText(U"prefix ")));
            auto text = document->session.TextUtf8();
            auto selection = document->session.Selection();
            auto state = document->session.ContentState();
            document->viewportPosition = selection.active;
            document->scrollOffset = 1234;
            document->viewportInset = -3;
            document->math.emplace();
            auto cache = &*document->math;
            document->controller.SuspendPresentation();
            source.Activate(successor->id);
            folia::reset_core_operation_counters();
            source.TransferTo(document->id, target, 0);
            expect(!source.Find(document->id));
            expect(target.Find(document->id).get() == document.get());
            expect(target.Items().front() == document && target.Active() == targetActive);
            expect(document->controller.IsModified() && document->kept && document->session.IsSourceMode());
            expect(document->viewportPosition == selection.active && document->scrollOffset == 1234.f && document->viewportInset == -3.f);
            expect(&*document->math == cache);
            expect(document->session.Selection() == selection && document->session.ContentState() == state);
            expect(document->session.TextUtf8() == text);
            expect(document->session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(document->session.ExecuteCommand(folia::Command{folia::CommandKind::Redo}));
            auto counters = folia::read_core_operation_counters();
            expect(counters.full_document_parses == 0_u && counters.full_document_serializations == 0_u);
            expect(document->session.TextUtf8() == text && document->session.Selection() == selection);
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
            first->controller.Attach(first->session, {fixture.renderer, fixture.input,
                {}, {}, {}, {}, {}, {}, {}});
            folia::Command insert{folia::CommandKind::InsertText};
            insert.text = U"first";
            expect(first->session.ExecuteCommand(insert));
            const auto selection = first->session.Selection();
            auto second = documents.Open(L"D:/notes/second.md", WorkspaceOpenMode::Preview).document;
            expect(first->kept);
            expect(!second->kept);
            documents.Activate(second->id);
            insert.text = U"second";
            expect(second->session.ExecuteCommand(insert));
            folia::reset_core_operation_counters();
            documents.Activate(first->id);
            expect(first->session.Selection() == selection);
            expect(first->session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(first->session.ExecuteCommand(folia::Command{folia::CommandKind::Redo}));
            auto counters = folia::read_core_operation_counters();
            expect(counters.full_document_parses == 0_u);
            expect(counters.full_document_serializations == 0_u);
            expect(first->session.TextUtf8() == "first");
            expect(second->session.TextUtf8() == "second");
        };

        "workspace documents explicit activation reorder and close preserve owners"_test = [] {
            WorkspaceDocuments documents;
            auto a = documents.Active();
            auto b = documents.New();
            auto c = documents.New();
            expect(documents.Active() == a);
            std::array order{c->id, a->id, b->id};
            documents.Reorder(order);
            expect(documents.Adjacent(a->id, 1) == b);
            expect(documents.Adjacent(c->id, -1) == b);
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
            first->session.SetText(L"**first** and $x+1$");
            second->session.SetText(L"_second_");
            expect(first->session.ToggleSourceMode());
            expect(first->session.ExecuteCommand(folia::Command::InsertText(U"prefix ")));
            auto source = first->session.TextUtf8();
            auto selection = first->session.Selection();
            auto state = first->session.ContentState();
            folia::reset_core_operation_counters();
            for (int i = 0; i < 10; ++i) {
                documents.Activate(second->id);
                expect(!second->session.IsSourceMode());
                documents.Activate(first->id);
                expect(first->session.IsSourceMode());
                expect(first->session.ContentState() == state);
                expect(first->session.Selection() == selection);
            }
            expect(first->session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(first->session.ExecuteCommand(folia::Command{folia::CommandKind::Redo}));
            auto counters = folia::read_core_operation_counters();
            expect(counters.full_document_parses == 0_u);
            expect(counters.full_document_serializations == 0_u);
            expect(first->session.TextUtf8() == source);
            expect(first->session.Selection() == selection);
            expect(second->session.TextUtf8() == "_second_");
        };
    };
}
