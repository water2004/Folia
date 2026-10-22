// Root-owned differential fixtures: reference results are obtained from upstream,
// never from a native implementation. Agents may not edit this file or its output.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '../..');
const npmRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
assert.equal(require(path.join(npmRoot, 'package.json')).version, '4.1.3');
const sources = {
  attributes: ['core/MmlTree/Attributes', 'c1e363944e0c059c7ed554b4b6aeb8ea56d43c844f84934c267ccc79b090468a'],
  bbox: ['util/BBox', 'a4232fac0fce35a15ad05a4dbb4af0999bf04f90ec3f0fe0c594b22c5da70b5b'],
  priority: ['util/PrioritizedList', 'a700459f984ba1a5f18bf0e7dc183c242961b94c33587a9aa81bbaacf97180c8']
};
for (const [source, hash] of Object.values(sources)) {
  const bytes = fs.readFileSync(path.join(root, '.reference/MathJax-src/ts', source + '.ts'));
  assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'), hash);
  // Ensure the reference executable actually comes from the same pinned sources.
  assert.equal(fs.readFileSync(path.join(npmRoot, 'ts', source + '.ts'), 'utf8').replaceAll('\r\n', '\n'),
    bytes.toString('utf8').replaceAll('\r\n', '\n'));
}
const {Attributes, INHERIT} = require(path.join(npmRoot, 'cjs/core/MmlTree/Attributes.js'));
const {BBox} = require(path.join(npmRoot, 'cjs/util/BBox.js'));
const {PrioritizedList} = require(path.join(npmRoot, 'cjs/util/PrioritizedList.js'));
const attributeRecords=[],bboxSnapshots=[],prioritySnapshots=[],objectExpressions=new Map();
const numeric=x=>Number.isNaN(x)?'NaN':Object.is(x,-0)?'-0':x===Infinity?'Infinity':x===-Infinity?'-Infinity':x;
function property(v){if(v===undefined)return{type:'missing'};if(typeof v==='number')return{type:'number',value:numeric(v)};if(typeof v==='string')return{type:'string',value:v};if(typeof v==='boolean')return{type:'boolean',value:v};const builtins={constructor:'constructor',__defineGetter__:'defineGetter',__defineSetter__:'defineSetter',hasOwnProperty:'hasOwnProperty',__lookupGetter__:'lookupGetter',__lookupSetter__:'lookupSetter',isPrototypeOf:'isPrototypeOf',propertyIsEnumerable:'propertyIsEnumerable',toString:'toString',valueOf:'valueOf',toLocaleString:'toLocaleString'};for(const[key,name]of Object.entries(builtins))if(v===Object.prototype[key])return{type:'builtin',name};if(v===Object.prototype)return{type:'builtin',name:'prototype'};if(objectExpressions.has(v))return{type:'view',name:objectExpressions.get(v)};throw Error('Unencoded Source property');}
const global = {a: 'global', g: 'global-only', marker: 'global-marker', f: true, z: 3, empty: 'g', '9': 9, '2': 2};
const defaults = {a: 'default', marker: INHERIT, f: false, z: 0, empty: '', '4294967295': 'max', '01': 'zero-one'};
;
;
;
const attrs = new Attributes(defaults, global);
for (const method of ['getAllAttributes', 'getAllInherited', 'getAllDefaults', 'getAllGlobals'])
  objectExpressions.set(attrs[method](),method);
const names = ['a', 'g', 'marker', 'f', 'z', 'empty', 'missing', 'new', 'toString', 'constructor', '__proto__', '9', '2', '01'];
function attributesSnapshot(){for(const method of ['get','getExplicit','getInherited','getDefault'])attributeRecords.push(['values',names.map(n=>property(attrs[method](n)))]);for(const method of ['hasExplicit','isSet','hasDefault'])attributeRecords.push(['booleans',names.map(n=>attrs[method](n))]);for(const method of ['getExplicitNames','getInheritedNames','getDefaultNames','getGlobalNames'])attributeRecords.push(['names',[...attrs[method]()]]);const list=attrs.getList(...names);attributeRecords.push(['names',Object.keys(list)]);attributeRecords.push(['values',names.map(n=>property(Object.hasOwn(list,n)?list[n]:undefined))]);for(const ns of [[],['missing'],['f','z'],['new','a']])attributeRecords.push(['boolean',attrs.hasOneOf(ns)]);}
attributesSnapshot();
const ops = [
  ['setInherited', 'a', 'inherited'], ['setInherited', 'f', false], ['setInherited', 'z', 0],
  ['setInherited', 'marker', INHERIT], ['set', 'a', 'explicit'], ['set', 'f', false],
  ['set', 'z', 0], ['set', 'empty', ''], ['set', 'missing', undefined], ['set', 'marker', INHERIT],
  ['unset', 'a'], ['unset', 'missing'], ['unset', 'g'], ['set', '__proto__', 'ignored'],
  ['set', '9', 'nine'], ['set', '2', 'two'], ['unset', '9'], ['set', '9', 'nine-again']
];
for (const [method, key, value] of ops) {
  if (method === 'unset') {attrs.unset(key); ;}
  else {attrs[method](key, value); ;}
  attributesSnapshot();
}
const updates = {new: 'list', a: false, '1': 1};
attrs.setList(updates); ; attributesSnapshot();
global.g = 'mutated'; ; attributesSnapshot();
defaults.a = 'must-not-change'; ; attributesSnapshot();
for (const [method, key, value] of [['getAllAttributes', 'a', 'handle'], ['getAllInherited', 'g', 0],
  ['getAllDefaults', 'g', false], ['getAllGlobals', 'marker', 'live-global']]) {
  attrs[method]()[key] = value;
  ;
  attributesSnapshot();
}
// Shadowing prototype-sensitive keys is allowed, but scalar __proto__ assignment is ignored.
const oddGlobal = JSON.parse('{"__proto__":"own-global","toString":"own-string"}');
const odd = new Attributes({}, oddGlobal); odd.set('__proto__', 'own-explicit');
;
;
;
; ;
;

;
attributeRecords.push(['value',property(odd.get('__proto__'))],['value',property(odd.get('toString'))],['names',[...odd.getExplicitNames()]]);
const fields = ['w', 'h', 'd', 'scale', 'rscale', 'L', 'R', 'ic', 'oc', 'sk', 'dx'];
function bboxSnapshot(variable,box){bboxSnapshots.push({values:fields.map(f=>numeric(box[f])),pwidth:box.pwidth});}
;
for (const [i, def] of [{}, {w: 0, h: 0, d: 0}, {w: -0, h: -0, d: -0}, {w: NaN, h: NaN, d: NaN},
  {w: 2}, {h: -2, d: -3}, {w: Infinity, h: -Infinity, d: Infinity}].entries()) {
  const b = new BBox(def); ; bboxSnapshot(`b${i}`, b);
}
let b = BBox.empty(); ; bboxSnapshot('box', b);
; b.clean(); bboxSnapshot('box', b);
; b = BBox.zero(); bboxSnapshot('box', b);
for (const f of fields.slice(3)) {b[f] = fields.indexOf(f) / 7; ;}
b.pwidth = '83%'; ;
let seed = 0x9e3779b9;
function random() { seed ^= seed << 13; seed ^= seed >>> 17; seed ^= seed << 5; return (seed >>> 0) / 0x100000000; }
for (let i = 0; i < 32; i++) {
  const c = new BBox({w: random() * 4 - 1, h: random() * 3 - 1, d: random() * 2 - 1});
  c.rscale = random() * 2; c.L = random() - 0.5; c.R = random() - 0.5;
  c.scale = 0.7; c.ic = 0.4; c.oc = 0.3; c.sk = 0.2; c.dx = 0.1; c.pwidth = i % 3 ? '' : '100%';
  ;
  for (const f of fields.slice(3)) ;
  ;
  const operation = i % 5;
  if (operation === 0) {const x=random()-0.5,y=random()-0.5; b.combine(c,x,y); ;}
  if (operation === 1) {b.append(c); ;}
  if (operation === 2) {b.updateFrom(c); ;}
  if (operation === 3) {const scale=random()*2-1; b.rescale(scale); ;}
  if (operation === 4) {b.empty(); ;}
  bboxSnapshot('box', b); ;
}
let copied=b.copy(); ; bboxSnapshot('copied',copied);
b.w=99; ; bboxSnapshot('copied',copied);
b = new BBox({w:1,h:2,d:3}); b.rscale=0.7; b.L=0.1;b.R=0.2;
;
b.append(b); ; bboxSnapshot('box', b);
b.combine(b,1,-1); ; bboxSnapshot('box', b);
;
for (const [i, [name, index, dimension]] of BBox.boxSides.entries()) {
  ;
  ;
  ;
}
;

;
const list = new PrioritizedList(); ;
// Checks for the deliberate C++ iterator adaptation, independently of the JS
// value snapshots. These protect native callers of the port's range interface.
;
;
function prioritySnapshot(){const values=[...list];prioritySnapshots.push({items:values.map(v=>numeric(v.item)),priorities:values.map(v=>numeric(v.priority))});}
for (const [item, priority] of [[1,5],[2,5],[3,-7],[1,9],[4,0],[5,NaN],[6,Infinity],[7,-Infinity],[NaN,2]]) {
  list.add(item,priority); ; prioritySnapshot();
}
;
for (const item of [1,NaN,99,2,1]) {
  list.remove(item); ; prioritySnapshot();
}
PrioritizedList.DEFAULTPRIORITY = 13; list.add(88); ; prioritySnapshot();
const crossType = new PrioritizedList(); crossType.add(17);
;
; ;
PrioritizedList.DEFAULTPRIORITY = 5; ;
const dynamic = new PrioritizedList(); dynamic.add(1,0); dynamic.add(2,1);
const it = dynamic[Symbol.iterator](); const seen=[];
let entry=it.next(); seen.push(entry.value.item); dynamic.remove(1); dynamic.add(3,2);
while (!(entry=it.next()).done) seen.push(entry.value.item);
;
; ;
const retaining = new PrioritizedList(); retaining.add(1); retaining.add(2);
const yielded=retaining[Symbol.iterator]().next().value; retaining.remove(1); yielded.priority=37;
;
; ;
; ;
;
; ;
; ;

const report=({attributes:attributeRecords,bbox:{snapshots:bboxSnapshots,fullWidth:BBox.fullWidth,boxSides:BBox.boxSides.map(row=>[...row])},priority:{snapshots:prioritySnapshots,crossTypeDefault:numeric([...crossType][0].priority),dynamicSeen:[...seen],yielded:[numeric(yielded.item),numeric(yielded.priority)]}});const gold=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/primitives-source.json'),'utf8'));assert.ok(process.argv.includes('--check'),'Source owner is check-only');assert.deepEqual(report,gold);console.log(JSON.stringify({profile:'primitives-source.json',cases:report.cases?.length??report.rows?.length,mode:'check'}));
