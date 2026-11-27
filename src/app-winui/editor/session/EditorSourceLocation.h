#pragma once
#include <cstddef>

namespace winrt::Folia {
    struct EditorSourceLocation {
        enum class Kind { Document, Line, Utf8Byte };
        Kind kind = Kind::Document;
        std::size_t value = 0;
    };
}
