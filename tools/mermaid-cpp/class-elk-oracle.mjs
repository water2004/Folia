import {readFileSync,writeFileSync,realpathSync} from 'node:fs';
import {createRequire,stripTypeScriptTypes} from 'node:module';
import vm from 'node:vm';
import {createHash} from 'node:crypto';
import path from 'node:path';
import assert from 'node:assert/strict';
const require=createRequire(realpathSync(new URL('./oracle/node_modules/mermaid/package.json',import.meta.url)));
const lock=JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json',import.meta.url)));
const base=path.dirname(require.resolve('elkjs/package.json'));
for(const f of lock.elkCycle.files)assert.equal(createHash('sha256').update(readFileSync(path.join(base,f.path))).digest('hex'),f.sha256);
const ELK=require('elkjs/lib/elk.bundled.js'),elk=new ELK();
const adapterMap=JSON.parse(readFileSync('tools/mermaid-cpp/oracle/node_modules/mermaid/'+lock.elkAdapter.map));
for(const f of lock.elkAdapter.files){const i=adapterMap.sources.findIndex(s=>s.endsWith(f.source));assert.equal(createHash('sha256').update(adapterMap.sourcesContent[i]).digest('hex'),f.sha256);}
const adapterSource=adapterMap.sourcesContent[adapterMap.sources.findIndex(s=>s.endsWith('/render.ts'))];
const js=stripTypeScriptTypes(adapterSource);
function originalFunction(name){const declaration=new RegExp('function\\s*\\*?\\s+'+name+'\\s*\\(').exec(js);assert(declaration,name);const start=declaration.index;const begin=js.indexOf('{',start);let depth=1,end=begin+1;for(;depth;end++){if(js[end]==='{')depth++;else if(js[end]==='}')depth--;}return js.slice(start,end);}
const originalHelpers=['segmentHitsBox','terminalLabelObstacles','placedTerminalLabels','slideTerminalLabelsOffFrames','putTerminalLabelsOnTheirSide','separateOppositeEdgeLabels'].map(originalFunction).join('\n');
const original=vm.runInNewContext(`(()=>{const TERMINAL_LABEL_FRAME_CLEARANCE=2,OPPOSITE_LABEL_MIN_GAP=4;
 const boxesOverlap=(a,b)=>a.x1<b.x2&&b.x1<a.x2&&a.y1<b.y2&&b.y1<a.y2;
 const labelBox=(center,size)=>({x1:center.x-size.width/2,y1:center.y-size.height/2,x2:center.x+size.width/2,y2:center.y+size.height/2});
 ${originalHelpers};return (edges,nodes)=>{slideTerminalLabelsOffFrames(edges,nodes);putTerminalLabelsOnTheirSide(edges,nodes);separateOppositeEdgeLabels(edges);};})()`);
const observations=JSON.parse(readFileSync('build/mermaid-cpp/class-elk-browser/original.json'));
const rows=['# Class ELK measured input: name,source hex,nodes(id,parent,group,width,height,labelwidth,labelheight,x,y,paintwidth,paintheight),label sizes(center/start/end),routes,center label centers,terminal label centers.'];
for(const r of observations.filter(r=>r.theme==='default')){
 const dir=/direction (\w+)/.exec(r.source)?.[1]??'TB';
 const options={'elk.algorithm':'layered','elk.hierarchyHandling':'INCLUDE_CHILDREN','elk.direction':{LR:'RIGHT',RL:'LEFT',TB:'DOWN',BT:'UP'}[dir],
 'spacing.baseValue':40,'elk.layered.cycleBreaking.strategy':'DEPTH_FIRST','elk.layered.nodePlacement.strategy':'BRANDES_KOEPF','elk.layered.nodePlacement.bk.fixedAlignment':'BALANCED',
 'elk.layered.considerModelOrder.strategy':'NODES_AND_EDGES','elk.layered.unnecessaryBendpoints':true,'elk.spacing.portsSurrounding':'[top=12,left=12,bottom=12,right=12]'};
 const root={id:'root',layoutOptions:options,children:[],edges:[]};const all=new Map(),parents=new Map();let parent=null;
 for(const line of r.source.split('\n')){if(line.startsWith('namespace '))parent=/namespace (\w+)/.exec(line)[1];else if(line==='}')parent=null;else if(parent&&line.startsWith('class '))parents.set(/class (\w+)/.exec(line)[1],parent);}
 const nodeRows=[];
 for(const group of r.clusters){const m=r.measured.groups.find(n=>n.id.endsWith('-'+group.id));assert(m);const node={id:group.id,children:[],labels:[{text:group.id,width:m.width,height:Math.max(0,m.height-2)}],layoutOptions:{...options,'elk.layered.considerModelOrder.strategy':'NONE','spacing.baseValue':24,'spacing.nodeNode':50,'elk.spacing.edgeEdge':20,'elk.layered.spacing.edgeNodeBetweenLayers':30,'elk.padding':'[top=24,left=24,bottom=24,right=24]','nodeLabels.placement':'[H_CENTER V_TOP, INSIDE]','nodeSize.constraints':'[MINIMUM_SIZE,NODE_LABELS]','nodeSize.minimum':`(${m.width+16},0)`}};all.set(node.id,node);root.children.push(node);nodeRows.push([node.id,'-',1,m.width,m.height,m.width,m.height,group.x+group.width/2,group.y+group.height/2,group.width,group.height]);}
 for(const n of r.nodes){const node={id:n.id,width:n.width,height:n.height};all.set(node.id,node);(parents.has(n.id)?all.get(parents.get(n.id)):root).children.push(node);nodeRows.push([n.id,parents.get(n.id)??'-',0,n.width,n.height,0,0,n.x+n.width/2,n.y+n.height/2,n.width,n.height]);}
 const relations=r.source.split('\n').filter(l=>l.includes('--')).map(line=>{const m=/^(\w+)(?: "([^"]*)")?\s+([.<|*o-]+>?)\s+(?:"([^"]*)"\s+)?(\w+)(?: : (.*))?$/.exec(line);assert(m,line);return {from:m[1],start:m[2],arrow:m[3],end:m[4],to:m[5],title:m[6]};});
 let c=0,t=0;const labelRows=[];
 root.edges=relations.map((e,i)=>{const center=e.title?r.measured.centers[c++]:{width:0,height:0},start=e.start?r.measured.terminals[t++]:{width:0,height:0},end=e.end?r.measured.terminals[t++]:{width:0,height:0};labelRows.push([center.width,center.height,start.width,start.height,end.width,end.height]);const labels=[];
  if(e.title)labels.push({id:'c'+i,text:e.title,...center,layoutOptions:{'edgeLabels.inline':true,'edgeLabels.placement':'CENTER'}});
  for(const [head,text,size,marker]of [[false,e.start,start,!e.arrow.startsWith('-')],[true,e.end,end,!e.arrow.endsWith('-')]])if(text)labels.push({id:'t'+(2*i+Number(head)),text,width:size.width+(marker?12:0),height:size.height+(marker?12:0),layoutOptions:{'edgeLabels.placement':head?'HEAD':'TAIL'}});
  return {id:'e'+i,sources:[e.from],targets:[e.to],labels};
 });
 const result=await elk.layout(root),origins=new Map([['root',[0,0]]]),outEdges=[];
 const walk=(n,x,y)=>{for(const child of n.children??[]){origins.set(child.id,[x+child.x,y+child.y]);walk(child,x+child.x,y+child.y);}for(const edge of n.edges??[])outEdges.push([edge,n.id]);};walk(result,0,0);
 for(const n of r.nodes){const p=origins.get(n.id);assert(Math.abs(p[0]-n.x)<1e-4,`${r.name}/${n.id} x ${p[0]} / ${n.x}`);assert(Math.abs(p[1]-n.y)<1e-4,`${r.name}/${n.id} y ${p[1]} / ${n.y}`);}
 const routes=[],centers=[],terminals=[];
 for(const [e,owner]of outEdges){const i=+e.id.slice(1),o=origins.get(e.container??owner),p=v=>[v.x+o[0],v.y+o[1]].join(',');const s=e.sections[0];routes[i]=[s.startPoint,...s.bendPoints??[],s.endPoint].map(p).join('|');
  for(const l of e.labels??[]){const center=[l.x+o[0]+l.width/2,l.y+o[1]+l.height/2].join(',');if(l.id[0]==='c')centers[i]=center;else terminals[+l.id.slice(1)]=center;}
 }
 const paintedEdges=relations.map((e,i)=>({start:e.from,end:e.to,label:e.title,width:labelRows[i][0],height:labelRows[i][1],x:centers[i]?+centers[i].split(',')[0]:undefined,y:centers[i]?+centers[i].split(',')[1]:undefined,
  points:routes[i].split('|').map(p=>{const [x,y]=p.split(',').map(Number);return {x,y};}),terminalLabelCenters:{},terminalLabelSizes:{}}));
 for(let i=0;i<relations.length;i++)for(const head of [false,true])if(terminals[2*i+Number(head)]){const [x,y]=terminals[2*i+Number(head)].split(',').map(Number),key=head?'endLeft':'startRight',l=labelRows[i];paintedEdges[i].terminalLabelCenters[key]={x,y};paintedEdges[i].terminalLabelSizes[key]={width:l[head?4:2],height:l[head?5:3]};}
 original(paintedEdges,nodeRows.map(n=>({x:n[7],y:n[8],width:n[9],height:n[10],isGroup:!!n[2]})));
 const finalTerminals=paintedEdges.flatMap(e=>['startRight','endLeft'].map(k=>e.terminalLabelCenters[k]?[e.terminalLabelCenters[k].x,e.terminalLabelCenters[k].y].join(','):'-'));
 const observed=[...r.svg.matchAll(/class="edgeTerminals" transform="translate\(([^)]+)\)"/g)].map(m=>m[1].split(',').map(Number));
 finalTerminals.filter(p=>p!=='-').forEach((p,i)=>p.split(',').map(Number).forEach((v,d)=>assert(Math.abs(v-observed[i][d])<1e-4,`${r.name} final terminal ${i}/${d}: ${v}/${observed[i][d]}`)));
 rows.push([r.name,Array.from(r.source,c=>c.charCodeAt(0).toString(16).padStart(4,'0')).join(''),nodeRows.map(n=>n.join(',')).join(';'),labelRows.map(n=>n.join(',')).join(';'),routes.join(';'),relations.map((_,i)=>centers[i]??'-').join(';'),relations.flatMap((_,i)=>[terminals[2*i]??'-',terminals[2*i+1]??'-']).join(';'),finalTerminals.join(';'),paintedEdges.map(e=>e.x===undefined?'-':[e.x,e.y].join(',')).join(';')].join('\t'));
}
const fixture=new URL('../../tests/mermaid_cpp/fixtures/class-elk.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log('Class ELK: five original measured scenes and terminal labels checked.');
