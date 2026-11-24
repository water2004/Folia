// Source-matched Class geometry, measured in an isolated browser.
import assert from 'node:assert/strict';
import {mkdirSync,readFileSync,writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
export const cases = [
 ["svg-label-members", '%%{init: {"htmlLabels": false}}%%\nclassDiagram\nclass Document {\n+title String\n+save() bool\n}\nDocument --> Store : saves'],
 ["svg-label-classifiers", '%%{init: {"htmlLabels": false}}%%\nclassDiagram\nclass Repository~T~ {\n<<service>>\n-items List~T~\n+count int$\n#find(id) T*\n~reset()$\n}'],
 ["class-font-style", 'classDiagram\nclass Document {\n+title String\n+save() bool\n}\nstyle Document fill:#ECECFF,font-size:20px,font-family:Courier New,font-weight:bold,font-style:italic\nDocument --> Store : saves'],
 ["svg-font-style", '%%{init: {"htmlLabels": false, "fontSize":20}}%%\nclassDiagram\nclass Document {\n+title String\n+save() bool\n}\nstyle Document fill:#ECECFF,font-size:24px,font-family:Courier New,font-weight:bold,font-style:italic\nDocument --> Store : saves'],
 ["lollipop", "classDiagram\nclass Service {\n+save() bool\n}\nService --() Storage\nInput ()-- Service"],
 ["lollipop-repeated", "classDiagram\ndirection LR\nEditor --() Storage : saves\nViewer --() Storage : reads"],
 ["class-direct-style", "classDiagram\nclass Document {\n+title String\n+save() bool\n}\nDocument --> Store : saves\nstyle Document fill:#ffeecc,stroke:#aa5500,color:#552200,stroke-width:2px"],
 ["class-defined-style", "classDiagram\nclass Document:::highlight\nclass Store\nclassDef highlight fill:#ffeecc,stroke:#aa5500,color:#552200,stroke-width:2px\nDocument --> Store : saves"],
 ["class-assigned-style", "classDiagram\nclass Document {\n+count int$\n+save() bool*\n}\nclass Store\ncssClass \"Document,Store\" highlight\nclassDef highlight fill:#ffeecc,stroke:#aa5500,color:#552200,stroke-width:2px,stroke-dasharray:4 2\nstyle Store fill:#ddffee,stroke:#008855\nDocument --> Store : saves"],
 ["namespace-compact", "%%{init: {\"class\": {\"hierarchicalNamespaces\": false}}}%%\nclassDiagram\nnamespace App.Storage[\"Storage\"] {\nclass Store\nnote for Store \"Local storage\"\n}\nnamespace App {\nclass Editor\n}\nEditor --> Store : saves"],
 ["namespace-compact-nested", "%%{init: {\"class\": {\"hierarchicalNamespaces\": false}}}%%\nclassDiagram\nnamespace App[\"Application\"] {\nnamespace Storage {\nclass Document\n}\nclass Editor\n}\nEditor --> Document"],
 ["note-emphasis", "classDiagram\nclass Document\nnote for Document \"**Validate** the *document*\nKeep revisions\""],
 ["note-nowrap", "classDiagram\nclass Document\nnote for Document \"Validate the document before saving changes to local storage and keep revisions\""],
 ["wrapped-edge","classDiagram\nclass Document\nDocument --> Store : **Validate** the document before saving changes to local storage"],
 ["display-emphasis","classDiagram\nclass D[\"Document **model**\"] {\n+**name** String\n+save() *bool*\n}\nD --> Store : **save** changes"],
 ["display-lines","classDiagram\nclass D[\"Document<br/>model\"] {\n+name<br/>String\n+save()<br/>bool\n}\nD --> Store : save<br/>changes"],
 ["display-entities","classDiagram\nclass D[\"Editor &amp; viewer\"]\nD --> Store : save #38; reload"],
 ["namespace","classDiagram\nnamespace Storage {\nclass Document {\n+save() bool\n}\nclass Store\nnote for Store \"Local storage\"\n}\nDocument --> Store : persists"],
 ["namespace-nested","classDiagram\nnamespace App[\"Application\"] {\nnamespace Storage {\nclass Document\n}\nclass Editor\n}\nEditor --> Document : saves"],
 ["namespace-qualified","classDiagram\nnamespace App.Storage {\nclass Store\n}\nnamespace App {\nclass Editor\n}\nEditor --> Store"],
 ['note-only','classDiagram\nnote "Document model"'],
 ['note-attached','classDiagram\nnote for Document "Validate before saving\nKeep revisions"\nclass Document {\n+save() bool\n}'],
 ['note-related','classDiagram\ndirection LR\nclass Document\nDocument --> Store : persists\nnote for Store "Local storage"\nnote for Missing "Unattached"'],
 ['self','classDiagram\nclass Document {\n+validate() bool\n}\nDocument --> Document : validates'],
 ['self-related','classDiagram\ndirection LR\nclass Document {\n+save() bool\n}\nDocument --> Store : persists\nDocument "1" o-- "*" Document : revisions'],
 ['title','---\ntitle: Document storage\n---\nclassDiagram\nclass Document {\n+save() bool\n}\nDocument --> Store : persists'],
 ['title-wide','---\ntitle: Read and validate the document before saving to storage\nconfig:\n  fontSize: 20\n  state:\n    titleTopMargin: 40\n---\nclassDiagram\nclass Document'],
 ['members','classDiagram\nclass Document {\n+String title\n+save() bool\n}\nclass Store {\n+load(id) Document\n}\nDocument --> Store : persists'],
 ['relations','classDiagram\ndirection LR\nclass Shape\n<<interface>> Shape\nclass Circle\nShape <|-- Circle\nCircle "1" o-- "*" Point : contains'],
 ['empty-boxes','classDiagram\nclass Document\nclass Store\nDocument ..> Store : uses'],
 ['classifiers','classDiagram\nclass Repository~T~ {\n<<service>>\n-items List~T~\n+count int$\n#find(id) T*\n~reset()$\n}'],
 ['attributes','classDiagram\nclass Document {\n+String title\n+int revision\n}'],
 ['configured','%%{init: {"fontSize": 20, "class": {"padding": 8, "hideEmptyMembersBox": true}, "state": {"nodeSpacing": 60, "rankSpacing": 70}}}%%\nclassDiagram\ndirection RL\nclass View\nclass Document\nView *-- Document'],
];
import inspectClass from './oracle/observations/class.mjs';
export {inspectClass};
if(!process.argv.includes('--library')) {
 const directory=resolve(process.argv[2]??'build/mermaid-cpp/class-browser');mkdirSync(directory,{recursive:true});
 const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
 const html=`<!doctype html><body><script type="module">
 const {default:mermaid}=await import(${JSON.stringify(url)});const inspect=${inspectClass.toString()};
 try{const rows=[];for(const [name,source] of ${JSON.stringify(cases)}) for(const theme of ['default','dark']) {
 mermaid.initialize({startOnLoad:false,theme,look:'classic',layout:'dagre',fontFamily:'Arial'});
 const {svg}=await mermaid.render('class'+rows.length,source);const host=document.createElement('div');host.innerHTML=svg;document.body.append(host);
 rows.push({name:name+'-'+theme,source,theme,svg,...inspect()});host.remove();}
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
}
