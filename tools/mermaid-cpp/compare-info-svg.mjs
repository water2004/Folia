import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, resolve } from 'node:path';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';

const baselines=JSON.parse(readFileSync(process.argv[2]??'build/mermaid-cpp/info-browser/original.json','utf8'));
const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true});
try {
  const page=await browser.newPage(); let max=0;
  for(const expected of baselines){
    const native=readFileSync(resolve(process.argv[3]??'build/mermaid-cpp/info-native',`info-${expected.name}.svg`),'utf8');
    await page.setContent(native);
    const actual=await page.evaluate(()=>{const e=document.querySelector('text.version'),s=document.querySelector('svg'),b=e.getBBox(),c=getComputedStyle(e),r=s.getBoundingClientRect();return {view:{width:s.viewBox.baseVal.width,height:s.viewBox.baseVal.height},width:s.getAttribute('width'),height:s.getAttribute('height'),style:s.getAttribute('style'),viewport:{x:r.x,y:r.y,width:r.width,height:r.height},text:e.textContent,box:{x:b.x,y:b.y,width:b.width,height:b.height},font:{size:c.fontSize,anchor:c.textAnchor,fill:c.fill}};});
    assert.equal(actual.text,expected.text,expected.name);
    assert.deepEqual(actual.view,expected.view,expected.name);
    assert.equal(actual.width,expected.width,expected.name); assert.equal(actual.height,expected.height,expected.name);
    assert.equal(actual.style,expected.style,expected.name);
    for(const key of ['x','y','width','height']){const delta=Math.abs(actual.viewport[key]-expected.viewport[key]);assert(delta<=1.5,`${expected.name}.viewport.${key}: ${actual.viewport[key]} != ${expected.viewport[key]} (tol 1.5)`);max=Math.max(max,delta);}
    for(const key of ['x','y','width','height']){const delta=Math.abs(actual.box[key]-expected.box[key]);assert(delta<=1.5,`${expected.name}.${key}: ${actual.box[key]} != ${expected.box[key]} (tol 1.5)`);max=Math.max(max,delta);}
    assert.equal(actual.font.size,expected.font.size,expected.name);assert.equal(actual.font.anchor,expected.font.anchor,expected.name);assert.equal(actual.font.fill,expected.font.fill,expected.name);
  }
  writeFileSync(resolve(dirname(process.argv[2]??'build/mermaid-cpp/info-browser/original.json'),'comparison.json'),JSON.stringify({count:baselines.length,geometryTolerance:1.5,maximumDifference:max},null,2));
  console.log(`Compared ${baselines.length} Info SVG cases; max geometry delta ${max}px.`);
} finally { await browser.close(); }
