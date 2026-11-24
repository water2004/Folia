#include "support/folia_test.hpp"
import folia.math.core.mml_tree.mml_document;
import folia.math.core.mml_tree.mml_nodes.mrow;
import folia.math.core.mml_tree.mml_nodes.mo;

using namespace boost::ut;
namespace mml = folia::math::mml;

suite native_mml_exact_clone_row = [] {
    "cross_document_clone_recomputes_row_core_without_source_links"_test = [] {
        mml::MmlDocument destination;
        mml::MmlMrow *clonedRow = nullptr;
        mml::MmlNode *clonedOperator = nullptr;

        {
            mml::MmlDocument source;
            auto &sourceFactory = source.factory();
            auto &sourceRow = dynamic_cast<mml::MmlMrow &>(sourceFactory.make(mml::NodeKind::mrow));
            auto &sourceOperator = sourceFactory.make(mml::NodeKind::mo);
            source.append(sourceRow, sourceOperator);

            // `core()` warms the row's borrowed cache before the exact clone.
            expect(sourceRow.isEmbellished());
            expect(sourceRow.core() == &sourceOperator);
            expect(sourceRow.coreMO() == &sourceOperator);

            clonedRow = dynamic_cast<mml::MmlMrow *>(&destination.cloneSubtree(sourceRow));
            expect(clonedRow != nullptr);
            expect(&clonedRow->document() == &destination);
            expect(clonedRow->children().size() == 1u);
            clonedOperator = clonedRow->children().front();
            expect(clonedOperator != &sourceOperator);
            expect(&clonedOperator->document() == &destination);
            expect(clonedRow->core() == clonedOperator);
            expect(clonedRow->coreMO() == clonedOperator);

            // Mutating the source tree after cloning cannot affect the target.
            source.remove(sourceOperator);
            expect(sourceRow.core() == &sourceRow);
            expect(clonedRow->core() == clonedOperator);
        }

        // Destroying the source document leaves all cloned links valid.
        expect(clonedRow->children().size() == 1u);
        expect(clonedRow->children().front() == clonedOperator);
        expect(clonedRow->core() == clonedOperator);

        // Replacing the cloned embellished child invalidates its semantic core;
        // adding a new embellished child causes core lookup to recompute again.
        destination.remove(*clonedOperator);
        expect(!clonedRow->isEmbellished());
        expect(clonedRow->core() == clonedRow);
        auto &replacement = destination.factory().make(mml::NodeKind::mo);
        destination.append(*clonedRow, replacement);
        expect(clonedRow->isEmbellished());
        expect(clonedRow->core() == &replacement);
        expect(clonedRow->coreMO() == &replacement);
    };

    "inferred_row_clone_keeps_concrete_type_and_destination_core"_test = [] {
        mml::MmlDocument destination;
        mml::MmlInferredMrow *clonedRow = nullptr;
        mml::MmlNode *clonedOperator = nullptr;

        {
            mml::MmlDocument source;
            auto &sourceRow = dynamic_cast<mml::MmlInferredMrow &>(
                source.factory().make(mml::NodeKind::inferred_mrow));
            auto &sourceOperator = source.factory().make(mml::NodeKind::mo);
            source.append(sourceRow, sourceOperator);

            expect(sourceRow.isEmbellished());
            expect(sourceRow.core() == &sourceOperator);

            clonedRow = dynamic_cast<mml::MmlInferredMrow *>(&destination.cloneSubtree(sourceRow));
            expect(clonedRow != nullptr);
            expect(clonedRow->kind() == mml::NodeKind::inferred_mrow);
            expect(clonedRow->isInferred());
            expect(clonedRow->notParent());
            clonedOperator = clonedRow->children().front();
            expect(clonedOperator != &sourceOperator);
            expect(&clonedOperator->document() == &destination);
        }

        // The inherited row clone hook leaves no core link into the destroyed
        // source document; lookup resolves to the actual destination child.
        expect(clonedRow->children().size() == 1u);
        expect(clonedRow->core() == clonedOperator);
        expect(clonedRow->coreMO() == clonedOperator);

        destination.remove(*clonedOperator);
        expect(clonedRow->core() == clonedRow);
        auto &replacement = destination.factory().make(mml::NodeKind::mo);
        destination.append(*clonedRow, replacement);
        expect(clonedRow->core() == &replacement);
        expect(clonedRow->isInferred());
    };
};
