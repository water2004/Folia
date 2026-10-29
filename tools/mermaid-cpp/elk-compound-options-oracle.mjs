// Full original API: same-direction nested groups, crossings and measured labels.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync,writeFileSync,realpathSync} from 'node:fs';
import {createRequire} from 'node:module';
import path from 'node:path';
const require=createRequire(realpathSync(new URL('./oracle/node_modules/mermaid/package.json',import.meta.url)));
const base=path.dirname(require.resolve('elkjs/package.json'));
const lock=JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json',import.meta.url)));
for(const file of lock.elkCycle.files)
 assert.equal(createHash('sha256').update(readFileSync(path.join(base,file.path))).digest('hex'),file.sha256,file.path);
const ELK=require('elkjs/lib/elk.bundled.js'),elk=new ELK();
const cases=[
 ['root-right-child-down',[[-1,1,0,0,2],[-1,0,60,30,0],[-1,0,70,40,0],[0,0,50,30,0],[0,0,60,40,0]],[[1,3],[3,4],[4,2]],[],[],0,[]],
 ['root-down-child-right',[[-1,1,0,0,0],[-1,0,60,30,0],[-1,0,70,40,0],[0,0,50,30,0],[0,0,60,40,0]],[[1,3],[1,4],[3,2],[4,2]],[],[],2,[]],
];
const options={'spacing.baseValue':40,'elk.algorithm':'layered','elk.hierarchyHandling':'INCLUDE_CHILDREN','elk.direction':'RIGHT',
 'elk.layered.cycleBreaking.strategy':'DEPTH_FIRST','elk.layered.nodePlacement.strategy':'BRANDES_KOEPF',
 'elk.layered.nodePlacement.bk.fixedAlignment':'BALANCED','elk.layered.unnecessaryBendpoints':true,
 'elk.spacing.portsSurrounding':'[top=12,left=12,bottom=12,right=12]'};
const encode=a=>a.map(v=>v.join(':')).join(',')||'-';
const rows=['# ELK INCLUDE_CHILDREN inherited direction and adapter per-group spacing; name,nodes,edges,centers,ends,direction,group boxes,bounds,boxes,centers,ends,paths,titles.'];
for(const [name,specs,endpoints,centers,ends,direction,groupBoxes]of cases) {
 options['elk.direction']=['RIGHT','LEFT','DOWN','UP'][direction];
 const root={id:'root',layoutOptions:{...options},children:[],edges:[]};
 const nodes=specs.map(([parent,group,width,height],i)=>({id:'n'+i,width,height,...(group?{children:[],layoutOptions:{...options,
  'elk.nodeSize.constraints':'MINIMUM_SIZE','elk.nodeSize.minimum':`(${width},${height})`,'elk.contentAlignment':'H_CENTER V_TOP','elk.direction':['RIGHT','LEFT','DOWN','UP'][specs[i][4]],
  'spacing.baseValue':24,'elk.spacing.nodeNode':50,'elk.spacing.edgeEdge':20,
  'elk.layered.spacing.edgeNodeBetweenLayers':30,'elk.padding':'[top=24,left=24,bottom=24,right=24]'}}:{})}));
 for(const [n,top,left,bottom,right,width,height]of groupBoxes) {
  nodes[n].layoutOptions['elk.padding']=`[top=${top},left=${left},bottom=${bottom},right=${right}]`;
  nodes[n].layoutOptions['elk.nodeSize.constraints']='[MINIMUM_SIZE, NODE_LABELS]';
  nodes[n].labels=[{id:'title'+n,text:'title',width,height,layoutOptions:{'elk.nodeLabels.placement':'[INSIDE,V_TOP,H_CENTER]'}}];
 }
 specs.forEach(([parent],i)=>(parent<0?root:nodes[parent]).children.push(nodes[i]));
 root.edges=endpoints.map(([s,t],e)=>({id:'e'+e,sources:['n'+s],targets:['n'+t],labels:[
  ...centers.flatMap(([edge,width,height],i)=>edge===e?[{id:'c'+i,text:'label',width,height,layoutOptions:{'edgeLabels.placement':'CENTER','edgeLabels.inline':true}}]:[]),
  ...ends.flatMap(([edge,head,width,height],i)=>edge===e?[{id:'t'+i,text:'end',width,height,layoutOptions:{'edgeLabels.placement':head?'HEAD':'TAIL'}}]:[])]}));
 const result=await elk.layout(root),boxes=new Map(),origins=new Map([['root',[0,0]]]),edges=[],titles=[];
 const visit=(node,x,y)=>{for(const label of node.labels??[])titles.push([Number(node.id.slice(1)),x+label.x,y+label.y,label.width,label.height]);for(const child of node.children??[]){const a=x+child.x,b=y+child.y;origins.set(child.id,[a,b]);boxes.set(child.id,[a,b,child.width,child.height]);visit(child,a,b);}for(const edge of node.edges??[])edges.push([edge,node.id]);};visit(result,0,0);
 const paths=[],centerOut=[],endOut=[];
 for(const [edge,owner]of edges) {
  const origin=origins.get(edge.container??owner),point=p=>[p.x+origin[0],p.y+origin[1]].join(',');
  assert.equal(edge.sections.length,1);const section=edge.sections[0],e=Number(edge.id.slice(1));
  paths[e]=[point(section.startPoint),point(section.endPoint),(section.bendPoints??[]).map(point).join(':')||'-'].join('|');
  for(const l of edge.labels??[])(l.id[0]==='c'?centerOut:endOut)[Number(l.id.slice(1))]=point(l);
 }
 rows.push([name,encode(specs),encode(endpoints),encode(centers),encode(ends),direction,encode(groupBoxes),[result.width,result.height].join(','),
  specs.map((_,i)=>boxes.get('n'+i).join(',')).join(';'),centerOut.join(';')||'-',endOut.join(';')||'-',paths.join(';'),titles.map(t=>t.join(',')).join(';')||'-'].join('\t'));
}
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-compound-options.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK inherited direction/per-group spacing: ${cases.length} original full geometry cases checked.`);

