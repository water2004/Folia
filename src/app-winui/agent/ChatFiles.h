#pragma once
import std;

namespace winrt::Folia::chat::storage {
    extern std::mutex files;
    std::filesystem::path LogFolder();
}
