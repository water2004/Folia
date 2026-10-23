// Execute the complete pinned config.ts module in a fresh scope per scenario.
import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';
import { root, lock, src, strip, withoutImports, defaults, themes, assign,
  diagramKey, sanitizeDirective, hex, snapshot } from './config-reference.mjs';

const clean = value => JSON.parse(JSON.stringify(value));
const clone = value => Array.isArray(value) ? value.map(clone) : value && typeof value === 'object'
  ? Object.fromEntries(Object.entries(value).map(([k,v]) => [k,clone(v)])) : value;
const baseline = clean(defaults);
const program = strip(withoutImports(src('config.ts'))) + '\n({setSiteConfig,updateSiteConfig,saveConfigFromInitialize,setConfig,addDirective,reset,setDiagramConfigScope,getConfig,getSiteConfig,getUserDefinedConfig})';
function session() {
  const warnings=[];
  const api=vm.runInNewContext(program, {config:clone(defaults),theme:themes,assignWithDepth:assign,
    getDiagramConfigKey:diagramKey,sanitizeDirective,log:{debug(){},warn(message){warnings.push(message);}}});
  return {api,warnings};
}
// Lossless recursive delta against the separately checked full default snapshot.
// '=' carries a complete replacement; 'U' marks deletion. No hashes or selected fields.
function delta(value, base) {
  if(snapshot(value)===snapshot(base)) return '';
  if(value && base && !Array.isArray(value) && !Array.isArray(base) &&
    typeof value==='object' && typeof base==='object') {
    return '{'+[...new Set([...Object.keys(value),...Object.keys(base)])].sort().map(key=>{
      const d=delta(value[key],base[key]); return d ? 'S'+hex(key)+';'+d : '';
    }).join('')+'}';
  }
  return '='+snapshot(value);
}
const scenarios=[];
const add=(steps,restriction='')=>scenarios.push({steps,restriction});
const op=(name,arg)=>arg===undefined?{op:name}:{op:name,arg};
add([op('get'),op('save',{themeVariables:{primaryColor:'#123456'}}),op('site',{theme:'default',themeVariables:{primaryColor:'#123456'}}),
  op('add',{theme:'dark'}),op('set',{theme:'default'}),op('scope','flowchart-v2'),op('reset'),op('scope','pie')]);
for(const type of ['flowchart-v2','flowchart-elk','pie','packet','classDiagram','stateDiagram','xychart','railroadPeg','unknown','']) {
  const section=diagramKey(type);
  add([op('save',{look:'handDrawn',themeVariables:{primaryColor:'#abc'}}),
    op('site',{theme:'default',look:'handDrawn',[section]:{theme:'dark',look:'neo',layout:'elk'}}),
    op('scope',type),op('add',{theme:'default',look:'classic'}),op('set',{layout:'custom'}),
    op('scope'),op('scope',type),op('reset'),op('scope',type)]);
}
for(const theme of ['default','dark','null','unknown','constructor','__proto__','',null,false]) {
  add([op('site',{theme}),op('add',{theme:'dark',themeVariables:{primaryColor:'#123'}}),
    op('update',{theme:'default'}),op('reset'),op('set',{theme}),op('reset',{theme:'dark',look:'neo'})]);
}
for(const secure of [[],['theme'],['fontFamily','primaryColor'],['securityLevel','startOnLoad']]) {
  add([op('site',{secure}),op('add',{secure:[],securityLevel:'loose',startOnLoad:false,theme:'dark',fontFamily:'Arial',themeVariables:{primaryColor:'#123'}}),
    op('set',{fontFamily:'<tag>',themeVariables:{primaryColor:'url(data:abc)',lineColor:'red'},__bad:1}),
    op('update',{secure:['theme','lineColor']}),op('scope','pie'),op('reset')]);
}
for(const vars of [{},{primaryColor:'#abcdef'},{fontFamily:'Arial'},{xyChart:{titleColor:'#123'}},
  {darkMode:false},{THEME_COLOR_LIMIT:0},{custom:'retained'}]) {
  add([op('save',{themeVariables:vars}),op('site',{theme:'dark',themeVariables:vars}),
    op('add',{theme:'default',themeVariables:{primaryColor:'#456'}}),op('add',{theme:'dark'}),op('reset'),op('set',{theme:'default'})]);
}
add([op('site',{fontFamily:'A',flowchart:{curve:'linear'}}),op('set',{fontFamily:'B',unknown:{x:1}}),
  op('add',{fontFamily:'C',unknown:{x:2}}),op('add',{fontFamily:'D',themeVariables:{fontFamily:'E'}}),
  op('reset',{theme:'null',fontFamily:'F'}),op('add',{look:'neo'}),op('reset')]);
add([op('save',{theme:'dark'}),op('site',{theme:'dark'}),op('site',{look:'neo'}),op('reset'),op('save',{}),op('get')]);
add([op('set',{lazyLoadedDiagrams:true}),op('reset'),op('set',{loadExternalDiagramsAtStartup:true}),op('reset')]);
add([op('site',{fontFamily:'A\ud800\n\u2028\udc00'}),op('set',{fontFamily:'B\ud800'}),op('reset')]);
let seed=0x516e123;
const random=()=>seed=(Math.imul(seed,1664525)+1013904223)>>>0;
for(let i=0;i<24;i++) {
  const steps=[op('save',{themeVariables:{primaryColor:'#abc'}}),op('site',{theme:'default'})];
  for(let j=0;j<12;j++) {
    const name=['site','update','set','add','reset','scope'][random()%6];
    const arg=name==='scope'?['pie','packet','flowchart-v2',undefined][random()%4]:name==='reset'?undefined:
      [{theme:'dark'},{theme:'default'},{look:'neo'},{fontFamily:'Arial'},{flowchart:{look:'handDrawn'}},{themeVariables:{lineColor:'#123'}}][random()%6];
    steps.push(op(name,name==='site'?{theme:'default',...arg}:arg));
  }
  add(steps);
}
// Terminal native restrictions retain the actual original result, not invented errors.
for(const theme of ['forest','base','neutral','neo']) add([op('site',{theme})],'unported-theme');
add([op('scope','flowchart-v2')],'unported-theme');
add([op('site',{theme:'dark',themeVariables:null})],'theme-overrides-mapping');
add([op('site',{custom:{a:{b:{c:1}}}})],'deep-shared-container');
add([op('site',{custom:[{x:1}]})],'object-array-identity');
add([op('site',{secure:'theme'})],'secure-string-list');
add([op('site',[])],'mapping-input');

const rows=[], values=[], ids=new Map();
const intern=value=>{
  if(!ids.has(value)) { ids.set(value,values.length); values.push(value); }
  return ids.get(value);
};
for(let i=0;i<scenarios.length;i++) {
  const {api,warnings}=session();
  const scenario=scenarios[i];
  for(let j=0;j<scenario.steps.length;j++) {
    const step=scenario.steps[j];
    let output;
    try {
      const arg=step.arg===undefined?undefined:clean(step.arg);
      const methods={site:'setSiteConfig',update:'updateSiteConfig',save:'saveConfigFromInitialize',set:'setConfig',add:'addDirective',reset:'reset',scope:'setDiagramConfigScope',get:'getConfig'};
      api[methods[step.op]](arg);
      output=[delta(clean(api.getSiteConfig()),baseline),delta(clean(api.getConfig()),baseline),snapshot(clean(api.getUserDefinedConfig()))].map(intern).join('|')+'|'+warnings.length;
    } catch(error) {
      if(!['TypeError','Error'].includes(error.name)) throw error;
      // Only known theme/color exceptions belong to source behavior.
      if(error.name==='Error' && !/color/i.test(error.message)) throw error;
      output='E';
    }
    rows.push([i,j,hex(JSON.stringify(step)),output,scenario.restriction||'-'].join('\t'));
  }
}
function writeOrCheck(relative,text) {
  const file=path.join(root,relative);
  if(process.argv.includes('--write')) writeFileSync(file,text);
  else assert.equal(readFileSync(file,'utf8').replaceAll('\r\n','\n'),text,relative);
}
writeOrCheck('tests/mermaid_cpp/fixtures/session.tsv',
  '# Original config.ts lifecycle: scenario/step/UTF-16 JSON operation/full-state delta/native restriction\n'+
  '# defaults\t'+snapshot(baseline)+'\n'+values.map((value,i)=>'# value\t'+i+'\t'+value).join('\n')+'\n'+rows.join('\n')+'\n');
// JSON data projection only: undefined properties and family font callbacks are
// absent. The session never calls those callbacks; family integration is separate.
const cppString=s=>'u'+JSON.stringify(s);
const literal=value=>{
  if(value===null) return 'ConfigValue{}';
  if(typeof value==='boolean') return 'ConfigValue{.kind=ConfigKind::boolean, .boolean='+value+'}';
  if(typeof value==='number') return 'ConfigValue{.kind=ConfigKind::number, .number='+value+'}';
  if(typeof value==='string') return 'config_detail::string('+cppString(value)+')';
  if(Array.isArray(value)) return 'ConfigValue{.kind=ConfigKind::sequence, .sequence={'+value.map(literal).join(',')+'}}';
  return 'ConfigValue{.kind=ConfigKind::mapping, .mapping={\n'+Object.entries(value).map(([k,v])=>'    {'+cppString(k)+', '+literal(v)+'}').join(',\n')+'\n}}';
};
writeOrCheck('third_party/mermaid-cpp/src/config_defaults.inc',
  '// Generated by session-oracle.mjs from original defaultConfig/schema/theme. MIT.\nreturn '+literal(baseline)+';\n');
console.log(JSON.stringify({commit:lock.commit,scenarios:scenarios.length,observations:rows.length,mode:process.argv.includes('--write')?'write':'check'}));
