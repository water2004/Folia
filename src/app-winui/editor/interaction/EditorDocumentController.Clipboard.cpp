#include "pch.h"
#include "editor/interaction/EditorDocumentControllerState.h"
#include "storage/ClipboardImageStore.h"
#include "localization/Localization.h"

import folia.markdown.serializer;
import folia.markdown.utf;

namespace winrt::Folia {
    winrt::Windows::Foundation::IAsyncOperation<bool> EditorDocumentController::PasteAsync(
        winrt::Windows::ApplicationModel::DataTransfer::DataPackageView content) {
        return PasteClipboardAsync(state_, state_->generation.load(), std::move(content));
    }

    winrt::Windows::Foundation::IAsyncOperation<bool> EditorDocumentController::PasteClipboardAsync(
        std::shared_ptr<State> state, std::uint64_t generation,
        winrt::Windows::ApplicationModel::DataTransfer::DataPackageView content) {
        if (!Active(state, generation) || state->pasting || state->documentLoading) co_return false;
        state->pasting = true;
        struct Pending { bool& value; ~Pending() { value = false; } } pending{state->pasting};
        auto document = state->loadSerial.load();
        auto identity = state->documentSerial;
        auto revision = state->session->Revision();
        auto selection = state->session->Selection();
        auto current = [&] {
            return Active(state, generation) && state->loadSerial.load() == document
                && state->documentSerial == identity
                && state->session->Revision() == revision && state->session->Selection() == selection;
        };
        winrt::apartment_context ui;
        std::vector<winrt::Windows::Storage::StorageFile> created;
        bool inserted = false;
        try {
            using namespace winrt::Windows::ApplicationModel::DataTransfer;
            if (!content) content = Clipboard::GetContent();
            folia::Command command;
            command.kind = folia::CommandKind::Paste;
            auto bitmap = content.Contains(StandardDataFormats::Bitmap());
            std::vector<winrt::Windows::Storage::StorageFile> files;
            if (!bitmap && content.Contains(StandardDataFormats::StorageItems())) {
                auto items = co_await content.GetStorageItemsAsync();
                co_await ui;
                if (!current()) co_return false;
                for (auto const& item : items)
                    if (auto file = item.try_as<winrt::Windows::Storage::StorageFile>(); file && file.ContentType().starts_with(L"image/"))
                        files.push_back(file);
            }
            if (bitmap || !files.empty()) {
                if (!state->session->HasFile()) {
                    if (state->setStatus) state->setStatus(Localize(L"SaveBeforePastingImage"));
                    if (!co_await SaveDocumentAsync(state, state->documentGeneration.load())) co_return false;
                    co_await ui;
                    if (!current()) co_return false;
                }
                auto base = state->session->BaseDirectory();
                if (bitmap) {
                    auto image = co_await content.GetBitmapAsync();
                    auto input = co_await image.OpenReadAsync();
                    if (current()) created.push_back(co_await ClipboardImageStore::SavePngAsync(input, winrt::hstring(base)));
                } else {
                    for (auto const& file : files) {
                        if (!current()) break;
                        created.push_back(co_await ClipboardImageStore::CopyFileAsync(file, winrt::hstring(base)));
                    }
                }
                co_await ui;
                if (current() && state->session->BaseDirectory() == base)
                    for (auto const& image : created) {
                        if (!command.text.empty()) command.text += U'\n';
                        command.text += folia::serialize_image_source(folia::utf8_to_cps(winrt::to_string(image.Name())));
                    }
            } else if (content.Contains(StandardDataFormats::Text())) {
                auto text = co_await content.GetTextAsync();
                co_await ui;
                if (current()) command.text = folia::utf8_to_cps(winrt::to_string(text));
            }
            if (current() && !command.text.empty() && state->executeCommand)
                inserted = state->executeCommand(command);
            if (!inserted && Active(state, generation) && state->setStatus && !current())
                state->setStatus(Localize(L"StatusPasteContextChanged"));
        } catch (winrt::hresult_error const& error) {
            if (Active(state, generation) && state->setStatus)
                state->setStatus(LocalizeFormat(L"StatusPasteFailed", {error.message()}));
        } catch (std::exception const& error) {
            if (Active(state, generation) && state->setStatus)
                state->setStatus(LocalizeFormat(L"StatusPasteFailed", {winrt::to_hstring(error.what())}));
        }
        if (!inserted)
            for (auto const& image : created)
                try { co_await image.DeleteAsync(winrt::Windows::Storage::StorageDeleteOption::PermanentDelete); } catch (...) {}
        co_return inserted;
    }
}
