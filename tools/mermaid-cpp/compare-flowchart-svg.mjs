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
   const nodes=[...svg.querySelectorAll('g.node')].map(n=>{
    const shape=n.querySelector('.label-container'),b=shape.getBBox(),text=n.querySelector('text'),t=text.getBBox();
    return {transform:n.getAttribute('transform'),width:b.width,height:b.height,label:text.textContent,
     fill:getComputedStyle(shape).fill,stroke:getComputedStyle(shape).stroke,
     contained:t.x>=b.x-1&&t.y>=b.y-1&&t.x+t.width<=b.x+b.width+1&&t.y+t.height<=b.y+b.height+1};
   });
   const edges=[...svg.querySelectorAll('.flowchart-link')].map(p=>{
    const length=p.getTotalLength();return {length,stroke:getComputedStyle(p).stroke,points:Array.from({length:7},(_,i)=>{const a=p.getPointAtLength(length*i/6);return {x:a.x,y:a.y};})};
   });
   return {offset:outer.getAttribute('transform'),nodes,edges};
  });
  const offset=translate(actual.offset);assert.equal(actual.nodes.length,row.nodes.length,row.name);
  for(let i=0;i<actual.nodes.length;++i) {
   const a=actual.nodes[i],e=row.nodes[i],p=translate(a.transform),q=translate(e.transform);
   assert.equal(a.label,e.label,row.name);assert(a.contained,`${row.name}: clipped label ${a.label}`);
   for(const key of ['width','height']) near(a[key],e[key],`${row.name} ${i} ${key}`);
   for(let axis=0;axis<2;++axis) near(p[axis]+offset[axis],q[axis],`${row.name} ${i} axis ${axis}`);
   assert.equal(a.fill,e.fill,`${row.name} fill`);assert.equal(a.stroke,e.stroke,`${row.name} stroke`);
  }
  assert.equal(actual.edges.length,row.edges.length,row.name);
  for(let i=0;i<actual.edges.length;++i) {
   const a=actual.edges[i],e=row.edges[i];near(a.length,e.length,`${row.name} edge ${i} length`,3);
   assert.equal(a.stroke,e.stroke,`${row.name} edge paint`);
   for(let j=0;j<a.points.length;++j) {near(a.points[j].x+offset[0],e.points[j].x,`${row.name} edge ${i} x`,3);near(a.points[j].y+offset[1],e.points[j].y,`${row.name} edge ${i} y`,3);}
  }
  report.push({name:row.name,nodes:actual.nodes.length,edges:actual.edges.length});
 }
 const selected=rows.filter(x=>['decision-default','horizontal-dark','unicode-default'].includes(x.name));
 await page.setContent('<body style="margin:20px;background:#f1f3f5;font:16px Arial"><div style="display:flex;gap:24px;align-items:start">'+selected.map(x=>
  `<div style="background:${x.theme==='dark'?'#202124':'white'};padding:20px;border-radius:12px"><h3 style="color:${x.theme==='dark'?'white':'#222'}">${x.name}</h3>`+readFileSync(resolve(output,'flowchart-'+x.name+'.svg'),'utf8')+'</div>').join('')+'</div></body>');
 await page.screenshot({path:resolve(dirname(reference),'native-preview.png'),fullPage:true});
} finally {await browser.close();}
writeFileSync(resolve(dirname(reference),'comparison.json'),JSON.stringify(report,null,2));
console.log(`Compared ${report.length} measured native Flowchart SVGs: node boxes, coordinates, text, paints and sampled edge geometry.`);
