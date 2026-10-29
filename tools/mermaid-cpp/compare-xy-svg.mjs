import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {resolve,dirname} from 'node:path';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
process.argv.push('--library');const {inspectXY}=await import('./xy-browser-probe.mjs');
const originals=JSON.parse(readFileSync(process.argv[2]));
const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true});
// DirectWrite's measured glyph bounds and Chromium's hinted SVG boxes have
// the same 1.5-unit boundary used by the measured Flowchart family. The second
// gate supplies original browser measurements and keeps layout accuracy 0.05.
const oracle=process.argv.includes('--oracle-measurements');
const geometryTolerance=oracle?0.05:1.5;
let maximumDifference=0,maximumDifferenceAt='';
function compare(a,e,path,tolerance=0){if(typeof e==='number'){const difference=Math.abs(a-e);assert(Number.isFinite(a)&&difference<=tolerance,`${path}: ${a} != ${e} (tolerance ${tolerance})`);if(difference>maximumDifference){maximumDifference=difference;maximumDifferenceAt=path;}}else if(Array.isArray(e)){assert.equal(a.length,e.length,path);e.forEach((v,i)=>compare(a[i],v,path+'.'+i,tolerance));}else if(e&&typeof e==='object')for(const [key,value]of Object.entries(e))compare(a[key],value,path+'.'+key,tolerance);else assert.equal(a,e,path);}
function normalized(row){return row.elements.map(e=>({type:e.type,group:e.groups.filter(Boolean).join('/'),text:e.text,box:e.box,transform:e.transform,paint:e.paint,path:e.attrs.d?(e.attrs.d.match(/[-+]?(?:\d*\.)?\d+(?:e[-+]?\d+)?/gi)??[]).map(Number):null})).map(e=>({...e,group:e.group==='main'?'background':e.group})).sort((a,b)=>a.group.localeCompare(b.group)||a.type.localeCompare(b.type)||a.text.localeCompare(b.text)||a.box.x-b.box.x||a.box.y-b.box.y);}
try{const page=await browser.newPage();for(const original of originals){await page.setContent(readFileSync(resolve(process.argv[3],(oracle?'xy-oracle-':'xy-')+original.name+'.svg'),'utf8'));const actual=await page.evaluate(inspectXY);compare(actual.view,original.view,original.name+'.view');const a=normalized(actual),e=normalized(original);assert.equal(a.length,e.length,original.name+' element count');for(let i=0;i<e.length;++i){const path=original.name+'.elements.'+i;for(const key of ['type','group','text','paint'])compare(a[i][key],e[i][key],path+'.'+key);compare(a[i].box,e[i].box,path+'.box',geometryTolerance);compare(a[i].path,e[i].path,path+'.path',geometryTolerance);compare(a[i].transform.slice(0,4),e[i].transform.slice(0,4),path+'.transform-linear',0.00001);compare(a[i].transform.slice(4),e[i].transform.slice(4),path+'.translation',geometryTolerance);}}}finally{await browser.close();}
writeFileSync(resolve(dirname(process.argv[2]),oracle?'original-measurements-comparison.json':'comparison.json'),JSON.stringify({checked:originals.length,measurementSource:oracle?'original-browser':'DirectWrite',geometryTolerance,maximumDifference,maximumDifferenceAt},null,2));console.log(`Compared ${originals.length} native XY SVGs (${oracle?'original measurements':'DirectWrite'}), max geometry difference ${maximumDifference} at ${maximumDifferenceAt}.`);
