// Original Sequence SVG geometry with an explicit Arial sans-serif environment.
import assert from 'node:assert/strict';
import {mkdirSync,readFileSync,writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {createHash} from 'node:crypto';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
const lock=JSON.parse(readFileSync('docs/mermaid-cpp/source-lock.json'));
for(const f of lock.files) assert.equal(createHash('sha256').update(readFileSync('.reference/mermaid-js/'+f.path)).digest('hex'),f.sha256,f.path);
const cases=[
 ['basic','sequenceDiagram\nparticipant U as User\nparticipant S as Service\nU->>S: Read document\nS-->>U: Document'],
 ['wide','sequenceDiagram\nparticipant A as Document editor\nparticipant B as Storage\nA->>B: Read and validate the document before saving changes\nB-->>A: Complete'],
 ['self','sequenceDiagram\nparticipant A as Worker\nA->>A: Save'],
 ['reverse','sequenceDiagram\nparticipant A as Editor\nparticipant B as Store\nparticipant C as Watcher\nC->>A: Open\nA->B: Read\nB-->A: Ready\nA<<->>C: Update\nC<<-->>A: Done'],
 ['spacing','%%{init: {"sequence": {"width": 120, "height": 55, "actorMargin": 40, "boxMargin": 8, "rightAngles": true}}}%%\nsequenceDiagram\nA->>A: Check\nA-->>B: Ready'],
 ['font','%%{init: {"fontSize": 20}}%%\nsequenceDiagram\nparticipant A as Source document\nA->>B: Read the updated document\nB-->>A: Saved'],
 ["activation-note", "sequenceDiagram\nactor U as User\nparticipant S as Service\nU->>+S: Save document\nNote right of S: Validate input\nS->>S: Write output\nS-->>-U: Saved"],
 ["note-over", "sequenceDiagram\nparticipant A as Editor\nparticipant B as Store\nNote over A,B: Save operations\nA->>B: Commit\nNote left of A: Ready"],
 ["note-spacing", "sequenceDiagram\nA->>B: Read\nNote right of A: Validate document before saving all changes\nNote over B: Storage ready\nNote left of B: Return the current document to the editor\nB-->>A: Done"],
 ["nested-activation", "sequenceDiagram\nactor A as User\nparticipant B as Service\nA->>+B: Save\nB->>+B: Validate\nNote over B: Checking\nB-->>-B: Valid\nB-->>-A: Saved"],
 ["loop","sequenceDiagram\nloop Retry\nA->>B: Save\nB-->>A: Done\nend"],
 ["alt","sequenceDiagram\nalt Valid\nA->>B: Save\nelse Invalid\nB-->>A: Reject\nend"],
 ["nested","sequenceDiagram\nopt Cached\nloop Read\nA->>+B: Read\nNote over B: Loading\nB-->>-A: Data\nend\nend"],
 ["parallel","sequenceDiagram\npar Read\nA->>B: Fetch\nand Write\nB->>A: Store\nend"],
 ["critical-break","sequenceDiagram\ncritical Save\nA->>B: Write\noption Failure\nbreak Abort\nB-->>A: Failed\nend\nend"],
 ["region-spacing","%%{init: {\"fontSize\": 20, \"sequence\": {\"boxMargin\": 8, \"boxTextMargin\": 7, \"labelBoxWidth\": 60, \"labelBoxHeight\": 24}}}%%\nsequenceDiagram\nloop Retry\nA->>A: Check\nA->>B: Save\nend"],
 ["background","sequenceDiagram\nrect rgb(220, 240, 255)\nA->>+B: Save\nNote over B: Write\nB-->>-A: Done\nend"],
 ["nested-background","sequenceDiagram\nrect rgba(200, 230, 255, 0.4)\nloop Retry\nrect rgb(245, 245, 200)\nA->>B: Save\nend\nB-->>A: Done\nend\nend"],
 ["title","sequenceDiagram\ntitle Save document\nA->>B: Save\nB-->>A: Done"],
 ["line-breaks","sequenceDiagram\nparticipant A as Document<br/>editor\nA->>B: Read<br/>document\nNote over A,B: Save<br/>changes\nB-->>A: Done"],
 ["entities","sequenceDiagram\nparticipant A as Editor #38; viewer\nA->>B: Save #60;document#62;\nNote over B: Value #35;1"],
 ["wrapped-labels","sequenceDiagram\nparticipant A as wrap: A document editor with a descriptive name\nA->>B: wrap: Read the document and validate every required field before saving\nNote over B: wrap: Store the updated document after validation succeeds"],
 ["region-wrap","sequenceDiagram\nloop Retry until the document has been saved successfully\nA->>B: Save\nB-->>A: Done\nend"],
 ["nested-wrap","sequenceDiagram\nalt The current document has unsaved local changes\nloop Retry saving the edited document to storage\nA->>+B: wrap: Save all changes made to the current document\nNote right of B: wrap: Validate the document before committing to storage\nB-->>-A: Saved\nend\nelse No local changes need to be written to storage\nA->>A: wrap: Continue editing the current document\nend"],
 ["tall-wrap","%%{init: {\"sequence\": {\"width\": 100, \"height\": 40}}}%%\nsequenceDiagram\nparticipant A as wrap: A document editor with a descriptive name\nparticipant B as Storage\nA->>B: wrap: Save the current document\nNote left of B: wrap: Review the current document before saving\nNote over A,B: wrap: Both participants finish the save operation"],
];
export function inspectSequence() {
 const root=document.querySelector('svg');
 const box=e=>{const b=e.getBBox();return {x:b.x,y:b.y,width:b.width,height:b.height};};
 const style=e=>{const s=getComputedStyle(e);return {fill:s.fill,stroke:s.stroke,strokeWidth:s.strokeWidth};};
 const actors=[...root.querySelectorAll('rect.actor')].map(e=>({id:e.getAttribute('name'),footer:e.classList.contains('actor-bottom'),...box(e),...style(e)})).sort((a,b)=>a.id.localeCompare(b.id)||Number(a.footer)-Number(b.footer));
 const regions=[...root.querySelectorAll('.loopLine,.labelBox')].map(e=>({tag:e.tagName,...box(e),...style(e),dash:getComputedStyle(e).strokeDasharray,points:e.tagName==='polygon'?[...e.points].map(p=>[p.x,p.y]):null}));
 const backgrounds=[...root.querySelectorAll('rect.rect')].map(e=>({...box(e),...style(e)}));
 const notes=[...root.querySelectorAll('rect.note')].map(e=>({...box(e),...style(e)}));
 const activations=[...root.querySelectorAll('rect[class^=activation]')].map(e=>({...box(e),...style(e)})).sort((a,b)=>a.x-b.x||a.y-b.y);
 const figures=[...root.querySelectorAll('g.actor-man')].flatMap(g=>[...g.querySelectorAll('line,circle')].map(e=>({id:g.getAttribute('name'),footer:g.classList.contains('actor-bottom'),...box(e),...style(e)}))).sort((a,b)=>a.id.localeCompare(b.id)||Number(a.footer)-Number(b.footer)||a.y-b.y||a.x-b.x);
 const lines=[...root.querySelectorAll('.actor-line')].map(e=>({...box(e),...style(e)})).sort((a,b)=>a.x-b.x);
 const texts=[...root.querySelectorAll('text')].map(e=>({text:e.textContent,...box(e),...style(e.querySelector('tspan')??e),size:getComputedStyle(e).fontSize})).sort((a,b)=>a.text.localeCompare(b.text)||a.y-b.y);
 const signals=[...root.querySelectorAll('.messageLine0,.messageLine1')].map(e=>{
   const marker=side=>{const value=e.getAttribute('marker-'+side);if(!value)return null;const id=value.slice(value.lastIndexOf('#')+1,-1);const m=root.querySelector('[id="'+id+'"]');const path=m.querySelector('path');return {refX:m.getAttribute('refX'),refY:m.getAttribute('refY'),d:path.getAttribute('d'),...style(path)};};
   return {kind:e.classList.contains('messageLine1')?1:0,...style(e),points:Array.from({length:21},(_,i)=>{const p=e.getPointAtLength(e.getTotalLength()*i/20);return [p.x,p.y];}),start:marker('start'),end:marker('end')};
 });
 const v=root.viewBox.baseVal;return {backgrounds,regions,actors,notes,activations,figures,lines,texts,signals,view:{x:v.x,y:v.y,width:v.width,height:v.height}};
}
if(!process.argv.includes('--library')) {
 const directory=resolve(process.argv[2]??'build/mermaid-cpp/sequence-browser');mkdirSync(directory,{recursive:true});
 const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
 const html=`<!doctype html><body><script type="module">
 const {default:mermaid}=await import(${JSON.stringify(url)});
 const inspect=${inspectSequence.toString()};try{const rows=[];
 for(const [name,source] of ${JSON.stringify(cases)}) for(const theme of ['default','dark']) {
  mermaid.initialize({startOnLoad:false,theme,look:'classic',fontFamily:'Arial'});
  const {svg}=await mermaid.render('sequence'+rows.length,source);const host=document.createElement('div');host.innerHTML=svg;document.body.append(host);
  rows.push({name:name+'-'+theme,source,theme,svg,...inspect()});host.remove();
 }
 document.body.innerHTML='<pre id="result"></pre>';document.querySelector('pre').textContent=JSON.stringify(rows);
 }catch(e){document.body.innerHTML='<pre id="error"></pre>';document.querySelector('pre').textContent=e.stack;}
 </script>`;
 const file=resolve(directory,'probe.html');writeFileSync(file,html);
 const browser=await chromium.launch({executablePath:process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true,args:['--allow-file-access-from-files']});
 let rows;
 try {
  const page=await browser.newPage();const cdp=await page.context().newCDPSession(page);
  // The original probes sans-serif as well as the configured family. Fix its
  // browser mapping, just as the native resolver explicitly maps it to Arial.
  await cdp.send('Page.setFontFamilies',{fontFamilies:{sansSerif:'Arial'},forScripts:['Hans','Hant','Latn','Zyyy'].map(script=>({script,fontFamilies:{sansSerif:'Arial'}}))});
  await page.goto(pathToFileURL(file).href);await page.waitForSelector('pre#result,pre#error',{timeout:60000});
  assert.equal(await page.locator('#error').count(),0,await page.locator('pre').textContent());
  rows=JSON.parse(await page.locator('#result').textContent());
 }finally{await browser.close();}
 assert.equal(rows.length,cases.length*2);
 writeFileSync(resolve(directory,'original.json'),JSON.stringify(rows,null,2));
 for(const row of rows) writeFileSync(resolve(directory,row.name+'.svg'),row.svg);
 console.log(`Measured ${rows.length} original Sequence SVGs in ${directory}`);
}
