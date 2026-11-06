#include "support/folia_test.hpp"
import folia.mermaid.tree_view;
namespace { using namespace boost::ut; using namespace folia::mermaid;
suite tree_view=[] {
 "mermaid_tree_view_indentation_annotations_and_default_root"_test=[] {
  auto d=parse_tree_view(u"treeView-beta\nroot/\n  \"long file.txt\" :::highlight icon(none) ## Main file\n    child.md\nsibling");
  expect(fatal(d.has_value()))<<(d?"":d.error());if(!d)return;
  expect(d->nodes.size()==5_u);expect(d->nodes[0].name==u"/");expect(d->nodes[0].type==TreeViewNodeType::directory);
  expect(d->nodes[1].name==u"root");expect(d->nodes[1].level==1_u);expect(d->nodes[1].type==TreeViewNodeType::directory);
  expect(d->nodes[2].name==u"long file.txt");expect(d->nodes[2].level==2_u);expect(d->nodes[2].css_class==u"highlight");expect(d->nodes[2].icon==u"none");expect(d->nodes[2].description==u"Main file");
  expect(d->nodes[3].level==3_u);expect(d->nodes[4].name==u"sibling");expect(d->nodes[4].level==1_u);
 };
 "mermaid_tree_view_box_drawing_and_metadata"_test=[] {
  auto d=parse_tree_view(u"treeView-beta\nRepository/\n├── src/\n│   └── main.cpp\n└── README.md\ntitle Project\naccTitle: Files\naccDescr: Source tree");
  expect(fatal(d.has_value()))<<(d?"":d.error());if(!d)return;
  expect(d->nodes.size()==5_u);expect(d->nodes[1].name==u"Repository");expect(d->nodes[2].name==u"src");expect(d->nodes[2].level==2_u);expect(d->nodes[3].name==u"main.cpp");expect(d->nodes[3].level==3_u);expect(d->nodes[4].name==u"README.md");expect(d->title==u"Project");expect(d->acc_title==u"Files");expect(d->acc_description==u"Source tree");
 };
 "mermaid_tree_view_rejects_missing_keyword_and_malformed_annotations"_test=[] {
  expect(!parse_tree_view(u"treeView\nfile"));expect(!parse_tree_view(u"treeView-beta\nfile icon(broken"));expect(!parse_tree_view(u"treeView-beta\n\"open name"));
 };
}; }
