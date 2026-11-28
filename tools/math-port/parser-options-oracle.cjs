// Root observations from the actual pinned classes, no substitute parser/jax.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {root,MmlFactory}=require('./simple-mml-reference.cjs');
const pkg=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src'),load=p=>require(path.join(pkg,'cjs',p));
const {Configuration,ConfigurationHandler,ParserConfiguration}=load('input/tex/Configuration.js');
const ParseOptions=load('input/tex/ParseOptions.js').default,TexParser=load('input/tex/TexParser.js').default,{TeX}=load('input/tex.js'),{TagsFactory}=load('input/tex/Tags.js');
const {MmlItem}=load('input/tex/base/BaseItems.js');
const {AbstractMathItem}=load('core/MathItem.js'),{ParseUtil}=load('input/tex/ParseUtil.js');
function tokens(node){if(node.isToken)return[node.kind+'='+node.getText()];return node.childNodes.flatMap(n=>n?tokens(n):[]);}
const val=p=>p===undefined?'U':p===null?'N':typeof p==='number'&&Object.is(p,-0)?'-0':typeof p==='object'?'object':typeof p==='function'?'function':String(p);
const rows=[];
function add(label,run){
 const trace=[],warn=console.warn;console.warn=(...v)=>trace.push(v.join(' '));
 let result,error='ok';try{result=run(trace);}catch(e){error=e.name+'|'+e.message;}finally{console.warn=warn;}
 rows.push({label,error,result,trace});
}
for(const priority of [undefined,null,0,3])add('configuration-default-priority:'+val(priority),()=>{
 const name='p46-priority-'+val(priority),c=Configuration.local({priority});
 return{name:c.name,registered:ConfigurationHandler.get(name)!==undefined,priority:c.priority,parser:c.parser,handlers:Object.keys(c.handler),methods:[val(c.initMethod),val(c.configMethod)],fresh:c.handler!==Configuration.local().handler};
});
add('configuration-retained-options-and-callback',()=>{
 const options={nested:{x:1}},fn=()=>{},c=Configuration.create('p46-retained',{options,init:fn,config:[fn,0],handler:{macro:['x']}});
 return{registered:ConfigurationHandler.get(c.name)===c,options:c.options===options,init:c.init===fn,config:c.config===fn,initPriority:c.initMethod[1],configPriority:c.configMethod[1],handler:Object.keys(c.handler),macro:c.handler.macro.join(',')};
});
for(const priority of [undefined,0,8])add('parser-configuration-append-priority:'+val(priority),trace=>{
 const p=new ParserConfiguration([]),c=Configuration.local({priority:7,options:{x:1},fallback:{macro:()=>trace.push('a')}});p.append(c,priority);
 p.append(Configuration.local({priority:7.5,fallback:{macro:()=>trace.push('b')}}));
 const jax=new TeX();jax.setMmlFactory(new MmlFactory());const parser=new TexParser('',{},jax.parseOptions);parser.mml();p.handlers.get('macro').parse([parser,'q']);
 return{option:p.options.x,handlers:Object.keys(c.handler)};
});
add('parser-configuration-registration-order',trace=>{
 for(const [name,priority]of [['a',2],['b',2],['c',1]])Configuration.create('p46-order-'+name,{priority,init:()=>trace.push('init:'+name)});
 const p=new ParserConfiguration(['p46-order-a','p46-order-b','p46-order-c']);p.init();
 return[...p.configurations].map(x=>x.item.name+':'+x.priority);
});
add('parser-configuration-target-and-warning',trace=>{
 Configuration.create('p46-other-target',{parser:'text'});const p=new ParserConfiguration(['p46-absent']);
 try{p.addPackage('p46-other-target');}catch(e){trace.push(e.name+'|'+e.message);}
 return[...p.configurations].length;
});
add('parse-options-constructor-owners',()=>{
 const config=new ParserConfiguration(['base']),p=new ParseOptions(config);
 return{handlers:p.handlers===config.handlers,nodeBackEdge:p.nodeFactory.configuration===p,itemBackEdge:p.itemFactory.configuration===p,tags:val(p.tags),math:val(p.mathItem),root:val(p.root),nodeLists:Object.keys(p.nodeLists),parsers:p.parsers.length,error:p.error,packages:p.packageData.size,style:p.mathStyle('A',true)};
});
add('parse-options-default-merge-and-aliases',()=>{
 const fn=()=>{},regex=/x/g,options={nested:{x:1},array:['a'],fn,regex};
 Configuration.create('p46-alias',{options});const config=new ParserConfiguration(['p46-alias']);
 const p=new ParseOptions(config,[{nested:{x:3,y:2},array:['b']}]),q=new ParseOptions(config);
 return{nested:p.options.nested,array:p.options.array,configFresh:config.options!==options,nestedFresh:p.options.nested!==config.options.nested,betweenFresh:p.options.nested!==q.options.nested,arraysFresh:p.options.array!==q.options.array,functions:p.options.fn===fn&&q.options.fn===fn,regex:p.options.regex===regex&&q.options.regex===regex};
});
for(const style of ['TeX','ISO','French','upright','unknown'])for(const [c,b]of [['A',false],['A',true],['\u0391',true],['é',false],['É',false],['𝑎',false]])add('parse-options-math-style:'+style+':'+c+':'+b,()=>{
 const p=new ParseOptions(new ParserConfiguration([]),[{mathStyle:style}]);return p.mathStyle(c,b);
});
add('actual-two-jax-option-identities',()=>{
 const fn=()=>{},regex=/x/g;Configuration.create('p46-two-jax',{options:{nested:{x:1},array:['a'],fn,regex}});
 const a=new TeX({packages:['base','p46-two-jax']}),b=new TeX({packages:['base','p46-two-jax']});
 a.parseOptions.options.nested.x=9;a.parseOptions.options.array.push('b');
 return{jaxAndParseDistinct:a.options!==a.parseOptions.options,betweenDistinct:a.parseOptions.options!==b.parseOptions.options,nested:[a.parseOptions.options.nested.x,b.parseOptions.options.nested.x],arrays:[a.parseOptions.options.array,b.parseOptions.options.array],fn:a.parseOptions.options.fn===fn&&b.parseOptions.options.fn===fn,regex:a.parseOptions.options.regex===regex&&b.parseOptions.options.regex===regex,tagsDistinct:a.parseOptions.tags!==b.parseOptions.tags,backEdges:a.parseOptions.tags.configuration===a.parseOptions&&b.parseOptions.tags.configuration===b.parseOptions};
});
add('parse-options-clear-persistence',trace=>{
 const jax=new TeX(),p=jax.parseOptions,m=new MmlFactory(),node=m.create('mi');p.nodeLists.mi=[node];p.parsers.push({marker:1});p.root=node;p.error=true;p.packageData.set('keep',{x:1});
 const refs={options:p.options,node:p.nodeFactory,item:p.itemFactory,tags:p.tags,column:p.columnParser,package:p.packageData,parsers:p.parsers,lists:p.nodeLists};
 const reset=p.tags.resetTag;p.tags.resetTag=function(){trace.push('resetTag');return reset.call(this);};p.clear();
 return{persistent:Object.entries(refs).filter(([k])=>!['parsers','lists'].includes(k)).map(([k,v])=>v===({node:p.nodeFactory,item:p.itemFactory,column:p.columnParser,package:p.packageData}[k]||p[k])),freshParsers:p.parsers!==refs.parsers,freshLists:p.nodeLists!==refs.lists,oldParsers:refs.parsers.length,oldLists:refs.lists.mi.length,root:val(p.root),error:p.error,packageSize:p.packageData.size};
});
add('parse-options-node-list-filter-and-alias',()=>{
 const p=new ParseOptions(new ParserConfiguration([])),m=new MmlFactory(),a=m.create('mi'),b=m.create('mn'),root=m.create('mrow',{},[a]);
 p.addNode('custom',a);p.addNode('custom',b);const old=p.nodeLists.custom;p.root=root;const list=p.getList('custom');
 return{registered:[a.getProperty('in-lists'),b.getProperty('in-lists')],size:list.length,node:list[0]===a,fresh:list!==old,oldSize:old.length,stored:p.nodeLists.custom===list};
});
add('parse-options-node-list-removal-identity',()=>{
 const p=new ParseOptions(new ParserConfiguration([])),m=new MmlFactory(),a=m.create('mi'),b=m.create('mn');p.addNode('mi',a);p.addNode('mi',a);p.addNode('mi',b);const list=p.nodeLists.mi;p.removeFromList('mi',[a]);
 return{same:list===p.nodeLists.mi,size:list.length,first:list[0]===a,last:list[1]===b,kindList:val(a.getProperty('in-lists')),otherList:b.getProperty('in-lists')};
});
add('actual-runtime-package-add-order',trace=>{
 const jax=new TeX(),p=jax.parseOptions,m=new MmlFactory();jax.setMmlFactory(m);
 const node=()=>m.create('mi'),beforeOptions=p.options,beforeFactory=p.nodeFactory;
 Configuration.create('p46-late',{items:{late:MmlItem},nodes:{late:node},tags:{late:p.tags.constructor},options:{late:{x:1}},init:()=>trace.push('init'),config:(config,j)=>trace.push('config:'+String(j===jax)+':'+j.parseOptions.options.late.x),preprocessors:[()=>trace.push('pre')],postprocessors:[()=>trace.push('post')]});
 jax.configuration.add('p46-late',jax,{late:{x:7}});
 return{sameOptions:p.options===beforeOptions,sameFactory:p.nodeFactory===beforeFactory,option:p.options.late.x,item:p.itemFactory.getNodeClass('late')===MmlItem,node:p.nodeFactory.get('late')===node,tag:TagsFactory.create('late').constructor===p.tags.constructor};
});
add('actual-config-callback-receivers',trace=>{
 let descriptor;
 function init(config){trace.push('init:'+(this&&this.item===init?'record':'other')+':'+(config instanceof ParserConfiguration));}
 function configure(config,jax){trace.push('config:'+(this===descriptor?'descriptor':this&&this.item===configure?'record':'other')+':'+(config===jax.configuration)+':'+(jax.parseOptions instanceof ParseOptions));}
 descriptor=Configuration.create('p46-receivers',{init,config:configure});const jax=new TeX({packages:['base','p46-receivers']});jax.configuration.add('p46-receivers',jax);
 return{initIdentity:descriptor.init===init,configIdentity:descriptor.config===configure};
});
add('actual-number-regexp-alias-and-live-consumption',()=>{
 const digits=/^[0-9]{2}/,jax=new TeX({digits});jax.setMmlFactory(new MmlFactory());const p=jax.parseOptions,alias=p.options.numberPattern===digits;
 const first=tokens(new TexParser('1234',{},p).mml());p.options.numberPattern=/^[0-9]+/;const second=tokens(new TexParser('1234',{},p).mml());
 return{alias,first,second,originalDigits:p.options.digits===digits,liveReplacement:p.options.numberPattern!==digits};
});
add('actual-format-error-option-receiver',trace=>{
 function formatError(jax,error){trace.push('formatError:'+(this===jax.options)+':'+error.id+':'+(error.constructor.name));return jax.formatError(error);}
 const jax=new TeX({formatError});jax.setMmlFactory(new MmlFactory());const math=new AbstractMathItem('\\p46Unknown',jax,false),root=jax.compile(math,null);
 return{callbackRetained:jax.options.formatError===formatError,root:root.kind,error:jax.parseOptions.error,errorKinds:root.childNodes[0].childNodes.map(n=>n.kind),mathIdentity:jax.parseOptions.mathItem===math};
});
add('actual-internal-math-option-live-receiver',trace=>{
 const jax=new TeX();jax.setMmlFactory(new MmlFactory());const p=jax.parseOptions,parser=new TexParser('',{},p);parser.mml();const first=[p.nodeFactory.create('token','mi',{},'a')],second=[p.nodeFactory.create('token','mi',{},'b')];
 function callback(actual,text,level,font){trace.push('call:'+(this===p.options)+':'+(actual===parser)+':'+text+':'+level+':'+font);return first;}
 p.options.internalMath=callback;const a=ParseUtil.internalMath(parser,' a  b ',0,'normal');p.options.internalMath=function(){trace.push('replacement:'+(this===p.options));return second;};const b=ParseUtil.internalMath(parser,'c');
 return{first:a===first,second:b===second,different:a!==b};
});
add('actual-textmacros-shared-options-and-internal-math',()=>{
 load('input/tex/textmacros/TextMacrosConfiguration.js');const jax=new TeX({packages:['base','textmacros']});jax.setMmlFactory(new MmlFactory());const p=jax.parseOptions,data=p.packageData.get('textmacros'),callback=p.options.internalMath;
 const root=jax.compile(new AbstractMathItem('\\text{a \\textbf{b}}',jax,false),null);
 return{sameOptions:data.parseOptions.options===p.options,samePackage:data.parseOptions.packageData===p.packageData,jax:data.jax===jax,callback:typeof callback,retained:p.options.internalMath===callback,actualParser:data.texParser instanceof TexParser,tokens:tokens(root),parsers:[p.parsers.length,data.parseOptions.parsers.length]};
});
const hashes=[];
for(const file of Object.keys(require.cache).filter(f=>f.startsWith(path.join(pkg,'cjs')+path.sep)).sort()){
 const relative=path.relative(path.join(pkg,'cjs'),file).replaceAll('\\','/'),source='ts/'+relative.replace(/\.js$/,'.ts');
 const upstream=fs.readFileSync(path.join(root,'.reference/MathJax-src',source)),installed=fs.readFileSync(path.join(pkg,source));
 assert.equal(installed.toString().replaceAll('\r\n','\n'),upstream.toString().replaceAll('\r\n','\n'),source+' production Source provenance');
 hashes.push({source,source_sha256:crypto.createHash('sha256').update(upstream).digest('hex'),cjs_sha256:crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex')});
}
assert.ok(rows.every(row=>row.error==='ok'),JSON.stringify(rows.filter(row=>row.error!=='ok')));
const report={stage:'original Source observations only; native parser classes and option bindings not accepted',node:process.version,hashes,rows},data=JSON.stringify(report,null,2)+'\n',target=path.join(root,'tools/math-port/profiles/evidence/p46-parser-options-source.json');
if(process.argv.includes('--check'))assert.equal(fs.readFileSync(target,'utf8').replaceAll('\r\n','\n'),data);else fs.writeFileSync(target,data);
console.log(JSON.stringify({source_cases:rows.length,loaded_source_files:hashes.length,unexpected_errors:rows.filter(x=>x.error!=='ok').map(x=>({label:x.label,error:x.error}))}));
