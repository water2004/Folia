// Pinned original NodeUtil observations; Native code only performs the calls.
'use strict';const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {root,MmlFactory,core}=require(path.join(process.env.FOLIA_MATH_PORT_TOOLS||__dirname,'simple-mml-reference.cjs')),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const source='ts/input/tex/NodeUtil.ts',pin=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/scope.json'))).files.find(f=>f.source===source);
const bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),pin.source_sha256);assert.equal(bytes.toString().replaceAll('\r\n','\n'),fs.readFileSync(path.join(base,source),'utf8').replaceAll('\r\n','\n'));
const u=require(path.join(base,'cjs/input/tex/NodeUtil.js')).default;
const value=v=>v===undefined?'U':v===null?'N':typeof v+':'+String(v);
const rows=[];function add(label,observe){let expected;try{expected=observe();}catch(e){expected=e.name+'|'+e.message;}rows.push({label,expected});}

const n=(kind='mo',text='+')=>{const f=new MmlFactory();return f.create(kind,{},[f.create('text').setText(text)]);};

for(const code of ['2026','22EF','1D538','0x41','  +41tail','10ffff','D800','-1','110000','xyz',''])add('entity:'+code,()=>hex(u.createEntity(code)));
function hex(s){return Array.from({length:s.length},(_,i)=>s.charCodeAt(i).toString(16).padStart(4,'0')).join(',');}
add('children-alias',()=>{let node=new MmlFactory().create('mrow'),a=u.getChildren(node);a.push(new MmlFactory().create('mi'));return(a===node.childNodes?'same':'copy')+':'+node.childNodes.length;});
add('children-null-array',()=>{let node=new MmlFactory().create('mrow');node.childNodes=null;return u.getChildren(node)?'array':'null';});
for(const kind of ['text','mi','mo','mtext','mrow'])add('text:'+kind,()=>u.getText(kind==='text'?n().childNodes[0]:n(kind)));
// Replace text observations with their character-exact UTF-16 representation.
for(const row of rows)if(row.label.startsWith('text:')&&!row.expected.includes('|'))row.expected=hex(row.expected);
add('append',()=>{let f=new MmlFactory(),node=f.create('mrow'),a=[f.create('mi'),f.create('mn')];u.appendChildren(node,a);return node.childNodes.length+':'+node.childNodes[0].kind+':'+(a[1].parent===node?'parent':'bad');});
add('append-empty-null',()=>{u.appendChildren(null,[]);return'ok';});
add('append-null-array',()=>{u.appendChildren(null,null);return'ok';});
for(const tag of ['U','N'])for(const form of ['texClass','inferred','empty']){
 const props=form==='empty'?{}:{[form]:true},arg=tag==='U'?undefined:null;
 add('properties-null:'+tag+':'+form,()=>{u.setProperties(arg,props);return'ok';});
}
for(const tag of ['U','N'])for(const op of ['getChildren','getText','setAttribute','setProperty','getProperty','getAttribute','removeAttribute','removeProperties','isType','isEmbellished','getTexClass','getCoreMO','isInferred','getForm','getOp','getMoAttribute']){
 const arg=tag==='U'?undefined:null;

 const jsExtra={setAttribute:['x',1],setProperty:['x',1],getProperty:['x'],getAttribute:['x'],removeAttribute:['x'],isType:['mo'],getMoAttribute:['accent']}[op]||[];
 add(op+':'+tag,()=>{u[op](arg,...jsExtra);return'ok';});
}
add('set-attribute',()=>{let node=n('mi');u.setAttribute(node,'mathcolor','red');return value(u.getAttribute(node,'mathcolor'));});
add('set-property',()=>{let node=n('mi');u.setProperty(node,'fnOP',false);return value(u.getProperty(node,'fnOP'));});
for(const key of [...u.attrs,'fnop','mathcolor'])add('route:'+key,()=>{let node=n('mi');u.setProperties(node,{[key]:false});return value(node.getProperty(key))+':'+value(node.attributes.get(key));});
add('remove-attribute',()=>{let node=n('mi');node.attributes.set('mathcolor','red');u.removeAttribute(node,'mathcolor');return value(node.attributes.get('mathcolor'));});
add('remove-properties',()=>{let node=n('mi');node.setProperty('a',1);node.setProperty('b',2);node.setProperty('c',3);u.removeProperties(node,'a','b');return value(node.getProperty('a'))+':'+value(node.getProperty('c'));});
for(const kind of ['mi','mo','mstyle'])add('properties:'+kind,()=>{let node=n(kind);u.setProperties(node,{texClass:3,movablelimits:false,inferred:true,fnOP:true,mathcolor:'red'});return[node.texClass,node.getProperty('texClass'),node.getProperty('movablelimits'),node.attributes.get('movablelimits'),node.getProperty('inferred'),node.getProperty('fnOP'),node.attributes.get('mathcolor')].map(value).join(':');});
add('copy-attributes',()=>{let old=n('mi'),next=n('mn');old.setProperty('a',false);old.attributes.set('mathcolor','red');u.copyAttributes(old,next);old.attributes.set('mathcolor','blue');return(old.attributes===next.attributes?'same':'copy')+':'+value(next.getProperty('a'))+':'+value(next.attributes.get('mathcolor'));});
for(const kind of ['mi','mo','mrow','inferredMrow','text','mspace'])add('queries:'+kind,()=>{let node=new MmlFactory().create(kind);return(u.isType(node,'mo')?'1':'0')+(u.isEmbellished(node)?'1':'0')+':'+value(u.getTexClass(node))+':'+(u.getCoreMO(node)===node?'self':'other')+':'+(u.isNode(node)?'node':'no')+':'+(u.isInferred(node)?'inferred':'explicit');});
add('isNode-scalar',()=>u.isNode(1)?'node':'no');
for(const text of ['+','(',')','∑','x',''])for(const form of ['infix','prefix','postfix','missing'])add('op:'+text+':'+form,()=>value(u.getOp(n('mo',text),form==='missing'?undefined:form)));
for(const [tag,form]of [['null',null],['bad','bad']])add('op-invalid:'+tag,()=>value(u.getOp(n('mo','x'),form)));
for(const kind of ['mo','mi'])for(const text of ['+','(',')','x'])add('form:'+kind+':'+text,()=>value(u.getForm(n(kind,text))));
for(const text of ['+','∑','^','x'])for(const attr of ['accent','movablelimits','linebreakstyle','lspace'])for(const explicit of [false,true])add('moattr:'+text+':'+attr+':'+explicit,()=>{let node=n('mo',text);if(explicit)node.attributes.set(attr,false);return value(u.getMoAttribute(node,attr));});

// Frozen original Source JSON remains byte-identical; only renderer-only cpp fields are projected.
assert.ok(process.argv.includes('--check'),'Source-only oracle supports read-only --check');
const frozenProfilePath=path.join(root,"docs/math-port/evidence/p40-node-util-source.json");
const frozenBytes=fs.readFileSync(frozenProfilePath);
function sourceFields(value){if(Array.isArray(value))return value.map(sourceFields);if(value&&typeof value==='object')return Object.fromEntries(Object.entries(value).filter(([key])=>key!=='cpp').map(([key,v])=>[key,sourceFields(v)]));return value;}
const actualSourceProfile=rows;
assert.deepStrictEqual(sourceFields(actualSourceProfile),sourceFields(JSON.parse(frozenBytes)),'Original Source inputs/expectations/pins changed');
console.log(JSON.stringify({sourceCases:rows.length,profile:"docs/math-port/evidence/p40-node-util-source.json",profileSha256:crypto.createHash('sha256').update(frozenBytes).digest('hex'),mode:'check'}));
