#include "pch.h"
#include "MainWindow.xaml.h"
#include "localization/Localization.h"

namespace winrt::Folia::implementation {
    winrt::Windows::Foundation::IAsyncOperation<Microsoft::UI::Xaml::Controls::ContentDialogResult>
        MainWindow::ConfirmUnsavedChangesAsync() {
        auto lifetime = get_strong();
        using namespace Microsoft::UI::Xaml::Controls;
        ContentDialog dialog;
        dialog.XamlRoot(Root().XamlRoot());
        dialog.Title(winrt::box_value(Localize(L"UnsavedChangesTitle")));
        dialog.Content(winrt::box_value(LocalizeFormat(L"UnsavedChangesMessage", {LocalizedDocumentName()})));
        dialog.PrimaryButtonText(Localize(L"Save"));
        dialog.SecondaryButtonText(Localize(L"DiscardChanges"));
        dialog.CloseButtonText(Localize(L"Cancel"));
        dialog.DefaultButton(ContentDialogButton::Primary);
        co_return co_await dialog.ShowAsync();
    }

    winrt::fire_and_forget MainWindow::ConfirmWindowCloseAsync() {
        if (closePending) co_return;
        auto lifetime = get_strong();
        closePending = true;
        try {
            if (co_await documentController.ConfirmCloseAsync()) {
                closeApproved = true;
                Close();
            }
        } catch (winrt::hresult_error const& error) {
            SetStatus(LocalizeFormat(L"StatusSaveFailed", {error.message()}));
        } catch (std::exception const& error) {
            SetStatus(LocalizeFormat(L"StatusSaveFailed", {winrt::to_hstring(error.what())}));
        }
        closePending = false;
    }
}
