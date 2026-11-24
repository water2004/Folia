#include "support/folia_test.hpp"
import folia.math.core.tree.factory;
import folia.math.core.tree.visitor;

using namespace boost::ut;
namespace tree = folia::math::tree;

namespace {
struct CreationArguments {
    int id = 0;
};

struct VisitArguments {
    std::vector<int> *order = nullptr;
    int token = 0;
};

struct Product : tree::FactoryNode<std::string> {
    Product(std::string kind, int id) : kind_(std::move(kind)), id(id) {
    }

    std::string kind() const override {
        return kind_;
    }

    std::string kind_;
    int id;
    std::vector<Product *> childNodes;
};

tree::FactoryClass<Product, CreationArguments> productClass(std::string kind) {
    return {
        [kind = std::move(kind)](CreationArguments const &args) -> std::unique_ptr<Product> {
            return std::make_unique<Product>(kind, args.id);
        },
        [](Product const &product) { return dynamic_cast<Product const *>(&product) != nullptr; },
    };
}

struct ProductFactory : tree::AbstractFactory<Product, std::string, CreationArguments> {
    ProductFactory()
        : AbstractFactory({{"root", productClass("root")}, {"leaf", productClass("leaf")}}) {
    }

    Product &adopt(std::unique_ptr<Product> value) override {
        auto *result = value.get();
        arena.push_back(std::move(value));
        return *result;
    }

    std::vector<std::unique_ptr<Product>> arena;
};

tree::VisitorChildren<Product> liveChildren() {
    return {
        [](Product &product) { return product.childNodes.size(); },
        [](Product &product, std::size_t index) { return product.childNodes.at(index); },
    };
}

using VoidVisitor = tree::AbstractVisitor<Product, std::string, VisitArguments, void>;
using StringVisitor = tree::AbstractVisitor<Product, std::string, VisitArguments, std::string>;

struct LifetimeToken {
    int *destructions;

    explicit LifetimeToken(int &value) : destructions(&value) {
    }

    ~LifetimeToken() {
        ++*destructions;
    }
};
} // namespace

suite native_tree_visitor = [] {
    "typed_tree_visitor_snapshots_registered_handlers_and_walks_live_children"_test = [] {
        ProductFactory factory;
        auto &root = factory.create("root", CreationArguments{1});
        auto &first = factory.create("leaf", CreationArguments{2});
        root.childNodes.push_back(&first);

        std::vector<int> order;
        int replacedCalls = 0;
        int ghostCalls = 0;
        VoidVisitor::HandlerTable handlers;
        handlers.emplace("leaf", [&factory, &root](Product &product, VisitArguments const &args) {
            expect(args.token == 73_i);
            args.order->push_back(product.id);
            if (product.id == 2) {
                auto &appended = factory.create("leaf", CreationArguments{3});
                root.childNodes.push_back(&appended);
            }
        });
        handlers.emplace("ghost",
                         [&ghostCalls](Product &, VisitArguments const &) { ++ghostCalls; });
        VoidVisitor visitor(factory, handlers, liveChildren());

        // Constructor-captured functions survive caller table replacement.
        handlers.insert_or_assign(
            "leaf", [&replacedCalls](Product &, VisitArguments const &) { ++replacedCalls; });
        VisitArguments arguments{&order, 73};
        visitor.visitTree(root, arguments);
        expect(order == std::vector<int>({2, 3}));
        expect(replacedCalls == 0_i);

        Product ghost("ghost", 4);
        visitor.visitNode(ghost, arguments);
        expect(ghostCalls == 0_i); // not in the factory-kind snapshot

        int updatedCalls = 0;
        visitor.setNodeHandler(
            "leaf", [&updatedCalls](Product &, VisitArguments const &) { ++updatedCalls; });
        visitor.visitNode(first, arguments);
        expect(updatedCalls == 1_i);
        visitor.removeNodeHandler("leaf");
        visitor.visitNode(first, arguments);
        expect(updatedCalls == 1_i);
        visitor.setNodeHandler("leaf", {});
        visitor.visitNode(first, arguments); // an empty handler selects default
        expect(updatedCalls == 1_i);

        root.childNodes.push_back(nullptr);
        bool nullRejected = false;
        try {
            visitor.visitTree(root, arguments);
        } catch (std::invalid_argument const &) {
            nullRejected = true;
        }
        expect(nullRejected);
    };

    "typed_tree_visitor_returns_handler_value_and_value_initialized_default"_test = [] {
        ProductFactory factory;
        auto &root = factory.create("root", CreationArguments{10});
        auto &leaf = factory.create("leaf", CreationArguments{11});
        root.childNodes.push_back(&leaf);
        std::vector<int> visited;

        StringVisitor::HandlerTable handlers;
        handlers.emplace("leaf", [](Product &product, VisitArguments const &args) {
            args.order->push_back(product.id);
            return std::to_string(args.token + product.id);
        });
        StringVisitor visitor(factory, std::move(handlers), liveChildren());
        VisitArguments arguments{&visited, 31};
        expect(visitor.visitTree(root, arguments).empty());
        expect(visited == std::vector<int>({11}));
        expect(visitor.visitNode(leaf, arguments) == std::string("42"));
    };

    "typed_tree_visitor_keeps_mutating_handler_alive_for_active_call"_test = [] {
        ProductFactory factory;
        auto &leaf = factory.create("leaf", CreationArguments{20});
        std::vector<int> calls;
        VoidVisitor visitor(factory, {}, liveChildren());

        int destructions = 0;
        auto token = std::make_shared<LifetimeToken>(destructions);
        std::weak_ptr<LifetimeToken> weak = token;
        visitor.setNodeHandler("leaf", [&, token](Product &, VisitArguments const &) {
            visitor.removeNodeHandler("leaf");
            expect(!weak.expired()); // active dispatch owns the removed target
            calls.push_back(1);
        });
        token.reset();
        visitor.visitNode(leaf);
        expect(weak.expired());
        expect(destructions == 1_i);
        expect(calls == std::vector<int>({1}));

        token = std::make_shared<LifetimeToken>(destructions);
        weak = token;
        visitor.setNodeHandler("leaf", [&, token](Product &, VisitArguments const &) {
            visitor.setNodeHandler("leaf",
                                   [&calls](Product &, VisitArguments const &) { calls.push_back(3); });
            expect(!weak.expired()); // replacement releases only the registry owner
            calls.push_back(2);
        });
        token.reset();
        visitor.visitNode(leaf);
        expect(weak.expired());
        expect(destructions == 2_i);
        visitor.visitNode(leaf);
        expect(calls == std::vector<int>({1, 2, 3}));
    };

    "typed_tree_visitor_preserves_mutable_handler_closure_state"_test = [] {
        ProductFactory factory;
        auto &leaf = factory.create("leaf", CreationArguments{30});
        std::vector<int> state;
        VoidVisitor::HandlerTable handlers;
        handlers.emplace("leaf", [count = 0, &state](Product &, VisitArguments const &) mutable {
            state.push_back(++count);
        });
        VoidVisitor visitor(factory, std::move(handlers), liveChildren());
        visitor.visitNode(leaf);
        visitor.visitNode(leaf);
        expect(state == std::vector<int>({1, 2}));
    };
};
