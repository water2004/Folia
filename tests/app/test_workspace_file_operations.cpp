#include "pch.h"
#include "support/folia_test.hpp"
#include "support/DocumentControllerFixture.h"
#include "workspace/WorkspaceFileOperations.h"

namespace {
    using namespace boost::ut;
    using namespace document_tests;
    using namespace winrt::Folia;
    using Kind = WorkspaceFileActionKind;
    std::filesystem::path Apply(Kind kind, std::filesystem::path source, std::filesystem::path target, bool directory = false) {
        return std::filesystem::path(Await(ApplyWorkspaceFileActionAsync({kind, std::move(source), std::move(target), directory})).c_str());
    }
    bool Rejected(Kind kind, std::filesystem::path source, std::filesystem::path target, bool directory = false) {
        try { Apply(kind, std::move(source), std::move(target), directory); return false; }
        catch (winrt::hresult_error const&) { return true; }
    }
    std::string Bytes(std::filesystem::path const& path) {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), {}};
    }

    suite workspace_file_operation_tests = [] {
        "workspace file operations validate Windows names without filesystem access"_test = [] {
            for (auto name : {L"notes.md", L"空格 & emoji😀.md", L".gitignore", L"CONSOLE.md"}) expect(ValidWorkspaceEntryName(name));
            for (auto name : {L"", L".", L"..", L"a/b", L"a\\b", L"a:", L"a*", L"file.", L"file ", L"CON", L"con.txt", L"PRN", L"LPT1.md", L"COM¹"})
                expect(!ValidWorkspaceEntryName(name));
        };
        "workspace file operations create rename move and copy without replacing destinations"_test = [] {
            TestDirectory directory;
            auto folder = directory.path / L"folder";
            auto first = directory.path / L"空格 & emoji😀.md";
            Apply(Kind::NewFolder, {}, folder, true);
            Apply(Kind::NewFile, {}, first);
            expect(std::filesystem::is_directory(folder));
            expect(std::filesystem::file_size(first) == 0_u);
            std::ofstream(first, std::ios::binary) << "original bytes\n";
            auto renamed = directory.path / L"renamed.md";
            Apply(Kind::Rename, first, renamed);
            expect(!std::filesystem::exists(first));
            expect(Bytes(renamed) == "original bytes\n");
            auto moved = folder / L"renamed.md";
            Apply(Kind::Move, renamed, moved);
            auto sameFolder = Apply(Kind::Copy, moved, moved);
            expect(sameFolder != moved && Bytes(sameFolder) == Bytes(moved));
            auto copied = directory.path / L"copied.md";
            Apply(Kind::Copy, moved, copied);
            expect(Bytes(copied) == Bytes(moved));
            expect(Rejected(Kind::NewFile, {}, copied));
            expect(Rejected(Kind::Move, moved, copied));
            expect(Rejected(Kind::Copy, moved, copied));
            expect(std::filesystem::exists(moved));
            expect(Bytes(copied) == "original bytes\n");
            auto besideFolder = Apply(Kind::Copy, folder, folder, true);
            expect(besideFolder != folder && Bytes(besideFolder / L"renamed.md") == Bytes(moved));
            auto folderCopy = directory.path / L"folder-copy";
            Apply(Kind::Copy, folder, folderCopy, true);
            expect(Bytes(folderCopy / L"renamed.md") == Bytes(moved));
            expect(Rejected(Kind::Copy, folder, folder / L"inside", true));
            expect(!std::filesystem::exists(folder / L"inside"));
            expect(Rejected(Kind::Rename, folder, folder / L"inside", true));
            auto caseOnly = directory.path / L"FOLDER";
            Apply(Kind::Rename, folder, caseOnly, true);
            expect(Bytes(caseOnly / L"renamed.md") == "original bytes\n");
            Apply(Kind::Rename, caseOnly, folder, true);
            auto before = std::distance(std::filesystem::directory_iterator(directory.path), std::filesystem::directory_iterator{});
            expect(Rejected(Kind::Copy, folder, folderCopy, true));
            auto after = std::distance(std::filesystem::directory_iterator(directory.path), std::filesystem::directory_iterator{});
            expect(before == after);
        };
        "workspace file copy reports byte progress off the UI thread and cleans failed staging"_test = [] {
            TestDirectory directory;
            auto source = directory.path / L"source.bin", target = directory.path / L"target.bin";
            std::ofstream output(source, std::ios::binary);
            std::string bytes(1024 * 1024, 'x');
            output << bytes << bytes << bytes << bytes;
            output.close();
            auto uiThread = ::GetCurrentThreadId();
            std::atomic<int> reports = 0;
            std::atomic<bool> wrongThread = false, invalidProgress = false;
            Await(ApplyWorkspaceFileActionAsync({Kind::Copy, source, target, false}, [&](WorkspaceFileProgress const& progress) {
                ++reports;
                if (::GetCurrentThreadId() == uiThread) wrongThread = true;
                if (progress.path != source || progress.total != 4 * 1024 * 1024 || progress.completed > progress.total) invalidProgress = true;
            }));
            expect(reports.load() > 0_i);
            expect(!wrongThread.load());
            expect(!invalidProgress.load());
            expect(Bytes(source) == Bytes(target));
            auto failed = directory.path / L"failed.bin";
            bool rejected = false;
            try { Await(ApplyWorkspaceFileActionAsync({Kind::Copy, source, failed, false}, [](auto const&) { throw std::runtime_error("copy progress callback stopped"); })); }
            catch (std::exception const&) { rejected = true; }
            catch (winrt::hresult_error const&) { rejected = true; }
            expect(rejected);
            expect(!std::filesystem::exists(failed));
            int remaining = 0;
            for (auto const& item : std::filesystem::directory_iterator(directory.path)) if (item.path().filename().native().starts_with(L".folia-copy-")) ++remaining;
            expect(remaining == 0_i);
        };
        "workspace file relocation projects only moved paths and keeps directory boundaries"_test = [] {
            WorkspaceFileAction rename{Kind::Rename, L"D:/work/project", L"D:/work/renamed", true};
            auto nested = RelocatedWorkspacePath(rename, L"d:/WORK/project/章节/notes.md");
            expect(nested.has_value());
            expect(nested->lexically_normal() == std::filesystem::path(L"D:/work/renamed/章节/notes.md").lexically_normal());
            expect(!RelocatedWorkspacePath(rename, L"D:/work/project-other/notes.md"));
            rename.kind = Kind::Copy;
            expect(!RelocatedWorkspacePath(rename, L"D:/work/project/notes.md"));
            rename = {Kind::Move, L"D:/work/a.md", L"D:/work/b.md", false};
            expect(RelocatedWorkspacePath(rename, L"D:/WORK/a.md") == std::filesystem::path(L"D:/work/b.md"));
        };
    };
}
