// Official Mermaid theme execution, independent of the native code generator.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import * as color from './oracle/node_modules/khroma/dist/index.js';
import { root, lock, themes, hex, snapshot } from './config-reference.mjs';

export const khromaRoot = path.join(root, 'tools/mermaid-cpp/oracle/node_modules/khroma');
for (const file of lock.packages.find(p => p.name === 'khroma').files) {
  assert.equal(createHash('sha256').update(readFileSync(path.join(khromaRoot, file.path))).digest('hex'), file.sha256, file.path);
}
const keywordSource = readFileSync(path.join(khromaRoot, 'src/color/keyword.ts'), 'utf8');
export const keywords = [...keywordSource.matchAll(/^    (\w+): '(#[a-f0-9]+)'/gm)].map(m => [m[1], m[2]]);
assert.equal(keywords.length, 147);
export function writeOrCheck(relative, content) {
  const file = path.join(root, relative);
  if (process.argv.includes('--write')) writeFileSync(file, content);
  else assert.equal(readFileSync(file, 'utf8').replaceAll('\r\n', '\n'), content, relative);
}
const encode = value => hex(JSON.stringify(value));
const clean = value => JSON.parse(JSON.stringify(value)); // ConfigValue has no undefined: omit absent properties.
const themeRows = [];
let seed = 0x573812;
const random = () => (seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0);
const randomHex = () => '#' + (random() & 0xffffff).toString(16).padStart(6, '0');
for (const name of ['default', 'dark']) {
  const baseline = clean(themes[name].getThemeVariables({}));
  const cases = [undefined, {}, null, false, 3, 'dark', [], {darkMode:true}, {darkMode:false},
    {THEME_COLOR_LIMIT:0}, {THEME_COLOR_LIMIT:1}, {THEME_COLOR_LIMIT:4}, {THEME_COLOR_LIMIT:12},
    {THEME_COLOR_LIMIT:13}, {THEME_COLOR_LIMIT:-1}, {THEME_COLOR_LIMIT:2.5},
    {fontFamily:'中文\ud800 & "quoted"', custom:{nested:[1,null,false]}},
    {xyChart:{titleColor:'#123', unknown:'retained'}, radar:{curveOpacity:0, custom:true}, packet:{labelColor:'#456'}},
    {cynefin:{domainFontSize:0}, wardley:{axisColor:null}}, {venn1:null}, {venn1:''},
    {quadrantPointFill:'#abcdef'}, {labelTextColor:'calculated'}, {scaleLabelColor:'calculated'},
    {updateColors:null}, {calculate:false}, JSON.parse('{"__proto__":{"primaryColor":"red"}}')];
  const colors = ['#123','#1234','#123456','#12345678','RED','transparent','rebeccapurple',
    'rgb(12, 40, 90)','rgba(10%, 20%, 30%, 0.4)','hsl(-30, 40%, 50%)','hsla(.5turn 40% 50% / 25%)',
    'hsl(1e2rad, 30%, 80%)','var(--color)','currentColor','not-a-color','',null,false,0,{},[]];
  for (const key of ['primaryColor','mainBkg','secondaryColor','tertiaryColor','background','primaryTextColor','labelTextColor','cScale0','git0']) {
    for (const value of colors) cases.push({[key]:value});
  }
  for (const key of ['pie1','pie12','venn1','flowContainerStroke','stateBkg','rowOdd','scaleLabelColor','gitInv0'])
    for (const value of [null,false,0,'','#abc']) cases.push({[key]:value});
  for (const key of ['xyChart','radar','cynefin','wardley','packet'])
    for (const value of [null,false,0,'',[],{unknown:1}]) cases.push({[key]:value});
  for (let i=0;i<48;i++) cases.push({primaryColor:randomHex(),mainBkg:randomHex(),secondaryColor:randomHex(),
    labelTextColor:randomHex(),darkMode:!!(i%2),cScale0:randomHex(),xyChart:{titleColor:randomHex()}});
  // One sweep over every resulting property tests final override restoration.
  for (const key of Object.keys(baseline)) cases.push({[key]:baseline[key]});
  themeRows.push([name,'baseline',encode({}),encode(baseline),'-'].join('\t'));
  for (let i=0;i<cases.length;i++) {
    const input=cases[i];
    const argument=input===undefined?undefined:clean(input);
    const before=snapshot(input);
    let result, error;
    try { result=themes[name].getThemeVariables(argument); }
    catch(e) {
      if (!(e instanceof TypeError) && !String(e.message).startsWith('Unsupported color format:')) throw e;
      error=e.name+': '+e.message;
    }
    let restriction='-';
    if(snapshot(argument)!==before) restriction='no-mutable-color-channel-objects';
    if(input!==undefined && (input===null || typeof input!=='object' || Array.isArray(input))) restriction='mapping-only';
    if(input && Object.keys(input).some(k=>['__proto__','constructor','prototype','updateColors','calculate'].includes(k))) restriction='no-prototype-or-method-overrides';
    if(input && typeof input==='object' && 'THEME_COLOR_LIMIT' in input && (!Number.isInteger(input.THEME_COLOR_LIMIT) || input.THEME_COLOR_LIMIT<0 || input.THEME_COLOR_LIMIT>12)) restriction='color-limit-0-through-12';
    let delta;
    if(error) delta='E'+hex(error);
    else {
      const output=clean(result), set={};
      for(const [key,value] of Object.entries(output)) if(snapshot(value)!==snapshot(baseline[key])) set[key]=value;
      delta=encode({set,remove:Object.keys(baseline).filter(k=>!(k in output))});
    }
    themeRows.push([name,String(i),input===undefined?'U':encode(input),delta,restriction].join('\t'));
  }
}
writeOrCheck('tests/mermaid_cpp/fixtures/themes.tsv',
  '# Official Mermaid 12.2.0: theme/id/UTF-16 JSON overrides (U=absent)/baseline or delta JSON (E=error)/native restriction\n'+themeRows.join('\n')+'\n');

const colorRows=[];
const paints=[...keywords.map(([k])=>k),'RED','#ABC','#abcd','#aabbcc','#aabbccdd',
  'rgb(100% 0% 50% / 25%)','rgba(1.5, 2.5, 3.5, .4)','rgb(-1,300,0)',
  'hsl(-400, 20%, 30%)','hsl(.5turn 50% 50%)','hsl(1rad, 0%, 30%)','hsl(100grad, 20%, 30%)',
  'HSLA(90DEG, 20%, 30%, 40%)','rgba(1,2,3,+.2)','hsl(1e-2,20%,40%)',
  '','#12','#12345','#1234567',' red','red ','currentColor','var(--x)','\ud800','rgb(1e-2,2,3)',
  'rgb(+1,2,3)','hsl(1,2,3)','rgb(1,2,3)\n'];
for(let i=0;i<80;i++) paints.push(randomHex());
const operations=[['invert',null],['isDark',null],['lighten',10],['darken',25],['lighten',0],
  ['lighten',undefined],['adjust',{h:120,s:-40,l:10}],['adjust',{r:5,g:-10,b:15}],['adjust',{h:0}]];
for(const paint of paints) for(const [op,arg] of operations) {
  let output;
  try { output=snapshot(color[op](paint,arg)); }
  catch(e) { if(!String(e.message).startsWith('Unsupported color format:')) throw e; output='E'; }
  colorRows.push([op,encode(paint),arg===undefined?'U':encode(arg),output].join('\t'));
}
writeOrCheck('tests/mermaid_cpp/fixtures/theme-colors.tsv',
  '# Khroma 2.1.0: operation/UTF-16 JSON color/argument (U=absent)/typed result\n'+colorRows.join('\n')+'\n');
writeOrCheck('third_party/mermaid-cpp/src/theme_color_names.inc',
  '// Generated from hash-locked Khroma 2.1.0 src/color/keyword.ts. MIT.\n'+
  'constexpr std::pair<std::string_view, std::string_view> color_names[] = {\n'+
  keywords.map(([k,v])=>`    {"${k}", "${v}"},`).join('\n')+'\n};\n');
console.log(JSON.stringify({themeObservations:themeRows.length,colorObservations:colorRows.length,mode:process.argv.includes('--write')?'write':'check'}));
