'use strict';
const assert = require('node:assert/strict');
const path = require('node:path');
const fs = require('node:fs');
function findRepositoryRoot(start = __dirname) {
  for (let current = path.resolve(start); current !== path.dirname(current); current = path.dirname(current)) {
    if (fs.existsSync(path.join(current, 'tools/math-port/README.md')) &&
        fs.existsSync(path.join(current, 'tools/mathjax-bundle/node_modules/@mathjax/src/package.json'))) return current;
  }
  return null;
}
const rootArg = process.argv.indexOf('--repo-root');
if (rootArg >= 0 && (!process.argv[rootArg + 1] || process.argv.length !== rootArg + 2)) {
  throw new Error('usage: node mml-native-scripts-source-witness.cjs [--repo-root <repository>]');
}
const repo = rootArg >= 0 ? path.resolve(process.argv[rootArg + 1]) : findRepositoryRoot();
assert.ok(repo, 'pass --repo-root <repository> or run this script from within the repository');
const crypto = require('node:crypto');
const sourceRoot = path.join(repo, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const sourcePackage = require(path.join(sourceRoot, 'package.json'));
assert.equal(sourcePackage.version, '4.1.3');
const sourceHashes = {
  'cjs/core/MmlTree/MmlNodes/msubsup.js': '7971b391a8d7c92e0d6115b17c4c394dafc828942fc00591ba6064b871610c18',
  'cjs/core/MmlTree/MmlNodes/munderover.js': '104a6ed2f1a8967726c4a0da6ffd0ffb917c23fe0b3847138161c975fa0785b9',
  'cjs/core/MmlTree/MmlNodes/mmultiscripts.js': 'db40a3543cffbd706c0070ae0877285ce22b07db3a7e530e56f42eaa7b2d922e',
  'cjs/core/MmlTree/MmlNode.js': '86c5eb572dcf2652c3c52a60b66c720d231e17f600cc9aeef3ace4aeafc81d45',
};
for (const [file, expected] of Object.entries(sourceHashes)) {
  const actual = crypto.createHash('sha256').update(fs.readFileSync(path.join(sourceRoot, file))).digest('hex');
  assert.equal(actual, expected, `pinned Source hash ${file}`);
}
const cjs = path.join(repo, 'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/MmlTree/MmlNodes');
const { MmlMsubsup, MmlMsub, MmlMsup } = require(path.join(cjs, 'msubsup.js'));
const { MmlMunderover, MmlMunder, MmlMover } = require(path.join(cjs, 'munderover.js'));
const { MmlMmultiscripts, MmlMprescripts, MmlNone } = require(path.join(cjs, 'mmultiscripts.js'));
const { AbstractMmlNode } = require(path.join(repo, 'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/core/MmlTree/MmlNode.js'));
const rows = [];
function check(name, actual, expected) { assert.deepStrictEqual(actual, expected, name); rows.push({name, actual}); }
function spyNode(kind='mi', opts={}) {
  const node = { kind, calls: [], isEmbellished: !!opts.embellished,
    setInheritedAttributes(...args) { this.calls.push(args); },
    isKind(name) { return this.kind === name; }, verifyTree() {},
    mError(message) { (this.errors ||= []).push(message); },
    coreMO() { return { attributes: { get: name => name === 'accent' ? !!opts.coreAccent : false } }; } };
  return node;
}
const inherited = [{name:'mathcolor', origin:'math', value:'blue'}];
const sub = Object.create(MmlMsubsup.prototype); const subNodes=[spyNode(),spyNode(),spyNode()]; sub.childNodes=subNodes;
MmlMsubsup.prototype.setChildInheritedAttributes.call(sub,inherited,true,2,false);
check('msubsup inheritance and indices', {indices:[sub.base,sub.sub,sub.sup], calls:subNodes.map(n=>n.calls)},
 {indices:[0,1,2],calls:[[[inherited,true,2,false]],[[inherited,false,3,true]],[[inherited,false,3,false]]]});
const sup=Object.create(MmlMsup.prototype);const supNodes=[spyNode(),spyNode()];sup.childNodes=supNodes;
MmlMsubsup.prototype.setChildInheritedAttributes.call(sup,inherited,true,4,false);
check('msup remaps script slots', {arity:sup.arity,indices:[sup.base,sup.sub,sup.sup],calls:supNodes.map(n=>n.calls)},
 {arity:2,indices:[0,2,1],calls:[[[inherited,true,4,false]],[[inherited,false,5,false]]]});
function limitNode(ctor, names, values, nodes) {
 const x=Object.create(ctor.prototype); x.childNodes=nodes;
 x.attributes={get:key=>values[key]??false,hasExplicit:()=>false,setInherited:(key,value)=>{values[key]=value;},getDefault:key=>false};
 x.constructor=ctor; return x;
}
const base=spyNode();base.coreMO=()=>({attributes:{get:key=>false}});
const under=spyNode(), over=spyNode();
const underNode=limitNode(MmlMunderover,['accentunder'],{accentunder:true,accent:false},[base,under,over]);
MmlMunderover.prototype.setChildInheritedAttributes.call(underNode,inherited,true,3,false);
check('munder/over accent mapping and script levels', {indices:[underNode.base,underNode.under,underNode.over],calls:[base,under,over].map(n=>n.calls),linebreak:underNode.linebreakContainer},
 {indices:[0,1,2],calls:[[[inherited,true,3,true]],[[inherited,false,3,true]],[[inherited,false,4,false]]],linebreak:true});
const moverBase=spyNode();moverBase.coreMO=()=>({attributes:{get:key=>false}});const moverOver=spyNode();
const mover=limitNode(MmlMover,[],{accent:true,accentunder:false},[moverBase,moverOver]);
MmlMunderover.prototype.setChildInheritedAttributes.call(mover,inherited,true,7,false);
check('mover reverses accent name and child index', {arity:mover.arity,under:mover.under,over:mover.over,calls:[moverBase,moverOver].map(n=>n.calls)},
 {arity:2,under:2,over:1,calls:[[[inherited,true,7,true]],[[inherited,false,7,false]]]});
const accentBase=spyNode();accentBase.coreMO=()=>({attributes:{get:()=>false}});
const accentChild=spyNode('mo',{embellished:true,coreAccent:true});
const accentParent=limitNode(MmlMunder,[],{accentunder:false,accent:false},[accentBase,accentChild]);
MmlMunderover.prototype.setChildInheritedAttributes.call(accentParent,inherited,true,1,false);
check('embellished under operator propagates inferred accent', {accent:accentParent.attributes.get('accentunder'),reinherited:accentChild.calls.at(-1)},
 {accent:true,reinherited:[{},true,1,false]});

const baseScript=spyNode('mi'), post=spyNode('mi'), marker=spyNode('mprescripts'), pre=spyNode('mi');
const multi=Object.create(MmlMmultiscripts.prototype);multi.childNodes=[baseScript,post,marker,pre];multi.factory={create:kind=>spyNode(kind)};
multi.appendChild=function(n){this.childNodes.push(n);n.parent=this;return n;};
multi.childNodes.splice=Array.prototype.splice;
MmlMmultiscripts.prototype.setChildInheritedAttributes.call(multi,inherited,true,2,false);
check('multiscripts balances marker pair and script primes', {kinds:multi.childNodes.map(n=>n.kind),calls:multi.childNodes.map(n=>n.calls)},
 {kinds:['mi','mi','none','mprescripts','mi','none'],calls:[[[inherited,true,2,false]],[[inherited,false,3,true]],[],[],[[inherited,false,3,false]],[[inherited,false,3,false]]]});
const appendMulti=Object.create(MmlMmultiscripts.prototype);const appendBase=spyNode(), loneScript=spyNode();appendMulti.childNodes=[appendBase,loneScript];appendMulti.factory={create:kind=>spyNode(kind)};
appendMulti.appendChild=function(n){this.childNodes.push(n);n.parent=this;return n;};
MmlMmultiscripts.prototype.setChildInheritedAttributes.call(appendMulti,inherited,false,5,true);
check('multiscripts appends trailing none', {kinds:appendMulti.childNodes.map(n=>n.kind),lastCall:appendMulti.childNodes[2].calls},
 {kinds:['mi','mi','none'],lastCall:[[inherited,false,6,true]]});
const defaults={sub:Object.keys(MmlMsub.defaults),under:{accent:MmlMunder.defaults.accent,accentunder:MmlMunder.defaults.accentunder,align:MmlMunder.defaults.align},noneKeys:Object.keys(MmlNone.defaults)};
check('static default ownership',defaults,{sub:Object.keys(MmlMsub.defaults),under:{accent:false,accentunder:false,align:'center'},noneKeys:Object.keys(MmlNone.defaults)});
const oldVerify=AbstractMmlNode.prototype.verifyChildren,oldVerifyTree=AbstractMmlNode.prototype.verifyTree;
try {
 AbstractMmlNode.prototype.verifyChildren=function(options){for(const child of this.childNodes)child.verifyTree(options);};
 const mkVerify=(nodes)=>{const x=Object.create(MmlMmultiscripts.prototype);x.childNodes=nodes;x.errors=[];x.mError=message=>x.errors.push(message);return x;};
 let v=mkVerify([spyNode(),spyNode(),spyNode('mprescripts')]);MmlMmultiscripts.prototype.verifyChildren.call(v,{fixMmultiscripts:false});
 check('verify rejects unpaired prescript when repair disabled',v.errors,['There must be an equal number of prescripts of each type','There must be an equal number of scripts of each type']);
 v=mkVerify([spyNode(),spyNode('mprescripts'),spyNode(),spyNode('mprescripts'),spyNode()]);MmlMmultiscripts.prototype.verifyChildren.call(v,{fixMmultiscripts:true});
 check('verify always rejects duplicate prescript marker',v.childNodes.flatMap(n=>n.errors||[]).concat(v.errors),['mprescripts can only appear once in mmultiscripts']);
 AbstractMmlNode.prototype.verifyTree=function(){};
 const orphan=Object.create(MmlNone.prototype);orphan.parent={isKind:()=>false};orphan.mError=message=>(orphan.errors||=[]).push(message);
 MmlNone.prototype.verifyTree.call(orphan,{});
 check('none enforces multiscripts parent',orphan.errors,['none must be a child of mmultiscripts']);
} finally {AbstractMmlNode.prototype.verifyChildren=oldVerify;AbstractMmlNode.prototype.verifyTree=oldVerifyTree;}
const result = {package: sourcePackage.name, version: sourcePackage.version, sourceHashes, sourceRoot:path.relative(repo,cjs).replaceAll('\\','/'),passed:rows.length,cases:rows};
const output=path.join(repo,'build/math-port/typed-mml-scripts/source-witness.json');
fs.mkdirSync(path.dirname(output),{recursive:true});
fs.writeFileSync(output,JSON.stringify(result,null,2)+'\n');
console.log(JSON.stringify({output:path.relative(repo,output),...result},null,2));
