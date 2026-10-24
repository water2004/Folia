// Official sanitizer, merge and appearance functions; no native implementation here.
import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { root, lock, defaults, configKeys, themes, sanitizeDirective, secureSanitize,
  assign, appearance, hex, snapshot } from './config-reference.mjs';

const cases = [];
const add = (op, input, divergence = '') => cases.push({op,input,divergence});
for (const input of [null, false, 7, 'raw', [], {}, [{theme:'dark',unknown:4}],
  {theme:'dark',securityLevel:'loose',unknown:1,config:{theme:'forest'},__x:1,constructor:1,prototype:2},
  {themeCSS:'a{}',fontFamily:'Arial',themeVariables:{primaryColor:'#123',fontFamily:'Arial'}},
  {themeVariables:{primaryColor:'var(--x)',fontSize:'16px',fontFamily:'a-b',darkMode:true}},
  {theme:null,flowchart:{theme:null,curve:'basis',unknown:4},secure:['theme']},
]) add('sanitize',input);
for (const key of [...configKeys].sort()) add('sanitize',{[key]:'test'});
for (const key of ['themeCSS','fontFamily','altFontFamily']) for (const value of ['','{}','}{','{{}}','{','}','x{y}z','\ud800{\udc00}','a\n{}',2,false,null,{},[]]) add('sanitize',{[key]:value});
for (const key of ['primaryColor','fontSize','fontFamily','pie1']) for (const value of ['#abcdef','rgb(1, 2, 3)','rgba(1,2,3,0.5)','hsl(30, 20%, 40%)','a-b','a_b','a/b','\n','\u00a0','\ud800','',true,0,null,{}]) add('sanitize',{themeVariables:{[key]:value}});
for (const dict of ['nodeColors','filenameIcons','extensionIcons']) {
  for (const value of ['red','RED','#123','#12345678','#12','rgb(1, 2, 3)','RGB(1%,2%,3%)','hsl(1,2%,3%)','rgba(1,2,3,1)','a:b','a_b-c','a:b:c','\u00a0','rgb(1\u00a02)','red\n','red\r\n','\ud800',null,1,false,{},[]]) {
    add('sanitize',{[dict]:{custom:value,prototype:value,__x:value,constructor:value,Proto:value}});
  }
  add('sanitize',{[dict]:['red','bad!']},'sparse-array');
}
add('sanitize', {themeVariables:'bad!'}, 'primitive-theme-variables');
for (const secure of [[],['theme'],['theme','primaryColor'],['securityLevel','startOnLoad']]) {
  for (const value of [{secure:[],theme:'dark',securityLevel:'loose',fontFamily:'<script>',flowchart:{theme:'forest',__x:1}},
    {themeVariables:{primaryColor:'url(data:abc)',lineColor:'red',fontFamily:'a>b'},themeCSS:'URL(data:abc)'},
    [{theme:'dark'},'<tag>','fine'],null,false,'hello']) {
    add('secure',{value,secure},Array.isArray(value)?'sparse-array':'');
  }
}
const values = [null,false,1,'x',[],[1,1,2],[{a:1},{a:1}],{}, {a:null}, {a:1}, {a:{b:1}}, {a:[1,2]}, {a:{b:{c:1,d:2}}}];
for (const depth of [0,1,2,3]) for (const dst of values) for (const source of values) {
  // Arrays with object properties/sparse indices cannot be represented by owned ConfigValue trees.
  add('merge',{dst,src:source,depth}, Array.isArray(dst) && source && !Array.isArray(source) && typeof source==='object' ? 'array-properties' : '');
}
add('merge',{dst:{a:[{x:1}]},src:{a:[{x:1}]},depth:2});
add('merge',{dst:{a:{b:{c:1,d:2}}},src:{a:{b:{c:null,e:3}}},depth:2});
for (const diagram of ['flowchart-v2','flowchart-elk','classDiagram','stateDiagram','xychart','railroadPeg','pie','packet','unknown',null]) {
  for (const key of ['theme','look','layout']) for (const value of ['dark','forest','default','neo','neo-dark','redux','redux-dark','redux-color','redux-dark-color','base','neutral','null','bogus','__proto__','constructor','classic','handDrawn','elk','',null,false,3]) {
    const section = {'flowchart-v2':'flowchart','flowchart-elk':'flowchart',classDiagram:'class',stateDiagram:'state',xychart:'xyChart',railroadPeg:'railroad'}[diagram] ?? diagram;
    add('appearance',{cfg:{theme:'default',look:'classic',layout:'dagre',[section]:{[key]:'default'}},sum:{[key]:value},directives:[],site:{},diagram});
    add('appearance',{cfg:{theme:'default',[section]:{[key]:'default'}},sum:{[key]:'dark',[section]:{[key]:value}},directives:[{[key]:'forest'}],site:{[key]:'neutral'},diagram});
  }
}
add('appearance',{cfg:{theme:'default',flowchart:{theme:'default'}},sum:{},directives:[{theme:'dark'},{theme:'forest'}],site:{theme:'neutral'},diagram:'flowchart-v2'});
add('appearance',{cfg:{},sum:{},directives:[],site:{},diagram:'flowchart-v2'});
add('appearance',{cfg:{},sum:{},directives:[],site:{},diagram:'pie'});
for (const value of ['<tag>','url(data:x)','abc']) for (const secure of [[],['0']]) add('secure',{value,secure});
for (const depth of [0,1,2]) add('merge',{
  dst:{},src:JSON.parse('{"__proto__":{"theme":"dark"},"constructor":4}'),depth
},depth===0?'prototype-setter':'');
add('sanitize',JSON.parse('{"__proto__":{"theme":"dark"},"constructor":4,"theme":"dark"}'));
add('appearance',{cfg:{},sum:{theme:['dark']},directives:[],site:{},diagram:'pie'});
add('appearance',{cfg:{},sum:{theme:['null']},directives:[],site:{},diagram:'pie'});
const rows = [];
function treeRestriction(value) {
  if (!value || typeof value !== 'object') return '';
  if (Array.isArray(value)) {
    if (Object.keys(value).some(k=>!/^\d+$/.test(k) || Number(k)>=value.length)) return 'array-properties';
    for(let i=0;i<value.length;i++) if(!Object.hasOwn(value,i)) return 'sparse-array';
  }
  for(const item of Object.values(value)) { const reason=treeRestriction(item); if(reason) return reason; }
  return '';
}
for (let i=0;i<cases.length;i++) {
  const {op,input,divergence} = cases[i];
  // Match the serialized input's independent owned trees, not accidental aliases
  // introduced by the Cartesian-product case builder reusing JS object references.
  const arg = JSON.parse(JSON.stringify(input));
  let restriction=divergence;
  let output;
  try {
    switch(op) {
      case 'sanitize': sanitizeDirective(arg); output=arg; break;
      case 'secure': output=secureSanitize(arg.value,arg.secure); break;
      case 'merge': output=assign(arg.dst,arg.src,{depth:arg.depth}); break;
      case 'appearance': output=appearance(arg.cfg,arg.sum,arg.directives,arg.site,arg.diagram); break;
    }
    restriction ||= treeRestriction(output);
    output=snapshot(output);
  } catch (error) {
    // Harness/assertion/wiring failures must never become expected native errors.
    if(error.name!=='TypeError') throw error;
    output='E';
  }
  rows.push([String(i),op,hex(JSON.stringify(input)),output,restriction||'-'].join('\t'));
}
const writeOrCheck = (relative,content) => {
  const file=path.join(root,relative);
  if(process.argv.includes('--write')) writeFileSync(file,content);
  else assert.equal(readFileSync(file,'utf8').replaceAll('\r\n','\n'),content,relative);
};
writeOrCheck('tests/mermaid_cpp/fixtures/config.tsv','# Official Mermaid 12.2.0: id/operation/UTF-16 JSON input/typed result/explicit native restriction\n'+rows.join('\n')+'\n');
const cpp = text => 'u"'+Array.from(text,c=>c==='"'?'\\"':c==='\\'?'\\\\':c).join('')+'"';
const appearanceDefaults=[];
for(const [section,value] of Object.entries(defaults)) {
  if(value && typeof value==='object' && !Array.isArray(value)) for(const key of ['theme','look','layout']) {
    if(value[key]!==undefined) appearanceDefaults.push(`    {${cpp(section)}, ${cpp(key)}, ${cpp(value[key])}},`);
  }
}
for(const key of ['theme','look','layout']) if(defaults[key]!==undefined) appearanceDefaults.push(`    {u"", ${cpp(key)}, ${cpp(defaults[key])}},`);
writeOrCheck('third_party/mermaid-cpp/src/config_schema_data.inc',
  '// Generated by config-oracle.mjs from pinned schema/defaultConfig/themes. MIT.\n'+
  'constexpr std::u16string_view config_keys[] = {\n'+[...configKeys].sort().map(k=>'    '+cpp(k)+',').join('\n')+'\n};\n'+
  'constexpr std::u16string_view theme_names[] = {\n'+Object.keys(themes).sort().map(k=>'    '+cpp(k)+',').join('\n')+'\n};\n'+
  'struct AppearanceDefault { std::u16string_view section, key, value; };\nconstexpr AppearanceDefault appearance_defaults[] = {\n'+appearanceDefaults.join('\n')+'\n};\n');
console.log(JSON.stringify({commit:lock.commit,observations:rows.length,configKeys:configKeys.size,mode:process.argv.includes('--write')?'write':'check'}));
