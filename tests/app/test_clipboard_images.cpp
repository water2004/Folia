#include "pch.h"
#include "support/folia_test.hpp"
#include "localization/Localization.h"
#include "support/DocumentControllerFixture.h"

import folia.markdown.instrumentation;

using namespace boost::ut;
using namespace document_tests;

namespace {
    suite clipboard_image_tests = [] {
        "clipboard image paste writes a relative PNG with undo in both editor views"_test = [] {
            for (bool sourceMode : {false, true}) {
                Fixture f;
                if (sourceMode) expect(f.session.EnterSourceMode());
                auto original = f.session.TextUtf8();
                auto selection = f.session.Selection();
                auto image = Await(Image());
                auto package = Package(image);
                folia::reset_core_operation_counters();
                expect(Await(f.controller.PasteAsync(package.GetView())));
                auto counters = folia::read_core_operation_counters();
                expect(counters.full_document_parses == 0_u);
                expect(counters.full_document_serializations == 0_u);
                auto images = f.Images();
                expect(fatal(images.size() == 1_u));
                auto source = f.session.TextUtf8();
                expect(source.find(images[0].filename().string()) != std::string::npos);
                expect(source.find(winrt::to_string(winrt::hstring(f.directory.path.wstring()))) == std::string::npos);
                expect(f.controller.HasUnsavedChanges());
                auto file = Await(StorageFile::GetFileFromPathAsync(images[0].wstring()));
                auto stream = Await(file.OpenReadAsync());
                auto decoder = Await(winrt::Windows::Graphics::Imaging::BitmapDecoder::CreateAsync(stream));
                expect(decoder.PixelWidth() == 2_u);
                expect(decoder.PixelHeight() == 3_u);
                auto pixels = Await(decoder.GetPixelDataAsync()).DetachPixelData();
                expect(pixels.size() == 24_u);
                expect(pixels[0] == 32_u && pixels[1] == 64_u && pixels[2] == 128_u && pixels[3] == 255_u);
                expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
                expect(f.session.TextUtf8() == original);
                expect(f.session.Selection() == selection);
                expect(!f.controller.HasUnsavedChanges());
                expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Redo}));
                expect(f.session.TextUtf8() == source);
                expect(std::filesystem::exists(images[0]));
                expect(!f.wrongThread);
            }
        };
        "clipboard image paste requires an untitled document to be saved first"_test = [] {
            Fixture f(false);
            auto image = Await(Image());
            auto package = Package(image);
            auto before = f.session.TextUtf8();
            expect(!Await(f.controller.PasteAsync(package.GetView())));
            expect(f.pickers == 1);
            expect(!f.session.HasFile());
            expect(f.session.TextUtf8() == before);
            expect(f.Images().empty());
            f.saveTarget = f.File(L"first save.md");
            expect(Await(f.controller.PasteAsync(package.GetView())));
            expect(f.session.HasFile());
            expect(f.DiskText() == before);
            expect(f.controller.HasUnsavedChanges()); // image insertion follows the initial save
            expect(f.Images().size() == 1_u);
            expect(Await(f.controller.PasteAsync(package.GetView())));
            auto images = f.Images();
            expect(images.size() == 2_u);
            expect(images[0] != images[1]);
        };
        "clipboard image paste leaves no files or edits for an invalid bitmap"_test = [] {
            Fixture f;
            winrt::Windows::Storage::Streams::InMemoryRandomAccessStream invalid;
            auto package = Package(invalid);
            auto before = f.session.TextUtf8();
            expect(!Await(f.controller.PasteAsync(package.GetView())));
            expect(f.Images().empty());
            expect(f.session.TextUtf8() == before);
            expect(!f.controller.HasUnsavedChanges());
            expect(!f.status.empty());
            expect(!f.wrongThread);
        };
        "clipboard image paste cancels a stale selection before creating a file"_test = [] {
            using namespace winrt::Windows::ApplicationModel::DataTransfer;
            Fixture f;
            auto image = Await(Image());
            DataPackage package;
            DataProviderRequest request{nullptr};
            DataProviderDeferral deferral{nullptr};
            package.SetDataProvider(StandardDataFormats::Bitmap(), [&](auto const& value) {
                request = value;
                deferral = request.GetDeferral();
            });
            auto pending = f.controller.PasteAsync(package.GetView());
            Pump([&] { return request != nullptr; });
            f.Type(U"typed meanwhile");
            auto before = f.session.TextUtf8();
            request.SetData(winrt::Windows::Storage::Streams::RandomAccessStreamReference::CreateFromStream(image));
            deferral.Complete();
            expect(!Await(pending));
            expect(f.session.TextUtf8() == before);
            expect(f.Images().empty());
        };
        "clipboard text paste still uses the normal reversible editing command"_test = [] {
            Fixture f;
            winrt::Windows::ApplicationModel::DataTransfer::DataPackage package;
            package.SetText(L"中文 **paste**");
            auto before = f.session.TextUtf8();
            expect(Await(f.controller.PasteAsync(package.GetView())));
            expect(f.session.TextUtf8() != before);
            expect(f.Images().empty());
            expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(f.session.TextUtf8() == before);
        };
        "clipboard image files are copied beside the document as one reversible paste"_test = [] {
            using namespace winrt::Windows::Storage;
            Fixture f;
            auto image = Await(Image());
            auto original = f.File(L"copied image.png");
            auto output = Await(original.OpenAsync(FileAccessMode::ReadWrite));
            Await(Streams::RandomAccessStream::CopyAsync(image, output));
            Await(output.FlushAsync());
            output.Close();
            auto second = f.File(L"another.png");
            image.Seek(0);
            output = Await(second.OpenAsync(FileAccessMode::ReadWrite));
            Await(Streams::RandomAccessStream::CopyAsync(image, output));
            Await(output.FlushAsync());
            output.Close();
            winrt::Windows::ApplicationModel::DataTransfer::DataPackage package;
            package.SetStorageItems(winrt::single_threaded_vector<IStorageItem>({original, second}));
            auto before = f.session.TextUtf8();
            expect(Await(f.controller.PasteAsync(package.GetView())));
            expect(f.Images().size() == 2_u);
            auto after = f.session.TextUtf8();
            for (auto const& path : f.Images()) {
                expect(after.find(path.filename().string()) != std::string::npos);
                expect(std::filesystem::file_size(path) == std::filesystem::file_size(std::filesystem::path(original.Path().c_str())));
            }
            expect(f.session.ExecuteCommand(folia::Command{folia::CommandKind::Undo}));
            expect(f.session.TextUtf8() == before);
            expect(!f.controller.HasUnsavedChanges());
            expect(std::filesystem::exists(std::filesystem::path(original.Path().c_str())));
        };
    };
}
