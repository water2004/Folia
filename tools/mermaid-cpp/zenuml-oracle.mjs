import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,mkdirSync,copyFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {createHash} from 'node:crypto';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
import {cases} from './zenuml-cases.mjs';
const source=resolve(process.env.FOLIA_ZENUML_SOURCE??'build/mermaid-cpp/zenuml-source'),directory=resolve(process.argv[2]??'build/mermaid-cpp/zenuml-oracle');mkdirSync(directory,{recursive:true});
assert.equal(createHash('sha512').update(readFileSync(resolve(source,'core-3.49.2.tgz'))).digest('base64'),'qeQMapuE0XETML2mRw2MIIrMCjcZBhRDSwW10//q8XQ+VyExzkAn39VCBwxIbbPdTBt6gJnaBBixF9A4hsIxcA==');
const bundle=readFileSync(resolve(source,'package/dist/zenuml.esm.mjs'),'utf8');
// This is the real renderToSvg call chain in the pinned bundle, not an unused
// source-module class. Fail if names change instead of recording empty models.
assert(bundle.includes('function XI(e, t) {\n  const n = R8(e);'));
assert(bundle.includes('new D8(n, b2), a = new Z8(n)'));
writeFileSync(resolve(directory,'zenuml-oracle.mjs'),bundle+'\nexport function oracleModel(code){const root=R8(code),horizontal=new D8(root,b2),vertical=new Z8(root);return {measurements:[...new Set([...horizontal.participantModels.map(p=>p.label||p.name),...horizontal.ownableMessages.map(m=>m.signature)])].map(text=>[text.trim(),b2(text,0)]),title:root.title?.()?.content?.()??"",participants:horizontal.participantModels,messages:horizontal.ownableMessages,constraints:horizontal.m,positions:horizontal.participantModels.map(p=>({name:p.name,x:horizontal.getPosition(p.name)})),vertical:vertical.entries(),height:vertical.getTotalHeight()};}\n');
copyFileSync(resolve(source,'package/dist/cloud-icons-eHuugVSv.js'),resolve(directory,'cloud-icons-eHuugVSv.js'));
const html=`<!doctype html><body><script type="module">
try{const {renderToSvg,oracleModel}=await import('./zenuml-oracle.mjs'),rows=[];for(const [name,source] of ${JSON.stringify(cases)}){const code=source.replace(/^\\s*zenuml/,''),model=oracleModel(code),rendered=renderToSvg(code);if(!model.participants.length||!model.messages.length||!model.vertical.length||!rendered.geometry)throw new Error(name+' empty oracle');rows.push({name,source,model,...rendered});}document.body.replaceChildren(Object.assign(document.createElement('pre'),{id:'result',textContent:JSON.stringify(rows)}));}catch(e){document.body.replaceChildren(Object.assign(document.createElement('pre'),{id:'error',textContent:String(e.stack)}));}
</script>`;
const file=resolve(directory,'probe.html');writeFileSync(file,html);
const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--allow-file-access-from-files']});
try{const page=await browser.newPage();await page.goto(pathToFileURL(file).href);await page.waitForSelector('pre#result,pre#error',{timeout:60000});assert.equal(await page.locator('#error').count(),0,await page.locator('body').textContent());const rows=JSON.parse(await page.locator('#result').textContent());assert.equal(rows.length,cases.length);writeFileSync(resolve(directory,'original.json'),JSON.stringify(rows,null,2));for(const row of rows)writeFileSync(resolve(directory,row.name+'.svg'),row.svg);const hex=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i).toString(16).padStart(4,'0')).join('');const fixture=rows.map(r=>r.name+'\t'+hex(r.source)+'\t'+hex(JSON.stringify(r.model))).join('\n')+'\n';writeFileSync(resolve(directory,'zenuml.tsv'),fixture);if(process.argv.includes('--write'))writeFileSync('tests/mermaid_cpp/fixtures/zenuml.tsv',fixture);else assert.equal(fixture,readFileSync('tests/mermaid_cpp/fixtures/zenuml.tsv','utf8').replace(/\r\n/g,'\n'));console.log(`ZenUML: ${rows.length} original parser, horizontal/vertical layout and SVG records.`);}finally{await browser.close();}
