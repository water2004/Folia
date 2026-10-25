// Pinned original Radar Langium parser, DB and polar/curve helper observations.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,readdirSync} from 'node:fs';
import vm from 'node:vm';
import {JSDOM} from './oracle/node_modules/jsdom/lib/api.js';
import {cases} from './radar-cases.mjs';
const dom=new JSDOM('<!doctype html><body></body>');globalThis.window=dom.window;globalThis.document=dom.window.document;
const {frontend}=await import('./frontend-oracle.mjs');
const {read,hex,strip,withoutImports}=await import('./config-reference.mjs');
const {default:mermaid}=await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const chunks=new URL('./oracle/node_modules/mermaid/dist/chunks/mermaid.core/',import.meta.url);
const map=JSON.parse(readFileSync(new URL(readdirSync(chunks).find(n=>n.endsWith('.mjs.map')&&readFileSync(new URL(n,chunks),'utf8').includes('/radar/renderer.ts')),chunks)));
for(const file of ['renderer.ts','db.ts','parser.ts','styles.ts']) {
 const index=map.sources.findIndex(n=>n.endsWith('/radar/'+file));assert(index>=0,file);
 assert.equal(map.sourcesContent[index].replaceAll('\r\n','\n'),read('packages/mermaid/src/diagrams/radar/'+file));
}
const helpers=vm.runInNewContext(strip(withoutImports(read('packages/mermaid/src/diagrams/radar/renderer.ts')))+'\n({relativeRadius,closedRoundCurve})');
const rows=['# Mermaid 12.2.0 Radar parser/DB/config and original curve-helper observations; UTF-16 hex JSON.'];let successes=0;
for(const [name,source,failure]of cases)for(const theme of ['default','dark']) {
 const site={theme,fontFamily:'Arial'};let result=null;
 try {
  const prepared=frontend(site)(source);mermaid.initialize({...prepared.effective,startOnLoad:false});
  const {db}=await mermaid.mermaidAPI.getDiagramFromText(prepared.code.cleaned,{title:prepared.title});
  const axes=db.getAxes(),curves=db.getCurves(),options=db.getOptions(),config=db.getConfig();
  const radius=Math.min(config.width,config.height)/2,max=options.max??Math.max(...curves.flatMap(c=>c.entries));
  const geometry=curves.map(c=>{
   if(c.entries.length!==axes.length)return null;
   const points=c.entries.map((v,i)=>{const angle=2*Math.PI*i/axes.length-Math.PI/2,r=helpers.relativeRadius(v,options.min,max,radius);return {x:r*Math.cos(angle),y:r*Math.sin(angle)};});
   return {points,path:options.graticule==='circle'?helpers.closedRoundCurve(points,config.curveTension):null};
  });
  const t=prepared.effective.themeVariables;
  result={title:db.getDiagramTitle(),accTitle:db.getAccTitle(),accDescription:db.getAccDescription(),axes,curves,options,config,
   theme:{fontSize:t.fontSize,textColor:t.textColor,titleColor:t.titleColor,radar:t.radar,colors:Array.from({length:t.THEME_COLOR_LIMIT},(_,i)=>t['cScale'+i])},geometry};
  assert.notEqual(failure,null,name+' unexpectedly parsed');++successes;
 }catch(e){assert.equal(failure,null,`${name}: ${e.stack}`);}
 rows.push([name+'-'+theme,hex(source),hex(JSON.stringify(site)),hex(JSON.stringify(result))].join('\t'));
}
const output=new URL('../../tests/mermaid_cpp/fixtures/radar.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(output,expected);else assert.equal(readFileSync(output,'utf8').replaceAll('\r\n','\n'),expected);
dom.window.close();console.log(`Radar: ${successes} original parser/config/curve inputs and ${rows.length-1-successes} common errors checked.`);
