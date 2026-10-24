import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
process.argv.push('--library');
const {inspectSequence}=await import('./sequence-browser-probe.mjs');
const reference=resolve(process.argv[2]),directory=resolve(process.argv[3]);
const rows=JSON.parse(readFileSync(reference));
const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true});
const near=(a,b,label,tolerance=1)=>assert(Number.isFinite(a)&&Number.isFinite(b)&&Math.abs(a-b)<=tolerance,`${label}: ${a} != ${b}`);
const report=[];
try {
 const page=await browser.newPage({viewport:{width:1600,height:1200}});
 for(const row of rows) {
  await page.setContent(readFileSync(resolve(directory,'sequence-'+row.name+'.svg'),'utf8'));
  const actual=await page.evaluate(inspectSequence);
  for(const key of ['x','y','width','height']) near(actual.view[key],row.view[key],row.name+' view '+key);
  for(const category of ['actors','lines','texts','signals']) {
   assert.equal(actual[category].length,row[category].length,row.name+' '+category+' count');
   actual[category].forEach((a,i)=>{
    const e=row[category][i],label=row.name+' '+category+' '+i;
    for(const key of ['id','footer','text','size','fill','stroke','strokeWidth','kind']) if(key in e) assert.equal(a[key],e[key],label+' '+key);
    for(const key of ['x','y','width','height']) if(key in e) near(a[key],e[key],label+' '+key,category==='texts'?1.1:1);
    if(category==='signals') {
     assert.deepEqual(a.start,e.start,label+' start marker');assert.deepEqual(a.end,e.end,label+' end marker');
     a.points.forEach((p,j)=>p.forEach((v,k)=>near(v,e.points[j][k],label+' path',1)));
    }
   });
  }
  report.push({name:row.name,actors:actual.actors.length,signals:actual.signals.length,status:'passed'});
 }
}finally{await browser.close();}
writeFileSync(resolve(dirname(reference),'comparison.json'),JSON.stringify(report,null,2));
console.log(`Compared ${report.length} original/native Sequence diagrams: actors, lifelines, labels, paths, markers, paints and viewports.`);
