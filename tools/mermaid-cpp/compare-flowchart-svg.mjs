// Compare real browser SVGs against real DirectWrite/native output on one host.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
const reference=resolve(process.argv[2]);const output=resolve(process.argv[3]);
const rows=JSON.parse(readFileSync(reference));
const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true});
const near=(a,b,label,tolerance=1.5)=>assert(Number.isFinite(a)&&Number.isFinite(b)&&Math.abs(a-b)<=tolerance,`${label}: ${a} != ${b}`);
const translate=s=>s.slice(s.indexOf('(')+1,s.indexOf(')')).trim().split(/[ ,]+/).map(Number);
const report=[];
try {
 const page=await browser.newPage({viewport:{width:1600,height:1200}});
 for(const row of rows) {
  const svg=readFileSync(resolve(output,'flowchart-'+row.name+'.svg'),'utf8');
  await page.setContent(svg);
  const actual=await page.evaluate(()=>{
   const svg=document.querySelector('svg'),outer=svg.querySelector('g[font-family]');
   const position=(element,x,y)=>{
    const p=new DOMPoint(x,y).matrixTransform(svg.getScreenCTM().inverse().multiply(element.getScreenCTM()));
    return {x:p.x,y:p.y};
   };
   const clusters=[...svg.querySelectorAll('g.cluster')].map(n=>{
    const shape=n.querySelector('rect'),b=shape.getBBox(),label=n.querySelector('text'),lb=label.getBBox();
    return {id:n.dataset.node,x:b.x+b.width/2,y:b.y+b.height/2,width:b.width,height:b.height,label:label.textContent,
     title:{x:lb.x+lb.width/2,y:lb.y+lb.height/2},fill:getComputedStyle(shape).fill,stroke:getComputedStyle(shape).stroke};
   });
   const nodes=[...svg.querySelectorAll('g.node')].map(n=>{
    const shape=n.querySelector('.label-container'),b=shape.getBBox(),text=n.querySelector('text'),t=text.getBBox();
    const local=n.getScreenCTM().inverse().multiply(shape.getScreenCTM());
    const corner=new DOMPoint(b.x,b.y).matrixTransform(local);
    const paths=shape.matches('path,polygon,circle')?[shape]:[...shape.querySelectorAll('path,circle')];
    const outline=paths.flatMap(p=>Array.from({length:201},(_,i)=>{
     const a=p.getPointAtLength(p.getTotalLength()*i/200);return position(p,a.x,a.y);
    }));
    return {transform:n.getAttribute('transform'),width:b.width,height:b.height,label:text.textContent,
     labelCenter:position(text,t.x+t.width/2,t.y+t.height/2),lines:[...text.querySelectorAll('tspan')].map(n=>n.textContent.trimEnd()),outline,
     fill:getComputedStyle(shape).fill,stroke:getComputedStyle(shape).stroke,
     contained:t.x>=corner.x-1&&t.y>=corner.y-1&&t.x+t.width<=corner.x+b.width+1&&t.y+t.height<=corner.y+b.height+1};
   });
   const edges=[...svg.querySelectorAll('.flowchart-link')].map(p=>{
    const length=p.getTotalLength();return {id:p.dataset.edge,length,stroke:getComputedStyle(p).stroke,dash:getComputedStyle(p).strokeDasharray,thickness:getComputedStyle(p).strokeWidth,points:Array.from({length:7},(_,i)=>{const a=p.getPointAtLength(length*i/6);return {x:a.x,y:a.y};})};
   });
   const labels=[...svg.querySelectorAll('g.edgeLabel')].map(n=>{
    const b=n.querySelector('rect').getBBox();return {text:n.textContent,x:b.x+b.width/2,y:b.y+b.height/2,width:b.width,height:b.height};
   });
   const bounds=outer.getBBox(),view=svg.viewBox.baseVal;
   return {offset:outer.getAttribute('transform'),nodes,edges,labels,clusters,
    bounds:{x:bounds.x,y:bounds.y,width:bounds.width,height:bounds.height},
    view:{x:view.x,y:view.y,width:view.width,height:view.height}};
  });
  const offset=translate(actual.offset);assert.equal(actual.nodes.length,row.nodes.length,row.name);
  const b=actual.bounds,v=actual.view;
  assert(b.x+offset[0]>=v.x-1 && b.y+offset[1]>=v.y-1 &&
    b.x+b.width+offset[0]<=v.x+v.width+1 && b.y+b.height+offset[1]<=v.y+v.height+1,`${row.name}: clipped SVG content`);
  for(let i=0;i<actual.nodes.length;++i) {
   const a=actual.nodes[i],e=row.nodes.find(n=>n.label===a.label),p=translate(a.transform);
   assert(e,`${row.name}: missing node ${a.label}`);const q=[e.x,e.y];
   assert.equal(a.label,e.label,row.name);assert(a.contained,`${row.name}: clipped label ${a.label}`);
   if(row.name.startsWith('labels-')) {
    assert.deepEqual(a.lines,e.lines,`${row.name}: displayed lines`);
    near(a.labelCenter.x,e.labelCenter.x,`${row.name} label center x`,2);
    near(a.labelCenter.y,e.labelCenter.y,`${row.name} label center y`,2);
   }
   for(const key of ['width','height']) near(a[key],e[key],`${row.name} ${i} ${key}`);
   for(let axis=0;axis<2;++axis) near(p[axis]+offset[axis],q[axis],`${row.name} ${i} axis ${axis}`);
   assert.equal(a.fill,e.fill,`${row.name} fill`);assert.equal(a.stroke,e.stroke,`${row.name} stroke`);
   if(e.outline.length) {
    near(a.labelCenter.x,e.labelCenter.x,`${row.name} label center x`,2);
    near(a.labelCenter.y,e.labelCenter.y,`${row.name} label center y`,2);
    assert(a.outline.length,`${row.name}: missing shape outline`);
    for(const [from,to] of [[a.outline,e.outline],[e.outline,a.outline]]) for(const p of from) {
     const distance=Math.min(...to.map(q=>Math.hypot(p.x-q.x,p.y-q.y)));
     near(distance,0,`${row.name} ${a.label} outline`,2);
    }
   }
  }
  assert.equal(actual.edges.length,row.edges.length,row.name);
  assert.equal(actual.labels.length,row.labels.length,row.name+' labels');
  // Label DOM insertion order predates self-loop merging; match by text.
  const expectedLabels=[...row.labels];
  for(const a of actual.labels) {
   const index=expectedLabels.findIndex(e=>e.text===a.text);assert(index>=0,`${row.name}: missing label ${a.text}`);
   const e=expectedLabels.splice(index,1)[0],p=[e.x,e.y];
   near(a.x+offset[0],p[0],`${row.name} label x`);near(a.y+offset[1],p[1],`${row.name} label y`);
   near(a.width,e.width,`${row.name} label width`);near(a.height,e.height,`${row.name} label height`);
  }
  for(let i=0;i<actual.edges.length;++i) {
   const a=actual.edges[i],e=row.edges.find(n=>n.id.endsWith("-"+a.id));
   assert(e,`${row.name}: missing edge ${a.id}`);near(a.length,e.length,`${row.name} edge ${i} length`,3);
   assert.equal(a.stroke,e.stroke,`${row.name} edge paint`);
   assert.equal(a.dash,e.dash,`${row.name} edge dash`);assert.equal(a.thickness,e.thickness,`${row.name} edge thickness`);
   for(let j=0;j<a.points.length;++j) {near(a.points[j].x+offset[0],e.points[j].x,`${row.name} edge ${i} x`,3);near(a.points[j].y+offset[1],e.points[j].y,`${row.name} edge ${i} y`,3);}
  }
  assert.equal(actual.clusters.length,row.clusters.length,`${row.name}: groups`);
  for(const a of actual.clusters) {
   const e=row.clusters.find(n=>n.id.endsWith('-'+a.id));assert(e,`${row.name}: missing group ${a.id}`);
   assert.equal(a.label,e.label,row.name+' group title');
   near(a.x+offset[0],e.x,row.name+' group x');near(a.y+offset[1],e.y,row.name+' group y');
   near(a.width,e.width,row.name+' group width');near(a.height,e.height,row.name+' group height');
   near(a.title.x+offset[0],e.title.x,row.name+' title x');near(a.title.y+offset[1],e.title.y,row.name+' title y',2);
   assert.equal(a.fill,e.fill,row.name+' group fill');assert.equal(a.stroke,e.stroke,row.name+' group stroke');
  }
  report.push({name:row.name,nodes:actual.nodes.length,edges:actual.edges.length,groups:actual.clusters.length});
 }
 const selected=rows.filter(x=>['labels-entities-default','labels-lines-dark','labels-wrap-default','labels-wrap-group-default'].includes(x.name));
 await page.setContent('<body style="margin:20px;background:#f1f3f5;font:16px Arial"><div style="display:flex;gap:24px;align-items:start">'+selected.map(x=>
  `<div style="background:${x.theme==='dark'?'#202124':'white'};padding:20px;border-radius:12px"><h3 style="color:${x.theme==='dark'?'white':'#222'}">${x.name}</h3>`+readFileSync(resolve(output,'flowchart-'+x.name+'.svg'),'utf8')+'</div>').join('')+'</div></body>');
 await page.screenshot({path:resolve(dirname(reference),'native-preview.png'),fullPage:true});
} finally {await browser.close();}
writeFileSync(resolve(dirname(reference),'comparison.json'),JSON.stringify(report,null,2));
console.log(`Compared ${report.length} measured native Flowchart SVGs: node boxes, coordinates, text, paints and sampled edge geometry.`);
