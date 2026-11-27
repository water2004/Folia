#include "pch.h"
#include "agent/ChatMotion.h"
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.System.h>
#include "agent/ChatComposerExtras.h"
#include "ui/ControlHelpers.h"
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <wil/resource.h>

namespace winrt::Folia::chat {
    using namespace ui;
    using namespace Windows::ApplicationModel::DataTransfer;
    using namespace Windows::Storage::Streams;
    using Windows::System::VirtualKey;
    namespace imaging = Windows::Graphics::Imaging;

    namespace {
        constexpr std::pair<std::string_view, wchar_t const*> commands[] = {
            {"/new", L"ChatNew"}, {"/model", L"ChatPickModel"}, {"/access", L"ChatAccess"}, {"/latest", L"ChatLatest"},
            {"/compact", L"ChatCompactCommand"}
        };

        bool CanStartMention(wchar_t previous) {
            // Allow mentions beside CJK text and punctuation, while leaving ordinary email addresses alone.
            return previous > 127 || (!iswalnum(previous) && std::wstring_view(L"._+-@").find(previous) == std::wstring_view::npos);
        }

        hstring MentionToken(std::string const& name, bool directory = false) {
            auto label = to_hstring(name);
            if (std::wstring_view(label).find_first_of(L" \t@") != std::wstring_view::npos)
                return L"@\"" + label + (directory ? L"" : L"\"");
            return L"@" + label;
        }
    }

    std::shared_ptr<ChatComposerExtras> ChatComposerExtras::Create(TextBox input, TabList tabs,
        std::function<std::filesystem::path()> folder, std::function<void(hstring const&)> report) {
        auto self = std::make_shared<ChatComposerExtras>();
        self->input_ = input;
        self->tabs_ = std::move(tabs);
        self->folder_ = std::move(folder);
        self->report_ = std::move(report);
        auto weak = self->weak_from_this();
        self->list_.Name(L"ChatMentionList");
        self->list_.MaxHeight(168);
        self->list_.IsItemClickEnabled(true);
        self->list_.SingleSelectionFollowsFocus(false);
        self->list_.ItemContainerStyle(ui::ResourceStyle(L"ChatModelPickerItemStyle"));
        self->list_.ItemClick([weak](auto const&, ItemClickEventArgs const& args) {
            if (auto view = weak.lock()) {
                std::uint32_t i{};
                if (view->list_.Items().IndexOf(args.ClickedItem(), i)) view->Accept(i);
            }
        });
        self->list_.Visibility(Visibility::Collapsed);
        self->thumbnails_.Orientation(Orientation::Horizontal);
        self->thumbnails_.Spacing(4);
        self->panel_.Children().Append(self->list_);
        self->panel_.Children().Append(self->thumbnails_);
        input.SelectionChanged([weak](auto const&, auto const&) { if (auto view = weak.lock()) view->Update(); });
        input.TextChanged([weak](auto const&, auto const&) { if (auto view = weak.lock()) view->Update(); });
        input.Paste([weak](auto const&, auto const& args) {
            auto view = weak.lock();
            if (!view) return;
            try {
                auto data = Clipboard::GetContent();
                if (data.Contains(StandardDataFormats::Bitmap()) ||
                    (data.Contains(StandardDataFormats::StorageItems()) && !data.Contains(StandardDataFormats::Text()))) {
                    args.Handled(true);
                    view->PasteImages(data);
                }
            } catch (hresult_error const& error) { view->report_(error.message()); }
        });
        return self;
    }

    void ChatComposerExtras::SetVision(bool value) { vision_ = value; }
    void ChatComposerExtras::BeginMention() {
        input_.Focus(FocusState::Programmatic);
        auto text = input_.Text();
        auto start = input_.SelectionStart();
        if (tokenStart_ >= 0 && std::wstring_view(text).substr(tokenStart_, 1) == L"@") {
            dismissed_ = {};
            Update();
            return;
        }
        input_.SelectedText(start && !CanStartMention(std::wstring_view(text)[start - 1]) ? L" @" : L"@");
        auto end = input_.SelectionStart() + input_.SelectionLength();
        input_.Select(end, 0);
        Update();
    }
    void ChatComposerExtras::Update() {
        if (accepting_) return;
        auto text = input_.Text();
        if (!dismissed_.empty() && text == dismissed_) return;
        dismissed_ = {};
        auto pos = input_.SelectionStart();
        std::wstring_view before(text.c_str(), (std::min)(static_cast<std::size_t>(pos), std::wstring_view(text).size()));
        if (command && before.starts_with(L"/") && before.size() == text.size() &&
            before.find_first_of(L" \t\r\n", 1) == before.npos && !input_.SelectionLength()) {
            lookup_.request_stop();
            choices_.clear();
            tokenStart_ = 0;
            caret_ = pos;
            auto prefix = to_string(hstring(before));
            for (auto [name, label] : commands) {
                if (std::string_view(name).starts_with(prefix)) choices_.push_back({std::string(name), 0, label});
            }
            PaintChoices();
            return;
        }
        auto at = before.find_last_of(L"\r\n");
        auto lineStart = at == before.npos ? 0 : at + 1;
        at = before.npos;
        bool quoted = false;
        for (auto i = lineStart; i < before.size(); ++i) {
            if (quoted) {
                if (before[i] == L'"') { quoted = false; at = before.npos; }
            } else if (before[i] == L'@' && (!i || CanStartMention(before[i - 1]))) {
                at = i;
                if (i + 1 < before.size() && before[i + 1] == L'"') { quoted = true; ++i; }
            } else if (iswspace(before[i])) at = before.npos;
        }
        lookup_.request_stop();
        choices_.clear();
        tokenStart_ = -1;
        if (input_.SelectionLength() || at == before.npos) {
            list_.Visibility(Visibility::Collapsed);
            return;
        }
        tokenStart_ = static_cast<int>(at);
        caret_ = pos;
        auto query = hstring(before.substr(at + (quoted ? 2 : 1)));
        for (auto const& tab : tabs_()) {
            auto title = to_hstring(tab.title);
            if (tab.read && (query.empty() || FindStringOrdinal(FIND_FROMSTART, title.c_str(), title.size(),
                query.c_str(), query.size(), TRUE) >= 0)) choices_.push_back({tab.title, tab.documentId});
            if (choices_.size() == 8) break;
        }
        PaintChoices();
        auto folder = folder_();
        if (!folder.empty()) {
            lookup_ = {};
            FindFiles(std::move(folder), to_string(query), lookup_.get_token());
        }
    }
    fire_and_forget ChatComposerExtras::FindFiles(std::filesystem::path folder, std::string query, std::stop_token stop) {
        auto weak = weak_from_this();
        apartment_context ui;
        co_await resume_background();
        std::vector<std::string> paths;
        try { paths = FindWorkspaceMentions(folder, query, stop); }
        catch (...) { co_return; }
        co_await ui;
        if (auto view = weak.lock(); view && !stop.stop_requested()) {
            for (auto& path : paths) {
                view->choices_.push_back({std::move(path)});
                if (view->choices_.size() >= 16) break;
            }
            view->PaintChoices(true);
        }
    }
    void ChatComposerExtras::PaintChoices(bool keepSelection) {
        auto selected = keepSelection ? list_.SelectedIndex() : 0;
        list_.Items().Clear();
        for (auto const& choice : choices_) {
            auto item = to_hstring(choice.title) + L"   ·   " + Localize(choice.commandLabel ? choice.commandLabel : choice.tab ? L"ChatOpenTab" : L"ChatWorkspace");
            list_.Items().Append(box_value(item));
        }
        list_.SelectedIndex(choices_.empty() ? -1 : (std::clamp)(selected, 0, static_cast<int>(choices_.size()) - 1));
        const bool appearing = list_.Visibility() != Visibility::Visible && !choices_.empty();
        list_.Visibility(choices_.empty() ? Visibility::Collapsed : Visibility::Visible);
        if (appearing) Reveal(list_, 0, 5);
    }
    void ChatComposerExtras::Accept(int index) {
        if (index < 0 || !std::cmp_less(index, choices_.size()) || tokenStart_ < 0) return;
        auto choice = choices_.at(index);
        if (choice.commandLabel) {
            input_.Text({});
            list_.Visibility(Visibility::Collapsed);
            command(choice.title);
            return;
        }
        bool folder = !choice.tab && choice.title.ends_with('/');
        auto token = MentionToken(choice.title, folder);
        if (!folder) token = token + L" ";
        accepting_ = true;
        auto start = tokenStart_;
        input_.Select(start, caret_ - start);
        input_.SelectedText(token);
        input_.Select(start + token.size(), 0);
        if (choice.tab) mentions_.push_back(std::move(choice));
        accepting_ = false;
        input_.Focus(FocusState::Programmatic);
        Update();
    }
    bool ChatComposerExtras::Mentions(OpenTab const& tab) const {
        return std::ranges::any_of(mentions_, [&](Choice const& mention) {
            if (mention.tab != tab.documentId) return false;
            auto text = input_.Text();
            std::wstring_view source(text);
            auto token = MentionToken(mention.title);
            for (auto pos = source.find(token); pos != source.npos; pos = source.find(token, pos + 1)) {
                auto end = pos + token.size();
                if ((!pos || CanStartMention(source[pos - 1])) &&
                    (end == source.size() || iswspace(source[end]) || std::wstring_view(L",;:!?，。；：！？、）)").find(source[end]) != std::wstring_view::npos ||
                     (std::wstring_view(token).back() == L'"' && source[end] > 127))) return true;
            }
            return false;
        });
    }
    bool ChatComposerExtras::RunTypedCommand() {
        if (!command) return false;
        auto value = to_string(input_.Text());
        auto match = std::ranges::find(commands, value, &std::pair<std::string_view, wchar_t const*>::first);
        if (match == std::end(commands)) return false;
        input_.Text({});
        list_.Visibility(Visibility::Collapsed);
        command(value);
        return true;
    }
    bool ChatComposerExtras::HandleKey(VirtualKey key) {
        if (key == VirtualKey::Enter && RunTypedCommand()) return true;
        if ((key == VirtualKey::Enter || key == VirtualKey::Tab) && list_.Visibility() == Visibility::Collapsed)
            Update();
        if (list_.Visibility() != Visibility::Visible) return false;
        if (key == VirtualKey::Escape) { dismissed_ = input_.Text(); lookup_.request_stop(); list_.Visibility(Visibility::Collapsed); return true; }
        if (key == VirtualKey::Tab || key == VirtualKey::Enter) { Accept(list_.SelectedIndex()); return true; }
        if (key != VirtualKey::Down && key != VirtualKey::Up) return false;
        auto index = (std::clamp)(list_.SelectedIndex() + (key == VirtualKey::Down ? 1 : -1), 0, static_cast<int>(choices_.size()) - 1);
        list_.SelectedIndex(index);
        list_.ScrollIntoView(list_.SelectedItem());
        return true;
    }
    void ChatComposerExtras::Clear() {
        lookup_.request_stop();
        mentions_.clear(); images_.clear(); previews_.clear(); choices_.clear();
        list_.Visibility(Visibility::Collapsed);
        PaintImages();
    }
    void ChatComposerExtras::PaintImages() {
        thumbnails_.Children().Clear();
        for (std::size_t i = 0; i < images_.size(); ++i) {
            Button remove;
            remove.Style(ui::ResourceStyle(L"ChatActionStyle"));
            remove.Padding({2,2,2,2}); remove.Height(46);
            StackPanel row;
            row.Orientation(Orientation::Horizontal);
            row.Spacing(3);
            Image picture; picture.Width(48); picture.Height(40); picture.Source(previews_[i]);
            row.Children().Append(picture);
            FontIcon x; x.Glyph(L"\uE711"); x.FontSize(10); row.Children().Append(x);
            remove.Content(row);
            Automation::AutomationProperties::SetName(remove, Localize(L"ChatRemoveImage"));
            ToolTipService::SetToolTip(remove, box_value(Localize(L"ChatRemoveImage")));
            remove.Click([weak = weak_from_this(), i](auto const&, auto const&) {
                if (auto view = weak.lock(); view && i < view->images_.size()) {
                    view->images_.erase(view->images_.begin() + i);
                    view->previews_.erase(view->previews_.begin() + i);
                    view->PaintImages();
                }
            });
            thumbnails_.Children().Append(remove);
        }
    }
    fire_and_forget ChatComposerExtras::PasteImages(DataPackageView data) {
        auto owner = shared_from_this();
        if (sending_ || pasting_) co_return;
        if (!vision_) { report_(Localize(L"ChatNeedsVision")); co_return; }
        pasting_ = true;
        auto done = wil::scope_exit([&] { pasting_ = false; });
        try {
            std::vector<IRandomAccessStream> streams;
            if (data.Contains(StandardDataFormats::Bitmap())) {
                auto image = co_await data.GetBitmapAsync();
                streams.push_back(co_await image.OpenReadAsync());
            } else {
                for (auto const& item : co_await data.GetStorageItemsAsync()) {
                    if (auto file = item.try_as<Windows::Storage::StorageFile>(); file && file.ContentType().starts_with(L"image/"))
                        streams.push_back(co_await file.OpenReadAsync());
                    if (streams.size() > 4) break;
                }
            }
            if (images_.size() + streams.size() > 4) { report_(Localize(L"ChatImageLimit")); co_return; }
            for (auto const& input : streams) {
                auto decoder = co_await imaging::BitmapDecoder::CreateAsync(input);
                auto width = decoder.PixelWidth(), height = decoder.PixelHeight();
                auto longest = (std::max)(width, height);
                if (!width || !height || longest > 32768) throw hresult_invalid_argument();
                imaging::BitmapTransform transform;
                InMemoryRandomAccessStream output;
                for (auto edge = (std::min)(longest, 1536u);; edge = (std::max)(1u, edge * 3 / 4)) {
                    transform.ScaledWidth((std::max)(1u, width * edge / longest));
                    transform.ScaledHeight((std::max)(1u, height * edge / longest));
                    auto pixels = co_await decoder.GetPixelDataAsync(imaging::BitmapPixelFormat::Bgra8, imaging::BitmapAlphaMode::Straight,
                        transform, imaging::ExifOrientationMode::IgnoreExifOrientation, imaging::ColorManagementMode::ColorManageToSRgb);
                    output = InMemoryRandomAccessStream();
                    auto encoder = co_await imaging::BitmapEncoder::CreateAsync(imaging::BitmapEncoder::PngEncoderId(), output);
                    encoder.SetPixelData(imaging::BitmapPixelFormat::Bgra8, imaging::BitmapAlphaMode::Straight, transform.ScaledWidth(),
                        transform.ScaledHeight(), 96, 96, pixels.DetachPixelData());
                    co_await encoder.FlushAsync();
                    if (output.Size() <= 1536 * 1024) break;
                }
                output.Seek(0);
                auto size = static_cast<std::uint32_t>(output.Size());
                auto encoded = co_await output.ReadAsync(Buffer(size), size, InputStreamOptions::None);
                com_array<std::uint8_t> bytes;
                Windows::Security::Cryptography::CryptographicBuffer::CopyToByteArray(encoded, bytes);
                output.Seek(0);
                Media::Imaging::BitmapImage preview;
                preview.DecodePixelWidth(112);
                co_await preview.SetSourceAsync(output);
                if (!vision_ || sending_) co_return;
                images_.push_back({std::vector<std::uint8_t>(bytes.begin(), bytes.end())});
                previews_.push_back(preview);
                if (imageAdded) imageAdded();
                PaintImages();
            }
        } catch (hresult_error const& error) { report_(error.message()); }
    }
}
