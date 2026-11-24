#include "support/folia_test.hpp"
import folia.math.core.tree.node;

using namespace boost::ut;
using namespace folia::math::tree;
namespace {
enum class Kind { row, token, empty, special };
class TestOwner;

class TestNode : public AbstractNode<TestNode, Kind> {
public:
    TestNode(TestOwner& owner, Kind kind, std::string value, int id)
        : owner_(&owner), kind_(kind), value_(std::move(value)), id_(id) {}
    Kind kind() const override { return kind_; }
    std::string const& value() const { return value_; }
    int id() const { return id_; }
protected:
    NodeOwner<TestNode, Kind>& treeOwner() const noexcept override;
private:
    friend class TestOwner;
    TestOwner* owner_;
    Kind kind_;
    std::string value_;
    int id_;
};

class OrdinaryNode final : public TestNode {
public:
    using TestNode::TestNode;
};
class TokenNode : public TestNode {
public:
    using TestNode::TestNode;
};
class EmptyNode final : public AbstractEmptyNode<TestNode, Kind, TestNode> {
public:
    using AbstractEmptyNode::AbstractEmptyNode;
};
class SpecialNode final : public TokenNode {
public:
    SpecialNode(TestOwner& owner, Kind, std::string value, int id)
        : TokenNode(owner, Kind::special, std::move(value), id) {}
};

class TestOwner final : public NodeOwner<TestNode, Kind> {
public:
    template<class T = OrdinaryNode>
    T& make(Kind kind, std::string value, int id) {
        auto node = std::make_unique<T>(*this, kind, std::move(value), id);
        auto& result = *node;
        arena_.push_back(std::move(node));
        return result;
    }
    void setChildren(TestNode& parent, std::span<TestNode* const> children) override {
        const std::vector<TestNode*> snapshot(children.begin(), children.end());
        for (auto* child : parent.children_) child->parent_ = nullptr;
        parent.children_.clear();
        for (auto* child : snapshot) append(parent, *child);
    }
    void append(TestNode& parent, TestNode& child) override {
        if (&parent == &child) return;
        detach(child);
        parent.children_.push_back(&child);
        child.parent_ = &parent;
    }
    void replace(TestNode& parent, TestNode& oldChild, TestNode& replacement) override {
        if (&oldChild == &replacement || !indexOf(parent, oldChild)) return;
        detach(replacement);
        const auto current = *indexOf(parent, oldChild);
        parent.children_[current] = &replacement;
        oldChild.parent_ = nullptr;
        replacement.parent_ = &parent;
    }
    void remove(TestNode& child) override { detach(child); }
    TestNode& clone(TestNode const& source, bool keepIds) override {
        TestNode& copy = dynamic_cast<SpecialNode const*>(&source)
            ? static_cast<TestNode&>(make<SpecialNode>(source.kind_, source.value_, cloneId(source.id_, keepIds)))
            : dynamic_cast<EmptyNode const*>(&source)
                ? static_cast<TestNode&>(make<EmptyNode>(source.kind_, source.value_, cloneId(source.id_, keepIds)))
                : dynamic_cast<TokenNode const*>(&source)
                    ? static_cast<TestNode&>(make<TokenNode>(source.kind_, source.value_, cloneId(source.id_, keepIds)))
                    : static_cast<TestNode&>(make<OrdinaryNode>(source.kind_, source.value_, cloneId(source.id_, keepIds)));
        for (auto* child : source.children_) {
            if (dynamic_cast<EmptyNode const*>(&copy)) break;
            auto& childCopy = clone(*child, keepIds);
            append(copy, childCopy);
        }
        return copy;
    }
    bool nodeIsKind(TestNode const& node, Kind kind) const override {
        if (kind == Kind::special) return dynamic_cast<SpecialNode const*>(&node) != nullptr;
        if (kind == Kind::token) return dynamic_cast<TokenNode const*>(&node) != nullptr;
        return node.kind_ == kind;
    }
    std::string_view kindName(Kind kind) const override {
        switch (kind) {
        case Kind::row: return "row";
        case Kind::token: return "token";
        case Kind::empty: return "empty";
        case Kind::special: return "special";
        }
        return "unknown";
    }
private:
    static std::optional<std::size_t> indexOf(TestNode const& parent, TestNode const& child) {
        const auto found = std::find(parent.children_.begin(), parent.children_.end(), &child);
        if (found == parent.children_.end()) return std::nullopt;
        return static_cast<std::size_t>(found - parent.children_.begin());
    }
    static void detach(TestNode& child) {
        if (!child.parent_) return;
        auto& siblings = child.parent_->children_;
        std::erase(siblings, &child);
        child.parent_ = nullptr;
    }
    int cloneId(int source, bool keepIds) { return keepIds ? source : ++nextId_; }
    std::vector<std::unique_ptr<TestNode>> arena_;
    int nextId_ = 1000;
};
NodeOwner<TestNode, Kind>& TestNode::treeOwner() const noexcept { return *owner_; }

suite native_tree_node_contract = [] {
    "typed owner operations preserve links and source no-ops"_test = [] {
        TestOwner owner;
        auto& root = owner.make<OrdinaryNode>(Kind::row, "root", 1);
        auto& first = owner.make<TokenNode>(Kind::token, "one", 2);
        auto& old = owner.make<TokenNode>(Kind::token, "old", 3);
        auto& replacement = owner.make<SpecialNode>(Kind::special, "replacement", 4);
        std::array<TestNode*, 2> initial{&first, &old};
        root.setChildren(initial);
        expect(root.children().size() == 2_u);
        expect(first.parent() == &root);
        expect(root.childIndex(old) == std::size_t{1});
        expect(&root.replaceChild(replacement, old) == &replacement);
        expect(replacement.parent() == &root);
        expect(old.parent() == nullptr);
        expect(root.childIndex(replacement) == std::size_t{1});
        expect(replacement.isKind(Kind::token));
        expect(replacement.isKind(Kind::special));
        auto& missing = owner.make(Kind::token, "missing", 5);
        expect(&root.replaceChild(missing, old) == &missing);
        expect(missing.parent() == nullptr);
        expect(&root.removeChild(old) == &old);
        expect(old.parent() == nullptr);
        expect(root.children().size() == 2_u);
        expect(replacement.isKind(Kind::special));
        expect(root.findNodes(Kind::special).size() == 1_u);
        expect(root.findNodes(Kind::token).size() == 2_u);
        expect(root.toString() == "row(token(),special())");

        auto& cloned = root.copy(true);
        expect(&cloned != &root);
        expect(cloned.id() == root.id());
        expect(cloned.value() == root.value());
        expect(cloned.children().size() == root.children().size());
        expect(cloned.children()[0] != root.children()[0]);
        expect(cloned.children()[0]->parent() == &cloned);
        expect(cloned.findNodes(Kind::special).size() == 1_u);
        expect(cloned.findNodes(Kind::token).size() == 2_u);
        auto& fresh = root.copy();
        expect(fresh.id() != root.id());

        auto& aliasRoot = owner.make<OrdinaryNode>(Kind::row, "alias", 6);
        auto& aliasChild = owner.make<TokenNode>(Kind::token, "kept", 7);
        aliasRoot.appendChild(aliasChild);
        aliasRoot.setChildren(aliasRoot.children());
        expect(aliasRoot.children().size() == 1_u);
        expect(aliasRoot.children()[0] == &aliasChild);
        expect(aliasChild.parent() == &aliasRoot);
    };

    "preorder walk visits children appended by the visitor"_test = [] {
        TestOwner owner;
        auto& root = owner.make<OrdinaryNode>(Kind::row, "root", 1);
        auto& child = owner.make<TokenNode>(Kind::token, "first", 2);
        auto& added = owner.make<SpecialNode>(Kind::special, "late", 3);
        root.appendChild(child);
        std::vector<std::string> visited;
        root.walkTree([&](TestNode& current) {
            visited.push_back(current.value());
            if (&current == &root) root.appendChild(added);
        });
        expect(visited.size() == 3_u);
        expect(visited[0] == "root");
        expect(visited[1] == "first");
        expect(visited[2] == "late");
    };

    "empty nodes retain source no-child overrides"_test = [] {
        TestOwner owner;
        auto& empty = owner.make<EmptyNode>(Kind::empty, "empty", 1);
        auto& child = owner.make<TokenNode>(Kind::token, "child", 2);
        std::array<TestNode*, 1> initial{&child};
        empty.setChildren(initial);
        expect(empty.children().empty());
        expect(&empty.appendChild(child) == &child);
        expect(child.parent() == nullptr);
        expect(empty.childIndex(child) == std::nullopt);
        expect(&empty.replaceChild(child, empty) == &empty);
        expect(&empty.removeChild(child) == &child);
        std::vector<TestNode*> visited;
        empty.walkTree([&](TestNode& node) { visited.push_back(&node); });
        expect(visited.size() == 1_u);
        expect(visited.front() == &empty);
        expect(empty.toString() == "empty");
    };
};
}
