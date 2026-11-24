import assert from 'node:assert/strict';
import { mkdirSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { cases, richCases } from './requirement-cases.mjs';

// Observe rendered nodes, routes, labels and the final viewport in SVG space.
// Geometry is transformed out of nested SVG groups before it is recorded.
import inspectRequirement from './oracle/observations/requirement.mjs';
export {inspectRequirement};

if(!process.argv.includes('--library')) {
  const directory=resolve(process.argv[2]??'build/mermaid-cpp/requirement-browser');
  mkdirSync(directory,{recursive:true});
  const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
  const successes=[...cases.filter(c=>c[2]===null),...richCases,[
    'svg-labels-title',
    '---\ntitle: Main\nconfig:\n  htmlLabels: false\n---\nrequirementDiagram\nrequirement R {\nverifyMethod: test\n}\nelement E {\ntype: module\n}\nR - verifies -> E\n',
  ]];
  const layouts=['dagre','elk'];
  const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--allow-file-access-from-files']});
  const rows=[];
  try {
    for(const layout of layouts) for(const [name,source] of successes) for(const theme of ['default','dark']) {
      const config={startOnLoad:false,theme,fontFamily:'Arial',themeVariables:{fontFamily:'Arial'},look:'classic'};
      if(layout==='dagre')config.layout=layout;
      const file=resolve(directory,'probe.html');
      writeFileSync(file,`<!doctype html><meta charset="utf-8"><body><script type="module">const {default:mermaid}=await import(${JSON.stringify(url)});try{mermaid.initialize(${JSON.stringify(config)});const {svg}=await mermaid.render('mermaid-requirement',${JSON.stringify(source)});document.body.innerHTML=svg;const observation=(${inspectRequirement.toString()})();document.body.innerHTML='<pre id="result"></pre>';document.querySelector('pre').textContent=JSON.stringify({svg,...observation});}catch(e){document.body.innerHTML='<pre id="error"></pre>';document.querySelector('pre').textContent=e.stack;}</script>`);
      const page=await browser.newPage({timezoneId:'UTC'});
      await page.goto(pathToFileURL(file).href);
      await page.waitForSelector('pre#result,pre#error',{timeout:60000});
      assert.equal(await page.locator('#error').count(),0,await page.locator('pre').textContent());
      const suffix=layout==='elk'?`-elk-${theme}`:`-${theme}`;
      const row={name:name+suffix,source,theme,layout,...JSON.parse(await page.locator('#result').textContent())};
      rows.push(row);writeFileSync(resolve(directory,`requirement-${row.name}.svg`),row.svg);await page.close();
    }
  } finally { await browser.close(); }
  assert.equal(rows.length,successes.length*2*layouts.length);
  writeFileSync(resolve(directory,'original.json'),JSON.stringify(rows,null,2));
  console.log(`Captured ${rows.length} Requirement SVG baselines for classic Dagre and default ELK in both themes.`);
}
