import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
process.argv.push('--library');
const { inspectRequirement } = await import('./requirement-browser-probe.mjs');
const originals=JSON.parse(readFileSync(process.argv[2]));
const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true});
let maximumDifference=0;
function compare(actual,expected,path,tolerance=0){
  if(typeof expected==='number'){const delta=Math.abs(actual-expected);assert(Number.isFinite(actual)&&delta<=tolerance,`${path}: ${actual} != ${expected} (tol ${tolerance})`);maximumDifference=Math.max(maximumDifference,delta);}
  else if(Array.isArray(expected)){assert.equal(actual.length,expected.length,path);expected.forEach((v,i)=>compare(actual[i],v,`${path}.${i}`,tolerance));}
  else if(expected&&typeof expected==='object'){for(const [key,value] of Object.entries(expected))compare(actual[key],value,`${path}.${key}`,tolerance);}
  else assert.equal(actual,expected,path);
}
function pathGeometry(actual,expected,path){
  const number=/[-+]?(?:\d*\.)?\d+(?:e[-+]?\d+)?/gi;
  compare(actual.replace(number,'#').replace(/[\s,]/g,''),expected.replace(number,'#').replace(/[\s,]/g,''),`${path}.commands`);
  compare([...actual.matchAll(number)].map(m=>Number(m[0])),[...expected.matchAll(number)].map(m=>Number(m[0])),`${path}.numbers`,1.5);
}
try {
  const page=await browser.newPage();
  for(const original of originals){
    const native=readFileSync(resolve(process.argv[3],`requirement-${original.name}.svg`),'utf8');
    assert(!native.includes('<foreignObject'),`${original.name}: labels must survive SVG normalization`);
    await page.setContent(native);
    const actual=await page.evaluate(inspectRequirement),prefix=original.name;
    compare(actual.view,original.view,`${prefix}.view`,1.5);
    for(const key of ['nodes','edges']){
      assert.equal(actual[key].length,original[key].length,`${prefix}.${key}.length`);
      for(let i=0;i<original[key].length;++i){const a=actual[key][i],e=original[key][i],p=`${prefix}.${key}.${i}`;
        compare(a.classes,e.classes,`${p}.classes`);
        compare(a.box,e.box,`${p}.box`,1.5);
        if(key==='nodes'){
          for(const property of ['fill','stroke','strokeWidth'])compare(a[property],e[property],`${p}.${property}`);
          compare(a.divider,e.divider,`${p}.divider`,1.5);
        } else {
          for(const property of ['startMarker','endMarker'])compare(a[property],e[property],`${p}.${property}`);
          pathGeometry(a.path,e.path,`${p}.path`);
        }
      }
    }
    assert.equal(actual.labels.length,original.labels.length,`${prefix}.labels.length`);
    for(let i=0;i<original.labels.length;++i){const a=actual.labels[i],e=original.labels[i],p=`${prefix}.labels.${i}`;
      compare(a.text,e.text,`${p}.text`);compare(a.box,e.box,`${p}.box`,1.5);compare(a.font,e.font,`${p}.font`);
      if(prefix.startsWith('rich-'))compare(a.runs,e.runs,`${p}.runs`);
    }
  }
  await browser.close();
} catch(e) { await browser.close(); throw e; }
writeFileSync(resolve(dirname(process.argv[2]),'comparison.json'),JSON.stringify({checked:originals.length,measuredGeometryTolerance:1.5,maximumDifference},null,2));
console.log(`Compared ${originals.length} native Requirement SVGs with the Mermaid 12.2.0 browser baselines.`);
