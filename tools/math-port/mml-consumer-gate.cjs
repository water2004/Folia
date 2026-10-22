// Root checks the actual restored consumer graph against independent Source gold.
'use strict';
const fs=require('node:fs'),path=require('node:path');
const root=path.resolve(__dirname,'../..'),cmake=fs.readFileSync(path.join(root,'CMakeLists.txt'),'utf8');
const modules=[...cmake.match(/target_sources\(folia_math PUBLIC FILE_SET CXX_MODULES FILES([\s\S]*?)\)/)[1].matchAll(/src\/math\/(\S+\.ixx)/g)].map(x=>x[1]),units=[...cmake.match(/target_sources\(folia_math PRIVATE([\s\S]*?)\)/)[1].matchAll(/src\/math\/(\S+\.cpp)/g)].map(x=>x[1]);
const mutants=[];
function body(file,sig){const source=fs.readFileSync(path.join(root,'src/math',file),'utf8').replaceAll('\r\n','\n'),start=source.indexOf(sig);if(start<0)throw Error(sig);const open=source.indexOf('{',start);let depth=1,end=open+1;for(;depth;end++){if(source[end]==='{')depth++;if(source[end]==='}')depth--;}return source.slice(open+1,end-1);}
function method(file,sig){const original=body(file,sig);return(name,from,to,all=false)=>{const count=original.split(from).length-1;if(all?count<1:count!==1)throw Error('Unique '+name);mutants.push({name,file,edits:[[original,all?original.replaceAll(from,to):original.replace(from,to)]]});};}
const base='core/mml_tree/mml_node.cpp',row='core/mml_tree/mml_nodes/mrow.cpp';
const error=method(base,'MmlNode* AbstractMmlNode::mError('),spacing=method(base,'bool AbstractMmlNode::hasSpacingAttributes(');
error('error-parent-cached','getParent().requireNode("isKind")','initialParent.requireNode("isKind")');
error('error-parent-shortcut-omitted','if (initialParent.isNode() &&\n        getParent().requireNode("isKind")->isKind("merror")) return nullptr;','');
error('error-options-normalized','const Property initialFullErrors = host::get(options, "fullErrors");','const Property initialFullErrors = host::get(PropertyList{}, "fullErrors");');
error('error-undefined-options-become-object','const NodeValue initialParent = getParent();','if(options.missing())options=PropertyList{}; const NodeValue initialParent = getParent();');
error('error-full-boolean-only','host::truthy(initialFullErrors)','initialFullErrors == Property(true)');
error('error-short-ignored',' || shortError','');
error('error-message-not-written','initialErrorAttributes->set("data-mjx-message", message);','');
error('error-factory-order-swapped','MmlNode* const mtext = factory.create("mtext", {});\n        MmlNode* const text = factory.create("text", {});','MmlNode* const text = factory.create("text", {});\n        MmlNode* const mtext = factory.create("mtext", {});');
error('error-text-full-value-cached','host::truthy(textFullErrors)','host::truthy(initialFullErrors)');
error('error-kind-instead-of-full-message','? host::text(message)','? host::text(kind())');
error('error-text-not-appended','NodeValue(mtext).requireNode("appendChild")->appendChild(NodeValue(text));','');
error('error-mtext-not-appended','merror->appendChild(NodeValue(mtext));','');
error('error-title-full-value-cached','!host::truthy(titleFullErrors)','!host::truthy(initialFullErrors)');
error('error-title-attributes-cached','auto titleAttributes = titleNode->attributes();','auto titleAttributes = initialErrorAttributes;');
error('error-wrap-append-before-replace','getParent().requireNode("replaceChild")->replaceChild(NodeValue(merror), NodeValue(this));\n        merror->appendChild(NodeValue(this));','merror->appendChild(NodeValue(this));\n        getParent().requireNode("replaceChild")->replaceChild(NodeValue(merror), NodeValue(this));');
error('error-entry-budget-omitted','tree::TreeCallGuard<MmlNode> guard;','');
spacing('spacing-shortcut-omitted','if (!isEmbellished()) return false;','');
spacing('spacing-uses-core','coreMO()','core()');
spacing('spacing-entry-budget-omitted','tree::TreeCallGuard<MmlNode> guard;','');
const backward=method(row,'MmlNode* MmlMrow::lastNonSpace('),rowClass=method(row,'NodeValue MmlMrow::setTeXclass('),joined=method(row,'std::string MmlInferredMrow::toString(');
backward('row-backward-array-cached','const auto children = this->childNodes;','const auto children = initialChildren;');
rowClass('row-class-length-frozen','for (std::size_t i = 0; i < children->size(); ++i)','const auto length=children->size();\n        for (std::size_t i = 0; i < length; ++i)',true);
rowClass('row-delimiter-check-truthiness','!host::nullish(getProperty("open")) || !host::nullish(getProperty("close"))','host::truthy(getProperty("open")) || host::truthy(getProperty("close"))');
rowClass('row-current-first-child-cached','const auto currentChildren = this->childNodes;','const auto currentChildren = children;');
joined('inferred-join-ignores-captured-length','i < length','i < children->size()');
const mi=method('core/mml_tree/mml_nodes/mi.cpp','NodeValue MmlMi::setTeXclass('),style=method('core/mml_tree/mml_nodes/mstyle.cpp','void MmlMstyle::setChildInheritedAttributes(');
mi('mi-autoop-null-counts-missing','autoOP.missing()','host::nullish(autoOP)');
mi('mi-class-property-null-counts-missing','texClassProperty.missing()','host::nullish(texClassProperty)');
mi('mi-skips-virtual-class-setter','setTexClass(static_cast<double>(TEXCLASS.OP));','initializeTexClass(static_cast<double>(TEXCLASS.OP));');
style('style-level-addition-normalized','host::add(level, Property(host::integer(text)))','Property(host::number(level)+host::integer(text))');
style('style-cramped-normalized','prime = cramped;','prime = host::truthy(cramped);');
style('style-displaystyle-truthiness','displaystyle == Property(true)','host::truthy(displaystyle)');
const math=method('core/mml_tree/mml_nodes/math.cpp','void MmlMath::verifyTree(');
math('math-local-default-omitted','if (options.missing()) options = PropertyNull{};','');
math('math-super-omitted','AbstractMmlNode::verifyTree(options);','');
math('math-null-skips-derived-check','AbstractMmlNode::verifyTree(options);','AbstractMmlNode::verifyTree(options); if(std::holds_alternative<PropertyNull>(options.value))return;');

for(const name of ['MmlMprescripts','MmlNone']){const methodChange=method('core/mml_tree/mml_nodes/mmultiscripts.cpp','void '+name+'::verifyTree(');methodChange(name+'-super-omitted','AbstractMmlNode::verifyTree(options);','');methodChange(name+'-raw-options-normalized','AbstractMmlNode::verifyTree(options);','if(options.missing())options=PropertyNull{}; AbstractMmlNode::verifyTree(options);');}
require('./native-mutation-runner.cjs')({name:'math-port-mml-consumers',modules,units,supportFiles:['util/option_storage.hpp'],fixture:'reference_mml_error.cpp',additionalFixtures:['reference_mml_node.cpp','reference_mml_verification_parents.inc','reference_mml_spacing.cpp'],pattern:'*math_port*',assertions:4612,tests:134,suiteSummaries:[{assertions:3208,tests:2},{assertions:1084,tests:43},{assertions:288,tests:88},{assertions:32,tests:1}],mutants});
