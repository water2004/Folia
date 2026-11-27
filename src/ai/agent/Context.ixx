export module folia.ai.agent.context;
import std;
import folia.ai.agent;

export namespace folia::ai::agent {
    // A call ID remains registered after its retained receipt has been used.
    using ToolReceipts = std::map<std::string, std::string>;
    std::vector<Message> ProjectHistory(History const&, Options const&);
    bool FitContext(ModelRequest&, std::size_t& historySize, ToolReceipts&, History&, Options const&,
                    Request const&, Progress const&, std::stop_token, bool force = false);
}
