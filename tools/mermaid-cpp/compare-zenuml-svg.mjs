import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
const rows=JSON.parse(readFileSync(process.argv[2],'utf8'));
const native=resolve(process.argv[3]);
function inspect(){
 const svg=document.querySelector('svg'),root=svg.getScreenCTM().inverse(),v=svg.viewBox.baseVal;
 const box=e=>{const b=e.getBBox(),m=root.multiply(e.getScreenCTM()),ps=[[b.x,b.y],[b.x+b.width,b.y],[b.x,b.y+b.height],[b.x+b.width,b.y+b.height]].map(([x,y])=>new DOMPoint(x,y).matrixTransform(m));return [Math.min(...ps.map(p=>p.x)),Math.min(...ps.map(p=>p.y)),Math.max(...ps.map(p=>p.x))-Math.min(...ps.map(p=>p.x)),Math.max(...ps.map(p=>p.y))-Math.min(...ps.map(p=>p.y))];};
 const paints=[...svg.querySelectorAll('rect,path,circle,ellipse,line,polyline,polygon')].filter(e=>!e.closest('defs')).map(e=>{const s=getComputedStyle(e);return {type:e.tagName,box:box(e),paint:[s.fill,s.stroke,s.strokeWidth,s.strokeDasharray,s.strokeDashoffset,s.opacity,s.fillOpacity],path:e.getAttribute('d')??'',radius:Number(e.getAttribute('rx')??0)};});
 const labels=[...svg.querySelectorAll('text')].filter(e=>!e.closest('defs')).map(e=>{const s=getComputedStyle(e);return{text:e.textContent.trim(),box:box(e),font:[s.fontFamily,s.fontSize,s.fontWeight,s.fill],opacity:s.opacity};});
 paints.sort((a,b)=>a.type.localeCompare(b.type)||a.box[1]-b.box[1]||a.box[0]-b.box[0]);labels.sort((a,b)=>a.text.localeCompare(b.text)||a.box[1]-b.box[1]||a.box[0]-b.box[0]);return{view:[v.x,v.y,v.width,v.height],paints,labels};
}
let maximumDifference=0;
function compare(a,e,path,tolerance=0){if(typeof e==='number'){const d=Math.abs(a-e);assert(Number.isFinite(a)&&d<=tolerance,`${path}: ${a} != ${e}`);maximumDifference=Math.max(maximumDifference,d);}else if(Array.isArray(e)){assert.equal(a.length,e.length,path);e.forEach((v,i)=>compare(a[i],v,path+'.'+i,tolerance));}else if(e&&typeof e==='object'){for(const[k,v]of Object.entries(e))compare(a[k],v,path+'.'+k,k==='box'||k==='view'?1.5:0);}else assert.equal(a,e,path);}
const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true});
try{const page=await browser.newPage();for(const row of rows){await page.setContent(row.svg);const expected=await page.evaluate(inspect);await page.setContent(readFileSync(resolve(native,'zenuml-'+row.name+'.svg'),'utf8'));const actual=await page.evaluate(inspect);compare(actual,expected,row.name);}}finally{await browser.close();}
writeFileSync(resolve(dirname(process.argv[2]),'comparison.json'),JSON.stringify({checked:rows.length,measuredGeometryTolerance:1.5,maximumDifference},null,2));console.log(`Compared ${rows.length} ZenUML original/native SVGs: exact paint/text/counts and measured geometry.`);
