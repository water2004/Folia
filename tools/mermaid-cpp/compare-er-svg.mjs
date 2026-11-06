import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';

process.argv.push('--library');
const {inspectEr}=await import('./er-browser-probe.mjs');
const originals=JSON.parse(readFileSync(process.argv[2]));
assert.equal(originals.length,32);
const tolerance=1.5;
let maximumDifference=0;
const numbers=/[-+]?(?:\d*\.)?\d+(?:e[-+]?\d+)?/gi;

function compare(a,e,path,epsilon=0) {
  if(typeof e==='number') {
    const delta=Math.abs(a-e);
    assert(Number.isFinite(a)&&delta<=epsilon,`${path}: ${a} != ${e}`);
    maximumDifference=Math.max(maximumDifference,delta);
  } else if(Array.isArray(e)) {
    assert.equal(a.length,e.length,path);
    e.forEach((v,i)=>compare(a[i],v,path+'.'+i,epsilon));
  } else if(e&&typeof e==='object') {
    for(const [key,value] of Object.entries(e)) compare(a[key],value,path+'.'+key,epsilon);
  } else {
    assert.equal(a,e,path);
  }
}

function commands(d) {
  return d.replace(numbers,'#').replace(/[\s,]/g,'');
}
function coordinates(d) {
  return [...d.matchAll(numbers)].map(m=>Number(m[0]));
}
function geometry(a,e,path) {
  compare(commands(a),commands(e),path+'.commands');
  compare(coordinates(a),coordinates(e),path+'.numbers',tolerance);
}

// RoughJS supplies random cubic control points even when roughness is zero.
// Those points parameterize eight straight strokes (two per side). Validate
// the command structure, all endpoints, and that both handles stay on their
// ordered segment before comparing the resulting geometric path.
function roughSegments(d,fill,path) {
  const p=coordinates(d);
  if(fill) {
    assert.equal(commands(d),'M##L##L##L##',path+'.fill commands');
    return p;
  }
  assert.equal(commands(d),'M##C######'.repeat(8),path+'.stroke commands');
  const segments=[];
  for(let i=0;i<p.length;i+=8) {
    const [x,y,cx,cy,dx,dy,X,Y]=p.slice(i,i+8);
    const sx=X-x,sy=Y-y,length=Math.hypot(sx,sy);
    assert(length>0,path+'.nonempty segment');
    let previous=0;
    for(const [px,py] of [[cx,cy],[dx,dy]]) {
      const vx=px-x,vy=py-y;
      assert(Math.abs(vx*sy-vy*sx)/length<=1e-7,path+'.collinear handles');
      const t=(vx*sx+vy*sy)/(length*length);
      assert(t>=previous&&t<=1,path+'.ordered handles');
      previous=t;
    }
    segments.push(x,y,X,Y);
  }
  return segments;
}

const browser=await chromium.launch({
  executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',
  headless:true,
});
try {
  const page=await browser.newPage();
  for(const original of originals) {
    const svg=readFileSync(resolve(process.argv[3],'er-'+original.name+'.svg'),'utf8');
    await page.setContent('<!doctype html>'+svg);
    const actual=await page.evaluate(inspectEr);
    compare(actual.view,original.view,original.name+'.view',tolerance);
    for(const key of ['elements','labels','backgrounds','markers']) {
      assert.equal(actual[key].length,original[key].length,original.name+'.'+key+'.length');
      for(let i=0;i<original[key].length;++i) {
        const a=actual[key][i],e=original[key][i],path=original.name+'.'+key+'.'+i;
        if(key==='markers') {
          const {contents:actualContents,...actualAttributes}=a;
          const {contents:originalContents,...originalAttributes}=e;
          compare(actualAttributes,originalAttributes,path+'.attributes');
          compare(actualContents,originalContents.map(({path,...rest})=>rest),path+'.contents');
          originalContents.forEach((item,j)=>{
            if(item.path) geometry(actualContents[j].path,item.path,path+'.contents.'+j+'.path');
          });
          continue;
        }
        compare(a.box,e.box,path+'.box',tolerance);
        if(key==='backgrounds') {
          compare(a.part,e.part,path+'.part');
          compare(a.color,e.color,path+'.color');
        } else if(key==='labels') {
          compare(a.text,e.text,path+'.text');
          compare(a.font,e.font,path+'.font');
        } else {
          compare(a.type,e.type,path+'.type');
          compare(a.rough,e.rough,path+'.rough');
          compare(a.paint,e.paint,path+'.paint');
          compare(a.markers,e.markers,path+'.markers');
          if(e.rough) {
            const fill=e.paint.fill!=='none';
            compare(roughSegments(a.path,fill,path),roughSegments(e.path,fill,path),path+'.path',tolerance);
          } else if(e.path) {
            geometry(a.path,e.path,path+'.path');
          }
        }
      }
    }
  }
} finally {
  await browser.close();
}
writeFileSync(resolve(dirname(process.argv[2]),'comparison.json'),
  JSON.stringify({checked:originals.length,measuredGeometryTolerance:tolerance,maximumDifference},null,2));
console.log(`Compared ${originals.length} native ER SVGs with original entities, attributes, routes, markers, and labels.`);
