import assert from 'node:assert/strict';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { cases } from './wardley-cases.mjs';
import { invalid } from './wardley-cases.mjs';
assert.equal(JSON.parse(readFileSync('tools/mermaid-cpp/oracle/node_modules/mermaid/package.json','utf8')).version,'12.2.0','Wardley oracle must use pinned Mermaid 12.2.0');

export const inspectWardley = root => {
  const svg = root.querySelector('svg');
  const box = e => { const b=e.getBBox(); return [b.x,b.y,b.width,b.height]; };
  const attrs = (selector, names) => [...svg.querySelectorAll(selector)].map(e => {
    const style=getComputedStyle(e);
    const rotation=(e.getAttribute('transform')||'').match(/^rotate\(\s*(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\s*\)$/);
    return {...Object.fromEntries(names.map(n=>[n,e.getAttribute(n)])),
      family:style.fontFamily,fill:style.fill,stroke:style.stroke,
      strokeWidth:style.strokeWidth,strokeDasharray:style.strokeDasharray,
      opacity:style.opacity,anchor:style.textAnchor,text:e.textContent,
      markerEnd:e.hasAttribute('marker-end'),markerStart:e.hasAttribute('marker-start'),
      rotation:rotation?{angle:Number(rotation[1]),cx:Number(rotation[2]),cy:Number(rotation[3])}:null,
      box:box(e)};
  });
  return {
    view:[svg.viewBox.baseVal.x,svg.viewBox.baseVal.y,svg.viewBox.baseVal.width,svg.viewBox.baseVal.height],
    axes:attrs('.wardley-axes line',['x1','y1','x2','y2']),
    stages:attrs('.wardley-stage-label',['x','y']),
    stageBoundaries:attrs('.wardley-stages line',['x1','y1','x2','y2']),
    nodes:attrs('.wardley-node-label',['x','y','font-size']),
    nodeShapes:attrs('.wardley-node circle, circle.wardley-node, circle.wardley-outsource-overlay, circle.wardley-buy-overlay, circle.wardley-build-overlay, circle.wardley-market-overlay, circle.wardley-market-dot, rect.wardley-pipeline-parent, .wardley-node rect',['cx','cy','r','x','y','width','height']),
    links:attrs('.wardley-link',['x1','y1','x2','y2','stroke-dasharray']),
    linkLabels:attrs('.wardley-link-label',['x','y','font-size']),
    trends:attrs('.wardley-trend',['x1','y1','x2','y2']),
    pipelines:attrs('.wardley-pipeline-box',['x','y','width','height']),
    pipelineLinks:attrs('.wardley-pipeline-evolution-link',['x1','y1','x2','y2']),
    annotations:attrs('circle.wardley-annotation, .wardley-annotation circle',['cx','cy','r']),
    annotationNumbers:attrs('.wardley-annotation-number, .wardley-annotation text',['x','y']),
    annotationBoxes:attrs('.wardley-annotations-box rect, rect.wardley-annotations-box',['x','y','width','height']),
    annotationLabels:attrs('.wardley-annotations-box text, text.wardley-annotation-box-label',['x','y']),
    accelerators:attrs('.wardley-accelerators path, .wardley-deaccelerators path, path.wardley-accelerator',[]),
    acceleratorLabels:attrs('.wardley-accelerators text, .wardley-deaccelerators text, text.wardley-accelerator-label',['x','y']),
    strategyOverlays:attrs('.wardley-outsource-overlay, .wardley-buy-overlay, .wardley-build-overlay, .wardley-market-overlay',['cx','cy','r']),
    marketDots:attrs('.wardley-market-dot',['cx','cy','r']),
    inertia:attrs('.wardley-inertia',['x1','y1','x2','y2']),
    notes:attrs('.wardley-notes text, text.wardley-note',['x','y']),
    width:svg.getAttribute('width'),height:svg.getAttribute('height'),maxWidth:svg.style.maxWidth,
  };
};
export const edge=process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe';
if(process.argv[1]&&pathToFileURL(resolve(process.argv[1])).href===import.meta.url){
  const dir=resolve(process.argv[2]??'build/mermaid-cpp/wardley-browser');mkdirSync(dir,{recursive:true});
  const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
  const file=resolve(dir,'probe.html');writeFileSync(file,'<!doctype html><meta charset="utf-8"><style>svg{font-family:"trebuchet ms"}</style><body></body>');
  const browser=await chromium.launch({executablePath:edge,headless:true,args:['--allow-file-access-from-files']});
  try {const page=await browser.newPage();await page.goto(pathToFileURL(file).href);await page.evaluate(async u=>{globalThis.mermaid=(await import(u)).default;},url);console.log('loaded Mermaid');await page.evaluate('globalThis.inspectWardley='+inspectWardley.toString()+'; void 0');console.log('installed inspector');
    const serialized=await page.evaluate(async payload=>{const {cases,invalid}=JSON.parse(payload);const rows=[];for(const theme of ['default','dark']){mermaid.initialize({startOnLoad:false,fontFamily:'Arial',look:'classic',theme});for(const [name,source] of cases){try{const result=await mermaid.render('wardley-'+theme+'-'+name,source);const host=document.createElement('div');host.innerHTML=result.svg;document.body.append(host);rows.push({name:'wardley-'+name+'-'+theme,svg:result.svg,...inspectWardley(host)});host.remove();}catch(e){return 'ERR '+name+' '+String(e);}}for(const source of invalid){if(await mermaid.parse(source,{suppressErrors:true}))return 'INVALID '+source;}}return JSON.stringify(rows);},JSON.stringify({cases,invalid}));if(serialized.startsWith('ERR')||serialized.startsWith('INVALID'))throw new Error(serialized);const rows=JSON.parse(serialized);
    assert.equal(rows.length,cases.length*2);writeFileSync(resolve(dir,'original.json'),JSON.stringify(rows,null,2));for(const row of rows)writeFileSync(resolve(dir,row.name+'.svg'),row.svg);console.log('Captured '+rows.length+' Wardley browser SVGs and verified '+invalid.length+' syntax errors');
  } finally {await browser.close();}
}
