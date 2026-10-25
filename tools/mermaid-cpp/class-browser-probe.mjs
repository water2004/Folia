// Source-matched Class geometry, measured in an isolated browser.
import assert from 'node:assert/strict';
import {mkdirSync,readFileSync,writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
export const cases = [
 ['members','classDiagram\nclass Document {\n+String title\n+save() bool\n}\nclass Store {\n+load(id) Document\n}\nDocument --> Store : persists'],
 ['relations','classDiagram\ndirection LR\nclass Shape\n<<interface>> Shape\nclass Circle\nShape <|-- Circle\nCircle "1" o-- "*" Point : contains'],
 ['empty-boxes','classDiagram\nclass Document\nclass Store\nDocument ..> Store : uses'],
 ['classifiers','classDiagram\nclass Repository~T~ {\n<<service>>\n-items List~T~\n+count int$\n#find(id) T*\n~reset()$\n}'],
 ['attributes','classDiagram\nclass Document {\n+String title\n+int revision\n}'],
 ['configured','%%{init: {"fontSize": 20, "class": {"padding": 8, "hideEmptyMembersBox": true}, "state": {"nodeSpacing": 60, "rankSpacing": 70}}}%%\nclassDiagram\ndirection RL\nclass View\nclass Document\nView *-- Document'],
];
export function inspectClass() {
 const root=document.querySelector('svg');
 const matrix=e=>root.getCTM().inverse().multiply(e.getCTM());
 const point=(e,x,y)=>new DOMPoint(x,y).matrixTransform(matrix(e));
 const box=e=>{const b=e.getBBox(),p=point(e,b.x,b.y),q=point(e,b.x+b.width,b.y+b.height);return {x:p.x,y:p.y,width:q.x-p.x,height:q.y-p.y};};
 const paint=e=>{const s=getComputedStyle(e);return {fill:s.fill,stroke:s.stroke,strokeWidth:s.strokeWidth};};
 const textBox=e=>{
   // HTML Range width is the inline advance. SVG getBBox also includes glyph
   // overhang; compare advance origin/width and retain vertical glyph bounds.
   if(e.tagName!=='foreignObject') {
     const first=e.getStartPositionOfChar(0),origin=point(e,first.x,first.y);
     return {...box(e),x:origin.x,width:e.getComputedTextLength()};
   }
   const walker=document.createTreeWalker(e,NodeFilter.SHOW_TEXT);const texts=[];while(walker.nextNode())if(walker.currentNode.textContent)texts.push(walker.currentNode);
   if(!texts.length)return box(e);
   const range=document.createRange();range.setStart(texts[0],0);range.setEnd(texts.at(-1),texts.at(-1).textContent.length);
   const b=range.getBoundingClientRect(),m=root.getScreenCTM().inverse(),p=new DOMPoint(b.x,b.y).matrixTransform(m),q=new DOMPoint(b.right,b.bottom).matrixTransform(m);
   return {x:p.x,y:p.y,width:q.x-p.x,height:q.y-p.y};
 };
 const nodes=[...root.querySelectorAll('g.node')].map(g=>{
   const outer=g.querySelector('.outer-path');
   const id=g.getAttribute('data-class-id')??g.id.replace(/^.*classId-/,'').replace(/-\d+$/,'');
   const groups=[...g.querySelectorAll('.text')].map(t=>({kind:[...t.classList].find(c=>c.endsWith('-group')),
     labels:[...t.querySelectorAll('.label')].map(l=>{
       const fo=l.querySelector('foreignObject'),text=l.querySelector('text'),element=fo??text;
       const b=textBox(element);
       const s=getComputedStyle(fo?.querySelector('span')??text);
       return {text:element.textContent,...b,color:fo?s.color:s.fill,size:s.fontSize,weight:s.fontWeight,italic:s.fontStyle,decoration:fo?getComputedStyle(l).textDecorationLine:s.textDecorationLine};
     })}));
   return {id,...box(outer),...paint(outer.querySelector('path')??outer),groups,
     dividers:[...g.querySelectorAll('.divider')].map(e=>box(e))};
 });
 const edges=[...root.querySelectorAll('path.relation')].map(e=>{
   const marker=side=>{const url=e.getAttribute('marker-'+side);if(!url)return null;const id=url.slice(url.lastIndexOf('#')+1,-1),m=root.querySelector('[id="'+id+'"]'),p=m.querySelector('path');return {refX:m.getAttribute('refX'),refY:m.getAttribute('refY'),d:p.getAttribute('d'),...paint(p)};};
   return {...paint(e),dash:getComputedStyle(e).strokeDasharray,
     points:Array.from({length:21},(_,i)=>{const p=e.getPointAtLength(e.getTotalLength()*i/20),q=point(e,p.x,p.y);return [q.x,q.y];}),start:marker('start'),end:marker('end')};
 });
 const labels=[...root.querySelectorAll('.edgeLabels foreignObject,.edgeLabels text')].filter(e=>e.textContent).map(e=>({text:e.textContent,...textBox(e)}));
 const v=root.viewBox.baseVal;
 return {nodes,edges,labels,view:{x:v.x,y:v.y,width:v.width,height:v.height}};
}
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
