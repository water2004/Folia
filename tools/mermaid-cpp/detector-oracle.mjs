// Run original detector modules, detectType and addDiagrams. No native code.
import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { stripTypeScriptTypes } from 'node:module';
import { fileURLToPath, pathToFileURL } from 'node:url';
import path from 'node:path';
import vm from 'node:vm';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const lockPath = path.join(root, 'docs/mermaid-cpp/source-lock.json');
const lock = JSON.parse(readFileSync(lockPath));
const sourceRoot = path.join(root, '.reference/mermaid-js/packages/mermaid/src');
const read = name => readFileSync(path.join(sourceRoot, name), 'utf8').replaceAll('\r\n', '\n');
const orchestration = read('diagram-api/diagram-orchestration.ts');
const imports = [...orchestration.matchAll(/^import (.+) from '([^']+)';$/gm)]
  .filter(([, , p]) => p.startsWith('../diagrams/') && !p.endsWith('/errorDiagram.js'))
  .map(([, binding, p]) => ({binding:binding.replace(/[{} ]/g,''), file:path.posix.normalize('diagram-api/'+p).replace(/\.js$/,'.ts')}));
const sourceFiles = ['diagram-api/detectType.ts','diagram-api/diagram-orchestration.ts','errors.ts',...imports.map(x=>x.file)];
// Pin new source files only on the explicit generation pass. Existing hashes
// are always checked and never silently refreshed.
for (const name of sourceFiles) {
  const p = 'packages/mermaid/src/'+name;
  if (!lock.files.some(x=>x.path===p)) {
    assert(process.argv.includes('--write'), `Unpinned source ${p}`);
    lock.files.push({path:p,sha256:createHash('sha256').update(readFileSync(path.join(sourceRoot,name))).digest('hex')});
  }
}
for (const file of lock.files) assert.equal(createHash('sha256').update(readFileSync(
  path.join(root,'.reference/mermaid-js',file.path))).digest('hex'),file.sha256,file.path);
const strip = text => '"use strict";\n'+stripTypeScriptTypes(text.replace(/^import[\s\S]*?;\n/gm,''))
  .replace(/^export default \w+;\n/gm,'').replace(/^export /gm,'');
const regexes = await import(pathToFileURL(path.join(sourceRoot,'diagram-api/regexes.ts')));
const {UnknownDiagramError} = await import(pathToFileURL(path.join(sourceRoot,'errors.ts')));
const plugins = Object.fromEntries(imports.map(({binding,file})=>[binding,
  vm.runInNewContext(strip(read(file))+'\n({id,detector,loader})') ]));
function registry(large) {
  const warnings=[];
  const api=vm.runInNewContext(strip(read('diagram-api/detectType.ts'))+
    '\n({detectors,detectType,addDetector,registerLazyLoadedDiagrams,getDiagramLoader})',
    {...regexes,UnknownDiagramError,log:{debug(){},warn(m){warnings.push(m);}}});
  const add=vm.runInNewContext(strip(orchestration)+'\naddDiagrams',{
    ...plugins, injected:{includeLargeFeatures:large}, errorDiagram:{},
    registerLazyLoadedDiagrams:api.registerLazyLoadedDiagrams,
    // Observe only registration of detectors. Styles, utils and rendering are
    // not executed or represented as implemented by this boundary.
    registerDiagram:(id,diagram,detector)=>api.addDetector(id,detector)
  });
  return {...api,add,warnings};
}
const hex = s => Array.from({length:s.length},(_,i)=>s.charCodeAt(i).toString(16).padStart(4,'0')).join('')||'-';
const json = v => hex(JSON.stringify(v).replace(/[\u2028\u2029'%]/g,c=>'\\u'+c.charCodeAt(0).toString(16).padStart(4,'0')));
const texts=new Set(['','error',' ERROR \n','error x','---','----','---\nx: 1\n---\npie','%% hi\npie','%% hi pie',
  '%%{init: {"theme":"dark"}}%%\nflowchart-elk TD','pie %% C4Container\n', 'pie C4Container',
  'garbage C4Context','garbage C4Container','garbage C4Component','garbage C4Dynamic','garbage C4Deployment']);
const keywords=['error','---','flowchart-elk','mindmap','architecture','agentflow-beta','C4Context','C4Container',
  'kanban','classDiagram','erDiagram','gantt','info','pie','requirement','requirementDiagram','sequenceDiagram',
  'swimlane-beta','graph','flowchart','timeline','gitGraph','stateDiagram','journey','quadrantChart','sankey',
  'packet','xychart','block','eventmodeling','treeView-beta','radar-beta','ishikawa','treemap','railroad-beta',
  'railroad-ebnf-beta','railroad-abnf-beta','railroad-peg-beta','venn-beta','wardley-beta','cynefin-beta','usecase-beta'];
const spaces=['\t','\n','\v','\f','\r',' ','\u00a0','\u1680',...Array.from({length:11},(_,i)=>String.fromCharCode(0x2000+i)),
  '\u2028','\u2029','\u202f','\u205f','\u3000','\ufeff','\u0085','\u180e','\u200b','\u2060','\ud800','\udfff','😀','\0'];
for(const k of keywords) {
  for(const s of ['', ' ', '\nA-->B', '-beta','-betaX','X','0','_',':',';','é','\u017f','\u212a','\ud800']) texts.add(k+s);
  texts.add(k.toUpperCase()); texts.add(k.toLowerCase()); texts.add('prefix '+k);
  for(const s of spaces) {texts.add(s+k);texts.add(k+s);}
  for(const p of ['---\ntitle: x\n---\n','  ---\nx\n  ---\n','---\nx\n---','%% c\n',
    '%% c\r\n','%% c\u2028\n',"%%{init:{'theme':'dark'}}%%\n",'%%{init: unclosed\n',
    "%%{init:{'layout':'elk'}}%%\n---\nx\n---\n",'---\nx\n---\n%% comment\n']) texts.add(p+k);
}
let seed=0x91823;
const random=()=>seed=(Math.imul(seed,1664525)+1013904223)>>>0;
for(let i=0;i<300;i++) texts.add(spaces[random()%spaces.length]+keywords[random()%keywords.length]+['X',':','-beta','\nC4Container',''][random()%5]);
// Bounded large adversarial strings; retain whole UTF-16 inputs and errors.
texts.add(' '.repeat(32768)+'pie'); texts.add('%%'+'x%%'.repeat(2048));
texts.add('---\n'+' \n'.repeat(4096));
const rows=[], registries=[];
for(const large of [false,true]) {
  const r=registry(large); r.add();
  const ids=Object.keys(r.detectors);
  registries.push([large?1:0,json(ids),json(ids.map(k=>Boolean(r.getDiagramLoader(k))))].join('\t'));
  r.add(); assert.deepEqual(Object.keys(r.detectors),ids); assert.equal(r.warnings.length,0);
  for(const text of texts) {
    const config={layout:'dagre',flowchart:{defaultRenderer:'elk'},custom:['retained']};
    let id='',error='';
    try {id=r.detectType(text,config);} catch(e) {if(e.name!=='UnknownDiagramError') throw e;error=e.message;}
    rows.push([large?1:0,hex(text),json(config),hex(id),hex(error)].join('\t'));
  }
}
// Execute the original registry mutation and Object.entries snapshot behavior.
const scenarios=[
  [{op:'add',id:'a',rule:'false',loader:'L1'},{op:'add',id:'b',rule:'true',loader:'L2'},
    {op:'detect',text:'%% x\npie'},{op:'add',id:'a',rule:'true'},{op:'detect',text:'---\nx\n---\npacket'}, {op:'load',id:'a'},{op:'load',id:'b'},{op:'load',id:'missing'}],
  ['10','2','a','01','0','4294967295','4294967294',''].map(id=>({op:'add',id,rule:'false'})).concat([{op:'detect',text:'unknown'}]),
  [{op:'add',id:'first',rule:'set'},{op:'add',id:'next',rule:'layout'},{op:'detect',text:'pie'}],
  [{op:'add',id:'bad',rule:'fail'},{op:'add',id:'next',rule:'true'},{op:'detect',text:'pie'}],
  [{op:'add',id:'first',rule:'replace'},{op:'add',id:'next',rule:'false'},{op:'detect',text:'pie'},{op:'detect',text:'pie'}],
  [{op:'add',id:'external',rule:'true',loader:'custom'},{op:'builtins'},{op:'detect',text:'pie'},{op:'builtins'}],
  [{op:'builtins'},{op:'add',id:'pie',rule:'false',loader:'custom'},{op:'builtins'},{op:'detect',text:'pie'},{op:'load',id:'pie'}],
  [{op:'add',id:'\ud800',rule:'true',loader:''},{op:'detect',text:'\udfff'},{op:'load',id:'\ud800'}]
];
const custom=[];
for(let i=0;i<scenarios.length;i++) {
  const r=registry(true),trace=[];
  const callback=(id,rule)=>(text,config)=>{
    trace.push({id,text,layout:config.layout});
    if(rule==='fail') throw new Error('detector failed');
    if(rule==='set') {config.layout='custom';return false;}
    if(rule==='replace') {r.addDetector('next',callback('next','true'));return false;}
    return rule==='true'||rule==='layout'&&config.layout==='custom';
  };
  for(const step of scenarios[i]) {
    trace.length=0;let result={},config={layout:'dagre'};
    if(step.op==='add') {
      const loader=step.loader===undefined?undefined:Object.assign(()=>{}, {key:step.loader});
      r.addDetector(step.id,callback(step.id,step.rule),loader);
    } else if(step.op==='builtins') r.add();
    else if(step.op==='load') {
      try {const l=r.getDiagramLoader(step.id);result={loader:l?.key??(l?'builtin':null)};}
      catch(e) {if(e.name!=='TypeError') throw e;result={missing:true};}
    } else {
      try {result={id:r.detectType(step.text,config)};}
      catch(e) {if(e.name!=='UnknownDiagramError'&&e.message!=='detector failed') throw e;result={error:e.message};}
    }
    custom.push([i,json(step),json({ids:Object.keys(r.detectors),warnings:r.warnings.length,trace,result,config})].join('\t'));
  }
}
function save(name,content) {
  const file=path.join(root,name);
  if(process.argv.includes('--write')) writeFileSync(file,content);
  else assert.equal(readFileSync(file,'utf8').replaceAll('\r\n','\n'),content,name);
}
if(process.argv.includes('--write')) writeFileSync(lockPath,JSON.stringify(lock,null,2)+'\n');
save('tests/mermaid_cpp/fixtures/detectors.tsv','# large/input UTF16/output config JSON UTF16/id UTF16/error UTF16\n'+rows.join('\n')+'\n');
save('tests/mermaid_cpp/fixtures/detector-order.tsv','# large/ids JSON UTF16/loader presence JSON UTF16\n'+registries.join('\n')+'\n');
save('tests/mermaid_cpp/fixtures/detector-registry.tsv','# scenario/operation JSON UTF16/observed state JSON UTF16\n'+custom.join('\n')+'\n');
console.log(JSON.stringify({commit:lock.commit,detectorModules:imports.length,observations:rows.length,registrySteps:custom.length}));
