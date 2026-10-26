#include "pch.h"
import std;
#include "MainWindow.xaml.h"

namespace winrt::Folia::implementation {
    namespace {
        bool IsApplicationAction(std::string_view action) {
            static constexpr std::array actions{
                "file.new", "file.open", "file.open_folder", "file.save", "file.save_as", "file.close",
                "file.export_pdf", "file.reload", "search.find", "search.replace", "insert.image",
                "view.source_mode", "view.next_tab", "view.previous_tab"};
            return std::ranges::find(actions, action) != actions.end();
        }
    }

    bool MainWindow::DispatchApplicationAction(std::string_view action) {
        if (!IsApplicationAction(action)) return false;
        if (closePending || closingDocument || windowClosed) return true;
        if (action == "file.new") NewDocument();
        else if (action == "file.open") PickWorkspaceFile();
        else if (action == "file.open_folder") PickWorkspaceFolder();
        else if (action == "file.close") CloseDocumentAsync(workspaceDocuments.Active()->id);
        else if (action == "file.save") documentController().SaveDocument();
        else if (action == "file.save_as") documentController().SaveDocumentAs();
        else if (action == "file.export_pdf") documentController().ExportPdf();
        else if (action == "file.reload") documentController().ReloadDocument();
        else if (action == "search.find") ShowFindBar(false);
        else if (action == "search.replace") ShowFindBar(true);
        else if (action == "insert.image") documentController().InsertImage();
        else if (action == "view.source_mode") ToggleSourceMode();
        else if (action == "view.next_tab" || action == "view.previous_tab") {
            auto document = workspaceDocuments.Adjacent(workspaceDocuments.Active()->id, action == "view.next_tab" ? 1 : -1);
            if (document) ActivateDocument(document->id);
        }
        return true;
    }

    void MainWindow::RegisterApplicationShortcuts() {
        auto collection = Root().KeyboardAccelerators();
        for (auto const& accelerator : applicationShortcuts) {
            std::uint32_t index = 0;
            if (collection.IndexOf(accelerator, index)) collection.RemoveAt(index);
        }
        applicationShortcuts.clear();
        using namespace folia::platform::editor;
        for (auto const& binding : appSettings.shortcutBindings) {
            if (binding.action_kind != EditorShortcutActionKind::BuiltIn || binding.scope != EditorShortcutScope::Global
                || !binding.gesture || !IsApplicationAction(binding.action_id)) continue;
            Microsoft::UI::Xaml::Input::KeyboardAccelerator accelerator;
            accelerator.Key(static_cast<Windows::System::VirtualKey>(binding.gesture->key));
            auto modifiers = Windows::System::VirtualKeyModifiers::None;
            if (binding.gesture->control) modifiers |= Windows::System::VirtualKeyModifiers::Control;
            if (binding.gesture->shift) modifiers |= Windows::System::VirtualKeyModifiers::Shift;
            if (binding.gesture->alt) modifiers |= Windows::System::VirtualKeyModifiers::Menu;
            accelerator.Modifiers(modifiers);
            accelerator.Invoked([weak = get_weak(), action = binding.action_id](auto const&, auto const& args) {
                if (auto window = weak.get()) args.Handled(window->DispatchApplicationAction(action));
            });
            collection.Append(accelerator);
            applicationShortcuts.push_back(accelerator);
        }
    }
}
