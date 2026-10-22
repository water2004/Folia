#include "math/support/tex_fixture.hpp"

suite stack = [] {
    "stack open frames copy environment and release after the last activation"_test = [] {
        Fixture fixture;
        TexEnvironment environment;
        environment.font = u"normal";
        Stack stack(fixture.factory(), environment, true);
        auto frame = fixture.item(StackKind::open);
        std::weak_ptr<BaseItem> lifetime = frame;
        stack.Push(frame);
        frame.reset();
        expect(!lifetime.expired());
        expect(stack.height() == 2_u);
        stack.env().color = u"red";
        auto popped = stack.Pop();
        expect(popped->env().color.value() == u"red");
        expect(!stack.env().color);
        expect(stack.env().font.value() == u"normal");
        popped.reset();
        expect(lifetime.expired());
        expect(stack.height() == 1_u);
    };
    "stack preserves two activations of the same begin frame and its shared state"_test = [] {
        Fixture fixture;
        Stack stack(fixture.factory());
        auto begin = fixture.item(StackKind::begin);
        std::weak_ptr<BaseItem> lifetime = begin;
        stack.Push(begin);
        stack.Push(begin);
        expect(stack.height() == 3_u);
        expect(stack.Top() == stack.Top(2));
        stack.Top()->env().bold_symbol = true;
        expect(stack.Top(2)->env().bold_symbol.value());
        begin.reset();
        auto first = stack.Pop();
        first.reset();
        expect(!lifetime.expired());
        auto second = stack.Pop();
        expect(second->env().bold_symbol.value());
        second.reset();
        expect(lifetime.expired());
    };
    "stack consumes final frames without retaining discarded products in the factory"_test = [] {
        Fixture fixture;
        Stack stack(fixture.factory());
        auto &token = fixture.token();
        auto frame = fixture.mml(token);
        std::weak_ptr<BaseItem> lifetime = frame;
        stack.Push(frame);
        frame.reset();
        expect(lifetime.expired());
        expect(stack.height() == 1_u);
        expect(stack.Prev(true) == &token);
        expect(stack.Prev() == &token);
        expect(stack.Prev(true) == nullptr);
    };
    "stack replays fraction completion and the incoming stop in Source order"_test = [] {
        Fixture fixture;
        Stack stack(fixture.factory());
        auto &numerator = fixture.token(NodeKind::mi, u"a");
        auto &denominator = fixture.token(NodeKind::mi, u"b");
        stack.Push(&numerator);
        auto over = fixture.item(StackKind::over);
        std::weak_ptr<BaseItem> lifetime = over;
        stack.Push(over);
        over.reset();
        stack.Push(&denominator);
        stack.Push(fixture.item(StackKind::stop));
        expect(lifetime.expired());
        expect(stack.height() == 1_u);
        expect(stack.Top()->kind() == StackKind::mml);
        auto root = stack.Top()->First();
        expect(root->kind() == NodeKind::mfrac);
        expect(root->children()[0] == &numerator);
        expect(root->children()[1] == &denominator);
    };
};

suite retained_mml_stack_values = [] {
    "mml_stack_integer_peek_and_clear_preserve_node_values"_test = [] {
        Fixture f;
        auto frame = f.item(StackKind::open);
        auto& a = f.token(NodeKind::mi, u"a");
        auto& b = f.token(NodeKind::mn, u"b");
        const std::array<MmlNode*, 4> sparse{&a, nullptr, nullptr, &b};
        frame->Push(sparse);
        expect(frame->Size() == 4_u);
        expect(frame->Peek(0).empty());
        expect(frame->Peek() == std::vector<MmlNode*>{&b});
        expect(frame->Peek(2) == (std::vector<MmlNode*>{nullptr, &b}));
        expect(frame->Peek(6) == (std::vector<MmlNode*>{nullptr, &b}));
        expect(frame->Peek(20) == (std::vector<MmlNode*>{&a, nullptr, nullptr, &b}));
        auto snapshot = frame->Peek(4);
        frame->Clear();
        expect(frame->Size() == 0_u);
        expect(frame->First() == nullptr && frame->Last() == nullptr);
        expect(snapshot == (std::vector<MmlNode*>{&a, nullptr, nullptr, &b}));
        frame->setLast(a);
        expect(frame->Size() == 1_u && frame->First() == &a && frame->Last() == &a);
        const std::array<MmlNode*, 1> added{&b};
        frame->Push(added);
        expect(frame->First() == &b && frame->Last() == &a);
        frame->setFirst(a); frame->setLast(b);
        expect(frame->Pop() == &a);
        expect(frame->Pop() == &b);
        expect(frame->Pop() == nullptr);
    };
    "mml_stack_to_mml_preserves_all_typed_row_branches"_test = [] {
        for (std::size_t count : {std::size_t{0}, std::size_t{1}, std::size_t{2}})
            for (bool inferred : {false, true})
                for (bool forceRow : {false, true}) {
                    Fixture f;
                    auto frame = f.item(StackKind::open);
                    auto& a = f.token(NodeKind::mi, u"a");
                    auto& b = f.token(NodeKind::mn, u"b");
                    const std::array<MmlNode*, 2> input{&a, &b};
                    frame->Push(std::span<MmlNode* const>(input).first(count));
                    auto& result = frame->toMml(inferred, forceRow);
                    expect(frame->Size() == count);
                    if (count == 1 && !forceRow) {
                        expect(&result == &a);
                    } else {
                        expect(result.kind() == (inferred ? NodeKind::inferred_mrow : NodeKind::mrow));
                        expect(result.children().size() == count);
                        for (std::size_t i = 0; i < std::min(count, result.children().size()); ++i) {
                            expect(result.children()[i] == input[i]);
                            expect(input[i]->parent() == &result);
                        }
                    }
                }
    };
};
