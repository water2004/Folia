#include "support/folia_test.hpp"
import folia.mermaid.usecase;
namespace { using namespace boost::ut; using namespace folia::mermaid;
suite usecase=[] {
 "mermaid_usecase_beta_models_actors_and_relationships"_test=[] {
  auto d=parse_usecase(u"usecase-beta\nactor User\nactor Admin\nUser --> Login\nLogin ..> : include Verify\n");expect(fatal(d.has_value()));
  expect(fatal(d->nodes.size()==4_u));expect(fatal(d->edges.size()==2_u));expect(bool(d->nodes[0].kind==UsecaseNodeKind::actor));expect(bool(d->nodes[2].label==u"Login"));expect(bool(d->edges[1].kind==UsecaseRelationship::include));
 };
 "mermaid_usecase_beta_preserves_quoted_labels_and_direction"_test=[] {
  auto d=parse_usecase(u"usecase-beta\ndirection TB\nactor \"Customer Service\"\n\"Customer Service\" --> \"Sign In\"\n");expect(fatal(d.has_value()));expect(bool(d->direction==u"TB"));expect(fatal(d->edges.size()==1_u));expect(bool(d->nodes[0].label==u"Customer Service"));expect(bool(d->nodes[1].label==u"Sign In"));
 };
 "mermaid_usecase_rejects_wrong_header_and_unknown_syntax"_test=[] {expect(!parse_usecase(u""));expect(!parse_usecase(u"flowchart LR\nA-->B"));expect(!parse_usecase(u"usecase-beta\nnot a statement"));};
}; }
