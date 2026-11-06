import {readFileSync,writeFileSync,realpathSync} from 'node:fs';
import {createRequire} from 'node:module';
import {createHash} from 'node:crypto';
import path from 'node:path';
import assert from 'node:assert/strict';
const require=createRequire(realpathSync(new URL('./oracle/node_modules/mermaid/package.json',import.meta.url)));
const lock=JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json',import.meta.url)));
const base=path.dirname(require.resolve('elkjs/package.json'));
for(const f of lock.elkCycle.files)assert.equal(createHash('sha256').update(readFileSync(path.join(base,f.path))).digest('hex'),f.sha256);
const ELK=require('elkjs/lib/elk.bundled.js'),elk=new ELK();
const observations=JSON.parse(readFileSync('build/mermaid-cpp/elk-flowchart-browser/original.json'));
const rows=['# Original measured default Flowchart ELK scenes: name,source hex,nodes(id,width,height,labelWidth,labelHeight,x,y,paintWidth,paintHeight),labels,raw routes,raw label centers.'];
for(const r of observations.filter(r=>r.theme==='default')) {
 const dir=/flowchart (\w+)/.exec(r.source)[1];
 const options={'elk.algorithm':'layered','elk.hierarchyHandling':'INCLUDE_CHILDREN','elk.direction':{LR:'RIGHT',RL:'LEFT',TD:'DOWN',TB:'DOWN',BT:'UP'}[dir],
 'spacing.baseValue':40,'elk.layered.cycleBreaking.strategy':'DEPTH_FIRST','elk.layered.nodePlacement.strategy':'BRANDES_KOEPF','elk.layered.nodePlacement.bk.fixedAlignment':'BALANCED',
 'elk.layered.considerModelOrder.strategy':'NODES_AND_EDGES','elk.layered.unnecessaryBendpoints':true,'elk.spacing.portsSurrounding':'[top=12,left=12,bottom=12,right=12]'};
 const groups=r.clusters.map(c=>{const id=c.id.replace(/^probe\d+-/,'');const start=r.svg.indexOf(c.id);const m=/<foreignObject[^>]*width="([^"]+)"[^>]*height="([^"]+)"/.exec(r.svg.slice(start));assert(m,id);
  return {id,width:+m[1],height:+m[2],observed:c};});
 const normals=r.nodes.map(n=>({...n,id:n.id.replace(/^probe\d+-flowchart-/,'').replace(/-\d+$/,'')}));
 const root={id:'root',layoutOptions:options,children:[],edges:[]};
 const all=new Map();
 for(const g of groups){const node={id:g.id,children:[],labels:[{text:g.id,width:g.width,height:Math.max(0,g.height-2)}],layoutOptions:{...options,'elk.layered.considerModelOrder.strategy':'NONE','spacing.baseValue':24,'spacing.nodeNode':50,'elk.spacing.edgeEdge':20,'elk.layered.spacing.edgeNodeBetweenLayers':30,'elk.padding':'[top=24,left=24,bottom=24,right=24]','nodeLabels.placement':'[H_CENTER V_TOP, INSIDE]','nodeSize.constraints':'[MINIMUM_SIZE,NODE_LABELS]','nodeSize.minimum':`(${g.width+15},0)`}};all.set(g.id,node);root.children.push(node);}
 let group=null;const parents=new Map();for(const line of r.source.split('\n')){if(line.startsWith('subgraph '))group=/subgraph (\w+)/.exec(line)[1];else if(line==='end')group=null;else if(group)for(const n of normals)if(new RegExp('\\b'+n.id+'[\\[({]').test(line))parents.set(n.id,group);}
 for(const n of normals){const node={id:n.id,width:n.width,height:n.height};all.set(n.id,node);(parents.has(n.id)?all.get(parents.get(n.id)):root).children.push(node);}
 const edges=r.edges.map(e=>{const m=/-L_([^_]+)_([^_]+)_/.exec(e.id);assert(m);return [m[1],m[2]];});
 let labelIndex=0;const labelSpecs=edges.map(([s,t],i)=>{const line=r.source.split('\n').find(l=>new RegExp('\\b'+s+'(?:[^\\n]*?)-->').test(l)&&new RegExp('\\b'+t+'(?:[\\[({]|$)').test(l));return line?.includes('|')?r.labels[labelIndex++]:null;});
 root.edges=edges.map(([s,t],i)=>({id:'e'+i,sources:[s],targets:[t],labels:labelSpecs[i]?[{id:'l'+i,text:labelSpecs[i].text,width:labelSpecs[i].width,height:labelSpecs[i].height,layoutOptions:{'edgeLabels.inline':true,'edgeLabels.placement':'CENTER'}}]:[{text:'',width:0,height:0,layoutOptions:{'edgeLabels.inline':'true','edgeLabels.placement':'CENTER'}}]}));
 const result=await elk.layout(root),origins=new Map([['root',[0,0]]]),outEdges=[];
 const walk=(n,x,y)=>{for(const c of n.children??[]){origins.set(c.id,[x+c.x,y+c.y]);walk(c,x+c.x,y+c.y);}for(const e of n.edges??[])outEdges.push([e,n.id]);};walk(result,0,0);
 for(const n of normals){const o=origins.get(n.id);assert(Math.abs(o[0]+n.width/2-n.x)<1e-4,`${r.name} ${n.id} x`);assert(Math.abs(o[1]+n.height/2-n.y)<1e-4,`${r.name} ${n.id} y ${o[1]+n.height/2} vs ${n.y}`);}
 const routes=[],centers=[];for(const [e,owner]of outEdges){const i=+e.id.slice(1),o=origins.get(e.container??owner),p=a=>[a.x+o[0],a.y+o[1]].join(',');const s=e.sections[0];routes[i]=[s.startPoint,...s.bendPoints??[],s.endPoint].map(p).join('|');for(const l of e.labels??[])if(l.text)centers[i]=[l.x+o[0]+l.width/2,l.y+o[1]+l.height/2].join(',');}
 const nodeRows=[...groups.map(g=>[g.id,g.width,g.height,g.width,g.height,g.observed.x,g.observed.y,g.observed.width,g.observed.height]),...normals.map(n=>[n.id,n.width,n.height,n.labelWidth,n.labelHeight,n.x,n.y,n.width,n.height])];
 rows.push([r.name,Array.from(r.source,c=>c.charCodeAt(0).toString(16).padStart(4,'0')).join(''),nodeRows.map(n=>n.join(',')).join(';'),labelSpecs.map(l=>l?[l.width,l.height].join(','):'0,0').join(';'),routes.join(';'),edges.map((_,i)=>centers[i]??'-').join(';')].join('\t'));
}
const target=new URL('../../tests/mermaid_cpp/fixtures/flowchart-elk.tsv',import.meta.url),text=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(target,text);else {
 const expected=readFileSync(target,'utf8').replaceAll('\r\n','\n').trimEnd().split('\n');
 const actual=text.trimEnd().split('\n');assert.equal(actual.length,expected.length);
 for(let row=0;row<expected.length;row++){
  if(expected[row].startsWith('#')){assert.equal(actual[row],expected[row]);continue;}
  const a=actual[row].split('\t'),e=expected[row].split('\t');assert.equal(a.length,e.length);
  for(let col=0;col<e.length;col++){
   if(col<2){assert.equal(a[col],e[col]);continue;}
   const av=a[col].split(/([,;|])/),ev=e[col].split(/([,;|])/);assert.equal(av.length,ev.length);
   for(let i=0;i<ev.length;i++){
    const numeric=/^-?\d+(?:\.\d+)?(?:e[+-]?\d+)?$/i;
    if(numeric.test(ev[i])&&numeric.test(av[i]))assert(Math.abs(Number(av[i])-Number(ev[i]))<1e-4,`${e[0]} column ${col} token ${i}: ${av[i]} vs ${ev[i]}`);
    else assert.equal(av[i],ev[i]);
   }
  }
 }
}

console.log('Flowchart ELK: original six measured scenes and raw paths checked.');
