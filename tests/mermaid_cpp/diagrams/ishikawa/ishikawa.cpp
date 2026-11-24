#include "support/folia_test.hpp"
import folia.mermaid.ishikawa;
namespace { using namespace boost::ut; using namespace folia::mermaid;
suite ishikawa=[] {
 "mermaid_ishikawa_indentation_tree_matches_12_2_0"_test=[] {
  auto d=parse_ishikawa(u"ishikawa-beta\n    Blurry Photo\n        Process\n            Out of focus\n        User\n            Shaky hands\n        Materials\n          Paper\n            Humidity\n");
  expect(fatal(d.has_value()));
  expect(bool(d->root.text==u"Blurry Photo"));
  expect(fatal(d->root.children.size()==3_u));
  const auto& process = d->root.children[0];
  const auto& user = d->root.children[1];
  const auto& materials = d->root.children[2];
  expect(bool(process.text==u"Process"));
  expect(bool(user.text==u"User"));
  expect(bool(materials.text==u"Materials"));
  expect(fatal(process.children.size()==1_u));
  expect(fatal(user.children.size()==1_u));
  expect(fatal(materials.children.size()==1_u));
  expect(bool(process.children[0].text==u"Out of focus"));
  expect(bool(user.children[0].text==u"Shaky hands"));
  const auto& paper = materials.children[0];
  expect(bool(paper.text==u"Paper"));
  expect(fatal(paper.children.size()==1_u));
  expect(bool(paper.children[0].text==u"Humidity"));
 };
 "mermaid_ishikawa_unindented_root_and_relative_causes"_test=[] {
  auto d=parse_ishikawa(u"ishikawa\nProblem\nCause A\n  Subcause A1\nCause B\n");expect(fatal(d.has_value()));
  expect(fatal(d->root.children.size()==2_u));
  expect(fatal(d->root.children[0].children.size()==1_u));
  expect(bool(d->root.children[0].children[0].text==u"Subcause A1"));
 };
 "mermaid_ishikawa_invalid_syntax_is_rejected"_test=[] {
  expect(!parse_ishikawa(u""));expect(!parse_ishikawa(u"flowchart LR\nA-->B"));expect(!parse_ishikawa(u"ishikawa-beta\n"));
 };
}; }
