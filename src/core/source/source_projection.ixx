// Derived source-view boundaries and owner maps. Unchanged block projections
// survive local edits; no source-view offset is stored in the document tree.
export module folia.core.source_projection;
import std;
import folia.markdown.ast;
import folia.markdown.block_tree;
import folia.markdown.document;
import folia.markdown.document_text;
import folia.markdown.ids;
import folia.markdown.serializer;
import folia.markdown.selection;
import folia.core.source_patch;
import folia.markdown.text_edit;

export namespace folia {

struct SourceProjectionRegion {
    std::size_t first=0;
    std::size_t last=0;
    std::size_t begin=0;
    std::size_t end=0;
    NodeId preceding{};
    NodeId following{};
};

struct SourceProjectionBlock {
    NodeId id{};
    std::size_t begin=0;
    std::size_t end=0;
    std::vector<SerializedSourceMap> owners;
};

namespace source_projection_detail {

inline std::vector<SourceProjectionBlock> blocks(
    const EditorDocument& document,const SerializedMarkdownProjection& projection,
    std::size_t first,std::size_t last,std::size_t origin) {
    std::vector<SourceProjectionBlock> result;
    std::unordered_map<std::uint64_t,std::size_t> by_owner;
    std::unordered_map<std::uint64_t,SerializedBlockRange> ranges;
    for (const auto& range:projection.blocks) ranges.emplace(range.owner.v,range);
    for (auto i=first;i<last;++i) {
        const auto& root=document.root.children[i];
        const auto found=ranges.find(root.id.v);
        if (found==ranges.end()) throw std::logic_error("source projection omitted a direct block");
        const auto index=result.size();
        result.push_back({root.id,origin+found->second.begin,origin+found->second.end,{}});
        walk_blocks(root,[&](const BlockNode& node){by_owner.emplace(node.id.v,index);});
    }
    for (const auto& owner:projection.source_maps) {
        const auto found=by_owner.find(owner.container_id.v);
        if (found==by_owner.end()) continue;
        auto map=owner;
        const auto relative_begin=result[found->second].begin-origin;
        for (auto& offset:map.source_to_serialized) offset-=relative_begin;
        result[found->second].owners.push_back(std::move(map));
    }
    return result;
}

inline std::optional<TextPosition> owner_position(
    std::span<const SerializedSourceMap> owners,std::size_t offset,TextAffinity affinity) {
    std::optional<TextPosition> before,after;
    std::size_t before_offset=0,after_offset=(std::numeric_limits<std::size_t>::max)();
    for (const auto& map:owners) {
        const auto& positions=map.source_to_serialized;
        const auto next=std::ranges::lower_bound(positions,offset);
        if (next!=positions.end() && *next==offset)
            return TextPosition{map.container_id,static_cast<std::size_t>(next-positions.begin()),affinity};
        if (next!=positions.end() && (!after || *next<after_offset)) {
            after=TextPosition{map.container_id,static_cast<std::size_t>(next-positions.begin()),affinity};
            after_offset=*next;
        }
        if (next!=positions.begin()) {
            const auto prior=next-1;
            if (!before || *prior>=before_offset) {
                before=TextPosition{map.container_id,static_cast<std::size_t>(prior-positions.begin()),affinity};
                before_offset=*prior;
            }
        }
    }
    return affinity==TextAffinity::Upstream ? (before ? before : after) : (after ? after : before);
}

inline TextPosition valid_position(const EditorDocument& document,std::optional<TextPosition> position) {
    if (position) {
        if (const auto text=document_editable_text_view(document,position->container_id)) {
            position->source_offset=(std::min)(position->source_offset,text->size());
            return *position;
        }
    }
    return first_document_text_position(document).value_or(TextPosition{});
}

} // namespace source_projection_detail

class SourceProjectionIndex {
public:
    const std::vector<SourceProjectionBlock>& blocks() const { return blocks_; }
    std::size_t size() const { return size_; }

    void reset(const EditorDocument& document,const SerializedMarkdownProjection& projection) {
        blocks_=source_projection_detail::blocks(document,projection,0,document.root.children.size(),0);
        size_=projection.text.size();
        owner_roots_.clear();
        for (const auto& block:blocks_) index_owners(block);
        rebuild_root_positions();
    }

    std::size_t block_at(std::size_t offset) const {
        if (blocks_.empty()) throw std::logic_error("source view has no block");
        const auto next=std::ranges::upper_bound(blocks_,offset,{},&SourceProjectionBlock::begin);
        return next==blocks_.begin() ? 0 : static_cast<std::size_t>(next-blocks_.begin()-1);
    }

    SourceProjectionRegion region(std::size_t first,std::size_t last) const {
        if (first>=last || last>blocks_.size()) throw std::out_of_range("source projection region");
        return {first,last,blocks_[first].begin,last==blocks_.size() ? size_ : blocks_[last-1].end,
            first ? blocks_[first-1].id : NodeId{},last<blocks_.size() ? blocks_[last].id : NodeId{}};
    }

    std::pair<std::size_t,std::size_t> current_region(
        const EditorDocument& document,const SourceProjectionRegion& region) const {
        const auto& roots=document.root.children;
        const auto first=region.preceding.v ? std::ranges::find(roots,region.preceding,&BlockNode::id) : roots.end();
        const auto last=region.following.v ? std::ranges::find(roots,region.following,&BlockNode::id) : roots.end();
        if ((region.preceding.v && first==roots.end()) || (region.following.v && last==roots.end()))
            throw std::logic_error("source region boundary was edited outside its patch");
        return {region.preceding.v ? static_cast<std::size_t>(first-roots.begin()+1) : 0,
            static_cast<std::size_t>(last-roots.begin())};
    }

    void replace(const SourceProjectionRegion& region,const EditorDocument& document,
        const SerializedMarkdownProjection& projection,std::size_t first,std::size_t last) {
        auto fresh=source_projection_detail::blocks(document,projection,first,last,region.begin);
        for (auto i=region.first;i<region.last;++i)
            for (const auto& map:blocks_[i].owners) owner_roots_.erase(map.container_id.v);
        const auto delta=static_cast<std::ptrdiff_t>(projection.text.size())
            -static_cast<std::ptrdiff_t>(region.end-region.begin);
        for (auto i=region.last;i<blocks_.size();++i) {
            blocks_[i].begin=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(blocks_[i].begin)+delta);
            blocks_[i].end=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(blocks_[i].end)+delta);
        }
        blocks_.erase(blocks_.begin()+region.first,blocks_.begin()+region.last);
        blocks_.insert(blocks_.begin()+region.first,std::make_move_iterator(fresh.begin()),std::make_move_iterator(fresh.end()));
        size_=static_cast<std::size_t>(static_cast<std::ptrdiff_t>(size_)+delta);
        for (auto i=region.first;i<region.first+fresh.size();++i) index_owners(blocks_[i]);
        rebuild_root_positions();
    }

    SourceViewAnchor anchor(std::size_t offset,TextAffinity affinity) const {
        const auto& block=blocks_[block_at(offset)];
        return {block.id,static_cast<std::ptrdiff_t>(offset)-static_cast<std::ptrdiff_t>(block.begin),affinity};
    }

    SourceViewSelection capture(SourceSelection selection) const {
        return {anchor(selection.anchor,selection.anchor_affinity),anchor(selection.active,selection.active_affinity)};
    }

    std::size_t offset(SourceViewAnchor anchor) const {
        const auto found=root_positions_.find(anchor.block.v);
        if (found==root_positions_.end()) throw std::logic_error("history source anchor is absent");
        return static_cast<std::size_t>((std::clamp)(
            static_cast<std::ptrdiff_t>(blocks_[found->second].begin)+anchor.offset,
            std::ptrdiff_t{},static_cast<std::ptrdiff_t>(size_)));
    }

    SourceSelection restore(SourceViewSelection selection) const {
        return {offset(selection.anchor),offset(selection.active),selection.anchor.affinity,selection.active.affinity};
    }

    std::optional<std::size_t> offset(TextPosition position) const {
        const auto found=owner_roots_.find(position.container_id.v);
        if (found==owner_roots_.end()) return std::nullopt;
        const auto& block=blocks_[root_positions_.at(found->second.v)];
        for (const auto& map:block.owners) {
            if (map.container_id!=position.container_id || map.source_to_serialized.empty()) continue;
            return block.begin+map.source_to_serialized[(std::min)(position.source_offset,map.source_to_serialized.size()-1)];
        }
        return std::nullopt;
    }

    TextPosition position(const EditorDocument& document,std::size_t offset,TextAffinity affinity) const {
        const auto& block=blocks_[block_at(offset)];
        const auto local=offset>block.begin ? offset-block.begin : 0;
        return source_projection_detail::valid_position(document,
            source_projection_detail::owner_position(block.owners,local,affinity));
    }

private:
    std::vector<SourceProjectionBlock> blocks_;
    std::size_t size_=0;
    std::unordered_map<std::uint64_t,NodeId> owner_roots_;
    std::unordered_map<std::uint64_t,std::size_t> root_positions_;
    void index_owners(const SourceProjectionBlock& block) {
        for (const auto& map:block.owners) owner_roots_[map.container_id.v]=block.id;
    }
    void rebuild_root_positions() {
        root_positions_.clear();
        for (std::size_t i=0;i<blocks_.size();++i) root_positions_[blocks_[i].id.v]=i;
    }
};

inline TextSelection source_view_selection_in_document(
    const EditorDocument& document,const SourceViewSelection& selection) {
    auto resolve=[&](SourceViewAnchor anchor) {
        const auto root=std::ranges::find(document.root.children,anchor.block,&BlockNode::id);
        if (root==document.root.children.end()) throw std::logic_error("history source block is absent");
        const auto index=static_cast<std::size_t>(root-document.root.children.begin());
        const auto projection=serialize_markdown_fragment_projection(document.root.children,index,index+1);
        const auto local=static_cast<std::size_t>((std::max)(anchor.offset,std::ptrdiff_t{}));
        return source_projection_detail::valid_position(document,
            source_position_for_serialized_offset(projection,local,anchor.affinity));
    };
    return {resolve(selection.anchor),resolve(selection.active)};
}

} // namespace folia
