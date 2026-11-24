#pragma once

#include <d2d1_1.h>
#include <documenttarget.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <filesystem>
#include <memory>
#include <string>

namespace folia::platform
{
    struct PdfPage
    {
        ::Microsoft::WRL::ComPtr<ID2D1CommandList> commands;
        D2D_SIZE_F size{};
    };

    struct PdfPrintJob
    {
        static std::unique_ptr<PdfPrintJob> Begin(
            std::filesystem::path const& path,
            std::wstring const& title,
            ID2D1Device* device,
            IWICImagingFactory* wicFactory);

        ~PdfPrintJob();
        PdfPrintJob(PdfPrintJob const&) = delete;
        PdfPrintJob& operator=(PdfPrintJob const&) = delete;

        void AddPage(PdfPage const& page);
        void Complete();

    private:
        PdfPrintJob() = default;
        void Initialize(
            std::filesystem::path const& path,
            std::wstring const& title,
            ID2D1Device* device,
            IWICImagingFactory* wicFactory);
        void ReleaseHandles() noexcept;

        std::filesystem::path path_;
        ::Microsoft::WRL::ComPtr<IStream> output_;
        ::Microsoft::WRL::ComPtr<IPrintDocumentPackageTarget> target_;
        ::Microsoft::WRL::ComPtr<ID2D1PrintControl> printControl_;
        bool completed_ = false;
    };
}
