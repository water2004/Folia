// Full pinned ELK API with the layered options used by Mermaid's default adapter.
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
const ELK=require('elkjs/lib/elk.bundled.js');const elk=new ELK();
const cases=[
 ['right',[[80,38],[70,42]],[[0,1]],[[0,36,14]],[],0],
 ['left',[[80,38],[70,42]],[[0,1]],[[0,36,14]],[],1],
 ['down',[[80,38],[70,42]],[[0,1]],[[0,36,14]],[],2],
 ['up',[[80,38],[70,42]],[[0,1]],[[0,36,14]],[],3],
 ['class-ends',[[80,38],[70,42]],[[0,1]],[[0,36,14]],[[0,0,24,14],[0,1,18,12]],0],
 ['class-down',[[80,38],[70,42]],[[0,1]],[[0,36,14]],[[0,0,24,14],[0,1,18,12]],2],
 ['feedback-long',[[80,38],[70,42],[60,36],[90,40]],[[0,1],[1,2],[2,3],[3,0]],[[3,36,14]],[],0],
 ['labelled-diamond',[[80,38],[70,42],[60,36],[90,40]],[[0,1],[0,2],[1,3],[2,3]],[[0,30,14],[1,20,12],[2,0,0],[3,28,12]],[],0],
];
const encode=a=>a.map(v=>v.join(':')).join(',')||'-';
const point=p=>[p.x,p.y].join(',');
const rows=['# Full ELK adapter layout; name,node sizes,edges,center labels,end labels,direction,bounds,node boxes,centerXY,endXY,paths(start|end|bends).'];
for(const [name,nodes,edges,centers,ends,direction] of cases) {
 const graph={id:'root',layoutOptions:{'elk.algorithm':'layered','elk.hierarchyHandling':'INCLUDE_CHILDREN',
  'elk.direction':['RIGHT','LEFT','DOWN','UP'][direction],'elk.layered.cycleBreaking.strategy':'DEPTH_FIRST',
  'elk.layered.nodePlacement.strategy':'BRANDES_KOEPF','elk.layered.nodePlacement.bk.fixedAlignment':'BALANCED',
  'elk.layered.unnecessaryBendpoints':true,'elk.spacing.portsSurrounding':'[top=12,left=12,bottom=12,right=12]'},
  children:nodes.map(([width,height],i)=>({id:'n'+i,width,height})),
  edges:edges.map(([s,t],i)=>({id:'e'+i,sources:['n'+s],targets:['n'+t],labels:[
   ...centers.flatMap(([e,width,height],j)=>e===i?[{id:'c'+j,text:'center',width,height,layoutOptions:{'edgeLabels.placement':'CENTER','edgeLabels.inline':true}}]:[]),
   ...ends.flatMap(([e,head,width,height],j)=>e===i?[{id:'t'+j,text:'terminal',width,height,layoutOptions:{'edgeLabels.placement':head?'HEAD':'TAIL'}}]:[])]}))};
 const result=await elk.layout(graph);
 const labels=result.edges.flatMap(e=>e.labels??[]);
 const centersOut=centers.map((_,i)=>point(labels.find(l=>l.id==='c'+i))).join(';')||'-';
 const endsOut=ends.map((_,i)=>point(labels.find(l=>l.id==='t'+i))).join(';')||'-';
 const paths=result.edges.map(e=>{assert.equal(e.sections.length,1);const s=e.sections[0];return [point(s.startPoint),point(s.endPoint),(s.bendPoints??[]).map(point).join(':')||'-'].join('|');}).join(';');
 rows.push([name,encode(nodes),encode(edges),encode(centers),encode(ends),direction,[result.width,result.height].join(','),
  result.children.map(n=>[n.x,n.y,n.width,n.height].join(',')).join(';'),centersOut,endsOut,paths].join('\t'));
}
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-layout.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK full flat layout: ${cases.length} original adapter-direction/label/path cases checked.`);
