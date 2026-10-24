// Mermaid 12.2.0 Sequence grammar and SequenceDB.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see ../LICENSE.
export module folia.mermaid.sequence;
import std;
export import folia.mermaid.frontend;

export namespace folia::mermaid {
enum class SequenceKind {
    solid = 0, dotted = 1, note = 2, solid_cross = 3, dotted_cross = 4,
    solid_open = 5, dotted_open = 6, active_start = 17, active_end = 18,
    solid_point = 24, dotted_point = 25, bidirectional_solid = 33, bidirectional_dotted = 34
};
struct SequenceActor {
    std::u16string id, name, description, type = u"participant";
    bool wrap = false;
};
struct SequenceMessage {
    std::u16string id, from, to, message;
    SequenceKind type = SequenceKind::solid;
    bool wrap = false, activate = false;
    // Original LEFTOF=0, RIGHTOF=1, OVER=2; absent for signals/activations.
    int placement = -1;
};
struct SequenceInput {
    std::u16string title, acc_title, acc_description;
    std::vector<SequenceActor> actors;
    std::vector<SequenceMessage> messages;
    ConfigValue effective_config;
};
struct SequenceLimits {
    FrontendLimits frontend;
    std::size_t actors = 10000, messages = 10000;
};
// Common participant/actor declarations, aliases, signals, notes and activation.
// Labels are owned source text; display conversion and measurement are separate.
// Unsupported statements return an error, never a partially rendered diagram.
std::expected<SequenceInput, std::string> prepare_sequence(
    std::u16string_view source, const ConfigSession& session, SequenceLimits limits = {});
}
