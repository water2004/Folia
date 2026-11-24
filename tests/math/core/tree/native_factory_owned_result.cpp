#include "support/folia_test.hpp"
import folia.math.core.tree.factory;

namespace tree = folia::math::tree;
using namespace boost::ut;

namespace {
struct Arguments { int value = 0; };
struct Product : tree::FactoryNode<std::string> {
    virtual int value() const = 0;
};
struct Tracked final : Product {
    Tracked(int value, int& destroyed) : value_(value), destroyed_(&destroyed) {}
    ~Tracked() override { ++*destroyed_; }
    std::string kind() const override { return "tracked"; }
    int value() const override { return value_; }
    int value_;
    int* destroyed_;
};
struct Guard {
    explicit Guard(int& releases) : releases_(&releases) {}
    ~Guard() { ++*releases_; }
    int* releases_;
};
struct ThrowingProduct final : Product {
    explicit ThrowingProduct(int& releases) : guard(releases) {
        throw std::runtime_error("construction failed");
    }
    std::string kind() const override { return "throws"; }
    int value() const override { return 0; }
    Guard guard;
};

tree::FactoryClass<Product, Arguments> trackedClass(int& destroyed, int bias = 0) {
    return {
        [&destroyed, bias](const Arguments& args) -> std::unique_ptr<Product> {
            return std::make_unique<Tracked>(args.value + bias, destroyed);
        },
        [](const Product& product) { return dynamic_cast<const Tracked*>(&product) != nullptr; },
    };
}
struct OwnedFactory final : tree::AbstractFactory<Product, std::string, Arguments,
                                                   std::unique_ptr<Product>> {
    using AbstractFactory::AbstractFactory;
    std::unique_ptr<Product> adopt(std::unique_ptr<Product> product) override {
        return product;
    }
};
}

suite math_native_factory_owned_result = [] {
    "factory_unique_result_replacement_fallback_and_caller_lifetime"_test = [] {
        int destroyed = 0;
        OwnedFactory factory({}, std::optional<std::string>("fallback"));
        factory.setNodeClass("fallback", trackedClass(destroyed, 100));
        factory.setNodeClass("requested", trackedClass(destroyed));
        factory.setNodeClass("last", trackedClass(destroyed));
        expect(factory.getKinds() == std::vector<std::string>({"fallback", "requested", "last"}));

        factory.setNodeClass("requested", trackedClass(destroyed, 10));
        expect(factory.getKinds() == std::vector<std::string>({"fallback", "requested", "last"}));
        {
            std::unique_ptr<Product> requested = factory.create("requested", Arguments{7});
            expect(requested->value() == 17_i);
            expect(factory.nodeIsKind(*requested, "requested"));
            auto fallback = factory.create("absent", Arguments{3});
            expect(fallback->value() == 103_i);
            expect(destroyed == 0_i);
        }
        expect(destroyed == 2_i);
        factory.deleteNodeClass("requested");
        factory.setNodeClass("requested", trackedClass(destroyed));
        expect(factory.getKinds() == std::vector<std::string>({"fallback", "last", "requested"}));
    };

    "factory_owned_result_unwinds_when_constructor_throws"_test = [] {
        int destroyed = 0;
        int memberReleases = 0;
        OwnedFactory factory;
        factory.setNodeClass("tracked", trackedClass(destroyed));
        factory.setNodeClass("throws", {
            [&memberReleases](const Arguments&) -> std::unique_ptr<Product> {
                return std::make_unique<ThrowingProduct>(memberReleases);
            },
            [](const Product&) { return false; },
        });
        {
            auto product = factory.create("tracked", Arguments{9});
            expect(product->value() == 9_i);
            expect(destroyed == 0_i);
        }
        expect(destroyed == 1_i);

        bool threw = false;
        try { (void)factory.create("throws", Arguments{}); }
        catch (const std::runtime_error&) { threw = true; }
        expect(threw);
        expect(memberReleases == 1_i);
        expect(destroyed == 1_i);
    };
};
