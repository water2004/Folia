#pragma once
import std;

namespace winrt::Folia::records {
    inline std::string NewId() {
        GUID id{};
        check_hresult(CoCreateGuid(&id));
        wchar_t text[39]{};
        StringFromGUID2(id, text, 39);
        std::string result;
        for (auto c : std::wstring_view(text))
            if (c != L'{' && c != L'}' && c != L'-') result += static_cast<char>(std::towlower(c));
        return result;
    }
}
