export module folia.ai.agent.tool_output;
import std;
import folia.ai.model;

export namespace folia::ai::agent {
    struct ToolOutput {
        std::string text;
        std::string preview;
        std::string retained;
        bool failed;
    };

    void AppendVerbatim(std::string& destination, std::string_view label, std::string_view text);
    ToolOutput FormatToolOutput(Json body);
}
