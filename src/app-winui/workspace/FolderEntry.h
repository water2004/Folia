#pragma once
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>

namespace winrt::Folia::explorer_detail {
    using Windows::Foundation::IInspectable;
    using Windows::UI::Xaml::Interop::TypeName;
    namespace Data = Microsoft::UI::Xaml::Data;
    namespace Input = Microsoft::UI::Xaml::Input;

    // Plain commands leave icon, tooltip and accessible-name presentation to
    // the row template instead of XamlUICommand's automatic content replacement.
    struct RootCommand : winrt::implements<RootCommand, Input::ICommand> {
        std::function<void()> execute;
        std::function<bool()> enabled;
        winrt::event<Windows::Foundation::EventHandler<IInspectable>> changed;
        explicit RootCommand(std::function<void()> action, std::function<bool()> canExecute = {})
            : execute(std::move(action)), enabled(std::move(canExecute)) {}
        bool CanExecute(IInspectable const&) const { return !enabled || enabled(); }
        void Execute(IInspectable const&) const { if (CanExecute(nullptr)) execute(); }
        winrt::event_token CanExecuteChanged(Windows::Foundation::EventHandler<IInspectable> const& handler) { return changed.add(handler); }
        void CanExecuteChanged(winrt::event_token const& token) { changed.remove(token); }
        void Notify() { changed(this->get_strong().as<Input::ICommand>(), nullptr); }
    };
    struct RootActions {
        Input::ICommand refresh{nullptr}, collapse{nullptr};
        winrt::hstring refreshName, collapseName;
        Input::ICommand newFile{nullptr}, newFolder{nullptr};
        winrt::hstring newFileName, newFolderName;
    };
    struct NameEdit {
        bool creating = false, busy = false;
        winrt::hstring name, error;
    };
    enum class EntryMember { Name, FullPath, Refresh, Collapse, RefreshName, CollapseName, Indent, NewFile, NewFolder, NewFileName, NewFolderName, EditName, EditError, EditEnabled, ErrorVisibility };

    // Display properties use WinUI's normal COM binding contract.
    // Enumeration/cancellation state stays native and is never exposed to XAML.
    struct FolderEntry : winrt::implements<FolderEntry, Windows::Foundation::IStringable, Data::ICustomPropertyProvider, Data::INotifyPropertyChanged> {
        std::filesystem::path path;
        winrt::weak_ref<Microsoft::UI::Xaml::Controls::TreeViewNode> node;
        bool directory = false, root = false;
        std::uint64_t ticket = 0;
        bool loading = false, loaded = false;
        std::stop_source stop;
        std::unique_ptr<RootActions> actions;
        std::unique_ptr<NameEdit> edit;
        Windows::Foundation::IAsyncAction pending{nullptr};
        winrt::event<Data::PropertyChangedEventHandler> changed;
        winrt::event_token PropertyChanged(Data::PropertyChangedEventHandler const& handler) { return changed.add(handler); }
        void PropertyChanged(winrt::event_token const& token) { changed.remove(token); }
        void Notify(winrt::hstring const& property) { changed(this->get_strong().as<Data::INotifyPropertyChanged>(), Data::PropertyChangedEventArgs(property)); }
        void NotifyEdit() {
            for (auto name : {L"EditName", L"EditError", L"EditEnabled", L"ErrorVisibility"}) Notify(name);
        }
        winrt::hstring ToString() const {
            auto name = path.filename();
            if (name.empty()) name = path.parent_path().filename();
            if (name.empty()) name = path.root_path();
            return winrt::hstring(name.c_str());
        }
        winrt::hstring GetStringRepresentation() const { return ToString(); }
        TypeName Type() const { return {L"Folia.FolderEntry", Windows::UI::Xaml::Interop::TypeKind::Custom}; }
        Data::ICustomProperty GetCustomProperty(winrt::hstring const& name) const;
        Data::ICustomProperty GetIndexedProperty(winrt::hstring const&, TypeName const&) const { return nullptr; }
    };

    struct EntryProperty : winrt::implements<EntryProperty, Data::ICustomProperty> {
        EntryMember member;
        explicit EntryProperty(EntryMember value) : member(value) {}
        winrt::hstring Name() const {
            switch (member) {
            case EntryMember::Name: return L"Name";
            case EntryMember::FullPath: return L"FullPath";
            case EntryMember::Refresh: return L"Refresh";
            case EntryMember::Collapse: return L"Collapse";
            case EntryMember::RefreshName: return L"RefreshName";
            case EntryMember::CollapseName: return L"CollapseName";
            case EntryMember::Indent: return L"Indent";
            case EntryMember::NewFile: return L"NewFile";
            case EntryMember::NewFolder: return L"NewFolder";
            case EntryMember::NewFileName: return L"NewFileName";
            case EntryMember::NewFolderName: return L"NewFolderName";
            case EntryMember::EditName: return L"EditName";
            case EntryMember::EditError: return L"EditError";
            case EntryMember::EditEnabled: return L"EditEnabled";
            case EntryMember::ErrorVisibility: return L"ErrorVisibility";
            }
            return {};
        }
        TypeName Type() const {
            if (member == EntryMember::Indent) return winrt::xaml_typename<Microsoft::UI::Xaml::Thickness>();
            if (member == EntryMember::ErrorVisibility)
                return winrt::xaml_typename<Microsoft::UI::Xaml::Visibility>();
            if (member == EntryMember::EditEnabled) return winrt::xaml_typename<bool>();
            return member != EntryMember::Refresh && member != EntryMember::Collapse && member != EntryMember::NewFile && member != EntryMember::NewFolder
                ? winrt::xaml_typename<winrt::hstring>() : winrt::xaml_typename<Input::ICommand>();
        }
        bool CanRead() const { return true; }
        bool CanWrite() const { return member == EntryMember::EditName; }
        IInspectable GetValue(IInspectable const& target) const {
            auto entry = winrt::get_self<FolderEntry>(target.as<Windows::Foundation::IStringable>());
            switch (member) {
            case EntryMember::Name: return winrt::box_value(entry->ToString());
            case EntryMember::FullPath: return winrt::box_value(winrt::hstring(entry->path.c_str()));
            case EntryMember::Refresh: return entry->actions ? entry->actions->refresh : nullptr;
            case EntryMember::Collapse: return entry->actions ? entry->actions->collapse : nullptr;
            case EntryMember::RefreshName: return winrt::box_value(entry->actions ? entry->actions->refreshName : winrt::hstring{});
            case EntryMember::CollapseName: return winrt::box_value(entry->actions ? entry->actions->collapseName : winrt::hstring{});
            case EntryMember::NewFile: return entry->actions ? entry->actions->newFile : nullptr;
            case EntryMember::NewFolder: return entry->actions ? entry->actions->newFolder : nullptr;
            case EntryMember::NewFileName: return winrt::box_value(entry->actions ? entry->actions->newFileName : winrt::hstring{});
            case EntryMember::NewFolderName: return winrt::box_value(entry->actions ? entry->actions->newFolderName : winrt::hstring{});
            case EntryMember::EditName: return winrt::box_value(entry->edit ? entry->edit->name : winrt::hstring{});
            case EntryMember::EditError: return winrt::box_value(entry->edit ? entry->edit->error : winrt::hstring{});
            case EntryMember::EditEnabled: return winrt::box_value(entry->edit && !entry->edit->busy);
            case EntryMember::ErrorVisibility: return winrt::box_value(entry->edit && !entry->edit->error.empty() ? Microsoft::UI::Xaml::Visibility::Visible : Microsoft::UI::Xaml::Visibility::Collapsed);
            case EntryMember::Indent: {
                auto node = entry->node.get();
                const double depth = node ? (std::max)(0, node.Depth()) : 0;
                return winrt::box_value(Microsoft::UI::Xaml::Thickness{depth * 8, 0, 0, 0});
            }
            }
            return nullptr;
        }
        void SetValue(IInspectable const& target, IInspectable const& value) const {
            if (!CanWrite()) throw winrt::hresult_not_implemented();
            auto entry = winrt::get_self<FolderEntry>(target.as<Windows::Foundation::IStringable>());
            if (!entry->edit) return;
            auto name = winrt::unbox_value<winrt::hstring>(value);
            if (entry->edit->name == name) return;
            entry->edit->name = name;
            entry->edit->error = {};
            entry->NotifyEdit();
        }
        IInspectable GetIndexedValue(IInspectable const&, IInspectable const&) const { throw winrt::hresult_not_implemented(); }
        void SetIndexedValue(IInspectable const&, IInspectable const&, IInspectable const&) const { throw winrt::hresult_not_implemented(); }
    };

    inline Data::ICustomProperty FolderEntry::GetCustomProperty(winrt::hstring const& name) const {
        static const std::array<Data::ICustomProperty, 15> properties = {
            winrt::make<EntryProperty>(EntryMember::Name), winrt::make<EntryProperty>(EntryMember::FullPath),
            winrt::make<EntryProperty>(EntryMember::Refresh), winrt::make<EntryProperty>(EntryMember::Collapse),
            winrt::make<EntryProperty>(EntryMember::RefreshName), winrt::make<EntryProperty>(EntryMember::CollapseName),
            winrt::make<EntryProperty>(EntryMember::Indent),
            winrt::make<EntryProperty>(EntryMember::NewFile), winrt::make<EntryProperty>(EntryMember::NewFolder),
            winrt::make<EntryProperty>(EntryMember::NewFileName), winrt::make<EntryProperty>(EntryMember::NewFolderName),
            winrt::make<EntryProperty>(EntryMember::EditName), winrt::make<EntryProperty>(EntryMember::EditError),
            winrt::make<EntryProperty>(EntryMember::EditEnabled), winrt::make<EntryProperty>(EntryMember::ErrorVisibility)};
        for (auto const& property : properties) if (property.Name() == name) return property;
        return nullptr;
    }
    inline FolderEntry* Entry(Microsoft::UI::Xaml::Controls::TreeViewNode const& node) {
        if (!node) return nullptr;
        auto value = node.Content().try_as<Windows::Foundation::IStringable>();
        return value ? winrt::get_self<FolderEntry>(value) : nullptr;
    }
    inline Microsoft::UI::Xaml::Controls::TreeViewNode Node(std::filesystem::path path, bool directory, bool root = false) {
        auto entry = winrt::make_self<FolderEntry>();
        entry->path = std::move(path);
        entry->directory = directory;
        entry->root = root;
        Microsoft::UI::Xaml::Controls::TreeViewNode node;
        entry->node = winrt::make_weak(node);
        node.Content(entry.as<Windows::Foundation::IStringable>());
        node.HasUnrealizedChildren(directory);
        return node;
    }

}
