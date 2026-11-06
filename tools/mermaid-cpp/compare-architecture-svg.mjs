import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
const originals=JSON.parse(readFileSync(process.argv[2]));
const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true});
let maximumDifference=0;
function compare(a,e,path,tolerance=0){if(typeof e==='number'){const delta=Math.abs(a-e);assert(Number.isFinite(a)&&delta<=tolerance,`${path}: ${a} != ${e}`);maximumDifference=Math.max(maximumDifference,delta);}else if(Array.isArray(e)){assert.equal(a.length,e.length,path);e.forEach((v,i)=>compare(a[i],v,path+'.'+i,tolerance));}else if(e&&typeof e==='object')for(const [key,value]of Object.entries(e))compare(a[key],value,path+'.'+key,tolerance);else assert.equal(a,e,path);}
function geometry(a,e,path){const re=/[-+]?(?:\d*\.)?\d+(?:e[-+]?\d+)?/gi;compare(a.replace(re,'#').replace(/[\s,]/g,''),e.replace(re,'#').replace(/[\s,]/g,''),path+'.commands');compare([...a.matchAll(re)].map(m=>Number(m[0])),[...e.matchAll(re)].map(m=>Number(m[0])),path+'.numbers',1.5);}
try{const page=await browser.newPage();for(const original of originals){
 await page.setContent(readFileSync(resolve(process.argv[3],'architecture-'+original.name+'.svg'),'utf8'));
 const actual=await page.evaluate(()=>{
  const root=document.querySelector('svg'),v=root.viewBox.baseVal;
  const box=e=>{const b=e.getBBox();return {x:b.x,y:b.y,width:b.width,height:b.height};};
  const position=e=>{const m=e.transform.baseVal.consolidate()?.matrix;return m?{x:m.e,y:m.f}:{x:0,y:0};};
  const globalBox=e=>{const b=e.getBBox(),m=root.getCTM().inverse().multiply(e.getCTM()),p=new DOMPoint(b.x,b.y).matrixTransform(m);return {x:p.x,y:p.y,width:b.width,height:b.height};};
  return {view:{x:v.x,y:v.y,width:v.width,height:v.height},services:[...root.querySelectorAll('.architecture-service,.architecture-junction')].map(n=>({position:position(n),box:box(n)})),groups:[...root.querySelectorAll('.architecture-groups>rect')].map(n=>({box:box(n),stroke:getComputedStyle(n).stroke,width:parseFloat(getComputedStyle(n).strokeWidth)})),edges:[...root.querySelectorAll('path.edge')].map(n=>({path:n.getAttribute('d'),stroke:getComputedStyle(n).stroke,width:parseFloat(getComputedStyle(n).strokeWidth)})),arrows:[...root.querySelectorAll('polygon.arrow')].map(n=>({points:n.getAttribute('points'),transform:n.getAttribute('transform'),fill:getComputedStyle(n).fill})),labels:[...root.querySelectorAll('text.architecture-label')].map(n=>({text:n.textContent,global:globalBox(n),font:getComputedStyle(n).fontFamily,size:parseFloat(getComputedStyle(n).fontSize),fill:getComputedStyle(n).fill})),iconTexts:[...root.querySelectorAll('text.architecture-icon-text')].map(n=>({text:n.textContent,fill:getComputedStyle(n).fill,global:globalBox(n)}))};
 });
 const name=original.name;compare(actual.view,original.view,name+'.view',1.5);
 compare(actual.services,original.services.map(({position,box})=>({position,box})),name+'.services',1.5);
 assert.equal(actual.groups.length,original.groups.length,name+'.groups');for(let i=0;i<original.groups.length;++i){const a=actual.groups[i],e=original.groups[i];compare(a.box,e.box,name+'.group'+i,1.5);compare(a.stroke,e.stroke,name+'.groupstroke');compare(a.width,e.width,name+'.groupwidth');}
 for(const key of ['edges','arrows','labels'])assert.equal(actual[key].length,original[key].length,name+'.'+key);
 for(let i=0;i<original.edges.length;++i){const a=actual.edges[i],e=original.edges[i];geometry(a.path,e.path,name+'.edge'+i);compare(a.stroke,e.stroke,name+'.edgepaint');compare(a.width,e.width,name+'.edgewidth');}
 for(let i=0;i<original.arrows.length;++i){const a=actual.arrows[i],e=original.arrows[i];geometry(a.points,e.points,name+'.arrow'+i);geometry(a.transform,e.transform,name+'.arrowtransform');compare(a.fill,e.fill,name+'.arrowfill');}
 for(let i=0;i<original.labels.length;++i){const a=actual.labels[i],e=original.labels[i];for(const key of ['text','font','size','fill'])compare(a[key],e[key],name+'.label'+i+'.'+key);compare(a.global,e.global,name+'.label'+i+'.box',1.5);}
 assert.equal(actual.iconTexts.length,original.iconTexts.length,name+'.iconTexts');for(let i=0;i<original.iconTexts.length;++i){const a=actual.iconTexts[i],e=original.iconTexts[i];compare(a.text,e.text,name+'.iconText');compare(a.fill,e.fill,name+'.iconTextPaint');compare(a.global,e.global,name+'.iconTextBox',1.5);}
}}finally{await browser.close();}
writeFileSync(resolve(dirname(process.argv[2]),'comparison.json'),JSON.stringify({checked:originals.length,measuredGeometryTolerance:1.5,maximumDifference},null,2));
console.log(`Compared ${originals.length} native Architecture SVGs with original layout, icons, groups, edges, arrows and labels.`);
