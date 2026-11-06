// Mermaid 12.2.0 Git graph parser and DB. MIT; see ../LICENSE.
export module folia.mermaid.git_graph;
export import folia.mermaid.frontend;
import std;
export namespace folia::mermaid {
enum class GitCommitType {normal,reverse,highlight,merge,cherry_pick};
struct GitCommit {std::u16string id,message,branch;std::size_t seq=0;GitCommitType type=GitCommitType::normal;std::optional<GitCommitType> custom_type;bool custom_id=false,generated=false;std::vector<std::u16string> tags;std::vector<std::size_t> parents;};
struct GitBranch {std::u16string name;double order=0;std::optional<std::size_t> head;};
struct GitGraphDocument {std::u16string title,acc_title,acc_description,direction=u"LR",current;std::optional<std::size_t> head;std::vector<GitBranch> branches;std::vector<GitCommit> commits;};
std::expected<GitGraphDocument,std::string> parse_git_graph(std::u16string_view,std::u16string_view main_branch=u"main",double main_order=0);
}
