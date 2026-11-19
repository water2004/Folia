// Mermaid 12.2.0 diagram-api/detectType.ts and diagram-orchestration.ts.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see ../LICENSE.
export module folia.mermaid.diagram_registry;
import std;
export import folia.mermaid.config_limits;

export namespace folia::mermaid {
struct DiagramRegistryLimits {
    std::size_t source_units = 4 * 1024 * 1024;
    std::size_t detectors = 1024;
    std::size_t registry_units = 64 * 1024; // Sum of UTF-16 ids and loader routes.
    ConfigLimits config;
};
// A host callback sees detection-cleaned UTF-16 text and a working configuration
// copy. Return an explicit error to stop dispatch. Captured host state and its
// lifetime remain the host's responsibility; callback side effects cannot roll back.
using DiagramDetector = std::function<std::expected<bool, std::u16string>(
    std::u16string_view, ConfigValue&)>;
struct DiagramDetectorInfo {
    std::u16string id;
    // Opaque host routing metadata. Presence corresponds to an upstream loader;
    // this library does NOT load or claim to implement that diagram renderer.
    std::optional<std::u16string> loader_route;
};
struct DiagramDetection {
    std::u16string id;
    ConfigValue config;
};
enum class DiagramDetectionErrorKind { unknown_diagram, invalid_input, detector_failure };
struct DiagramDetectionError {
    DiagramDetectionErrorKind kind;
    std::u16string message;
};
class DiagramRegistry {
public:
    explicit DiagramRegistry(DiagramRegistryLimits limits = {}) : limits_(limits) {}
    // Returns true when replacing an existing record (the upstream warning).
    // Replacement preserves key order and replaces/removes the old loader route.
    // Object.prototype names are explicitly rejected; ids otherwise use JS
    // Object.entries ordering, including canonical uint32 index keys first.
    std::expected<bool, std::string> add_detector(std::u16string_view id,
        DiagramDetector detector, std::optional<std::u16string> loader_route = std::nullopt);
    // Atomic, once per registry. The first successful call fixes the large-feature
    // switch. Returns number of replaced records; second/subsequent calls return 0.
    std::expected<std::size_t, std::string> add_builtin_diagrams(bool include_large_features = true);
    [[nodiscard]] std::vector<DiagramDetectorInfo> detectors() const;
    [[nodiscard]] std::expected<std::optional<std::u16string>, std::string>
        loader_route(std::u16string_view id) const;
    // Frontmatter fence removal -> directive removal -> detection comments ->
    // first matching detector. Input config must be a mapping (omission = {}).
    // Inputs are unchanged; success returns an owned config including ELK writes.
    // Failures expose no partial config. This is not shared frontend preprocessing.
    // Snapshots records before invoking callbacks, as Object.entries does. Changes
    // registered by a callback become visible to the next detect call only.
    [[nodiscard]] std::expected<DiagramDetection, DiagramDetectionError>
        detect(std::u16string_view text, const ConfigValue* config = nullptr) const;
private:
    struct Entry { DiagramDetectorInfo info; DiagramDetector detector; };
    // Shared immutable records preserve callback identity in dispatch snapshots.
    // Registry copies own their registration vectors but share callback handles.
    std::vector<std::shared_ptr<const Entry>> entries_;
    DiagramRegistryLimits limits_;
    std::size_t registry_units_ = 0;
    bool builtins_added_ = false;
};
}
