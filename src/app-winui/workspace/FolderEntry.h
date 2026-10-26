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
        explicit RootCommand(std::function<void()> action) : execute(std::move(action)) {}
        bool CanExecute(IInspectable const&) const { return true; }
        void Execute(IInspectable const&) const { execute(); }
        winrt::event_token CanExecuteChanged(Windows::Foundation::EventHandler<IInspectable> const&) const noexcept { return {}; }
        void CanExecuteChanged(winrt::event_token const&) const noexcept {}
    };
    struct RootActions {
        Input::ICommand refresh{nullptr}, collapse{nullptr};
        winrt::hstring refreshName, collapseName;
    };
    enum class EntryMember { Name, FullPath, Refresh, Collapse, RefreshName, CollapseName };

    // Immutable display properties use WinUI's normal COM binding contract.
    // Enumeration/cancellation state stays native and is never exposed to XAML.
    struct FolderEntry : winrt::implements<FolderEntry, Windows::Foundation::IStringable, Data::ICustomPropertyProvider> {
        std::filesystem::path path;
        bool directory = false, root = false;
        std::uint64_t ticket = 0;
        bool loading = false, loaded = false;
        std::stop_source stop;
        std::unique_ptr<RootActions> actions;
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
            }
            return {};
        }
        TypeName Type() const {
            return member != EntryMember::Refresh && member != EntryMember::Collapse
                ? winrt::xaml_typename<winrt::hstring>() : winrt::xaml_typename<Input::ICommand>();
        }
        bool CanRead() const { return true; }
        bool CanWrite() const { return false; }
        IInspectable GetValue(IInspectable const& target) const {
            auto entry = winrt::get_self<FolderEntry>(target.as<Windows::Foundation::IStringable>());
            switch (member) {
            case EntryMember::Name: return winrt::box_value(entry->ToString());
            case EntryMember::FullPath: return winrt::box_value(winrt::hstring(entry->path.c_str()));
            case EntryMember::Refresh: return entry->actions ? entry->actions->refresh : nullptr;
            case EntryMember::Collapse: return entry->actions ? entry->actions->collapse : nullptr;
            case EntryMember::RefreshName: return winrt::box_value(entry->actions ? entry->actions->refreshName : winrt::hstring{});
            case EntryMember::CollapseName: return winrt::box_value(entry->actions ? entry->actions->collapseName : winrt::hstring{});
            }
            return nullptr;
        }
        void SetValue(IInspectable const&, IInspectable const&) const { throw winrt::hresult_not_implemented(); }
        IInspectable GetIndexedValue(IInspectable const&, IInspectable const&) const { throw winrt::hresult_not_implemented(); }
        void SetIndexedValue(IInspectable const&, IInspectable const&, IInspectable const&) const { throw winrt::hresult_not_implemented(); }
    };

    inline Data::ICustomProperty FolderEntry::GetCustomProperty(winrt::hstring const& name) const {
        static const std::array<Data::ICustomProperty, 6> properties = {
            winrt::make<EntryProperty>(EntryMember::Name), winrt::make<EntryProperty>(EntryMember::FullPath),
            winrt::make<EntryProperty>(EntryMember::Refresh), winrt::make<EntryProperty>(EntryMember::Collapse),
            winrt::make<EntryProperty>(EntryMember::RefreshName), winrt::make<EntryProperty>(EntryMember::CollapseName)};
        for (auto const& property : properties) if (property.Name() == name) return property;
        return nullptr;
    }
}
