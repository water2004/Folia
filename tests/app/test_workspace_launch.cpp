#include "pch.h"
#include "support/folia_test.hpp"
#include "workspace/WorkspaceLaunch.h"

namespace {
    using namespace boost::ut;
    using namespace winrt::Folia;
    suite workspace_launch_tests = [] {
        "workspace launch preserves quoted Unicode paths and isolates only explicit new instances"_test = [] {
            auto request = ReadWorkspaceCommandLine(LR"("D:\Folia app\Folia.exe" "D:\文档\第一 章.md" --new-instance "D:\文档\第二章.md")");
            expect(request.newInstance && request.documents.size() == 2_u);
            expect(request.documents[0] == std::filesystem::path(L"D:/文档/第一 章.md"));
            expect(request.documents[1] == std::filesystem::path(L"D:/文档/第二章.md"));
            auto relative = ReadWorkspaceCommandLine(LR"(Folia.exe notes\..\chapter.md)");
            expect(relative.documents.size() == 1_u && relative.documents[0].is_absolute());
            expect(relative.documents[0].filename() == L"chapter.md");
            auto literal = ReadWorkspaceCommandLine(L"Folia.exe -- --new-instance");
            expect(!literal.newInstance && literal.documents.size() == 1_u);
            expect(literal.documents[0].filename() == L"--new-instance");
            expect(ReadWorkspaceCommandLine(L"Folia.exe").documents.empty());
        };
        "workspace activation carries absolute paths by value including an empty new window request"_test = [] {
            WorkspaceLaunchRequest request{false, {L"D:/文档/第一 章.md", L"D:/notes/x.md"}};
            auto copy = ReadWorkspaceActivation(MakeWorkspaceActivation(request));
            expect(copy.documents == request.documents && !copy.newInstance);
            expect(ReadWorkspaceActivation(MakeWorkspaceActivation({})).documents.empty());
            expect(static_cast<bool>(WorkspaceInstanceKey(L"D:/APP/Folia.exe") == WorkspaceInstanceKey(L"d:/app/Folia.exe")));
            expect(static_cast<bool>(WorkspaceInstanceKey(L"D:/app/Folia.exe") != WorkspaceInstanceKey(L"D:/candidate/Folia.exe")));
        };
    };
}
