#include "pch.h"
#include "MainWindow.xaml.h"
#include "localization/Localization.h"

namespace winrt::Folia::implementation {
    winrt::Windows::Foundation::IAsyncOperation<Microsoft::UI::Xaml::Controls::ContentDialogResult>
        MainWindow::ConfirmUnsavedChangesAsync(std::uint64_t id) {
        auto lifetime = get_strong();
        auto document = workspaceDocuments.Find(id);
        if (!document || windowClosed) co_return Microsoft::UI::Xaml::Controls::ContentDialogResult::None;
        using namespace Microsoft::UI::Xaml::Controls;
        ContentDialog dialog;
        dialog.XamlRoot(Root().XamlRoot());
        dialog.Title(winrt::box_value(Localize(L"UnsavedChangesTitle")));
        auto name = document->session.HasFile() ? document->session.DisplayName() : Localize(L"UntitledMarkdown");
        dialog.Content(winrt::box_value(LocalizeFormat(L"UnsavedChangesMessage", {name})));
        dialog.PrimaryButtonText(Localize(L"Save"));
        dialog.SecondaryButtonText(Localize(L"DiscardChanges"));
        dialog.CloseButtonText(Localize(L"Cancel"));
        dialog.DefaultButton(ContentDialogButton::Primary);
        co_return co_await dialog.ShowAsync();
    }

    winrt::fire_and_forget MainWindow::ConfirmWindowCloseAsync() {
        if (closePending || closingDocument || windowClosed) co_return;
        auto lifetime = get_strong();
        closePending = true;
        UpdateLoadingState();
        try {
            textInputController.FocusLeave();
            std::vector<std::shared_ptr<winrt::Folia::WorkspaceDocument>> documents(workspaceDocuments.Items().begin(), workspaceDocuments.Items().end());
            bool approved = true;
            for (auto const& document : documents)
                if (!co_await document->controller.ConfirmCloseAsync()) { approved = false; break; }
            if (approved) { closeApproved = true; Close(); }
        } catch (winrt::hresult_error const& error) {
            SetStatus(LocalizeFormat(L"StatusSaveFailed", {error.message()}));
        } catch (std::exception const& error) {
            SetStatus(LocalizeFormat(L"StatusSaveFailed", {winrt::to_hstring(error.what())}));
        }
        closePending = false;
        if (!windowClosed) UpdateLoadingState();
    }
}
