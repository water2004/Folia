import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
process.argv.push('--library');
const {inspectClass}=await import('./class-browser-probe.mjs');
const originals=JSON.parse(readFileSync(process.argv[2]));
const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true});
const near=(a,b,tolerance,path)=>assert(Number.isFinite(a)&&Number.isFinite(b)&&Math.abs(a-b)<=tolerance,`${path}: ${a} != ${b}`);
let checked=0;
try {
 const page=await browser.newPage();
 for(const original of originals) {
  await page.setContent(readFileSync(resolve(process.argv[3],'class-'+original.name+'.svg'),'utf8'));
  const actual=await page.evaluate(inspectClass);const name=original.name;
  for(const key of ['nodes','edges','labels'])assert.equal(actual[key].length,original[key].length,name+'.'+key);
  for(const key of ['x','y','width','height'])near(actual.view[key],original.view[key],0.2,name+'.view.'+key);
  for(let i=0;i<original.nodes.length;i++) {
   const a=actual.nodes[i],e=original.nodes[i];assert.equal(a.id,e.id,name+'.id');
   for(const key of ['x','y','width','height'])near(a[key],e[key],0.2,name+'.'+a.id+'.'+key);
   for(const key of ['fill','stroke','strokeWidth'])assert.equal(a[key],e[key],name+'.'+a.id+'.'+key);
   assert.equal(a.dividers.length,e.dividers.length,name+'.dividers');
   for(let j=0;j<e.dividers.length;j++)for(const key of ['x','y','width','height'])near(a.dividers[j][key],e.dividers[j][key],0.2,name+'.divider.'+key);
   for(let j=0;j<e.groups.length;j++) {
    const ag=a.groups[j],eg=e.groups[j];assert.equal(ag.kind,eg.kind);assert.equal(ag.labels.length,eg.labels.length);
    for(let k=0;k<eg.labels.length;k++) {
     const at=ag.labels[k],et=eg.labels[k];
     for(const key of ['text','color','size','weight','italic','decoration'])assert.equal(at[key],et[key],name+'.text.'+key);
     near(at.x,et.x,0.25,name+'.text.x');near(at.width,et.width,0.25,name+'.text.width');
     near(at.y+at.height/2,et.y+et.height/2,2.5,name+'.text.centerY');
    }
   }
  }
  for(let i=0;i<original.edges.length;i++) {
   const a=actual.edges[i],e=original.edges[i];
   for(const key of ['fill','stroke','strokeWidth','dash','start','end'])assert.deepEqual(a[key],e[key],name+'.edge.'+key);
   for(let j=0;j<e.points.length;j++)for(let d=0;d<2;d++)near(a.points[j][d],e.points[j][d],0.2,name+'.edge.point');
  }
  for(let i=0;i<original.labels.length;i++) {
   const a=actual.labels[i],e=original.labels[i];assert.equal(a.text,e.text,name+'.edgeLabel');
   near(a.x,e.x,0.25,name+'.edgeLabel.x');near(a.width,e.width,0.25,name+'.edgeLabel.width');
   near(a.y+a.height/2,e.y+e.height/2,2.5,name+'.edgeLabel.centerY');
  }
  checked++;
 }
} finally {await browser.close();}
writeFileSync(resolve(dirname(process.argv[2]),'comparison.json'),JSON.stringify({checked,layout:'dagre',themes:['default','dark'],geometryTolerance:0.2,textWidthTolerance:0.25,textVerticalTolerance:2.5},null,2));
console.log(`Compared ${checked} original/native Class SVGs: boxes, compartments, text, relationships, markers and multiplicity.`);
