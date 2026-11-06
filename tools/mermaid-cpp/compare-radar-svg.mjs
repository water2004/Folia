import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
process.argv.push('--library');const {inspectRadar}=await import('./radar-browser-probe.mjs');
const originals=JSON.parse(readFileSync(process.argv[2]));
const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true});
function compare(a,e,path){if(typeof e==='number')assert(Number.isFinite(a)&&Math.abs(a-e)<=0.05,`${path}: ${a} != ${e}`);else if(Array.isArray(e)){assert.equal(a.length,e.length,path);e.forEach((v,i)=>compare(a[i],v,path+'.'+i));}else if(e&&typeof e==='object')for(const [key,value]of Object.entries(e))compare(a[key],value,path+'.'+key);else assert.equal(a,e,path);}
try{const page=await browser.newPage();for(const original of originals){await page.setContent(readFileSync(resolve(process.argv[3],'radar-'+original.name+'.svg'),'utf8'));const actual=await page.evaluate(inspectRadar);for(const key of ['view','shapes','texts'])compare(actual[key],original[key],original.name+'.'+key);}}finally{await browser.close();}
writeFileSync(resolve(dirname(process.argv[2]),'comparison.json'),JSON.stringify({checked:originals.length,geometryAndTextTolerance:0.05},null,2));console.log(`Compared ${originals.length} native Radar SVGs: polar shapes, cubic curves, paints, legends and text bounds.`);
