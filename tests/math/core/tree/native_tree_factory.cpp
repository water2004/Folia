#include "support/folia_test.hpp"
import folia.math.core.tree.factory;
import folia.math.core.tree.wrapper;
import folia.math.core.tree.wrapper_factory;

namespace tree = folia::math::tree;
using namespace boost::ut;

namespace {
struct Args { int value = 0; };
struct Product : tree::FactoryNode<std::string> {
    virtual int value() const = 0;
};
struct Basic : Product {
    explicit Basic(int value) : value_(value) {}
    std::string kind() const override { return "basic"; }
    int value() const override { return value_; }
    int value_;
};
struct Specialized : Basic { using Basic::Basic; };
struct Tracked : Basic {
    Tracked(int value, int& destructions) : Basic(value), destructions_(&destructions) {}
    ~Tracked() override { ++*destructions_; }
    int* destructions_;
};

tree::FactoryClass<Product, Args> recordFor(bool specialized = false) {
    return {
        [specialized](const Args& args) -> std::unique_ptr<Product> {
            if (specialized) return std::make_unique<Specialized>(args.value);
            return std::make_unique<Basic>(args.value);
        },
        [specialized](const Product& product) {
            return specialized ? dynamic_cast<const Specialized*>(&product) != nullptr
                               : dynamic_cast<const Basic*>(&product) != nullptr;
        },
    };
}
struct ProductArena : tree::AbstractFactory<Product, std::string, Args> {
    using AbstractFactory::AbstractFactory;
    Product& adopt(std::unique_ptr<Product> product) override {
        auto* result = product.get();
        arena.push_back(std::move(product));
        return *result;
    }
    std::vector<std::unique_ptr<Product>> arena;
};

struct Source {
    std::string kind() const { return "wrapper"; }
    int value;
};
struct TestWrapper;
struct WrapArgs {
    tree::WrapperServices<TestWrapper, Source>* service = nullptr;
    Source* source = nullptr;
    TestWrapper* parent = nullptr;
};
struct TestWrapper : tree::AbstractWrapper<TestWrapper, Source> {
    TestWrapper(tree::WrapperServices<TestWrapper, Source>& service, Source& source)
        : AbstractWrapper(service, source) {}
    int sourceValue() const { return node->value; }
};
struct WrapperArena : tree::AbstractWrapperFactory<TestWrapper, Source, WrapArgs> {
    using AbstractWrapperFactory::AbstractWrapperFactory;
    TestWrapper& adopt(std::unique_ptr<TestWrapper> product) override {
        auto* result = product.get();
        arena.push_back(std::move(product));
        return *result;
    }
    std::vector<std::unique_ptr<TestWrapper>> arena;
protected:
    WrapArgs creationArguments(Source& source, TestWrapper* parent) override {
        seenParent = parent;
        return {this, &source, parent};
    }
public:
    TestWrapper* seenParent = nullptr;
};
tree::FactoryClass<TestWrapper, WrapArgs> wrapperRecord() {
    return {
        [](const WrapArgs& args) -> std::unique_ptr<TestWrapper> {
            if (!args.service || !args.source) throw std::invalid_argument("missing wrapper args");
            return std::make_unique<TestWrapper>(*args.service, *args.source);
        },
        [](const TestWrapper& product) { return dynamic_cast<const TestWrapper*>(&product) != nullptr; },
    };
}
}

suite math_native_tree_factory = [] {
    "typed_factory_constructor_ingests_ordered_map_and_optional_default"_test = [] {
        tree::FactoryClassMap<Product, std::string, Args> classes{
            {"first", recordFor()},
            {"second", recordFor()},
            {"first", recordFor(true)},
        };
        ProductArena factory(std::move(classes), std::optional<std::string>("second"));
        expect(factory.getKinds() == std::vector<std::string>({"first", "second"}));
        auto& first = factory.create("first", Args{41});
        auto& fallback = factory.create("missing", Args{42});
        expect(dynamic_cast<Specialized*>(&first) != nullptr);
        expect(first.value() == 41_i);
        expect(fallback.value() == 42_i);
        expect(factory.defaultKind == std::optional<std::string>("second"));

        ProductArena unregisteredDefault({}, std::optional<std::string>("missing-default"));
        unregisteredDefault.setNodeClass("present", recordFor());
        bool unregisteredFallbackRejected = false;
        try { (void)unregisteredDefault.create("absent", Args{}); }
        catch (const std::out_of_range&) { unregisteredFallbackRejected = true; }
        expect(unregisteredFallbackRejected);

        enum class Kind { only };
        struct EnumProduct : tree::FactoryNode<Kind> {
            Kind kind() const override { return Kind::only; }
        };
        struct EnumArena : tree::AbstractFactory<EnumProduct, Kind, Args> {
            using AbstractFactory::AbstractFactory;
            EnumProduct& adopt(std::unique_ptr<EnumProduct> product) override {
                auto* result = product.get(); arena.push_back(std::move(product)); return *result;
            }
            std::vector<std::unique_ptr<EnumProduct>> arena;
        } enumFactory;
        enumFactory.setNodeClass(Kind::only, {
            [](const Args&) { return std::make_unique<EnumProduct>(); },
            [](const EnumProduct& product) { return dynamic_cast<const EnumProduct*>(&product) != nullptr; },
        });
        expect(enumFactory.create(Kind::only, Args{}).kind() == Kind::only);
    };

    "typed_factory_registration_fallback_subtyping_and_order"_test = [] {
        ProductArena factory({}, std::optional<std::string>("fallback"));
        factory.setNodeClass("fallback", recordFor());
        factory.setNodeClass("first", recordFor(true));
        factory.setNodeClass("second", recordFor());
        auto& requested = factory.create("first", Args{17});
        auto& fallback = factory.create("missing", Args{23});
        expect(requested.value() == 17_i);
        expect(factory.nodeIsKind(requested, "first"));
        expect(factory.nodeIsKind(requested, "fallback")); // base-class instance predicate
        expect(fallback.value() == 23_i);

        auto captured = factory.getNodeClass("first");
        factory.setNodeClass("first", recordFor());
        auto oldProduct = captured->construct(Args{31});
        expect(dynamic_cast<Specialized*>(oldProduct.get()) != nullptr);
        expect(factory.getKinds() == std::vector<std::string>({"fallback", "first", "second"}));
        factory.deleteNodeClass("first");
        factory.setNodeClass("first", recordFor());
        expect(factory.getKinds() == std::vector<std::string>({"fallback", "second", "first"}));
    };

    "typed_factory_missing_default_and_requested_kind_throw"_test = [] {
        ProductArena noDefault;
        noDefault.setNodeClass("only", recordFor());
        bool missing = false;
        try { (void)noDefault.create("missing", Args{}); }
        catch (const std::out_of_range&) { missing = true; }
        expect(missing);
        bool absentDefault = false;
        try { (void)noDefault.create("only", Args{}); }
        catch (...) { absentDefault = true; }
        expect(!absentDefault);
        noDefault.deleteNodeClass("only");
        bool noFallback = false;
        try { (void)noDefault.create("missing", Args{}); }
        catch (const std::out_of_range&) { noFallback = true; }
        expect(noFallback);
    };

    "typed_factory_constructor_and_adoption_failures_unwind_unique_owners"_test = [] {
        ProductArena factory;
        int destructions = 0;
        factory.setNodeClass("tracked", {
            [&destructions](const Args& args) -> std::unique_ptr<Product> {
                return std::make_unique<Tracked>(args.value, destructions);
            },
            [](const Product&) { return true; },
        });
        factory.setNodeClass("throws", {
            [](const Args&) -> std::unique_ptr<Product> { throw std::runtime_error("constructor"); },
            [](const Product&) { return false; },
        });
        factory.setNodeClass("null-creator", {
            {}, [](const Product&) { return false; },
        });
        factory.setNodeClass("null-product", {
            [](const Args&) -> std::unique_ptr<Product> { return {}; },
            [](const Product&) { return false; },
        });
        bool constructorThrew = false;
        try { (void)factory.create("throws", Args{}); }
        catch (const std::runtime_error&) { constructorThrew = true; }
        expect(constructorThrew);
        expect(factory.arena.empty());
        bool nullCreatorRejected = false;
        try { (void)factory.create("null-creator", Args{}); }
        catch (const std::invalid_argument&) { nullCreatorRejected = true; }
        expect(nullCreatorRejected);
        bool nullProductRejected = false;
        try { (void)factory.create("null-product", Args{}); }
        catch (const std::runtime_error&) { nullProductRejected = true; }
        expect(nullProductRejected);

        struct ThrowingArena : ProductArena {
            using ProductArena::ProductArena;
            Product& adopt(std::unique_ptr<Product> product) override {
                throw std::runtime_error("adoption");
            }
        } throwing;
        throwing.setNodeClass("tracked", factory.getNodeClass("tracked").value());
        bool adoptionThrew = false;
        try { (void)throwing.create("tracked", Args{9}); }
        catch (const std::runtime_error&) { adoptionThrew = true; }
        expect(adoptionThrew);
        expect(destructions == 1_i);
    };

    "typed_factory_adopted_product_lifetime_and_address_survive_registry_changes"_test = [] {
        int destructions = 0;
        Product* stable = nullptr;
        {
            ProductArena factory;
            factory.setNodeClass("tracked", {
                [&destructions](const Args& args) -> std::unique_ptr<Product> {
                    return std::make_unique<Tracked>(args.value, destructions);
                },
                [](const Product&) { return true; },
            });
            stable = &factory.create("tracked", Args{55});
            for (int index = 0; index < 64; ++index)
                factory.setNodeClass("registry-" + std::to_string(index), recordFor());
            expect(stable->value() == 55_i);
            expect(destructions == 0_i);
        }
        expect(destructions == 1_i);
    };

    "typed_wrapper_factory_forwards_source_parent_and_live_preorder"_test = [] {
        WrapperArena factory({}, std::optional<std::string>("wrapper"));
        factory.setNodeClass("wrapper", wrapperRecord());
        Source rootSource{1}, childSource{2}, addedSource{3};
        auto& root = factory.wrap(rootSource);
        auto& child = factory.wrap(childSource, &root);
        expect(factory.seenParent == &root);
        root.childNodes.push_back(&child);
        std::vector<int> order;
        root.walkTree([&](TestWrapper& current) {
            order.push_back(current.sourceValue());
            if (&current == &root) {
                auto& added = factory.wrap(addedSource, &root);
                root.childNodes.push_back(&added);
            }
        });
        expect(order == std::vector<int>({1, 2, 3}));
    };
};
