// Algorithm isolation at Mermaid's measured-layout boundary. These fixed boxes
// are input data, not fabricated font measurements or SVG acceptance evidence.
// Execute the original adapter, including subgraph options/policies and final
// frame tightening. No upstream option builder is reimplemented here.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync,writeFileSync,mkdirSync} from 'node:fs';
import {fileURLToPath} from 'node:url';
import {resolve,dirname} from 'node:path';
import {JSDOM} from './oracle/node_modules/jsdom/lib/api.js';

const root=fileURLToPath(new URL('../..',import.meta.url));
const lock=JSON.parse(readFileSync(resolve(root,'tools/mermaid-cpp/source-lock.json')));
const digest=bytes=>createHash('sha256').update(bytes).digest('hex');
assert.equal(lock.commit,'1fb7869866dd619168d840291c9c54b4f129758b');
const packageRoot=new URL('./oracle/node_modules/mermaid/',import.meta.url);
assert.equal(JSON.parse(readFileSync(new URL('package.json',packageRoot))).version,'12.2.0');
const bundle=new URL('dist/chunks/mermaid.esm/elk-V4PLJRSU.mjs',packageRoot);
const map=JSON.parse(readFileSync(new URL(bundle.href+'.map')));
for(let i=0;i<map.sources.length;++i){
  const name=map.sources[i];
  if(!name.includes('/layout-algorithms/elk/'))continue;
  const relative='packages/mermaid/'+name.slice(name.indexOf('src/'));
  const entry=lock.files.find(item=>item.path===relative);
  assert(entry,'Unlocked official adapter source '+relative);
  const source=readFileSync(resolve(root,'.reference/mermaid-js',relative));
  assert.equal(digest(source),entry.sha256,relative);
  assert.equal(map.sourcesContent[i].replaceAll('\r\n','\n'),source.toString().replaceAll('\r\n','\n'),relative);
}
const elkIndex=map.sources.findIndex(name=>name.endsWith('/elkjs/lib/elk.bundled.js'));
const elkLock=lock.elkCycle.files.find(file=>file.path==='lib/elk.bundled.js');
assert(elkIndex>=0&&elkLock,'Missing locked original ELK engine');
assert.equal(digest(map.sourcesContent[elkIndex]),elkLock.sha256,'Bundled ELK engine');
const instrumented=new URL('folia-elk-directed-scopes-oracle.mjs',bundle);
writeFileSync(instrumented,readFileSync(bundle,'utf8')+
  '\nexport {buildElkGraphFromLayoutData, runElkLayoutCore, getElkLayoutContext, calcOffset};\n');
// The real Flowchart parser sanitizes labels. Its DOM is used only for parsing,
// never to manufacture font metrics or geometric measurements.
const dom=new JSDOM('<!doctype html><body></body>');
globalThis.window=dom.window;
globalThis.document=dom.window.document;
globalThis.Option=dom.window.Option;
const {buildElkGraphFromLayoutData,runElkLayoutCore,getElkLayoutContext,calcOffset}=await import(instrumented.href);
const {default:mermaid}=await import(new URL('dist/mermaid.core.mjs',packageRoot));
mermaid.initialize({theme:'default',look:'classic'});
const configuration=mermaid.mermaidAPI.getConfig();
const log={debug(){},info(){},warn(){},error(...args){console.error(...args);}};

const group=(id,parentId,dir,width=146,height=24)=>({id,parentId:parentId||undefined,
  label:id+' measured title',isGroup:true,shape:'rect',dir:dir||undefined,padding:15,
  labelBBox:{width,height}});
const node=(id,parentId,width=74,height=38)=>({id,parentId:parentId||undefined,
  label:id,isGroup:false,shape:'rect',width,height});
const edge=(start,end,width=0,height=18,terminals)=>({start,end,label:width?'measured relation':'',
  width,height:width?height:0,arrowTypeStart:'none',arrowTypeEnd:'none',...(terminals??{})});
const diamond=(parent,prefix)=>({
  nodes:[node(prefix+'a',parent,74,38),node(prefix+'b',parent,108,46),
    node(prefix+'c',parent,86,58),node(prefix+'d',parent,96,42)],
  edges:[edge(prefix+'a',prefix+'b',43),edge(prefix+'a',prefix+'c'),
    edge(prefix+'b',prefix+'d'),edge(prefix+'c',prefix+'d',57,20)]
});
const cases=[];
const add=(id,direction,nodes,edges,features,elk={})=>{
  assert(!cases.some(item=>item.id===id));
  cases.push({id,direction,nodes,edges:edges.map((item,index)=>({id:'e'+index,...item})),features,elk});
};
for(const [direction,childDirection]of [['TB','LR'],['BT','RL'],['LR','TB'],['RL','BT']]){
  const inner=diamond('G','g');
  add('root-'+direction.toLowerCase()+'-child-'+childDirection.toLowerCase(),direction,
    [group('G','',childDirection),...inner.nodes,node('entry','',88,44),node('exit','',92,52)],
    [...inner.edges,edge('entry','G',61),edge('G','exit',49)],
    ['independent-direction','diamond','container-endpoints','center-labels']);
}
for(const [direction,outerDirection,innerDirection]of [['TB','LR','RL'],['LR','TB','BT']]){
  const inner=diamond('Inner','i');
  add('nested-opposite-'+direction.toLowerCase(),direction,
    [group('Outer','',outerDirection,188,30),group('Inner','Outer',innerDirection,126,24),
      ...inner.nodes,node('before','Outer',82,46),node('after','Outer',112,36),node('start',''),node('finish','')],
    [...inner.edges,edge('before','Inner',42),edge('Inner','after',53),edge('start','Outer'),edge('Outer','finish')],
    ['two-independent-scopes','opposite-directions','nested-container-endpoints','center-labels']);
}
{
  const left=diamond('Left','l'),right=diamond('Right','r');
  const nodes=[group('Left','','TB',200,28),group('Right','','BT',270,36),...left.nodes,...right.nodes,
    node('start','',80,32),node('finish','',100,60)];
  const edges=[...left.edges,...right.edges,edge('start','Left'),edge('Left','Right',69,24),edge('Right','finish')];
  add('siblings-boundary-endpoints','LR',nodes,edges,
    ['opposite-sibling-directions','three-boundary-edges','asymmetric-boxes','long-title']);
  add('siblings-cross-child-include','LR',nodes,[...edges,edge('ld','ra',93,20)],
    ['cross-child','lca-root','include-children-fallback','boundary-and-descendant-edges']);
}
{
  const inner=diamond('Inner','i');
  const nodes=[group('Outer','','TB',182,24),group('Inner','Outer','RL',174,30),...inner.nodes,
    node('sibling','Outer',116,62),node('external','',104,48)];
  add('nested-cross-child-include','LR',nodes,
    [...inner.edges,edge('id','sibling',74),edge('Outer','external',46)],
    ['nested-cross-child','lca-container','include-children-fallback','ancestor-policy-fallback','nested-center-labels']);
  add('container-to-descendant','BT',nodes,
    [...inner.edges,edge('Outer','ia',66),edge('id','sibling',38),edge('sibling','Inner'),edge('Outer','external')],
    ['container-descendant-endpoints','nested-feedback','include-children-fallback','upward-root']);
}
{
  const deep=diamond('Deep','d'),independent=diamond('Independent','u');
  add('deep-crossing-retains-independent-sibling','TB',
    [group('Outer','','LR',158,24),group('Middle','Outer','BT',192,30),group('Deep','Middle','RL',240,40),
      group('Independent','','LR',170,26),...deep.nodes,...independent.nodes,node('outside','',112,44)],
    [...deep.edges,...independent.edges,edge('dd','outside',81),edge('outside','Independent',55)],
    ['three-levels','cross-child-to-root','mixed-separate-and-include','unaffected-sibling-direction']);
}
{
  const inner=diamond('Loop','l');
  add('independent-feedback-cycle','TB',[group('Loop','','LR',210,28),...inner.nodes,node('outside','')],
    [...inner.edges,edge('ld','la',72),edge('lb','lc',35),edge('Loop','outside',51)],
    ['independent-scope','two-feedback-cycles','center-labels','boundary-exit']);
}
{
  const a=diamond('First','a'),b=diamond('Second','b');
  add('sibling-boundary-feedback','RL',[group('First','','BT',160,24),group('Second','','TB',224,32),...a.nodes,...b.nodes],
    [...a.edges,...b.edges,edge('First','Second',62),edge('Second','First',84)],
    ['sibling-scopes','opposite-directions','boundary-feedback','opposite-edge-labels']);
}
{
  const inner=diamond('Labeled','n');
  const terminals={arrowTypeStart:'arrow_point',arrowTypeEnd:'arrow_point',
    startLabelLeft:'one',startLabelRight:'many',endLabelLeft:'owner',endLabelRight:'items',
    terminalLabelSizes:{startLeft:{width:23,height:13},startRight:{width:34,height:17},
      endLeft:{width:41,height:15},endRight:{width:29,height:19}}};
  inner.edges[0]={...inner.edges[0],...terminals};
  inner.edges[3]={...inner.edges[3],startLabelRight:'from',endLabelLeft:'to',
    terminalLabelSizes:{startRight:{width:28,height:14},endLeft:{width:18,height:12}}};
  add('independent-center-and-terminal-labels','LR',[group('Labeled','','BT',248,34),...inner.nodes,node('outside','',124,56)],
    [...inner.edges,edge('Labeled','outside',103,26)],
    ['center-labels','all-four-terminal-slots','marker-clearance','mixed-terminal-boxes','independent-direction']);
}
{
  const inner=diamond('Inner','l');
  add('long-titles-with-small-children','BT',[group('Outer','','LR',620,60),group('Inner','Outer','TB',470,48),
    ...inner.nodes,node('side','Outer',54,30),node('outside','',98,42)],
    [...inner.edges,edge('side','Inner',28),edge('Outer','outside',44)],
    ['long-title-minimum-width','tall-title-strip','nested-independent-directions','small-children']);
}
{
  const a=diamond('A','a'),b=diamond('B','b');
  add('disconnected-directional-scopes','TB',[group('A','','RL',148,24),group('B','','BT',238,32),
    ...a.nodes,...b.nodes,node('solo1','',130,48),node('solo2','',104,56)],
    [...a.edges,...b.edges,edge('solo1','solo2',66)],
    ['disconnected-components','two-independent-scopes','different-directions','component-packing']);
  const nodes=[group('A','','LR',148,24),group('B','','BT',238,32),...a.nodes,...b.nodes];
  const edges=[...a.edges,...b.edges,edge('ad','ba',51),edge('bd','aa',73)];
  add('cross-hierarchy-feedback-oriented','TB',nodes,edges,
    ['cross-child-feedback','projected-cycle','include-children-fallback','center-labels']);
  add('cross-hierarchy-feedback-unoriented','TB',nodes,edges,
    ['cross-child-feedback','preserved-edge-direction','include-children-fallback','center-labels'],{orientFeedbackEdges:false});
}
{
  const source='flowchart TB\nsubgraph Empty[Long title]\ndirection LR\nend\nEmpty --> outside';
  const diagram=await mermaid.mermaidAPI.getDiagramFromText(source);
  const parsed=diagram.db.getData();
  const parsedNodes=structuredClone(parsed.nodes);
  for(const node of parsed.nodes){
    if(node.isGroup)node.labelBBox={width:250,height:32};
    else Object.assign(node,{width:120,height:46});
  }
  add('empty-directional-group',diagram.db.getDirection(),parsed.nodes,
    parsed.edges.map(edge=>({...edge,width:0,height:0})),
    ['official-parser-record','empty-directional-group','boundary-endpoint','group-label-without-container-options']);
  Object.assign(cases.at(-1),{source,parsedNodes});
}
{
  const source=`flowchart LR
subgraph Outer[Outer title]
  direction BT
  subgraph Empty[Vacant module]
    direction LR
  end
  subgraph Sibling[Worker group]
    direction RL
    a --> b
    a --> c
    b --> d
    c -->|result| d
  end
  Empty --> handoff
  handoff --> Sibling
end
entry --> Outer
Outer --> outside`;
  const diagram=await mermaid.mermaidAPI.getDiagramFromText(source);
  const parsed=diagram.db.getData();
  const parsedNodes=structuredClone(parsed.nodes);
  const labels={Outer:{width:190,height:28},Empty:{width:250,height:32},Sibling:{width:174,height:26}};
  const boxes={a:[82,38],b:[108,46],c:[86,58],d:[96,42],handoff:[94,44],entry:[88,40],outside:[120,46]};
  for(const node of parsed.nodes){
    if(node.isGroup){assert(labels[node.id]);node.labelBBox=labels[node.id];}
    else {assert(boxes[node.id]);[node.width,node.height]=boxes[node.id];}
  }
  add('nested-empty-group-and-independent-sibling',diagram.db.getDirection(),parsed.nodes,
    parsed.edges.map(edge=>({...edge,width:edge.label?57:0,height:edge.label?20:0})),
    ['official-parser-record','nested-empty-group','independent-sibling-direction','diamond',
      'boundary-endpoints','zero-height-child-frame-filter','center-label']);
  Object.assign(cases.at(-1),{source,parsedNodes});
}
{
  const a=diamond('Components','p'),b=diamond('Components','q');
  const dimensions=[[162,28],[68,74],[130,40],[84,92]];
  b.nodes.forEach((node,index)=>{[node.width,node.height]=dimensions[index];});
  add('disconnected-components-inside-directional-scope','TB',
    [group('Components','','LR',220,30),...a.nodes,...b.nodes,node('solo','Components',43,24),
      node('entry','',80,46),node('exit','',112,54)],
    [...a.edges,...b.edges,edge('entry','Components',67),edge('Components','exit',93)],
    ['disconnected-components','inside-directional-scope','two-asymmetric-diamonds','isolated-child',
      'component-packing','group-boundary-endpoints','center-labels']);
}
assert.equal(cases.length,21);

const rows=['# Mermaid 12.2.0 original ELK adapter: name, UTF-16 JSON input, UTF-16 JSON expected.',
  '# Fixed measured boxes isolate layout. This is not typography, browser, or SVG acceptance.',
  '# Nodes include official post-layout frame tightening. Paths/labels are the raw ELK scene before paint clipping.'];
const observations=[];
const hex=text=>Buffer.from(text,'utf16le').swap16().toString('hex');
for(const item of cases){
  const config={...configuration,elk:{...configuration.elk,...item.elk}};
  const context={options:{algorithm:'elk.layered'},helpers:{getConfig:()=>config,log}};
  const data={direction:item.direction,config,nodes:structuredClone(item.nodes),edges:structuredClone(item.edges)};
  const prepared=buildElkGraphFromLayoutData({...data,nodes:structuredClone(data.nodes),
    edges:structuredClone(data.edges)},getElkLayoutContext(context));
  const policies=Object.entries(prepared.nodeDb).filter(([,node])=>node.isGroup).map(([id,node])=>
    ({id,hierarchy:node.layoutOptions?.['elk.hierarchyHandling']??'INCLUDE_CHILDREN',
      direction:node.layoutOptions?.['elk.direction']??'inherited'}));
  if(item.features.includes('empty-directional-group')){
    assert(policies.every(node=>node.hierarchy==='INCLUDE_CHILDREN'),item.id+' no independent empty scope');
    assert.equal(prepared.nodeDb.Empty.layoutOptions,undefined,item.id+' no container options');
  }else assert(policies.some(node=>item.features.includes('include-children-fallback')?
    node.hierarchy==='INCLUDE_CHILDREN':node.hierarchy==='SEPARATE_CHILDREN'),item.id+' policy coverage');
  if(item.features.includes('nested-empty-group'))
    assert.equal(prepared.nodeDb.Empty.layoutOptions,undefined,item.id+' no empty child container options');
  const ends=[];
  for(let e=0;e<prepared.elkGraph.edges.length;++e)
    for(const label of prepared.elkGraph.edges[e].labels??[])
      if(label.terminal)ends.push({id:ends.length,edge:e,head:label.terminal.startsWith('end'),
        slot:label.terminal,width:label.width,height:label.height});
  const result=await runElkLayoutCore(data,context);
  const byId=new Map(),edges=new Map();
  const collect=graph=>{
    byId.set(graph.id,graph);
    for(const edge of graph.edges??[])edges.set(edge.id,edge);
    for(const child of graph.children??[])collect(child);
  };collect(result);
  assert.equal(edges.size,item.edges.length,item.id+' all routes');
  const nodeDb=Object.fromEntries(byId);
  const expected={bounds:[result.width,result.height],nodes:item.nodes.map(node=>{
    const placed=data.nodes.find(candidate=>candidate.id===node.id);
    assert(placed&&[placed.x,placed.y,placed.width,placed.height].every(Number.isFinite),item.id+' node '+node.id);
    return [node.id,placed.x,placed.y,placed.width,placed.height];
  }),routes:[],centers:[],ends:[],policies};
  for(let index=0;index<item.edges.length;++index){
    const input=item.edges[index],edge=edges.get(input.id),offset=calcOffset(input.start,input.end,prepared.parentLookupDb,nodeDb);
    assert.equal(edge.sections?.length,1,item.id+' section '+input.id);
    const section=edge.sections[0],point=p=>[p.x+offset.x,p.y+offset.y];
    const route=[section.startPoint,...section.bendPoints??[],section.endPoint].map(point);
    if(edge.layoutReversed)route.reverse();
    expected.routes.push(route);
    if(input.label){const label=edge.labels[0];expected.centers.push([index,...point({x:label.x+label.width/2,y:label.y+label.height/2})]);}
    for(const inputLabel of ends.filter(label=>label.edge===index)){
      const label=edge.labels.find(candidate=>candidate.terminal===inputLabel.slot);
      assert(label&&Number.isFinite(label.x)&&Number.isFinite(label.y),item.id+' terminal '+inputLabel.slot);
      expected.ends.push([inputLabel.id,index,...point(label),label.width,label.height]);
    }
  }
  const input={direction:item.direction,config:{elk:config.elk},features:item.features,
    nodes:item.nodes.map(node=>({id:node.id,parent:node.parentId??'',direction:node.dir??'',group:node.isGroup,
      shape:node.shape,padding:node.padding??15,width:node.width??0,height:node.height??0,
      labelWidth:node.labelBBox?.width??0,labelHeight:node.labelBBox?.height??0})),
    edges:item.edges.map(edge=>({id:edge.id,start:edge.start,end:edge.end,label:edge.label,width:edge.width,height:edge.height})),ends};
  rows.push([item.id,hex(JSON.stringify(input)),hex(JSON.stringify(expected))].join('\t'));
  observations.push({id:item.id,features:item.features,source:item.source,parsedNodes:item.parsedNodes,
    input,expected,adapterNodes:data.nodes,adapterEdges:data.edges});
}
dom.window.close();
const fixture=resolve(root,'tests/mermaid_cpp/fixtures/elk-directed-scopes.tsv');
const generated=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,generated);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),generated,'Official directed-scope fixture changed');
const output=resolve(root,'build/mermaid-regression/elk-directed-scopes-official.json');
mkdirSync(dirname(output),{recursive:true});
writeFileSync(output,JSON.stringify({upstream:{version:'12.2.0',commit:lock.commit},
  stage:'fixed-box-algorithm',bundleSha256:digest(readFileSync(bundle)),fontMeasured:false,
  svgCompared:false,cases:observations},null,2)+'\n');
console.log(JSON.stringify({cases:cases.length,nodes:observations.reduce((n,item)=>n+item.input.nodes.length,0),
  edges:observations.reduce((n,item)=>n+item.input.edges.length,0),fixture,output,svgCompared:false}));
