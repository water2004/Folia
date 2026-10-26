// Real browser observations from the locked Mermaid bundle. No estimated boxes.
import assert from 'node:assert/strict';
import {mkdirSync,writeFileSync,readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {chromium} from './oracle/node_modules/playwright-core/index.mjs';
import {createHash} from 'node:crypto';
const lock=JSON.parse(readFileSync('docs/mermaid-cpp/source-lock.json'));
for(const file of lock.files) assert.equal(createHash('sha256').update(readFileSync('.reference/mermaid-js/'+file.path)).digest('hex'),file.sha256,file.path);
for(const file of lock.flowchartCurve.files) assert.equal(createHash('sha256').update(readFileSync('tools/mermaid-cpp/oracle/node_modules/d3-shape/'+file.path)).digest('hex'),file.sha256,file.path);
const directory=resolve(process.argv[2]??'build/mermaid-cpp/flowchart-browser');
mkdirSync(directory,{recursive:true});
const cases=[
  ["svg-label-plain", "flowchart LR\nA[\"Read input<br/>then save\"] -->|\"next step<br/>when ready\"| B[\"Save the source document and finish\"]"],
  ["svg-label-markdown", "flowchart TD\nA[\"`Read **source** and *save*`\"] -->|\"`next **step** and *write*`\"| B[\"`Write the **updated document** and finish`\"]"],
  ["rich-group-html", "flowchart TD\nsubgraph g[\"Read <strong>source</strong> and <em>write</em>\"]\nA[Read]-->B[Write]\nend\ng-->C[Done]"],
  ["rich-group-html-nested", "flowchart LR\nsubgraph outer[\"<b>Processing</b><br/>queue\"]\nsubgraph inner[\"<em>Storage</em> and <strong>files</strong>\"]\ndirection TB\nA[Read]-->B[Write]\nend\nend\nouter-->C[Done]"],
  ['edge-ids-interpolation','flowchart TD\nA first@--> B\nA second@-- next --> C\nB --> D\nC --> D\nlinkStyle 0,1 interpolate linear stroke:#226688\nlinkStyle 2 interpolate basis'],
  ['edge-interpolation-default','flowchart LR\nA-->B\nlinkStyle default interpolate linear\nB-->C\nlinkStyle default interpolate basis\nC-->D\nA-->D'],
  ['rich-markdown','flowchart LR\nA["`Read **bold** and *italic*`"] -->|"`next **operation**`"| B["`Save ***both***\nthen finish`"]'],
  ['rich-markdown-group','flowchart TD\nsubgraph g[Operations]\ndirection LR\nA["`Read the __source document__ and _validate input_`"] -->|"`continue with **next step** and *save*`"| B["`Write **updated _data_**`"]\nend\ng --> C[Done]'],
  ['rich-markdown-literal','flowchart LR\nA["`Read \\*literal\\* &amp; **bold**`"] --> B["`Save file_name and *unfinished`"]'],
  ['rich-html','flowchart LR\nA["Read <b>bold</b> and <i>italic</i>"] -->|"next <strong>operation</strong>"| B["Save <strong><em>both</em></strong><br/>then finish"]'],
  ['rich-html-group','flowchart TD\nsubgraph g[Operations]\ndirection LR\nA["Read the <strong>source document</strong> and <em>validate input</em>"] -->|"continue with <b>next step</b> and <i>save</i>"| B["Write <i>updated data</i>"]\nend\ng --> C[Done]'],
  ['rich-html-font','flowchart LR\nA["Read <b>bold</b> and normal"] --> B["Write <em>italic</em> and normal"]\nstyle A font-size:20px,font-weight:300\nstyle B font-size:18px,font-weight:bold'],
  ["fonts-nodes", "flowchart LR\nA[Read the source document]:::large -->|next| B[Save the updated document]:::large\nclassDef large font-size:20px,font-weight:bold,color:#334466\nstyle B font-size:18px,font-weight:normal,font-style:italic"],
  ["fonts-edges", "flowchart TD\nA[Read] -->|Continue with the next operation| B[Write]\nB -->|Saved output| C[Done]\nlinkStyle 0 font-size:20px,font-weight:bold,color:#773311\nlinkStyle 1 font-size:18px,font-family:Courier New,font-style:italic"],
  ["fonts-group", "flowchart LR\nsubgraph g[Operations]\ndirection TB\nA[Read input]:::mono --> B[Write output]\nend\ng -->|done| C[Exit]\nclassDef mono font-family:Courier New,font-size:18px,font-weight:700\nstyle B font-size:22px\nstyle g color:#887733"],

  ["styles-nodes", "flowchart LR\nA[Read]:::base -->|next| B([Write]):::base\nclassDef default fill:#eeeeee,stroke:#444444\nclassDef base fill:#ddeeff,stroke:#224466,color:#112233,stroke-width:2px,stroke-dasharray:5 3\nstyle A fill:#ffddaa,color:#663300\nstyle B stroke:#aa3300,stroke-width:3px"],
  ["styles-group", "flowchart LR\nsubgraph g[Storage]\ndirection TB\nA[(Records)]:::record --> B{Ready?}\nend\ng -->|done| C(((Exit)))\nclassDef record fill:#ddffee,stroke:#228855,color:#114433\nclassDef group fill:#fff4cc,stroke:#997711,color:#665500,stroke-width:2px,stroke-dasharray:6 2\nclass g group\nstyle g fill:#ffeebb\nstyle B fill:none,stroke:#aa5522,stroke-width:2px\nstyle C fill:#eeddee,stroke:#884488,color:#442244"],
  ["styles-edges", "flowchart TD\nA[Read] -->|next| B[Check]\nB o--o|open| C[Save]\nC x--x|stop| D[Done]\nlinkStyle 0 stroke:#bb3300,color:#663300,stroke-width:3px,stroke-dasharray:5 3\nlinkStyle 1 stroke:#228844,color:#114422,stroke-width:2px\nlinkStyle 2 stroke:#3344bb,color:#112266,stroke-width:2px"],
  ["styles-default-link", "flowchart LR\nsubgraph g[Work]\ndirection TB\nA[Read] -->|retry| A\nA --> B[Write]\nend\ng -->|done| C[Exit]\nlinkStyle default stroke:#226688,color:#114466,stroke-width:2px\nlinkStyle 2 stroke:#aa3355,stroke-width:3px,stroke-dasharray:4 2"],

  ['title-short','flowchart TD\nA[Read] --> B[Write]','Document pipeline'],
  ['title-wide','flowchart TD\nA[Read] --> B[Write]','Read and validate the source document before saving the updated output'],
  ['title-group','%%{init: {"flowchart": {"titleTopMargin": 40}}}%%\nflowchart LR\nsubgraph g[Storage]\ndirection TB\nA[Read] --> B[Write]\nend\ng --> C[Done]','Storage & recovery'],
  ['markers-ends','flowchart LR\nA[Read] --o|open| B[Check]\nB --x|stop| C[Done]'],
  ['markers-both','flowchart TD\nA[Read] o--o|open| B[Check]\nB x--x|stop| C[Save]\nC <--> D[Done]'],
  ['markers-group','flowchart RL\nsubgraph g[Operations]\ndirection BT\nA[Read] x--x B[Check]\nend\ng o--o|ready| C[Done]'],
  ['labels-wrap','flowchart TD\nA["Read the input and validate the document"] -->|"continue with the next operation"| B("Write the updated document to disk")'],
  ['labels-wrap-group','%%{init: {"flowchart": {"wrappingWidth": 150, "minNodeWidth": 0}}}%%\nflowchart LR\nsubgraph g["Input and output operations"]\ndirection TB\nA["Read the source document"] --> B["Write the output document"]\nend\ng --> C["All work completed"]'],
  ['labels-entities','flowchart LR\nA["Say #quot;hello#quot;"] -->|"A &amp; B"| B["Cost #36;5"]\nB --> C["x &lt; 5"]'],
  ['labels-lines','flowchart TD\nA["Read<br/>input"] -->|"next\\nstep"| B("Check\nresult")\nB --> C[("Save<br>record")]'],
  ['labels-group','flowchart LR\nsubgraph g["Read<br/>and write"]\ndirection TB\nA["Input #38; data"] --> B["Write<br />output"]\nend\ng -->|"all<br/>done"| C["End"]'],
  ['shapes-polygons','flowchart TD\nA{{Prepare}} --> B[/Read/]\nA --> C[\\Write\\]\nB --> D[/Collect\\]\nC --> E[\\Expand/]\nD --> F>Route]\nE --> F\nF --> G(((Done)))'],
  ['shapes-polygons-group','%%{init: {"flowchart": {"minNodeWidth": 0}}}%%\nflowchart LR\nsubgraph g[Steps]\ndirection BT\nA{{Init}} --> B[/In/] --> C[\\Out\\] --> D[/Join\\] --> E[\\Fork/] --> F>Next]\nend\ng --> G(((End)))'],
  ['shapes-down','flowchart TD\nA([Start]) -->|run| B[[Process]]\nA --> C[(Database)]\nB --> D([Finish])\nC --> D'],
  ['shapes-right','flowchart LR\nA([Start]) --> B[[Process]] --> C[(Database)]'],
  ['shapes-group','flowchart TD\nsubgraph g[Storage]\ndirection LR\nA([Begin]) --> B[[Save]] --> C[(Records)]\nC -->|retry| C\nend\ng --> D([End])'],
  ['chain','flowchart TD\nA[Start] --> B[Finish]'],
  ['decision','flowchart TD\nA[Read] --> B{Valid?}\nB -->|yes| C(Done)\nB -->|no| A'],
  ['horizontal','flowchart LR\nA((Input)) --> B[Process]\nA --> C[Store]\nB --> D(Output)\nC --> D'],
  ['reverse','flowchart RL\nA[First] -->|next| B[Second]'],
  ['upward','flowchart BT\nA[First] --> B[Second]'],
  ['parallel','flowchart TD\nA[Source] -->|one| B[Target]\nA -. two .-> B'],
  ['loop-down','flowchart TD\nA[Retry] -->|again| A\nA --> B[Done]'],
  ['loop-right','flowchart LR\nA((Retry)) -->|again| A\nA --> B[Done]'],
  ['loop-left','flowchart RL\nA[Retry] --> A\nA --> B[Done]'],
  ['loop-up','flowchart BT\nA{Retry?} -->|yes| A\nA --> B[Done]'],
  ['group-independent','flowchart TD\nsubgraph g[Group]\ndirection LR\nA[Read]-->B[Write]\nend\ng-->|done|C[Exit]'],
  ['group-incoming','flowchart LR\nC[Enter]-->g\nsubgraph g[Group]\ndirection BT\nA[Read]-->B[Write]\nend'],
  ['group-external','flowchart TD\nsubgraph g[Group]\ndirection LR\nA[Read]-->B[Write]\nend\nB-->C[Exit]\ng-->|done|D[Report]'],
  ['group-nested','flowchart TD\nsubgraph g[Outer]\ndirection LR\nsubgraph h[Inner]\ndirection BT\nA[Read]-->B[Write]\nend\nh-->C[Store]\nend\ng-->D[Exit]'],
  ['group-loop','flowchart LR\nsubgraph g[Group]\ndirection TB\nA[Retry]-->|again|A\nA-->B[Done]\nend\ng-->C[Exit]'],
  ['group-margins','%%{init: {"flowchart": {"subGraphTitleMargin": {"top": 6, "bottom": 4}}}}%%\nflowchart LR\nsubgraph g[Group title]\ndirection TB\nA[Read]-->|next|B[Write]\nend\ng-->|done|C[Exit]'],
  ['unicode','flowchart LR\nA[中文] --> B[Café 😀]'],
];
const url=pathToFileURL(resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs')).href;
assert.equal(JSON.parse(readFileSync('tools/mermaid-cpp/oracle/node_modules/mermaid/package.json')).version,'12.2.0');
const html=`<!doctype html><meta charset="utf-8"><body><script type="module">
const {default:mermaid}=await import(${JSON.stringify(url)});
try {
const result=[];
for(const [name,body,title] of ${JSON.stringify(cases)}) {
 const source=title?'---\\ntitle: '+JSON.stringify(title)+'\\n---\\n'+body:body;
 for(const theme of ['default','dark']) {
  mermaid.initialize({startOnLoad:false,theme,look:'classic',layout:'dagre',fontFamily:'Arial',themeVariables:{fontFamily:'Arial'},htmlLabels:!name.startsWith('svg-label-'),flowchart:{htmlLabels:!name.startsWith('svg-label-')}});
  const {svg}=await mermaid.render('probe'+result.length,source);
  const host=document.createElement('div');host.innerHTML=svg;document.body.append(host);
  const root=host.querySelector('svg');
  const position=(element,x=0,y=0)=>{
   const p=new DOMPoint(x,y).matrixTransform(root.getScreenCTM().inverse().multiply(element.getScreenCTM()));
   return {x:p.x,y:p.y};
  };
  const displayLines=label=>{
   const lines=[];const walker=document.createTreeWalker(label,NodeFilter.SHOW_TEXT);
   while(walker.nextNode()) {
    const node=walker.currentNode;
    for(let i=0;i<node.length;i++) {
     const range=document.createRange();range.setStart(node,i);range.setEnd(node,i+1);
     const rect=range.getBoundingClientRect();if(!rect.width&&!rect.height) continue;
     let line=lines.find(line=>Math.abs(line.y-rect.y)<1);
     if(!line) {line={y:rect.y,text:''};lines.push(line);}
     line.text+=node.data[i];
    }
   }
   return lines.map(line=>line.text.trimEnd());
  };
  const fontStyle=label=>{const s=getComputedStyle(label.querySelector('span')??label);return {family:s.fontFamily,size:s.fontSize,weight:s.fontWeight,style:s.fontStyle};};
  const styledCharacters=label=>{
   const chars=[],walker=document.createTreeWalker(label,NodeFilter.SHOW_TEXT);
   while(walker.nextNode()) {
    const n=walker.currentNode,s=getComputedStyle(n.parentElement);
    for(const c of n.textContent) if(!/\\s/.test(c)) chars.push([c,s.fontWeight,s.fontStyle]);
   }
   return chars;
  };
  const clusters=[...host.querySelectorAll('g.cluster')].map(n=>{
   const shape=n.querySelector('rect'),b=shape.getBBox(),label=n.querySelector('.cluster-label'),lb=label.getBBox();
   return {id:n.id,...position(shape,b.x+b.width/2,b.y+b.height/2),width:b.width,height:b.height,label:label.textContent,
    title:position(label,lb.x+lb.width/2,lb.y+lb.height/2),fill:getComputedStyle(shape).fill,stroke:getComputedStyle(shape).stroke,
    characters:styledCharacters(label),thickness:getComputedStyle(shape).strokeWidth,dash:getComputedStyle(shape).strokeDasharray,color:label.querySelector('text')?getComputedStyle(label.querySelector('text')).fill:getComputedStyle(label.querySelector('span')??label).color,font:fontStyle(label)};
  });
  const nodes=[...host.querySelectorAll('g.node')].map(n=>{
   const shape=n.querySelector('.label-container');const b=shape.getBBox();const label=n.querySelector('.label');
   const lb=label.getBBox();
   const paths=shape.matches('path,polygon,circle')?[shape]:[...shape.querySelectorAll('path,circle')];
   const outline=name.startsWith('shapes-')?paths.flatMap(p=>Array.from({length:201},(_,i)=>{
    const a=p.getPointAtLength(p.getTotalLength()*i/200);return position(p,a.x,a.y);
   })):[];
   const fillShape=paths.find(p=>getComputedStyle(p).fill!=='none')??shape;
   const strokeShape=paths.find(p=>getComputedStyle(p).stroke!=='none')??shape;
   return {id:n.id,...position(n),transform:n.getAttribute('transform'),width:b.width,height:b.height,label:label.textContent,labelWidth:lb.width,labelHeight:lb.height,
    labelCenter:position(label,lb.x+lb.width/2,lb.y+lb.height/2),lines:displayLines(label),characters:styledCharacters(label),outline,
    fill:getComputedStyle(fillShape).fill,stroke:getComputedStyle(strokeShape).stroke,
    thickness:getComputedStyle(strokeShape).strokeWidth,dash:getComputedStyle(strokeShape).strokeDasharray,color:label.querySelector('text')?getComputedStyle(label.querySelector('text')).fill:getComputedStyle(label.querySelector('span')??label).color,font:fontStyle(label)};
  });
  const edges=[...host.querySelectorAll('.flowchart-link')].map(p=>{
    const marker=side=>{
     const ref=p.getAttribute('marker-'+side);if(!ref) return null;
     const m=root.querySelector('[id="'+ref.slice(ref.lastIndexOf('#')+1,-1)+'"]');
     const shape=m.querySelector('path,circle'),style=getComputedStyle(shape);
     return {attributes:Object.fromEntries(['viewBox','refX','refY','markerUnits','markerWidth','markerHeight','orient'].map(key=>[key,m.getAttribute(key)])),
      fill:style.fill,stroke:style.stroke,width:style.strokeWidth,dash:style.strokeDasharray,
      points:Array.from({length:21},(_,i)=>{const a=shape.getPointAtLength(shape.getTotalLength()*i/20);return {x:a.x,y:a.y};})};
    };
    const length=p.getTotalLength();return {id:p.id,length,start:marker('start'),end:marker('end'),stroke:getComputedStyle(p).stroke,dash:getComputedStyle(p).strokeDasharray,thickness:getComputedStyle(p).strokeWidth,points:Array.from({length:7},(_,i)=>{const a=p.getPointAtLength(length*i/6);return position(p,a.x,a.y);})};
  });
  const labels=[...host.querySelectorAll('g.edgeLabel')].filter(n=>n.textContent.trim()).map(n=>{
    const b=n.getBBox();return {text:n.textContent,...position(n),transform:n.getAttribute('transform'),width:b.width,height:b.height,color:n.querySelector('text')?getComputedStyle(n.querySelector('text')).fill:getComputedStyle(n.querySelector('span')??n).color,font:fontStyle(n),lines:displayLines(n),characters:styledCharacters(n)};
  });
  const heading=root.querySelector('.flowchartTitleText');
  const diagramTitle=heading?{text:heading.textContent,...position(heading,heading.x.baseVal[0].value,heading.y.baseVal[0].value),
   width:heading.getBBox().width,height:heading.getBBox().height,fill:getComputedStyle(heading).fill,size:getComputedStyle(heading).fontSize}:null;
  const view=root.viewBox.baseVal;
  result.push({name:name+'-'+theme,source,theme,svg,nodes,edges,labels,clusters,diagramTitle,
   view:{x:view.x,y:view.y,width:view.width,height:view.height}});host.remove();
 }
}
document.body.replaceChildren(Object.assign(document.createElement('pre'),{id:'result',textContent:encodeURIComponent(JSON.stringify(result))}));
} catch(e) {document.body.innerHTML='<pre id="error"></pre>';document.querySelector('pre').textContent=String(e.stack);}
</script>`;
const file=resolve(directory,'probe.html');writeFileSync(file,html);
const browser=process.env.FOLIA_BROWSER??'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe';
const instance=await chromium.launch({executablePath:browser,headless:true,args:['--allow-file-access-from-files']});
let rows;
try {
 const page=await instance.newPage({viewport:{width:1600,height:1200}});
 await page.goto(pathToFileURL(file).href);
 await page.waitForSelector('pre#result,pre#error',{timeout:60000});
 const error=await page.locator('pre#error').count();assert.equal(error,0,error?await page.locator('pre#error').textContent():'');
 rows=JSON.parse(decodeURIComponent(await page.locator('pre#result').textContent()));
 assert.equal(rows.length,cases.length*2);
} finally {await instance.close();}
writeFileSync(resolve(directory,'original.json'),JSON.stringify(rows,null,2));
for(const row of rows) {assert(row.nodes.length>1);assert(row.edges.length>0);writeFileSync(resolve(directory,row.name+'.svg'),row.svg);}
console.log(`Measured ${rows.length} official Flowchart SVGs in ${directory}`);
