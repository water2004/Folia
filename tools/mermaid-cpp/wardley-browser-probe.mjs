import { browserExecutable } from './oracle/browser-executable.mjs';
import assert from 'node:assert/strict';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { cases } from './wardley-cases.mjs';
import { invalid } from './wardley-cases.mjs';
assert.equal(JSON.parse(readFileSync('tools/mermaid-cpp/oracle/node_modules/mermaid/package.json','utf8')).version,'12.2.0','Wardley oracle must use pinned Mermaid 12.2.0');

import inspectWardley from './oracle/observations/wardley.mjs';
export {inspectWardley};
export const edge=browserExecutable();
if(process.argv[1]&&pathToFileURL(resolve(process.argv[1])).href===import.meta.url){
  const dir=resolve(process.argv[2]??'build/mermaid-cpp/wardley-browser');mkdirSync(dir,{recursive:true});
  const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
  const file=resolve(dir,'probe.html');writeFileSync(file,'<!doctype html><meta charset="utf-8"><style>svg{font-family:"trebuchet ms"}</style><body></body>');
  const browser=await chromium.launch({executablePath:edge,headless:true,args:['--allow-file-access-from-files']});
  try {const page=await browser.newPage();await page.goto(pathToFileURL(file).href);await page.evaluate(async u=>{globalThis.mermaid=(await import(u)).default;},url);console.log('loaded Mermaid');await page.evaluate('globalThis.inspectWardley='+inspectWardley.toString()+'; void 0');console.log('installed inspector');
    const serialized=await page.evaluate(async payload=>{const {cases,invalid}=JSON.parse(payload);const rows=[];for(const theme of ['default','dark']){mermaid.initialize({startOnLoad:false,fontFamily:'Arial',look:'classic',theme});for(const [name,source] of cases){try{const result=await mermaid.render('wardley-'+theme+'-'+name,source);const host=document.createElement('div');host.innerHTML=result.svg;document.body.append(host);rows.push({name:'wardley-'+name+'-'+theme,svg:result.svg,...inspectWardley(host)});host.remove();}catch(e){return 'ERR '+name+' '+String(e);}}for(const source of invalid){if(await mermaid.parse(source,{suppressErrors:true}))return 'INVALID '+source;}}return JSON.stringify(rows);},JSON.stringify({cases,invalid}));if(serialized.startsWith('ERR')||serialized.startsWith('INVALID'))throw new Error(serialized);const rows=JSON.parse(serialized);
    assert.equal(rows.length,cases.length*2);writeFileSync(resolve(dir,'original.json'),JSON.stringify(rows,null,2));for(const row of rows)writeFileSync(resolve(dir,row.name+'.svg'),row.svg);console.log('Captured '+rows.length+' Wardley browser SVGs and verified '+invalid.length+' syntax errors');
  } finally {await browser.close();}
}
