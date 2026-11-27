#pragma once

#include "settings/storage/ModelCatalog.h"
#include "settings/storage/ProviderPresets.h"
#include "windows/completion/CompletionHttpTransport.h"

namespace winrt::Folia::settings_ui {
    class ProviderModelForm : public std::enable_shared_from_this<ProviderModelForm> {
    public:
        using Save = std::function<void(models::ConfiguredModel)>;
        using Command = std::function<void()>;
        static std::shared_ptr<ProviderModelForm> Create(models::ConfiguredModel, models::ProviderRoute const&,
            Save save, Command cancel, Command remove, Command fetch);
        Microsoft::UI::Xaml::UIElement Element() const { return panel_; }
        void QueryStatus(winrt::hstring const& text, bool busy = false, bool error = false);
        void SetCatalog(std::vector<folia::platform::completion::CompletionModel>);

    private:
        ProviderModelForm(models::ConfiguredModel, models::ProviderRoute const&);
        void Build(Save, Command cancel, Command remove, Command fetch);
        void Fill(models::ConfiguredModel const&);
        void AddEffort(std::string_view level, bool checked);
        std::vector<std::string> CheckedEfforts() const;
        void RefreshDefault(std::string_view selection);
        void Apply(Save const&);

        models::ConfiguredModel draft_;
        models::ProviderRoute const& route_;
        std::vector<folia::platform::completion::CompletionModel> catalog_;
        std::vector<std::pair<std::string, Microsoft::UI::Xaml::Controls::CheckBox>> levels_;
        Microsoft::UI::Xaml::Controls::StackPanel panel_{nullptr};
        Microsoft::UI::Xaml::Controls::VariableSizedWrapGrid efforts_;
        Microsoft::UI::Xaml::Controls::TextBox name_, upstream_, context_, output_;
        Microsoft::UI::Xaml::Controls::ComboBox choices_{nullptr}, defaultEffort_{nullptr}, searchFormat_{nullptr};
        Microsoft::UI::Xaml::Controls::ToggleSwitch vision_, search_;
        Microsoft::UI::Xaml::Controls::Button fetch_;
        Microsoft::UI::Xaml::Controls::TextBlock queryStatus_, validation_;
    };
}
