#pragma once

namespace winrt::Folia {
    struct WorkspaceBarSettings {
        bool autoHideTop = false;
        bool autoHideBottom = false;
        bool operator==(WorkspaceBarSettings const&) const = default;
    };
}
