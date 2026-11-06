import assert from 'node:assert/strict';
import {mkdirSync,writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
import {cases} from './er-cases.mjs';

export function inspectEr() {
  const svg = document.querySelector('svg');
  const v = svg.viewBox.baseVal;
  const root = svg.getScreenCTM().inverse();
  const matrix = e => root.multiply(e.getScreenCTM());
  const box = (e,b=e.getBBox()) => {
    const m = matrix(e);
    const ps = [[b.x,b.y],[b.x+b.width,b.y],[b.x,b.y+b.height],[b.x+b.width,b.y+b.height]]
      .map(([x,y]) => new DOMPoint(x,y).matrixTransform(m));
    const x = Math.min(...ps.map(p=>p.x)), y = Math.min(...ps.map(p=>p.y));
    return {x,y,width:Math.max(...ps.map(p=>p.x))-x,height:Math.max(...ps.map(p=>p.y))-y};
  };
  const clientBox = r => {
    const a = new DOMPoint(r.x,r.y).matrixTransform(root);
    const b = new DOMPoint(r.right,r.bottom).matrixTransform(root);
    return {x:a.x,y:a.y,width:b.x-a.x,height:b.y-a.y};
  };
  const paint = e => {
    const s = getComputedStyle(e);
    return {fill:s.fill,stroke:s.stroke,width:s.strokeWidth,dash:s.strokeDasharray,
      opacity:s.opacity,rx:e.tagName==='rect'?s.rx:null,ry:e.tagName==='rect'?s.ry:null};
  };
  const elements = [...svg.querySelectorAll('rect,circle,path')]
    .filter(e=>!e.closest('defs')&&!e.classList.contains('er-label-background')&&
      (e.getBBox().width||e.getBBox().height))
    .map(e=>{
      const m = matrix(e);
      let index = 0;
      const path = e.getAttribute('d')?.replace(/[-+]?(?:\d*\.)?\d+(?:e[-+]?\d+)?/gi,
        n=>String(Number(n)+(index++%2?m.f:m.e)));
      const rough = [...(e.closest('.outer-path,.row-rect-even,.row-rect-odd,.divider')?.classList??[])]
        .find(c=>c==='outer-path'||c==='row-rect-even'||c==='row-rect-odd'||c==='divider')??null;
      return {type:e.tagName,box:box(e),path:path??null,rough,paint:paint(e),
        markers:['marker-start','marker-end'].map(a=>e.getAttribute(a))};
    });
  const labels = [...svg.querySelectorAll('foreignObject,text')].filter(e=>!e.closest('defs')).map(e=>{
    let text,rect,font;
    if(e.tagName==='foreignObject') {
      const span=e.querySelector('span')??e.firstElementChild,fontNode=span.querySelector('p')??span;
      const walker=document.createTreeWalker(span,NodeFilter.SHOW_TEXT),rs=[];
      let n;
      while(n=walker.nextNode()) {
        if(!n.textContent.trim()) continue;
        const range=document.createRange();
        range.selectNodeContents(n);
        rs.push(...range.getClientRects());
      }
      const x=Math.min(...rs.map(r=>r.x)),y=Math.min(...rs.map(r=>r.y));
      rect=clientBox({x,y,right:Math.max(...rs.map(r=>r.right)),bottom:Math.max(...rs.map(r=>r.bottom))});
      text=span.textContent.trim().replace(/\s+/g,' ');
      font=getComputedStyle(fontNode);
    } else {
      const lines=[...e.querySelectorAll('tspan')];
      text=(lines.length?lines.map(l=>l.textContent).join(' '):e.textContent).trim().replace(/\s+/g,' ');
      font=getComputedStyle(e);
      rect=box(e);
    }
    return {text,box:rect,font:{family:font.fontFamily,size:font.fontSize,weight:font.fontWeight,
      color:e.tagName==='foreignObject'?font.color:font.fill}};
  }).filter(l=>l.text);
  const backgrounds = [...svg.querySelectorAll('foreignObject div.labelBkg,foreignObject span.edgeLabel,rect.er-label-background')]
    .map(e=>{
      const s=getComputedStyle(e);
      if(e.tagName==='rect') return {part:'rect',box:box(e),color:s.fill};
      return {part:e.tagName.toLowerCase(),box:clientBox(e.getBoundingClientRect()),color:s.backgroundColor};
    });
  const markers = [...svg.querySelectorAll('defs marker')].map(e=>{
    const fields=(n,keys)=>Object.fromEntries(keys.map(key=>[key,n.getAttribute(key)]));
    return {
      ...fields(e,['id','refX','refY','markerWidth','markerHeight','markerUnits','orient','viewBox']),
      contents:[...e.children].map(n=>({type:n.tagName,paint:paint(n),
        path:n.getAttribute('d'),circle:n.tagName==='circle'?fields(n,['cx','cy','r']):null})),
    };
  });
  const sort=(a,b)=>a.box.y-b.box.y||a.box.x-b.box.x||
    String(a.type??a.text??a.part).localeCompare(String(b.type??b.text??b.part));
  elements.sort(sort);
  labels.sort(sort);
  backgrounds.sort(sort);
  return {view:{x:v.x,y:v.y,width:v.width,height:v.height},elements,labels,backgrounds,markers};
}

if(!process.argv.includes('--library')) {
  const directory=resolve(process.argv[2]??'build/mermaid-cpp/er-browser');
  mkdirSync(directory,{recursive:true});
  const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
  const successes=cases.filter(c=>c[2]!==null);
  const browser=await chromium.launch({
    executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
    headless:true,args:['--allow-file-access-from-files'],
  });
  const rows=[];
  try {
    for(const [name,source] of successes) for(const theme of ['default','dark']) for(const elk of [false,true]) {
      const file=resolve(directory,'probe.html');
      const config={startOnLoad:false,theme,fontFamily:'Arial',themeVariables:{fontFamily:'Arial'},look:'classic'};
      if(!elk) config.layout='dagre';
      writeFileSync(file,`<!doctype html><meta charset="utf-8"><body><script type="module">
        const {default:mermaid}=await import(${JSON.stringify(url)});
        try {
          mermaid.initialize(${JSON.stringify(config)});
          const {svg}=await mermaid.render('mermaid-er',${JSON.stringify(source)});
          document.body.innerHTML=svg;
          const observation=(${inspectEr.toString()})();
          document.body.innerHTML='<pre id="result"></pre>';
          document.querySelector('pre').textContent=JSON.stringify({svg,...observation});
        } catch(e) {
          document.body.innerHTML='<pre id="error"></pre>';
          document.querySelector('pre').textContent=e.stack;
        }
      </script>`);
      const page=await browser.newPage({timezoneId:'UTC'});
      await page.goto(pathToFileURL(file).href);
      await page.waitForSelector('pre#result,pre#error',{timeout:60000});
      assert.equal(await page.locator('#error').count(),0,await page.locator('pre').textContent());
      rows.push({name:name+(elk?'-elk-':'-')+theme,source,theme,layout:elk?'elk':'dagre',
        ...JSON.parse(await page.locator('#result').textContent())});
      await page.close();
    }
  } finally {
    await browser.close();
  }
  assert.equal(rows.length,32);
  writeFileSync(resolve(directory,'original.json'),JSON.stringify(rows,null,2));
  for(const row of rows) writeFileSync(resolve(directory,row.name+'.svg'),row.svg);
  console.log(`Captured ${rows.length} original ER SVGs across explicit Dagre and default ELK.`);
}
