import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';

const oraclePath=resolve(process.argv[2]),nativeDirectory=resolve(process.argv[3]);
const originals=JSON.parse(readFileSync(oraclePath,'utf8'));const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true});
const tolerance=1.5;let maximumDifference=0,checked=0;
function inspect(svg){
 const translated=(el)=>{let x=0,y=0;for(let n=el;n&&n!==svg;n=n.parentElement){const match=(n.getAttribute?.('transform')??'').match(/translate\(\s*(-?[\d.eE+-]+)(?:[ ,]+(-?[\d.eE+-]+))?\s*\)/);if(match){x+=Number(match[1]);y+=Number(match[2]??0);}}return{x,y};};
 const box=(el)=>{const b=el.getBBox(),t=translated(el);return{x:b.x+t.x,y:b.y+t.y,width:b.width,height:b.height};};
 const view=svg.viewBox.baseVal;
 const clusters=[...svg.querySelectorAll('g.cluster')].map(el=>({label:(el.querySelector('.cluster-label')?.textContent??'').trim().replace(/\s+/g,' '),box:box(el)}));
 const nodes=[...svg.querySelectorAll('g.node')].map(el=>{const shape=el.querySelector('.label-container')??el.querySelector('rect,polygon,path,circle,ellipse'),path=shape?.tagName.toLowerCase()==='path'?shape:shape?.querySelector('path'),length=path?.getTotalLength()??0,label=el.querySelector('foreignObject')??el.querySelector('.nodeLabel'),lb=label?box(label):null;return{label:(el.querySelector('.nodeLabel')?.textContent??el.querySelector('.label')?.textContent??'').trim().replace(/\s+/g,' '),box:shape?box(shape):null,shapeSamples:path?[0,.1,.2,.3,.4,.5,.6,.7,.8,.9,1].map(f=>{const p=path.getPointAtLength(length*f),t=translated(path);return{x:p.x+t.x,y:p.y+t.y};}):[],labelCenter:lb?{x:lb.x+lb.width/2,y:lb.y+lb.height/2}:null};});
 const edges=[...svg.querySelectorAll('path[data-edge="true"],path.flowchart-link')].map(el=>{let points=[];const encoded=el.getAttribute('data-points');if(encoded)points=JSON.parse(atob(encoded));else{const d=el.getAttribute('d')??'';const first=d.match(/M\s*(-?[\d.eE+.-]+)[, ]+(-?[\d.eE+.-]+)/),nums=[...d.matchAll(/-?(?:\d+\.?\d*|\.\d+)(?:e[-+]?\d+)?/gi)].map(m=>Number(m[0]));if(first&&nums.length>=4){const t=translated(el);points=[{x:Number(first[1])+t.x,y:Number(first[2])+t.y},{x:nums.at(-2)+t.x,y:nums.at(-1)+t.y}];}}const length=el.getTotalLength(),t=translated(el);return{id:el.getAttribute('data-id')??el.id??'',points,samples:[0,.25,.5,.75,1].map(f=>{const p=el.getPointAtLength(length*f);return{x:p.x+t.x,y:p.y+t.y};}),marker:el.hasAttribute('marker-end')};});
 const labels=[...svg.querySelectorAll('g.edgeLabel')].map(group=>{const el=group.querySelector('foreignObject,rect,text')??group;return{text:(group.textContent??'').trim().replace(/\s+/g,' '),box:box(el)};});
 return{view:{x:view.x,y:view.y,width:view.width,height:view.height},clusters,nodes,edges,labels};
}
function near(a,b,path){const d=Math.abs(a-b);assert(Number.isFinite(a)&&d<=tolerance,`${path}: ${a} != ${b} (tol ${tolerance})`);maximumDifference=Math.max(maximumDifference,d);}
function compare(actual,expected,path){for(const key of ['x','y','width','height'])near(actual[key],expected[key],`${path}.${key}`);}
function comparePoint(actual,expected,path){for(const key of ['x','y'])near(actual[key],expected[key],`${path}.${key}`);}
try{
 const page=await browser.newPage();
 for(const original of originals){const path=resolve(nativeDirectory,`agentflow-${original.name}-${original.theme}.svg`);await page.setContent(readFileSync(path,'utf8'));const actual=await page.locator('svg').first().evaluate(inspect);const prefix=`${original.name}/${original.theme}`;
  assert.equal(actual.clusters.length,original.clusters.length,`${prefix}.clusters.length`);for(let i=0;i<actual.clusters.length;i++){assert.equal(actual.clusters[i].label,original.clusters[i].label,`${prefix}.clusters[${i}].label`);compare(actual.clusters[i].box,original.clusters[i].box,`${prefix}.clusters[${i}].box`);}
  assert.equal(actual.nodes.length,original.nodes.length,`${prefix}.nodes.length`);for(let i=0;i<actual.nodes.length;i++){assert.equal(actual.nodes[i].label,original.nodes[i].label,`${prefix}.nodes[${i}].label`);assert(actual.nodes[i].box,`${prefix}.nodes[${i}] has no shape geometry`);compare(actual.nodes[i].box,original.nodes[i].box,`${prefix}.nodes[${i}].box`);if(actual.nodes[i].labelCenter&&original.nodes[i].labelCenter)comparePoint(actual.nodes[i].labelCenter,original.nodes[i].labelCenter,`${prefix}.nodes[${i}].labelCenter`);if(actual.nodes[i].label==='Style guide'){for(let j=0;j<actual.nodes[i].shapeSamples.length;j++)comparePoint(actual.nodes[i].shapeSamples[j],original.nodes[i].shapeSamples[j],`${prefix}.nodes[${i}].shapeSample[${j}]`);}}
  for(const key of ['x','y','width','height'])near(actual.view[key],original.view[key],`${prefix}.view.${key}`);
  assert.equal(actual.edges.length,original.edges.length,`${prefix}.edges.length`);for(let i=0;i<actual.edges.length;i++){assert.equal(actual.edges[i].points.length>=2,true,`${prefix}.edges[${i}] has no routed endpoints`);const e=original.edges[i],a=actual.edges[i];assert.equal(a.marker,e.marker,`${prefix}.edges[${i}].marker`);assert.equal(a.samples.length,e.samples.length);for(let j=0;j<a.samples.length;j++)comparePoint(a.samples[j],e.samples[j],`${prefix}.edges[${i}].sample[${j}]`);}
  assert.equal(actual.labels.length,original.labels.length,`${prefix}.labels.length`);for(let i=0;i<actual.labels.length;i++){assert.equal(actual.labels[i].text,original.labels[i].text,`${prefix}.labels[${i}].text`);compare(actual.labels[i].box,original.labels[i].box,`${prefix}.labels[${i}].box`);}
  checked++;
 }
 await page.close();
}finally{await browser.close();}
writeFileSync(resolve(dirname(oraclePath),'agentflow-comparison.json'),JSON.stringify({checked,geometryTolerance:tolerance,maximumDifference},null,2));console.log(`Compared ${checked} Agentflow default/dark SVGs at ${tolerance} px tolerance (max difference ${maximumDifference.toFixed(4)} px).`);
