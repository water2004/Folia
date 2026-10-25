// Mermaid 12.2.0 Sequence grammar and SequenceDB.
// Copyright (c) 2014-2022 Knut Sveidqvist. MIT; see ../LICENSE.
export module folia.mermaid.sequence;
import std;
export import folia.mermaid.frontend;

export namespace folia::mermaid {
enum class SequenceKind {
    solid = 0, dotted = 1, note = 2, solid_cross = 3, dotted_cross = 4,
    solid_open = 5, dotted_open = 6, active_start = 17, active_end = 18,
    loop_start = 10, loop_end = 11, alt_start = 12, alt_else = 13, alt_end = 14,
    opt_start = 15, opt_end = 16, par_start = 19, par_and = 20, par_end = 21,
    rect_start = 22, rect_end = 23, critical_start = 27, critical_option = 28,
    critical_end = 29, break_start = 30, break_end = 31, par_over_start = 32,
    solid_point = 24, dotted_point = 25, autonumber = 26, bidirectional_solid = 33, bidirectional_dotted = 34
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
    std::optional<double> number_start, number_step;
    bool number_visible = false;
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
// Common participants, signals, notes, activations and nested control regions.
// Labels are owned source text; display conversion and measurement are separate.
// Unsupported statements return an error, never a partially rendered diagram.
std::expected<SequenceInput, std::string> prepare_sequence(
    std::u16string_view source, const ConfigSession& session, SequenceLimits limits = {});
}
