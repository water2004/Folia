// Full pinned ELK layouts at the measured-box boundary. Node/label dimensions
// are explicit algorithm inputs; these fixtures do not claim font/SVG parity.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync,writeFileSync,realpathSync} from 'node:fs';
import {createRequire} from 'node:module';
import path from 'node:path';

const require=createRequire(realpathSync(new URL('./oracle/node_modules/mermaid/package.json',import.meta.url)));
const base=path.dirname(require.resolve('elkjs/package.json'));
const lock=JSON.parse(readFileSync(new URL('./source-lock.json',import.meta.url)));
assert.equal(JSON.parse(readFileSync(path.join(base,'package.json'))).version,'0.9.3');
for(const file of lock.elkCycle.files)
  assert.equal(createHash('sha256').update(readFileSync(path.join(base,file.path))).digest('hex'),file.sha256,file.path);
const ELK=require('elkjs/lib/elk.bundled.js'),elk=new ELK();
const directions=['RIGHT','LEFT','DOWN','UP'];
const distributions=['NORTH','NORTH_SOUTH','EQUALLY'];
const orderings=['STACKED','REVERSE_STACKED','SEQUENCED'];
const node=(width=96,height=56,distribution=0,ordering=0,parent=-1)=>
  [parent,0,width,height,distribution,ordering];
const group=(parent=-1,width=0,height=0,distribution=0,ordering=0)=>
  [parent,1,width,height,distribution,ordering];
const loops=(owner,count)=>Array.from({length:count},()=>[owner,owner]);
const labels=(count,start=0)=>Array.from({length:count},(_,i)=>[start+i,31+13*(i%4),12+3*(i%3)]);
const cases=[];
const add=(name,nodes,edges,centers=[],ends=[],direction=0,spacing=[10,10,2,0])=>
  cases.push({name,nodes,edges,centers,ends,direction,spacing});

add('one-north-loop',[node()],loops(0,1));
for(let ordering=0;ordering<orderings.length;++ordering)
  add('north-three-'+orderings[ordering].toLowerCase(),[node(118,62,0,ordering)],loops(0,3),labels(3));
add('equally-eight-sides-and-corners',[node(144,86,2)],loops(0,8),labels(8));
add('equally-twelve-stacked',[node(172,94,2)],loops(0,12),labels(12));
add('equally-twelve-sequenced',[node(172,94,2,2)],loops(0,12),labels(12));
add('north-south-seven',[node(154,72,1)],loops(0,7),labels(7));
add('north-south-nine-sequenced',[node(154,72,1,2)],loops(0,9),labels(9));
for(let direction=0;direction<directions.length;++direction){
  add('mixed-chain-'+directions[direction].toLowerCase(),
    [node(82,38),node(132,76,2),node(108,46)],
    [[0,1],[1,2],...loops(1,9)],[[0,51,18],[1,73,22],...labels(9,2)],
    [[0,0,24,14],[1,1,32,16],[4,0,39,17],[4,1,27,13]],direction);
}
add('branching-feedback-with-node-loops',
  [node(92,44,1),node(126,68,2),node(78,54,0,2),node(104,62)],
  [[0,1],[0,2],[1,3],[2,3],[3,0],...loops(0,3),...loops(1,5),...loops(2,2)],
  [[0,48,16],[3,61,20],[4,82,24],...labels(10,5)],[[4,0,37,13],[4,1,29,17]]);
add('labels-center-head-tail-on-one-loop',[node(84,42)],loops(0,2),
  [[0,117,24],[1,69,16]],[[0,0,48,18],[0,1,57,20],[1,0,25,12],[1,1,33,14]]);
add('long-loop-labels-custom-spacing',[node(52,28,1,1)],loops(0,4),
  [[0,227,30],[1,164,42],[2,193,26],[3,85,18]],[[0,0,62,22],[3,1,57,24]],0,[18,14,4,3]);
for(let direction=0;direction<directions.length;++direction){
  add('compound-leaf-loops-'+directions[direction].toLowerCase(),
    [group(),node(84,42,0,0,0),node(126,64,2,0,0),node(92,54)],
    [[3,1],[1,2],[2,3],...loops(1,2),...loops(2,9)],
    [[0,64,18],[1,43,16],[2,76,22],...labels(11,3)],[[0,0,27,13],[2,1,35,17]],direction);
}
add('group-loop-without-external-ports',
  [group(),node(84,42,0,0,0),node(126,64,0,0,0)],[[1,2],[0,0]],
  [[0,44,16],[1,98,22]]);
add('group-loops-with-boundary-endpoints',
  [group(),node(84,42,0,0,0),node(126,64,0,0,0),node(78,46),node(112,58)],
  [[1,2],[3,0],[0,4],...loops(0,3)],[[0,44,16],[1,61,18],[2,79,20],...labels(3,3)],
  [[1,0,31,13],[2,1,49,17],[3,0,37,15],[3,1,53,19]]);
add('nested-group-and-leaf-loops',
  [group(),group(0,0,0,1),node(86,48,2,0,1),node(102,62,0,0,1),node(78,40,0,0,0),node(118,58)],
  [[5,0],[0,5],[1,4],[2,3],[3,4],...loops(0,2),...loops(1,3),...loops(2,9)],
  [[0,71,18],[1,49,22],[2,62,16],[4,87,20],...labels(14,5)],[[2,1,42,15]],2);
add('minimum-sized-group-with-equally-loops',
  [group(-1,410,240,2),node(62,34,0,0,0),node(88,46,0,0,0),node(104,54)],
  [[1,2],[3,0],[0,3],...loops(0,10)],[[0,45,14],[1,69,20],[2,57,18],...labels(10,3)],[],1);
assert.equal(cases.length,24);

const common={
  'elk.algorithm':'layered','elk.hierarchyHandling':'INCLUDE_CHILDREN',
  'elk.layered.cycleBreaking.strategy':'DEPTH_FIRST',
  'elk.layered.nodePlacement.strategy':'BRANDES_KOEPF',
  'elk.layered.nodePlacement.bk.fixedAlignment':'BALANCED',
  'elk.layered.unnecessaryBendpoints':true,
  'elk.spacing.portsSurrounding':'[top=12,left=12,bottom=12,right=12]'
};
const encode=rows=>rows.map(row=>row.join(':')).join(',')||'-';
const rows=[
  '# ELK 0.9.3 complete self-loop geometry. Explicit boxes isolate algorithms, not font/SVG acceptance.',
  '# name,direction,nodes(parent/group/width/height/distribution/ordering),edges,centers,ends,spacing(nodeLoop/edgeEdge/edgeLabel/labelLabel),bounds,boxes,centerXY,endXY,paths.',
  '# Distribution and ordering are node options, not inherited root options. Directions RIGHT LEFT DOWN UP; distributions NORTH NORTH_SOUTH EQUALLY; ordering STACKED REVERSE_STACKED SEQUENCED.'
];
let nodeCount=0,edgeCount=0;
for(const item of cases){
  const {name,nodes:specs,edges,centers,ends,direction,spacing}=item;
  const options={...common,'elk.direction':directions[direction],
    'elk.spacing.nodeSelfLoop':spacing[0],'elk.spacing.edgeEdge':spacing[1],
    'elk.spacing.edgeLabel':spacing[2],'elk.spacing.labelLabel':spacing[3]};
  const root={id:'root',layoutOptions:options,children:[],edges:[]};
  const nodes=specs.map(([parent,isGroup,width,height,distribution,ordering],i)=>({
    id:'n'+i,width,height,...(isGroup?{children:[]}:{}),layoutOptions:{
      ...(isGroup?{...options,'elk.nodeSize.constraints':'MINIMUM_SIZE',
        'elk.nodeSize.minimum':`(${width},${height})`,'elk.contentAlignment':'H_CENTER V_TOP'}:{}),
      'elk.layered.edgeRouting.selfLoopDistribution':distributions[distribution],
      'elk.layered.edgeRouting.selfLoopOrdering':orderings[ordering]
    }
  }));
  specs.forEach(([parent],i)=>(parent<0?root:nodes[parent]).children.push(nodes[i]));
  root.edges=edges.map(([source,target],e)=>({id:'e'+e,sources:['n'+source],targets:['n'+target],labels:[
    ...centers.flatMap(([edge,width,height],i)=>edge===e?[{id:'c'+i,text:'center',width,height,
      layoutOptions:{'edgeLabels.placement':'CENTER','edgeLabels.inline':true}}]:[]),
    ...ends.flatMap(([edge,head,width,height],i)=>edge===e?[{id:'t'+i,text:'terminal',width,height,
      layoutOptions:{'edgeLabels.placement':head?'HEAD':'TAIL'}}]:[])
  ]}));
  let result;
  try{result=await elk.layout(root);}catch(error){throw new Error(name+': '+error.message,{cause:error});}
  const origins=new Map([['root',[0,0]]]),boxes=new Map(),allEdges=[];
  const visit=(owner,x,y)=>{
    for(const child of owner.children??[]){
      const a=x+child.x,b=y+child.y;origins.set(child.id,[a,b]);
      boxes.set(child.id,[a,b,child.width,child.height]);visit(child,a,b);
    }
    for(const edge of owner.edges??[])allEdges.push([edge,owner.id]);
  };
  visit(result,0,0);
  const paths=[],centerOut=[],endOut=[];
  for(const [edge,owner]of allEdges){
    const origin=origins.get(edge.container??owner);
    assert(origin,name+' edge container');
    const point=p=>[p.x+origin[0],p.y+origin[1]].join(',');
    assert.equal(edge.sections?.length,1,name+' '+edge.id+' sections');
    const section=edge.sections[0],e=Number(edge.id.slice(1));
    paths[e]=[point(section.startPoint),point(section.endPoint),
      (section.bendPoints??[]).map(point).join(':')||'-'].join('|');
    for(const label of edge.labels??[])
      (label.id[0]==='c'?centerOut:endOut)[Number(label.id.slice(1))]=point(label);
  }
  assert.equal(paths.filter(Boolean).length,edges.length,name+' all edges');
  assert.equal(centerOut.filter(Boolean).length,centers.length,name+' all center labels');
  assert.equal(endOut.filter(Boolean).length,ends.length,name+' all terminal labels');
  rows.push([name,direction,encode(specs),encode(edges),encode(centers),encode(ends),spacing.join(','),
    [result.width,result.height].join(','),specs.map((_,i)=>boxes.get('n'+i).join(',')).join(';'),
    centerOut.join(';')||'-',endOut.join(';')||'-',paths.join(';')].join('\t'));
  nodeCount+=specs.length;edgeCount+=edges.length;
}
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-self-loops.tsv',import.meta.url);
const expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK self loops: ${cases.length} full original layouts, ${nodeCount} nodes, ${edgeCount} edges checked.`);
