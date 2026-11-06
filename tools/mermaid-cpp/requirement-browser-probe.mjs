import assert from 'node:assert/strict';
import { mkdirSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { cases, richCases } from './requirement-cases.mjs';

// Observe rendered nodes, routes, labels and the final viewport in SVG space.
// Geometry is transformed out of nested SVG groups before it is recorded.
export function inspectRequirement() {
  const svg = document.querySelector('svg');
  const view = svg.viewBox.baseVal;
  const inverse = svg.getScreenCTM().inverse();
  const matrix = e => inverse.multiply(e.getScreenCTM());
  const box = (e, b = e.getBBox()) => {
    const m = matrix(e);
    const points = [[b.x,b.y],[b.x+b.width,b.y],[b.x,b.y+b.height],[b.x+b.width,b.y+b.height]]
      .map(([x,y]) => ({x:m.a*x+m.c*y+m.e,y:m.b*x+m.d*y+m.f}));
    return {x:Math.min(...points.map(p=>p.x)),y:Math.min(...points.map(p=>p.y)),
      width:Math.max(...points.map(p=>p.x))-Math.min(...points.map(p=>p.x)),
      height:Math.max(...points.map(p=>p.y))-Math.min(...points.map(p=>p.y))};
  };
  const nodes=[...svg.querySelectorAll('g.nodes > g.node')].map(e=>{
    const shape=e.querySelector('.outer-path');
    const fill=getComputedStyle(shape.firstElementChild??shape).fill;
    const outline=getComputedStyle(shape.lastElementChild??shape);
    const divider=e.querySelector('.divider');
    return {classes:e.getAttribute('class')??'',box:box(e),fill,stroke:outline.stroke,
      strokeWidth:outline.strokeWidth,divider:divider?box(divider):null};
  });
  const edges=[...svg.querySelectorAll('g.edgePaths > path')].map(e=>({classes:e.getAttribute('class')??'',
    box:box(e),path:e.getAttribute('d'),startMarker:e.hasAttribute('marker-start'),endMarker:e.hasAttribute('marker-end')}));
  const labels = [...svg.querySelectorAll('text,foreignObject')].filter(e=>!e.closest('defs')).map(e=>{
    let text, rect, font;
    const runs=[];
    const inspectRuns=root=>{
      const walker=document.createTreeWalker(root,NodeFilter.SHOW_TEXT);
      let node;
      while(node=walker.nextNode()) {
        const {fontWeight:weight,fontStyle:style}=getComputedStyle(node.parentElement);
        const last=runs.at(-1);
        if(last?.weight===weight&&last?.style===style)last.text+=node.textContent;
        else runs.push({text:node.textContent,weight,style});
      }
    };
    if(e.tagName==='foreignObject'){
      const span=e.querySelector('span')??e.firstElementChild;
      inspectRuns(span);
      const walker=document.createTreeWalker(span,NodeFilter.SHOW_TEXT),rects=[];let n;
      while(n=walker.nextNode()){if(!n.textContent.trim())continue;const range=document.createRange();range.selectNodeContents(n);rects.push(...range.getClientRects());}
      if(!rects.length)return null;
      const x=Math.min(...rects.map(r=>r.x)),y=Math.min(...rects.map(r=>r.y)),right=Math.max(...rects.map(r=>r.right)),bottom=Math.max(...rects.map(r=>r.bottom));
      const a=new DOMPoint(x,y).matrixTransform(inverse),b=new DOMPoint(right,bottom).matrixTransform(inverse);
      rect={x:a.x,y:a.y,width:b.x-a.x,height:b.y-a.y};text=span.textContent.trim().replace(/\s+/g,' ');font=getComputedStyle(span);
    } else {
      inspectRuns(e);
      text=e.textContent.trim().replace(/\s+/g,' ');rect=box(e);font=getComputedStyle(e);
    }
    return text?{text,box:rect,font:{family:font.fontFamily,size:font.fontSize,weight:font.fontWeight,
      color:e.tagName==='foreignObject'?font.color:font.fill},
      runs:runs.map(run=>({...run,text:run.text.trim().replace(/\s+/g,' ')})).filter(run=>run.text)}:null;
  }).filter(Boolean);
  const sort=(a,b)=>a.box.y-b.box.y||a.box.x-b.box.x;
  nodes.sort(sort);edges.sort(sort);labels.sort(sort);
  return {view:{x:view.x,y:view.y,width:view.width,height:view.height},nodes,edges,labels};
}

if(!process.argv.includes('--library')) {
  const directory=resolve(process.argv[2]??'build/mermaid-cpp/requirement-browser');
  mkdirSync(directory,{recursive:true});
  const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
  const successes=[...cases.filter(c=>c[2]===null),...richCases,[
    'svg-labels-title',
    '---\ntitle: Main\nconfig:\n  htmlLabels: false\n---\nrequirementDiagram\nrequirement R {\nverifyMethod: test\n}\nelement E {\ntype: module\n}\nR - verifies -> E\n',
  ]];
  const layouts=['dagre','elk'];
  const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--allow-file-access-from-files']});
  const rows=[];
  try {
    for(const layout of layouts) for(const [name,source] of successes) for(const theme of ['default','dark']) {
      const config={startOnLoad:false,theme,fontFamily:'Arial',themeVariables:{fontFamily:'Arial'},look:'classic'};
      if(layout==='dagre')config.layout=layout;
      const file=resolve(directory,'probe.html');
      writeFileSync(file,`<!doctype html><meta charset="utf-8"><body><script type="module">const {default:mermaid}=await import(${JSON.stringify(url)});try{mermaid.initialize(${JSON.stringify(config)});const {svg}=await mermaid.render('mermaid-requirement',${JSON.stringify(source)});document.body.innerHTML=svg;const observation=(${inspectRequirement.toString()})();document.body.innerHTML='<pre id="result"></pre>';document.querySelector('pre').textContent=JSON.stringify({svg,...observation});}catch(e){document.body.innerHTML='<pre id="error"></pre>';document.querySelector('pre').textContent=e.stack;}</script>`);
      const page=await browser.newPage({timezoneId:'UTC'});
      await page.goto(pathToFileURL(file).href);
      await page.waitForSelector('pre#result,pre#error',{timeout:60000});
      assert.equal(await page.locator('#error').count(),0,await page.locator('pre').textContent());
      const suffix=layout==='elk'?`-elk-${theme}`:`-${theme}`;
      const row={name:name+suffix,source,theme,layout,...JSON.parse(await page.locator('#result').textContent())};
      rows.push(row);writeFileSync(resolve(directory,`requirement-${row.name}.svg`),row.svg);await page.close();
    }
  } finally { await browser.close(); }
  assert.equal(rows.length,successes.length*2*layouts.length);
  writeFileSync(resolve(directory,'original.json'),JSON.stringify(rows,null,2));
  console.log(`Captured ${rows.length} Requirement SVG baselines for classic Dagre and default ELK in both themes.`);
}
