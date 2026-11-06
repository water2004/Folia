// Source-matched Class geometry, measured in an isolated browser.
import assert from 'node:assert/strict';
import {mkdirSync,readFileSync,writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
import {cases} from './class-elk-cases.mjs';
process.argv.push('--library');
const {inspectClass}=await import('./class-browser-probe.mjs');
process.argv.pop();
import {createHash} from 'node:crypto';
const lock=JSON.parse(readFileSync('docs/mermaid-cpp/source-lock.json'));
const map=JSON.parse(readFileSync('tools/mermaid-cpp/oracle/node_modules/mermaid/'+lock.elkAdapter.map));
for(const f of lock.elkAdapter.files){const i=map.sources.findIndex(s=>s.endsWith(f.source));assert(i>=0);assert.equal(createHash('sha256').update(map.sourcesContent[i]).digest('hex'),f.sha256);}
 const directory=resolve(process.argv[2]??'build/mermaid-cpp/class-elk-browser');mkdirSync(directory,{recursive:true});
 const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
 const html=`<!doctype html><body><script type="module">
 const {default:mermaid}=await import(${JSON.stringify(url)});const inspect=${inspectClass.toString()};
 try{const rows=[];for(const [name,source] of ${JSON.stringify(cases)}) for(const theme of ['default','dark']) {
 mermaid.initialize({startOnLoad:false,theme,look:'classic',layout:'elk',fontFamily:'Arial'});
 const {svg}=await mermaid.render('class'+rows.length,source);const host=document.createElement('div');host.innerHTML=svg;document.body.append(host);
 const root=host.querySelector('svg');
 const size=g=>{const f=g.querySelector('foreignObject');return f?{width:+f.getAttribute('width'),height:+f.getAttribute('height')}:{width:0,height:0};};
 const measured={centers:[...root.querySelectorAll('g.edgeLabels > g.edgeLabel')].map(size),terminals:[...root.querySelectorAll('g.edgeLabels > g.edgeTerminals')].map(size),groups:[...root.querySelectorAll('g.cluster')].map(g=>({id:g.id,...size(g.querySelector('.cluster-label'))}))};
 rows.push({measured,name:name+'-'+theme,source,theme,svg,...inspect()});host.remove();}
 document.body.innerHTML='<pre id="result"></pre>';document.querySelector('pre').textContent=JSON.stringify(rows);
 }catch(e){document.body.innerHTML='<pre id="error"></pre>';document.querySelector('pre').textContent=e.stack;}</script>`;
 const file=resolve(directory,'probe.html');writeFileSync(file,html);
 const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--allow-file-access-from-files']});
 let rows;
 try { const page=await browser.newPage();await page.goto(pathToFileURL(file).href);
 await page.waitForSelector('pre#result,pre#error',{timeout:60000});assert.equal(await page.locator('#error').count(),0,await page.locator('pre').textContent());
 rows=JSON.parse(await page.locator('#result').textContent()); } finally {await browser.close();}
 assert.equal(rows.length,cases.length*2);writeFileSync(resolve(directory,'original.json'),JSON.stringify(rows,null,2));
 for(const row of rows)writeFileSync(resolve(directory,row.name+'.svg'),row.svg);
 console.log(`Measured ${rows.length} original Class SVGs in ${directory}`);
