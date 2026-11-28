'use strict';
// Actual pinned CJS observations for the textmacros package. All method runs
// use real TeX/ParseOptions/TextParser/MML owners; no native results imported.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'), packageRoot=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const scopeBytes=fs.readFileSync(path.join(root,'tools/math-port/profiles/scope.json'));
const scopeHash=crypto.createHash('sha256').update(scopeBytes).digest('hex'),scope=JSON.parse(scopeBytes.toString('utf8'));
const lock={
 'ts/input/tex/textmacros/TextMacrosConfiguration.ts':'5d712a805bfe3ff3db425f24c1887eab300782a8570363abbbe77641222faefc',
 'ts/input/tex/textmacros/TextMacrosMappings.ts':'9c0e47239e9b9c59e47d045fb56d67b8bfe3d025214641b2ba7f56ad60202cb2',
 'ts/input/tex/textmacros/TextMacrosMethods.ts':'15b736370eed35d3e65af14d772bfe1b1ad3287844ed44f908f6a9b14004d9c3',
 'ts/input/tex/textmacros/TextParser.ts':'538059c8482022c1a4bc6f8e763fdc21993fa6353f1956655ae11c9271788a30',
 'ts/input/tex/base/BaseMethods.ts':'7336240c8f9c5037f9fdcd5a84ea079312854f1ef3f406aeb5a8b6a8ff765cda',
 'ts/input/tex/base/BaseConfiguration.ts':'578d2c6b03f2f3e902476a0b73a5323b12262215fcbdc831a1135c1d9c1737ad',
 'ts/input/tex/base/BaseItems.ts':'0c0d8448f3577be6642c95a412682310362819d2d383ab83b817d2888e4e5f0c',
 'ts/input/tex/ParseUtil.ts':'b43646071997f88f27df51610c7972a8cb5791ea76194e7318d17d1a2f81f76a',
 'ts/input/tex/HandlerTypes.ts':'5b9d091fc53640603956ab4ae39f146735621def9c36f312f8abd00afb043cfc',
 'ts/input/tex/MapHandler.ts':'7cacdaef3ed1df90b4f05cc0b3183a2956ae8f848f20ae8833932faa4efcb2a2',
 'ts/input/tex/TokenMap.ts':'0209895b13569008b2129b61c88050e7c70e70ce9477d234b9a6e303a86a738e',
 'ts/input/tex/TexError.ts':'30f398ff1910edecf029b1e4dde846bef10990539f60c43a4ef2da8783ba7ac8',
 'ts/util/Retries.ts':'c517df1c4c67c8cfac65a296b0e185089fd166739d3038eebb89647f4b37c921',
 'input/tex/textmacros/TextMacrosConfiguration.js':'5803611b187a6256cae6289cfc2f61fafa7f93395432d3a7b365f8089334b5cf',
 'input/tex/textmacros/TextMacrosMappings.js':'d331e33a2c3327eeeb2bc1fd5e7e98b0ef17fd7f317aaf91c19f9d665a1a5260',
 'input/tex/textmacros/TextMacrosMethods.js':'59e0abbeffeb9c859b2150d705abb1dc0a59a7ec71fb830ae6379d887c499817',
 'input/tex/textmacros/TextParser.js':'ec2fa0145f5e90ef231b023a38ed7d9c1064a8cc58a46bb683d46439bd8fb400',
 'input/tex/base/BaseMethods.js':'b8d5c74e01f85ae391ec8d16ffcd216a4ca4267bd8ffa091c5bfe1462ad7b491',
 'input/tex/base/BaseConfiguration.js':'0feedc60ac600b647cf2a1437c7244196a5aec04715f4a301fe6e9ec09722c57',
 'input/tex/base/BaseItems.js':'5310d0429b7b62d405ae9b93c8d21ad17b787e5eb3fd1f01439c62f2094f7d1a',
 'input/tex/ParseUtil.js':'adafa18cefad39e8408bcc0e4eb7103dceb6d16c9e7ea2eb013f23602c3f8941',
 'input/tex.js':'ef2852b197173f373e62326016e980aef01ffc838f5f766ab6076262f944eada',
 'input/tex/Configuration.js':'86f62a6f6e44fec20156546b7e8d424cac493c7465c2c8ec621c604332a276f3',
 'input/tex/ParseOptions.js':'ff19d70539cb76911c5f1d7673ddfd908183dcf5849c91cb068b19e58da8a9ec',
 'input/tex/TexParser.js':'106d7ff9298dce899d046339eab1996358f3efca3a7ce66f5ca915747dc9c6a8',
 'input/tex/StackItem.js':'53baa91d7c27475a91a9b6993235071ab32b04a29736c5920bdda2836ab5a91b',
 'input/tex/HandlerTypes.js':'bf6216270252efca24688a2bf8afda18a9adb762b4a7ae41471d013063bf4103',
 'input/tex/MapHandler.js':'d82e3034776d2392689aad86500b5e60dbe0aa5540853b48f308841879f3cf67',
 'input/tex/TokenMap.js':'9e3f086080a6ab052b68b22a5d9ee3dbc5e17d7b0c03a31e789664b01df36cf7',
 'input/tex/TexError.js':'a5713111ff2ab646740da13ca6273cb5f2a7ceff9d0fb6a2e116185e4994e64f',
 'util/Retries.js':'0ffe30ec3d65f1d5edfdbb505e5b76cf5b66ec1a920216ab53595df8e666b549',
 'input/tex/base/BaseItems.js':'5310d0429b7b62d405ae9b93c8d21ad17b787e5eb3fd1f01439c62f2094f7d1a',
 'core/MmlTree/MmlFactory.js':'d0c4e25237d04a9d21d12ee44776e46d30777e7f284db6f4f3ed1cc625b752d3',
 'core/Tree/Factory.js':'739db2e5760c29d7bb4f1bc7baa08ff8d727f4d117522aff5ede017f812cffef'
};
const modules=[];
for(const [file,expected] of Object.entries(lock)){
 const cjsFile=file.startsWith('ts/')?file.slice(3).replace(/\.ts$/,'.js'):file;
 const cjs=path.join(packageRoot,'cjs',cjsFile);const bytes=fs.readFileSync(cjs),hash=crypto.createHash('sha256').update(bytes).digest('hex');
 if(expected&&!file.startsWith('ts/'))assert.equal(hash,expected,`${file} CJS hash`);
 if(file.startsWith('ts/')){
  const sourceLock=scope.files.find(x=>x.source===file);assert.ok(sourceLock,`${file} missing scope lock`);
  const original=fs.readFileSync(path.join(root,'.reference/MathJax-src',file));assert.equal(crypto.createHash('sha256').update(original).digest('hex'),sourceLock.source_sha256,`${file} pinned source`);
  const installed=fs.readFileSync(path.join(packageRoot,file)),normalize=b=>b.toString('utf8').replaceAll('\r\n','\n');
  assert.equal(normalize(installed),normalize(original),`${file} installed TS differs beyond line endings`);
  modules.push({source:file,source_sha256:sourceLock.source_sha256,installed_ts_sha256:crypto.createHash('sha256').update(installed).digest('hex'),cjs:cjsFile,cjs_sha256:hash});
 }else modules.push({cjs:file,cjs_sha256:hash});
}
const load=file=>require(path.join(packageRoot,'cjs',file));
const {TeX}=load('input/tex.js'),TexParser=load('input/tex/TexParser.js').default;
const {TextParser}=load('input/tex/textmacros/TextParser.js'),{ParseUtil}=load('input/tex/ParseUtil.js');
const {TextMacrosMethods}=load('input/tex/textmacros/TextMacrosMethods.js');
const BaseMethods=load('input/tex/base/BaseMethods.js').default;
const {MapHandler}=load('input/tex/MapHandler.js');
const {MmlFactory}=load('core/MmlTree/MmlFactory.js');load('input/tex/textmacros/TextMacrosConfiguration.js');
const rows=[];
function plain(v,seen=new WeakSet()){
 if(v===undefined)return{$:'undefined'};if(v===null||typeof v==='string'||typeof v==='boolean')return v;
 if(typeof v==='number')return Number.isNaN(v)?{$:'NaN'}:Object.is(v,-0)?{$:'-0'}:v;
 if(typeof v==='function')return{$:'function',name:v.name||'',length:v.length};if(typeof v!=='object')return String(v);
 if(seen.has(v))return{$:'cycle'};seen.add(v);
 if(Array.isArray(v))return Array.from({length:v.length},(_,i)=>Object.hasOwn(v,i)?plain(v[i],seen):{$:'hole'});
 if(v instanceof Map)return [...v].map(([k,x])=>[plain(k,seen),plain(x,seen)]);
 const result={};for(const k of Reflect.ownKeys(v))result[typeof k==='symbol'?`symbol:${k.description||''}`:k]=plain(v[k],seen);return result;
}
function node(n){return !n?null:{kind:n.kind,attrs:plain(n.attributes?.getAllAttributes?.()||{}),properties:plain(n.getAllProperties?.()||{}),children:(n.childNodes||[]).map(node)};}
function capture(fn){try{return{value:plain(fn()),error:null};}catch(e){return{value:null,error:{name:e.name||e.constructor.name,id:e.id??null,message:String(e.message)}};}}
function row(name,fn){rows.push({name,...capture(fn)});}
function makeSession(){const jax=new TeX({packages:['base','textmacros']});jax.setMmlFactory(new MmlFactory());const parser=new TexParser('',{},jax.parseOptions),data=jax.parseOptions.packageData.get('textmacros');data.parseOptions.nodeFactory.setMmlFactory(jax.mmlFactory);return{jax,parser,data};}
function runText(source,level){const s=makeSession();s.data.texParser=s.parser;const p=new TextParser(source,{},s.data.parseOptions,level);return{result:capture(()=>node(p.mml())),index:p.i,text:p.text,envStack:plain(p.envStack),innerParsers:s.data.parseOptions.parsers.length,mainParserStillRegistered:s.jax.parseOptions.parsers[0]===s.parser};}
row('textmacros-package-configuration-identity-and-canonical-function',()=>{const s=makeSession(),main=s.jax.parseOptions,inner=s.data.parseOptions;return{packageKeys:Object.keys(s.data),sharedOptions:inner.options===main.options,sharedPackageData:inner.packageData===main.packageData,defaultTags:inner.tags===s.jax.parseOptions.tags,packageConf:s.data.textConf.constructor.name,callback:{name:main.options.internalMath.name,length:main.options.internalMath.length},sameCallback:main.options.internalMath===inner.options.internalMath,baseMethodAliases:Object.fromEntries(['Macro','Spacer','Hskip','rule','Rule','HandleRef','UnderOver','Lap','Phantom','Smash','MmlToken'].map(name=>[name,TextMacrosMethods[name]===BaseMethods[name]]))};});
row('textmacros-mapping-registration-and-canonical-method-aliases',()=>{const s=makeSession();const character=['$','%','^','_','&','#','~',' ','\t','\r','\n','\u00a0','{','}','`',"'"];const macro=['(','$','_','%','{','}',' ','&','#','\\',"'",'\u2019','`','\u2018','^','"','~','=','.', 'u','v','emph','rm','mit','oldstyle','cal','it','bf','sf','tt','frak','Bbb','Tiny','tiny','scriptsize','SMALL','Small','footnotesize','small','normalsize','large','Large','LARGE','huge','Huge','HUGE','textnormal','textup','textrm','textit','textbf','textsf','texttt','dagger','ddagger','S','AA','ldots','vdots',',',':','>',';','!','enspace','quad','qquad','thinspace','negthinspace','hskip','hspace','kern','mskip','mspace','mkern','rule','Rule','Space','color','textcolor','colorbox','fcolorbox','href','style','class','data','cssId','unicode','U','char','ref','eqref','underline','llap','rlap','phantom','vphantom','hphantom','smash','mmlToken'];const special=MapHandler.getMap('text-special'),commands=MapHandler.getMap('text-macros');return{character:character.map(x=>[x,capture(()=>plain(special.lookup(x)))]),macro:macro.map(x=>[x,capture(()=>plain(commands.lookup(x)))]),fallbacks:Object.keys(s.data.textConf.fallback||{}),itemKinds:Object.keys(s.data.textConf.items||{})};});
row('textparser-spacing-specials-quotes-and-groups',()=>['plain  many   spaces','a\u00a0b','`open\'','``pair\'\'','{group}','a}'].map(x=>[x,capture(()=>runText(x))]));
row('textparser-font-size-and-text-argument-macros',()=>['\\bf bold','\\it italic','\\tiny small','\\emph emphatic','\\textbf{bold}','\\textit{italic}','\\texttt{mono}'].map(x=>[x,capture(()=>runText(x))]));
row('textmacros-method-branch-corpus',()=>['\\%','\\dagger',"\\'{e}",'\\frak{x}','\\quad','\\hspace{1em}','\\rule{1em}{2pt}','\\Rule','\\ref{missing}','\\underline{x}','\\llap{x}','\\phantom{x}','\\smash{x}','\\mmlToken{mi}{x}','\\color{red}x'].map(source=>[source,capture(()=>runText(source))]));
row('textparser-math-delimiters-and-mode-errors',()=>['$x$','\\(x\\)','x_{y}','^x','&','#','\\unknown'].map(x=>[x,capture(()=>runText(x))]));
row('internalmath-callback-normalization-level-font-and-shared-state',()=>{const out=[];for(const [source,level,font] of [['a   b',undefined,undefined],['x',2,undefined],['y',undefined,'bold']]){const s=makeSession();out.push({source,level,font,result:capture(()=>ParseUtil.internalMath(s.parser,source,level,font).map(node)),mainParser:s.data.texParser===s.parser,innerParsers:s.data.parseOptions.parsers.length,sharedMap:s.data.parseOptions.packageData===s.jax.parseOptions.packageData});}return out;});
row('textmacros-copyLists-transfers-actual-node-list-membership',()=>{const s=makeSession(),main=s.jax.parseOptions,inner=s.data.parseOptions,node=s.parser.create('node','mi',[],{},s.parser.create('text','x'));inner.addNode('custom',node);inner.addNode('mi',node);s.data.texParser=s.parser;const p=new TextParser('',{},inner);p.copyLists();return{mainKeys:Object.keys(main.nodeLists),innerKeys:Object.keys(inner.nodeLists),sameNode:main.nodeLists.custom?.[0]===node,mainMi:main.nodeLists.mi?.length};});
const output={schema:1,scope_sha256:scopeHash,modules,observations:rows};const dest=path.join(root,'tools/math-port/profiles/evidence/textmacros-package-original-source.json'),serialized=JSON.stringify(output,null,2)+'\n';
if(process.argv.includes('--check'))assert.equal(fs.readFileSync(dest,'utf8').replaceAll('\r\n','\n'),serialized);else fs.writeFileSync(dest,serialized);
console.log(JSON.stringify({observations:rows.length,modules:modules.length,output:path.relative(root,dest)}));
