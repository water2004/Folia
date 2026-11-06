import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { inspectUsecase, edge } from './usecase-browser-probe.mjs';

const [originalFile,nativeDirectory]=process.argv.slice(2);assert(originalFile&&nativeDirectory,'Usage: node compare-usecase-svg.mjs original.json nativeSvgDir');
const original=JSON.parse(readFileSync(originalFile,'utf8'));const tolerance=1.5;const numbers=/[-+]?(?:\d*\.)?\d+(?:e[-+]?\d+)?/gi;
function numeric(actual,expected,key){assert(Number.isFinite(actual)&&Math.abs(actual-expected)<=tolerance,`${key}: ${actual} vs ${expected}`);}
function box(a,e,key){for(const k of ['x','y','width','height'])numeric(a[k],e[k],key+'.'+k);}
function inspectSvg(page,svg){return page.evaluate(source=>{document.body.innerHTML=source;return inspectUsecase();},svg);}
const browser=await chromium.launch({executablePath:edge,headless:true});
let compared=0;try{const page=await browser.newPage();await page.evaluate('globalThis.inspectUsecase='+inspectUsecase.toString());for(const {svg,name,...expected}of original){if(!svg)continue;const actual=await inspectSvg(page,readFileSync(resolve(nativeDirectory,`usecase-${name}.svg`),'utf8'));for(const k of ['width','height'])numeric(actual.view[k],expected.view[k],name+'.view.'+k);assert.equal(actual.labels.length,expected.labels.length,name+'.label count');expected.labels.forEach((e,i)=>{const a=actual.labels[i];assert.equal(a.text,e.text,name+'.label.'+i+'.text');box(a.box,e.box,name+'.label.'+i);});assert.equal(actual.nodes.length,expected.nodes.length,name+'.node count');expected.nodes.forEach((e,i)=>{const a=actual.nodes[i];box(a.box,e.box,name+'.node.'+i);assert.equal(a.shape,e.shape,name+'.node.'+i+'.shape');});assert.equal(actual.edges.length,expected.edges.length,name+'.edge count');expected.edges.forEach((e,i)=>{const a=actual.edges[i];assert.equal((a.d.match(/[A-Z]/g)||[]).join(''),(e.d.match(/[A-Z]/g)||[]).join(''),name+'.edge.'+i+'.commands');const x=a.d.match(numbers).map(Number),y=e.d.match(numbers).map(Number);assert.equal(x.length,y.length,name+'.edge.'+i+'.coordinate count');x.forEach((v,j)=>numeric(v,y[j],name+'.edge.'+i+'.coord.'+j));for(const side of ['markerStart','markerEnd']){assert.equal(!!a[side],!!e[side],name+'.edge.'+i+'.'+side);if(a[side]&&e[side]){assert.equal(a[side].shape,e[side].shape,name+'.edge.'+i+'.'+side+'.shape');for(const k of ['width','height','refX','refY'])numeric(a[side][k],e[side][k],name+'.edge.'+i+'.'+side+'.'+k);}}});++compared;}}finally{await browser.close();}
console.log(`Compared ${compared} Usecase SVGs at ${tolerance} px.`);
