#include "support/folia_test.hpp"
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.maligngroup;
import folia.math.core.mml_tree.mml_nodes.malignmark;
import folia.math.core.mml_tree.mml_nodes.mtable;
import folia.math.core.mml_tree.mml_nodes.mtr;
import folia.math.core.mml_tree.mml_nodes.mtd;

using namespace boost::ut;
using namespace folia::math::mml;

suite typed_mml_table = [] {
    "table family reports closed native kinds and Source arities"_test = [] {
        MmlDocument document;
        auto &factory = document.factory();
        auto &table = factory.make(NodeKind::mtable);
        auto &row = factory.make(NodeKind::mtr);
        auto &labeled = factory.make(NodeKind::mlabeledtr);
        auto &cell = factory.make(NodeKind::mtd);
        auto &group = factory.make(NodeKind::maligngroup);
        auto &mark = factory.make(NodeKind::malignmark);
        expect(table.kind() == NodeKind::mtable);
        expect(row.kind() == NodeKind::mtr);
        expect(labeled.kind() == NodeKind::mlabeledtr);
        expect(cell.kind() == NodeKind::mtd);
        expect(group.kind() == NodeKind::maligngroup);
        expect(mark.kind() == NodeKind::malignmark);
        expect(row.arity() > 1.0_d);
        expect(labeled.arity() == 1.0_d);
        expect(cell.arity() == -1.0_d);
        expect(mark.arity() == 0.0_d);
        expect(table.linebreakContainer());
        expect(row.linebreakContainer());
        expect(cell.linebreakContainer());
        expect(cell.linebreakAlign() == "columnalign");
        expect(group.isSpacelike());
        expect(mark.isSpacelike());
        expect(labeled.isKind(NodeKind::mtr));
        expect(std::get<std::u16string>(*table.attributes().lookup(u"side")) == u"right");
        expect(std::get<std::u16string>(*row.attributes().lookup(u"data-break-align")) == u"top");
        expect(std::get<double>(*cell.attributes().lookup(u"rowspan")) == 1.0);
        expect(std::get<std::u16string>(*group.attributes().defaultValue(u"groupalign")) ==
               u"_inherit_");
        expect(std::get<std::u16string>(*mark.attributes().lookup(u"edge")) == u"left");
    };

    "table inheritance keeps row origin separate from scalar value"_test = [] {
        AttributeList attributes{
            {u"data-vertical-align", NodeKind::mtr, std::u16string{u"middle"}}};
        expect(attributes.front().origin == NodeKind::mtr);
        expect(std::get<std::u16string>(attributes.front().value) == u"middle");
    };

    "table and row distribute row and column alignment across cells"_test = [] {
        MmlDocument document;
        auto &factory = document.factory();
        AttributeSet tableAttributes;
        tableAttributes.set(u"rowalign", std::u16string{u"baseline top"});
        tableAttributes.set(u"columnalign", std::u16string{u"left right"});
        tableAttributes.set(u"data-break-align", std::u16string{u"middle bottom"});
        auto &table = factory.make(NodeKind::mtable, std::move(tableAttributes));
        auto &row = factory.make(NodeKind::mtr);
        auto &first = factory.make(NodeKind::mtd);
        auto &second = factory.make(NodeKind::mtd);
        row.appendChild(first);
        row.appendChild(second);
        table.appendChild(row);
        table.setInheritedAttributes({}, false, 0.0, false);

        const auto *firstColumn = first.attributes().inherited(u"columnalign");
        const auto *secondColumn = second.attributes().inherited(u"columnalign");
        const auto *firstRow = first.attributes().inherited(u"rowalign");
        const auto *secondRow = second.attributes().inherited(u"rowalign");
        const auto *firstBreak = first.attributes().inherited(u"data-break-align");
        const auto *secondBreak = second.attributes().inherited(u"data-break-align");
        const auto *firstVertical = first.attributes().inherited(u"data-vertical-align");
        const auto *secondVertical = second.attributes().inherited(u"data-vertical-align");
        expect(firstRow && std::get<std::u16string>(*firstRow) == u"baseline");
        expect(secondRow && std::get<std::u16string>(*secondRow) == u"baseline");
        expect(firstColumn && std::get<std::u16string>(*firstColumn) == u"left");
        expect(secondColumn && std::get<std::u16string>(*secondColumn) == u"right");
        expect(firstBreak == nullptr);
        expect(secondBreak == nullptr);
        expect(firstVertical && std::get<std::u16string>(*firstVertical) == u"middle");
        expect(secondVertical && std::get<std::u16string>(*secondVertical) == u"bottom");
    };

    "table indent attributes move into the inherited layer"_test = [] {
        MmlDocument document;
        auto &factory = document.factory();
        AttributeSet tableAttributes;
        tableAttributes.set(u"indentalign", std::u16string{u"explicit-right"});
        tableAttributes.set(u"indentshift", std::u16string{u"explicit-shift"});
        tableAttributes.set(u"rowalign", std::u16string{u"baseline"});
        auto &table = factory.make(NodeKind::mtable, std::move(tableAttributes));
        AttributeList incoming{
            {u"indentalign", NodeKind::math, std::u16string{u"inherited-center"}},
            {u"indentshiftfirst", NodeKind::mstyle, std::u16string{u"1em"}}};
        table.setInheritedAttributes(incoming, false, 0.0, false);
        expect(table.attributes().own(u"indentalign") == nullptr);
        expect(table.attributes().inherited(u"indentalign") &&
               std::get<std::u16string>(*table.attributes().inherited(u"indentalign")) ==
                   u"inherited-center");
        expect(table.attributes().own(u"indentshift") == nullptr);
        expect(table.attributes().inherited(u"indentshiftfirst") &&
               std::get<std::u16string>(*table.attributes().inherited(u"indentshiftfirst")) ==
                   u"1em");
    };

    "labeled row reserves the side-aligned label column"_test = [] {
        MmlDocument document;
        auto &factory = document.factory();
        AttributeSet tableAttributes;
        tableAttributes.set(u"side", std::u16string{u"left"});
        tableAttributes.set(u"columnalign", std::u16string{u"center right"});
        tableAttributes.set(u"data-break-align", std::u16string{u"baseline bottom"});
        tableAttributes.set(u"rowalign", std::u16string{u"top"});
        auto &table = factory.make(NodeKind::mtable, std::move(tableAttributes));
        auto &row = factory.make(NodeKind::mlabeledtr);
        auto &label = factory.make(NodeKind::mtd);
        auto &value = factory.make(NodeKind::mtd);
        row.appendChild(label);
        row.appendChild(value);
        table.appendChild(row);
        table.setInheritedAttributes({}, false, 0.0, false);
        expect(row.arity() == 1.0);
        expect(std::get<std::u16string>(*label.attributes().inherited(u"columnalign")) == u"left");
        expect(std::get<std::u16string>(*value.attributes().inherited(u"columnalign")) ==
               u"center");
        expect(label.attributes().inherited(u"data-break-align") == nullptr);
        expect(value.attributes().inherited(u"data-break-align") == nullptr);
        expect(std::get<std::u16string>(*label.attributes().inherited(u"data-vertical-align")) ==
               u"top");
        expect(std::get<std::u16string>(*value.attributes().inherited(u"data-vertical-align")) ==
               u"baseline");
    };

    "verifyTree fixMtables true repairs direct table children"_test = [] {
        MmlDocument document;
        auto &factory = document.factory();
        auto &mi = factory.make(NodeKind::mi);
        auto &cell = factory.make(NodeKind::mtd);
        auto &mo = factory.make(NodeKind::mo);
        std::array<MmlNode *, 3> direct{&mi, &cell, &mo};
        auto &table = factory.make(NodeKind::mtable, {}, direct);
        VerifyOptions options{.checkArity = false,
                              .checkAttributes = false,
                              .checkMathvariants = false,
                              .fullErrors = false,
                              .fixMmultiscripts = true,
                              .fixMtables = true};
        table.verifyTree(options);
        expect(table.children().size() == 1_u);
        expect(table.children().front()->kind() == NodeKind::mtr);
        expect(table.children().front()->children().size() == 3_u);
        for (auto *child : table.children().front()->children())
            expect(child->kind() == NodeKind::mtd);
    };

    "verifyTree fixMtables false leaves an error node in the repaired cell"_test = [] {
        MmlDocument document;
        auto &factory = document.factory();
        auto &mi = factory.make(NodeKind::mi);
        std::array<MmlNode *, 1> direct{&mi};
        auto &table = factory.make(NodeKind::mtable, {}, direct);
        VerifyOptions options{.checkArity = false,
                              .checkAttributes = false,
                              .checkMathvariants = false,
                              .fullErrors = false,
                              .fixMmultiscripts = true,
                              .fixMtables = false};
        table.verifyTree(options);
        auto *row = table.children().front();
        expect(row->kind() == NodeKind::mtr);
        expect(row->children().front()->kind() == NodeKind::mtd);
        auto *inferred = row->children().front()->children().front();
        expect(inferred->kind() == NodeKind::inferred_mrow);
        expect(inferred->children().front()->kind() == NodeKind::merror);
    };

    "row verifyTree repairs a non-cell child and honors fixMtables false"_test = [] {
        MmlDocument document;
        auto &factory = document.factory();
        auto &mi = factory.make(NodeKind::mi);
        std::array<MmlNode *, 1> direct{&mi};
        auto &row = factory.make(NodeKind::mtr, {}, direct);
        VerifyOptions options{.checkArity = false,
                              .checkAttributes = false,
                              .checkMathvariants = false,
                              .fullErrors = false,
                              .fixMmultiscripts = true,
                              .fixMtables = false};
        row.verifyTree(options);
        expect(row.children().front()->kind() == NodeKind::mtd);
        auto *inferred = row.children().front()->children().front();
        expect(inferred->kind() == NodeKind::inferred_mrow);
        expect(inferred->children().front()->kind() == NodeKind::merror);
    };

    "maligngroup inherits explicit values and malignmark keeps its Source defaults"_test = [] {
        MmlDocument document;
        auto &factory = document.factory();
        AttributeSet groupAttributes;
        groupAttributes.set(u"groupalign", std::u16string{u"{left}"});
        groupAttributes.set(u"mathcolor", std::u16string{u"purple"});
        auto &child = factory.make(NodeKind::mi);
        std::array<MmlNode *, 1> children{&child};
        auto &group = factory.make(NodeKind::maligngroup, std::move(groupAttributes), children);
        group.setInheritedAttributes({}, false, 0.0, false);
        auto &mark = factory.make(NodeKind::malignmark);
        expect(child.attributes().inherited(u"groupalign") == nullptr);
        expect(std::get<std::u16string>(*child.attributes().inherited(u"mathcolor")) == u"purple");
        expect(std::get<std::u16string>(*mark.attributes().lookup(u"edge")) == u"left");
        expect(mark.arity() == 0.0);
        expect(mark.isSpacelike());
    };
    "typed childPosition uses logical parent slots across inferred cell rows"_test = [] {
        MmlDocument document;
        auto& factory = document.factory();
        auto& table = factory.make(NodeKind::mtable);
        auto& row = factory.make(NodeKind::mtr);
        auto& firstCell = factory.make(NodeKind::mtd);
        auto& secondCell = factory.make(NodeKind::mtd);
        auto& firstToken = factory.make(NodeKind::mi);
        auto& secondToken = factory.make(NodeKind::mi);

        // Mtd initialization owns an inferred mrow; appends go through it.
        firstCell.appendChild(firstToken);
        firstCell.appendChild(secondToken);
        row.appendChild(firstCell);
        row.appendChild(secondCell);
        table.appendChild(row);

        expect(firstCell.children().size() == 1_u);
        if (firstCell.children().empty()) return;
        auto* firstInferred = firstCell.children().front();
        expect(firstInferred->kind() == NodeKind::inferred_mrow);
        expect(firstInferred->children().size() == 2_u);
        expect(firstInferred->childIndex(secondToken) == std::optional<std::size_t>(1));

        // Cells resolve to their physical row slot. Token positions climb past
        // the inferred wrapper and report the wrapper's slot in the cell.
        expect(firstCell.Parent() == &row);
        expect(secondCell.Parent() == &row);
        expect(firstCell.childPosition() == std::optional<std::size_t>(0));
        expect(secondCell.childPosition() == std::optional<std::size_t>(1));
        expect(firstToken.Parent() == &firstCell);
        expect(secondToken.Parent() == &firstCell);
        expect(firstToken.childPosition() == std::optional<std::size_t>(0));
        expect(secondToken.childPosition() == std::optional<std::size_t>(0));
    };
    "logical positions cover detached nodes and nested inferred rows"_test = [] {
        MmlDocument document;
        auto &node = document.factory().make(NodeKind::mi);
        expect(node.Parent() == nullptr);
        expect(!node.childPosition().has_value());

        auto &root = document.factory().make(NodeKind::mrow);
        auto &first = document.factory().make(NodeKind::mi);
        auto &outer = document.factory().make(NodeKind::inferred_mrow);
        auto &inner = document.factory().make(NodeKind::inferred_mrow);
        document.append(root, first);
        document.append(root, outer);
        document.append(outer, inner);
        document.append(inner, node);

        expect(node.parent() == &inner);
        expect(inner.parent() == &outer);
        expect(node.Parent() == &root);
        expect(inner.Parent() == &root);
        expect(node.childPosition() == std::optional<std::size_t>(1));
        expect(inner.childPosition() == std::optional<std::size_t>(1));
    };

};
