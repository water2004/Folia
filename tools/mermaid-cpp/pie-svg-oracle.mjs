// Capture the pinned official Pie renderer. Acceptance belongs to compare-pie.py.
import assert from 'node:assert/strict';
import { readFileSync, writeFileSync, readdirSync, mkdirSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { dirname, resolve } from 'node:path';
import { JSDOM } from './oracle/node_modules/jsdom/lib/api.js';
for(const argument of process.argv.slice(2)) assert(
 argument==='--write'||argument==='--check'||argument.startsWith('--output='),
 'Use --check/--write [--output=file.json]; native comparison is compare-pie.py.');
const lock = JSON.parse(readFileSync(new URL('./source-lock.json',import.meta.url)));
for (const file of lock.files) assert.equal(createHash('sha256').update(readFileSync(
  new URL('../../.reference/mermaid-js/'+file.path,import.meta.url))).digest('hex'),file.sha256);
const dom = new JSDOM('<!doctype html><body></body>');
globalThis.window=dom.window;
globalThis.document=dom.window.document;
const {default:mermaid}=await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const {parse}=await import('./oracle/node_modules/@mermaid-js/parser/dist/mermaid-parser.core.mjs');
const chunks=new URL('./oracle/node_modules/mermaid/dist/chunks/mermaid.core/',import.meta.url);
assert.equal(JSON.parse(readFileSync(new URL('./oracle/node_modules/mermaid/package.json',import.meta.url))).version,'12.2.0');
const chunk=readdirSync(chunks).find(name=>/^pieDiagram-.*\.mjs$/.test(name));
assert(chunk);
const map=JSON.parse(readFileSync(new URL(chunk+'.map',chunks)));
for(const name of ['pieRenderer.ts','pieStyles.ts']) {
 const i=map.sources.findIndex(source=>source.endsWith('/diagrams/pie/'+name));
 assert(i>=0);
 assert.equal(map.sourcesContent[i].replaceAll('\r\n','\n'),readFileSync(new URL(
  '../../.reference/mermaid-js/packages/mermaid/src/diagrams/pie/'+name,import.meta.url),'utf8').replaceAll('\r\n','\n'));
}
const {diagram:definition}=await import(new URL(chunk,chunks));
const hex=text=>Array.from({length:text.length},(_,i)=>text.charCodeAt(i).toString(16).padStart(4,'0')).join('')||'-';
const palette=['#112233','#223344','#334455','#445566','#556677','#667788',
 '#778899','#8899aa','#99aabb','#aabbcc','#bbccdd','#ccddee'];
const cases=[];
for(const position of ['right','left','top','bottom','center']) {
 for(const [name,source] of [
  ['single','pie\n"Only":1'],
  ['donut','pie showData\ntitle Long title\n"A":2\n"B":3'],
  ['filtered','pie\n"tiny":0.5\n"A":30\n"B":69.5'],
  ['unicode','pie\ntitle Cafe\u0301 \u4e2d\u6587\n"\u{1f600}":1\n"\u0645\u0631\u062d\u0628\u0627":2'],
  ['xml','pie\ntitle A & B < C\n"A & B":1\n"<tag>":2'],
  ['zero','pie\n"A":0\n"B":0'],
  ['cycle','pie\n'+Array.from({length:14},(_,i)=>`"S${i}":1`).join('\n')],
  ['empty','pie']
 ]) {
  for(const dark of [false,true]) cases.push({name:`${position}-${name}-${dark?'dark':'light'}`,
   position,source,dark,donut:name==='donut'?0.5:0,textPosition:0.75,
   highlight:name==='donut'?'A':name==='filtered'?'hover':'',maxWidth:!dark,
   titleWidth:name==='donut'?1300:120});
 }
}
const fixture=['# name source position widths title-width donut text-position highlight max-width dark'];
const observations=[];
const hash=value=>createHash('sha256').update(value).digest('hex');
const numeric=value=>Number.parseFloat(value);
function snapshot(svg) {
 const translate=node=>(node.getAttribute('transform')??'translate(0,0)').slice(10,-1).split(/[, ]+/).map(Number);
 const attr=(node,key)=>node.style.getPropertyValue(key)||node.getAttribute(key)||'';
 const group=svg.querySelector('g');
 return {
  viewBox:svg.getAttribute('viewBox').split(' ').map(Number),
  width:svg.getAttribute('width'),height:svg.getAttribute('height'),maxWidth:svg.style.maxWidth,
  outer:translate(group),pie:translate(group.querySelector('g')),
  radius:numeric(svg.querySelector('circle').getAttribute('r')),
  paths:[...svg.querySelectorAll('path')].map(n=>[n.getAttribute('d'),attr(n,'fill'),n.getAttribute('class')]),
  slices:[...svg.querySelectorAll('.slice')].map(n=>[n.textContent,...translate(n),attr(n,'text-anchor')]),
  title:[svg.querySelector('.pieTitleText').textContent,svg.querySelector('.pieTitleText').getAttribute('x'),svg.querySelector('.pieTitleText').getAttribute('y')],
  legends:[...svg.querySelectorAll('.legend')].map(n=>[...translate(n),n.querySelector('text').textContent,
   attr(n.querySelector('rect'),'fill'),attr(n.querySelector('rect'),'stroke'),
   n.querySelector('rect').getAttribute('width'),n.querySelector('rect').getAttribute('height'),
   n.querySelector('text').getAttribute('x'),n.querySelector('text').getAttribute('y')])
 };
}
for(const item of cases) {
 const ink=item.dark?'#eeeeee':'#111111';
 const theme={fontFamily:'Arial',pieStrokeColor:ink,pieOuterStrokeColor:ink,
  pieTitleTextColor:ink,pieSectionTextColor:ink,pieLegendTextColor:ink,
  pieStrokeWidth:'2px',pieOuterStrokeWidth:'2px',pieOpacity:'0.7',
  pieTitleTextSize:'25px',pieSectionTextSize:'17px',pieLegendTextSize:'17px'};
 palette.forEach((paint,i)=>theme[`pie${i+1}`]=paint);
 mermaid.initialize({startOnLoad:false,theme:'base',themeVariables:theme,
  pie:{legendPosition:item.position,donutHole:item.donut,textPosition:item.textPosition,
   highlightSlice:item.highlight,useMaxWidth:item.maxWidth}});
 const diagram=await mermaid.mermaidAPI.getDiagramFromText('pie');
 diagram.db.clear(); await diagram.parser.parse(item.source);
 // Compare the family renderer on identical display input. commonDb sanitizes
 // title strings before this boundary; the native raw parser deliberately does
 // not implement that shared MermaidAPI preprocessing stage yet.
 const raw=await parse('pie',item.source);
 const originalTitle=diagram.db.getDiagramTitle;
 diagram.db.getDiagramTitle=()=>raw.title??'';
 const widths=[...diagram.db.getSections()].map((_,i)=>40+i*7.25);
 document.body.innerHTML='<svg id="probe" xmlns="http://www.w3.org/2000/svg"></svg>';
 dom.window.Element.prototype.getBoundingClientRect=function(){
  if(this.classList.contains('pieTitleText')) return {width:item.titleWidth};
  const i=[...document.querySelectorAll('.legend text')].indexOf(this);
  assert(i>=0);return {width:widths[i]};
 };
 await diagram.render('probe','12.2.0');
 diagram.db.getDiagramTitle=originalTitle;
 const svg=document.querySelector('svg');
 const finite=snapshot(svg).viewBox.every(Number.isFinite) && !svg.outerHTML.includes('Infinity');
 fixture.push([item.name,hex(item.source),item.position,widths.join(',')||'-',item.titleWidth,
  item.donut,item.textPosition,hex(item.highlight),Number(item.maxWidth),Number(item.dark),Number(finite)].join('\t'));
 const css=definition.styles(theme).replace(/\/\/[^\n]*/g,'');
 const styles=[...css.matchAll(/([^{}]+)\{([^{}]*)\}/g)].map(([,selectorText,body])=>({
  selector:selectorText.trim(),
  declarations:body.split(';').map(value=>value.trim()).filter(Boolean).map(value=>{
   const colon=value.indexOf(':');return [value.slice(0,colon).trim(),value.slice(colon+1).trim()];
  })
 }));
 observations.push({name:item.name,sourceSha256:hash(item.source),finite,
  svg:svg.outerHTML,svgSha256:hash(svg.outerHTML),snapshot:finite?snapshot(svg):null,styles});
}
const file=new URL('../../tests/mermaid_cpp/fixtures/pie-svg.tsv',import.meta.url);
const output=fixture.join('\n')+'\n';
if(process.argv.includes('--write')) writeFileSync(file,output);
else assert.equal(readFileSync(file,'utf8').replaceAll('\r\n','\n'),output);
const destination=resolve(process.argv.find(arg=>arg.startsWith('--output='))?.slice(9)??'build/mermaid-cpp/pie-browser/original.json');
mkdirSync(dirname(destination),{recursive:true});
writeFileSync(destination,JSON.stringify({schemaVersion:1,
 upstream:{version:'12.2.0',commit:lock.commit},
 sourceLockSha256:hash(readFileSync(new URL('./source-lock.json',import.meta.url))),
 fixtureSha256:hash(readFileSync(file)),cases:observations},null,2)+'\n');
dom.window.close();
console.log(`Captured ${cases.length} official Pie SVG cases: ${destination}`);
