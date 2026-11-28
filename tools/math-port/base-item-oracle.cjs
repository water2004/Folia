// Root actual pinned BaseItem/StackItemFactory observations; no native evaluator.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {root,MmlFactory}=require(path.join(path.resolve(__dirname,'../..'),'tools/math-port/simple-mml-reference.cjs')),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
for(const[file,hash]of [['StackItem.ts','ac138d321e1efbc54afbd961a4b6bfa24f7d0923eddba1c8d891dee9424602b5'],['StackItemFactory.ts','229bf0f6bd58642bff86102bb8fb6fd2b9a61d3f52ad31ce738f3f2cefa2e405']]){
 const p='ts/input/tex/'+file,bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',p));assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),hash);assert.equal(bytes.toString().replaceAll('\r\n','\n'),fs.readFileSync(path.join(base,p),'utf8').replaceAll('\r\n','\n'));
}
const {BaseItem}=require(path.join(base,'cjs/input/tex/StackItem.js')),SF=require(path.join(base,'cjs/input/tex/StackItemFactory.js')).default,NF=require(path.join(base,'cjs/input/tex/NodeFactory.js')).NodeFactory,TE=require(path.join(base,'cjs/input/tex/TexError.js')).default;
const TC=require(path.join(base,'cjs/input/tex/TexConstants.js')).TexConstant;
const val=p=>p===undefined?'U':p===null?'N':p&&typeof p.kind==='string'&&p.getProperty?'node:'+p.kind:typeof p==='object'?'object':String(p);
const hex=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i).toString(16).padStart(4,'0')).join(',');
const record=p=>Object.keys(p).map(k=>k+'='+val(p[k])).join(',');
const tuple=t=>t===BaseItem.fail?'fail:'+val(t[0])+':'+val(t[1]):t===BaseItem.success?'success:'+val(t[0])+':'+val(t[1]):val(t);
function env(){
 TC.Attr.LATEX='data-latex';TC.Attr.LATEXITEM='data-latex-item';
 BaseItem.fail[0]=BaseItem.success[0]=null;BaseItem.fail[1]=false;BaseItem.success[1]=true;
 const trace=[],m=new MmlFactory(),nf=new NF(),f=new SF();nf.setMmlFactory(m);nf.configuration={addNode(k){trace.push('add:'+k);}};f.configuration={nodeFactory:nf};
 class Probe extends BaseItem{get array(){return this.nodes;}}
 f.setNodeClass('probe',Probe);const x=f.create('probe'),y=f.create('probe');
 return{trace,m,nf,f,Probe,x,y,a:m.create('mi'),b:m.create('mn')};
}
const rows=[];function add(label,fn){const e=env();let result='THREW',error='ok';try{result=fn(e);}catch(x){error=x instanceof TE?'TexError|'+val(x.id)+'|'+x.message:x.name+'|'+x.message;}rows.push({label,expected:error+'|'+result+'|'+e.trace.join('/')});}
add('initial',({x})=>x.kind+':'+val(x.env)+':'+x.copyEnv+':'+record(x.global)+':'+val(x.getProperty('absent'))+':'+x.isOpen+':'+x.isClose+':'+x.isFinal);
add('independent-instance-state',({x,y})=>{x.global.v=1;x.setProperty('v',2);return val(y.global.v)+':'+val(y.getProperty('v'));});
for(const key of ['base','open','BASE',''])add('isKind:'+key,({x})=>val(x.isKind(key)));
add('isKind-virtual-kind',({x,trace})=>{Object.defineProperty(x,'kind',{get(){trace.push('kind');return'open';}});return val(x.isKind('open'));});
for(const[tag,p]of [['undefined',undefined],['null',null],['false',false],['zero',0],['text','abc']])add('property:'+tag,({x})=>{let returned=x.setProperty('name',p);return(returned===x?'same':'bad')+':'+val(x.getProperty('name'))+':'+val(x.getName());});
add('node-property-identity',({x,a})=>{x.setProperty('num',a);return val(x.getProperty('num'))+':'+(x.getProperty('num')===a?'same':'bad');});
add('getName-virtual-property',({x,trace})=>{x.getProperty=k=>{trace.push(k);return'override';};return val(x.getName());});
add('inherited-builtin-property',({x})=>val(x.getProperty('toString')));
add('assign-key-order-and-node',({x,a,b})=>{const def={'10':a,'2':b,z:1,a:2};const returned=x.setProperties(def);return(returned===x?'same':'bad')+':'+val(x.getProperty('10'))+':'+val(x.getProperty('2'))+':'+val(x.getProperty('z'));});
add('assign-own-only',({x,a,b})=>{const def=Object.create({inherited:a});def.own=b;x.setProperties(def);return val(x.getProperty('inherited'))+':'+val(x.getProperty('own'));});
add('assign-style-alias',({x})=>{const styles={mathcolor:'red'};x.setProperties({styles});styles.mathcolor='blue';return(x.getProperty('styles')===styles?'same':'bad')+':'+styles.mathcolor;});
add('assign-recursive-record-alias',({x,a,b})=>{const nested={num:a};x.setProperties({nested});nested.num=b;return(x.getProperty('nested')===nested?'same':'bad')+':'+val(nested.num);});
for(const[desc,p]of [['U',undefined],['N',null],['string','ab'],['number',2]])add('assign-boxed:'+desc,({x})=>{x.setProperties(p);return val(x.getProperty('0'))+':'+val(x.getProperty('1'));});
add('prototype-record-shadow',({x,a,b})=>{const parent={inherited:a};x.setProperty('__proto__',parent);const before=x.getProperty('inherited');x.setProperty('inherited',b);return val(before)+':'+val(x.getProperty('inherited'))+':'+val(parent.inherited);});
add('prototype-null',({x})=>{x.setProperty('__proto__',null);return val(x.getProperty('toString'))+':'+val(x.getProperty('__proto__'));});
add('env-alias-clear-own-only',({x})=>{const value=Object.create({p:1});value.a=2;value.b=3;x.env=value;const same=x.env===value;x.clearEnv();return(same?'same':'bad')+':'+record(value)+':'+val(value.p);});
for(const[tag,p]of [['U',undefined],['N',null],['false',false]])add('clear-env:'+tag,({x})=>{x.env=p;x.clearEnv();return'ok';});
add('clear-env-live-target',({x,trace})=>{const first={a:1,b:2},second={a:3,b:4,c:5};let n=0;Object.defineProperty(x,'env',{get(){trace.push('env:'+ ++n);return n===1?first:second;}});x.clearEnv();return record(first)+':'+record(second);});
for(const kind of ['end','close','right','middle','absent'])add('errors:'+kind,({x})=>{const got=x.getErrors(kind);return Array.isArray(got)?got.map(val).join(';'):val(got);});
add('errors-class-prototype-and-live-array',({x,Probe})=>{const table=Object.create(BaseItem.errors),custom=['id','msg'];table.extra=custom;Probe.errors=table;const got=x.getErrors('extra');custom[1]='changed';return(got===custom?'same':'bad')+':'+got.map(val).join(';')+':'+x.getErrors('close').map(val).join(';');});
for(const[v,legacyLabel]of [[null,'PropertyNull{}'],[false,'false'],[0,'0'],['','""']])add('errors-falsy-fallback:'+legacyLabel,({x,Probe})=>{Probe.errors={close:v};return x.getErrors('close').map(val).join(';');});
add('errors-empty-array-identity',({x,Probe})=>{const empty=[];Probe.errors={close:empty};return(x.getErrors('close')===empty?'same':'bad')+':'+empty.map(val).join(';');});
add('check-normal-success',({x,y})=>tuple(x.checkItem(y)));
add('check-final-push',({x,y,a})=>{y.Push(a);Object.defineProperty(y,'isFinal',{get(){return true;}});const got=x.checkItem(y);return tuple(got)+':'+val(x.First)+':'+y.Size();});
add('check-fail-shared-mutable-tuple',({x,y})=>{Object.defineProperty(y,'isFinal',{get(){return true;}});BaseItem.fail[1]=true;const got=x.checkItem(y);return tuple(got)+':'+(got===BaseItem.fail?'same':'bad');});
for(const open of [false,true])for(const linebreak of [false,true])add('check-cell:'+open+':'+linebreak,({x,y})=>{Object.defineProperty(x,'isOpen',{get(){return open;}});Object.defineProperty(y,'kind',{get(){return'cell';}});y.setProperty('name','&');y.setProperty('linebreak',linebreak);return tuple(x.checkItem(y));});
for(const kind of ['end','close','right','middle','other'])add('check-close:'+kind,({x,y})=>{Object.defineProperty(y,'kind',{get(){return kind;}});Object.defineProperty(y,'isClose',{get(){return true;}});y.setProperty('name','foo');return tuple(x.checkItem(y));});
add('check-over-partial-setter-throw',({x,y,a,trace})=>{x.Push(a);Object.defineProperty(x,'isOpen',{get(){return true;}});Object.defineProperty(y,'kind',{get(){return'over';}});y.setProperty=(k,v)=>{trace.push(k+':'+val(v)+':'+x.Size());throw Error('setter');};return tuple(x.checkItem(y));});
add('check-over-clears-rebind',({x,y,a})=>{x.Push(a);const old=x.array;Object.defineProperty(x,'isOpen',{get(){return true;}});Object.defineProperty(y,'kind',{get(){return'over';}});const got=x.checkItem(y);return tuple(got)+':'+val(y.getProperty('num'))+':'+old.length+':'+x.Size();});
add('check-repeat-errors-and-kind',({x,y,trace})=>{y.isKind=k=>{trace.push('isKind:'+k);return false;};Object.defineProperty(y,'isClose',{get(){trace.push('isClose');return true;}});let n=0;Object.defineProperty(y,'kind',{get(){trace.push('kind:'+ ++n);return n===1?'one':'two';}});x.getErrors=k=>{trace.push('errors:'+k);return['id','bad %1'];};y.getProperty=k=>{trace.push('prop:'+k);return'name';};return tuple(x.checkItem(y));});
for(const[v,legacyLabel]of [[undefined,'Missing{}'],[null,'PropertyNull{}']])add('check-errors-second-nullish:'+legacyLabel,({x,y,trace})=>{Object.defineProperty(y,'isClose',{get(){return true;}});let n=0;x.getErrors=()=>{trace.push('errors:'+ ++n);return n===1?[]:v;};return tuple(x.checkItem(y));});
add('check-errors-empty-array',({x,y,trace})=>{Object.defineProperty(y,'isClose',{get(){return true;}});x.getErrors=()=>[];y.getProperty=k=>{trace.push('prop:'+k);return'name';};return tuple(x.checkItem(y));});
add('toString-sparse-live-list',({x,a,b})=>{x.array.length=4;x.array[0]=a;x.array[2]=null;x.array[3]=b;return x.toString();});
for(const[str,start,end,prefix]of [['abc',0,3,''],['}',0,1,''],['}',0,1,'pre'],['',0,0,'pre'],['abcdef',-3,-1,'p'],['a😀b',1,2,''],['abcdef',1.9,4.9,''],['abcdef',4,1,'']])add('latex:'+JSON.stringify([str,start,end,prefix]),({x,a})=>{x.startStr=str;x.startI=start;x.stopI=end;x.addLatexItem(a,prefix);const aa=a.attributes.getExplicit('data-latex-item'),bb=a.attributes.getExplicit('data-latex');return(aa===undefined?'U':hex(aa))+':'+(bb===undefined?'U':hex(bb));});
for(const kind of ['base','dummy','unknown'])add('factory-empty:'+kind,()=>new SF().create(kind).kind);
add('factory-default-field-mismatch',()=>{const empty=new SF();return empty.defaultKind+':'+empty.getKinds().length+':'+Object.keys(SF.DefaultStackItems)[0]+':'+(empty.configuration===null);});
add('factory-explicit-default-map',()=>{const f=new SF(SF.DefaultStackItems);return f.create('base').kind+':'+f.getKinds().length;});
add('factory-single-identity',({f,x})=>{const one=x,two=x;return(one===two?'same':'bad')+':actual:one-factory';});
add('constructor-virtual-open-before-fields',({f,a,trace})=>{class Phase extends BaseItem{constructor(factory,...nodes){super(factory,...nodes);this.flag=1;}get isOpen(){trace.push('open:'+val(this.flag)+':'+record(this.global)+':'+val(this.getProperty('p'))+':'+val(this.env));return true;}}const raw=new Phase(f,a);return val(raw.env)+':'+val(raw.flag)+':'+val(raw.First);});
add('errors-captured-class-after-replacement',({f,x,Probe})=>{Probe.errors={extra:['old','old message']};class Other extends BaseItem{}Other.errors={extra:['new','new message']};f.setNodeClass('probe',Other);return x.getErrors('extra').map(val).join(';');});
add('factory-null-fallback',()=>{const f=new SF();f.node.dummy=null;return f.create('unknown').kind;});
add('toString-kind-before-nodes-and-live-join',({m,x,b,trace})=>{const reader=m.create('mi'),old=x.array;old.push(reader,b);reader.toString=()=>{trace.push('node');old.length=1;return'A';};Object.defineProperty(x,'kind',{get(){trace.push('kind');return'base';}});Object.defineProperty(x,'nodes',{get(){trace.push('nodes');return old;}});return x.toString();});
add('latex-empty-touches-no-null-node',({x})=>{x.startStr='';x.addLatexItem(null);return'ok';});
add('latex-null-node-with-source',({x})=>{x.startStr='x';x.stopI=1;x.addLatexItem(null);return'ok';});
add('latex-rereads-attributes',({x,a,b,trace})=>{const first=a.attributes,second=b.attributes;let n=0;Object.defineProperty(a,'attributes',{get(){trace.push('attrs:'+ ++n);return n===1?first:second;}});x.startStr='x';x.stopI=1;x.addLatexItem(a);return hex(first.getExplicit('data-latex-item'))+':'+val(first.getExplicit('data-latex'))+':'+hex(second.getExplicit('data-latex'));});
add('latex-retains-first-write-on-second-failure',({x,a,trace})=>{const first=a.attributes;let n=0;Object.defineProperty(a,'attributes',{get(){trace.push('attrs:'+ ++n);if(n===2){trace.push('retained:'+hex(first.getExplicit('data-latex-item')));throw Error('second attrs');}return first;}});x.startStr='x';x.stopI=1;x.addLatexItem(a);return'ok';});
add('latex-null-attributes',({x,a,trace})=>{Object.defineProperty(a,'attributes',{get(){trace.push('attrs');return null;}});x.startStr='x';x.stopI=1;x.addLatexItem(a);return'ok';});
add('latex-current-constant-after-receiver-read',({x,a,trace})=>{const original=a.attributes;Object.defineProperty(a,'attributes',{get(){trace.push('attrs');TC.Attr.LATEXITEM='item-now';TC.Attr.LATEX='latex-now';return original;}});x.startStr='x';x.stopI=1;x.addLatexItem(a);return hex(original.getExplicit('item-now'))+':'+hex(original.getExplicit('latex-now'));});
add('check-over-explicit-row',({x,y,a,b})=>{x.Push(a,b);Object.defineProperty(x,'isOpen',{get(){return true;}});Object.defineProperty(y,'kind',{get(){return'over';}});const got=x.checkItem(y);return tuple(got)+':'+val(y.getProperty('num'))+':'+x.Size();});
assert.ok(process.argv.includes('--check'),'Source-only oracle is check-only');
assert.deepEqual(JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/evidence/p42b-base-item-source.json'),'utf8')),rows);
console.log(JSON.stringify({cases:rows.length,orderedExpectedValues:rows.length,mode:'check'}));
