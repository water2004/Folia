// Original Architecture consumer, with its hidden DOM container replaced by
// Cytoscape's headless surface. Both original layout.run() calls are retained.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {createRequire,stripTypeScriptTypes} from 'node:module';
const require=createRequire(import.meta.url);
const isolated='../../build/mermaid-cpp/mindmap-layout-oracle/';
assert.equal(createHash('sha512').update(readFileSync(new URL(isolated+'cytoscape-3.34.0.tgz',import.meta.url))).digest('base64'),'62rNSrioXw93uliKFBwjukeQyeWwH2PqDrTac31r2P6464u3AUvTk0xS4LVvT251g7IgkFunrI48ZEZGjywSOg==');
const pins=JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json',import.meta.url))).fcose.files;
for(const pin of pins){const [pkg,...rest]=pin.source.split('/');const name=pkg.slice(0,pkg.lastIndexOf('@'));const suffix=pkg.startsWith('cytoscape-fcose@')?'_cytoscape@3.34.4':'';const bytes=readFileSync(new URL(name==='cytoscape'?isolated+'package/'+rest.join('/'):'./oracle/node_modules/.pnpm/'+pkg+suffix+'/node_modules/'+name+'/'+rest.join('/'),import.meta.url));assert.equal(createHash('sha256').update(bytes).digest('hex'),pin.sha256,pin.source);}
const cytoscape=require(isolated+'package/dist/cytoscape.cjs.js');
cytoscape.use(require('./oracle/node_modules/.pnpm/cytoscape-fcose@2.2.0_cytoscape@3.34.4/node_modules/cytoscape-fcose'));
assert.equal(cytoscape.version,'3.34.0');
const coseBase=require('./oracle/node_modules/.pnpm/cose-base@2.2.0/node_modules/cose-base');
const handler=coseBase.ConstraintHandler;
const originalEnforce=handler.handleConstraints;
let constraintPasses=[];
handler.handleConstraints=layout=>{
 const project=()=>layout.getAllNodes().filter(n=>n.getChild()==null).map(n=>({id:n.id,x:n.getCenterX(),y:n.getCenterY()}));
 const input=project(),constraints=structuredClone(layout.constraints);
 originalEnforce(layout);
 const pass={input,constraints,output:project(),movement:[]};constraintPasses.push(pass);layout.oraclePass=pass;
};
const CoSELayout=coseBase.CoSELayout;
const originalMovement=CoSELayout.prototype.updateDisplacements;
CoSELayout.prototype.updateDisplacements=function(){
 const project=()=>this.getAllNodes().map(n=>({id:n.id,x:n.getCenterX(),y:n.getCenterY(),dx:n.displacementX,dy:n.displacementY}));
 const input=this.totalIterations<=10?project():null;
 originalMovement.call(this);
 if(input)this.oraclePass.movement.push({iteration:this.totalIterations,input,output:project()});
};
const base='.reference/mermaid-js/packages/mermaid/src/diagrams/architecture/';
const moduleOf=async file=>import('data:text/javascript;base64,'+Buffer.from(stripTypeScriptTypes(readFileSync(base+file,'utf8'),{mode:'transform'})).toString('base64'));
const types=await moduleOf('architectureTypes.ts'),seed=await moduleOf('architectureSeed.ts');
const source=readFileSync(base+'architectureRenderer.ts','utf8');
const code=stripTypeScriptTypes(source.slice(source.indexOf('function addServices('),source.indexOf('export const draw:')),{mode:'transform'});
const selection={append(){return this;},attr(){return this;},remove(){}};
let runs=0;
const headless=options=>{
 const cy=cytoscape({...options,container:undefined,headless:true,styleEnabled:true});
 const layout=cy.layout.bind(cy);
 cy.layout=options=>{const value=layout(options),run=value.run.bind(value);value.run=()=>{++runs;return run();};return value;};
 return cy;
};
const dependencies={...types,...seed,cytoscape:headless,select:()=>selection,document:{getElementById:()=>undefined},log:{info(){}}};
const layoutArchitecture=new Function(...Object.keys(dependencies),code+'\nreturn layoutArchitecture;')(...Object.values(dependencies));
const decode=s=>s==='-'?'':s.match(/.{4}/g).map(x=>String.fromCharCode(parseInt(x,16))).join('');
const hex=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i).toString(16).padStart(4,'0')).join('');
const rows=['# Original Architecture fCoSE 2.2.0 / CoSE 2.2.0 / Cytoscape 3.34.0: source, site, positioned data as UTF-16 JSON.'];
for(const row of readFileSync('tests/mermaid_cpp/fixtures/architecture.tsv','utf8').trim().split(/\r?\n/).filter(s=>!s.startsWith('#'))) {
 const [name,source,site,json]=row.split('\t'),data=JSON.parse(decode(json));if(!data)continue;
 const nodes=data.nodes.map(n=>({...n,in:n.parent||undefined})),groups=data.groups.map(n=>({...n,in:n.parent||undefined}));
 const db={getNode:id=>nodes.find(n=>n.id===id),getConfigField:key=>data.config[key],getLayoutHints:()=>data.hints};
 runs=0;constraintPasses=[];
 const cy=await layoutArchitecture(nodes.filter(n=>n.type==='service'),nodes.filter(n=>n.type==='junction'),groups,data.edges,db,{spatialMaps:data.spatialMaps.map(m=>new Map(m)),groupAlignments:new Map(data.groupAlignments)});
 const result={runs,constraintPasses,nodes:cy.nodes().map(n=>({id:n.id(),x:n.position('x'),y:n.position('y'),width:n.width(),height:n.height()}))};
 assert.equal(runs,2,name);
 assert.equal(constraintPasses.length,2,name+' original constraint passes');
 for(const pass of constraintPasses){assert(pass.input.length>0,name+' constraint nodes');assert.equal(pass.movement.length,10,name+' movement snapshots');}
 rows.push([name,source,site,hex(JSON.stringify(result))].join('\t'));cy.destroy();
}
const file='tests/mermaid_cpp/fixtures/architecture-layout.tsv',expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(file,expected);else assert.equal(readFileSync(file,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`Architecture: ${rows.length-1} original two-pass fCoSE layouts checked.`);
