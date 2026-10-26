// Capture the original plugin's compound graph boundary for the same six
// Architecture diagrams and both actual incremental layout passes.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {createRequire} from 'node:module';
const require=createRequire(import.meta.url);
const {CoSELayout}=require('./oracle/node_modules/.pnpm/cose-base@2.2.0/node_modules/cose-base');
const original=CoSELayout.prototype.runLayout,passes=[];
CoSELayout.prototype.runLayout=function(){
 const nodes=this.getAllNodes().map(n=>({id:n.id,parent:n.owner.parent.id??'',x:n.getCenterX(),y:n.getCenterY(),width:n.rect.width,height:n.rect.height,padding:n.paddingLeft}));
 const edges=this.getAllEdges().map(e=>({source:e.source.id,target:e.target.id,ideal:e.idealLength,elasticity:e.edgeElasticity}));
 const constraints=structuredClone(this.constraints);
 const result=original.call(this);
 passes.push({input:{nodes,edges,constraints},output:{iterations:this.totalIterations,nodes:this.getAllNodes().map(n=>({id:n.id,x:n.getCenterX(),y:n.getCenterY(),width:n.rect.width,height:n.rect.height}))}});
 return result;
};
// Its oracle already verifies the original end positions and snapshots.
const writing=process.argv.includes('--write');
process.argv=process.argv.filter(arg=>arg!=='--write');
await import('./architecture-layout-oracle.mjs');
const cases=readFileSync('tests/mermaid_cpp/fixtures/architecture-layout.tsv','utf8').trim().split(/\r?\n/).filter(s=>!s.startsWith('#')).map(s=>s.split('\t')[0]);
assert.equal(passes.length,2*cases.length);
const hex=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i).toString(16).padStart(4,'0')).join('');
const rows=['# Original fCoSE compound graphs, each Architecture diagram consumed in its original two passes.'];
for(let i=0;i<cases.length;++i)rows.push(cases[i]+'\t'+hex(JSON.stringify(passes.slice(2*i,2*i+2))));
const file='tests/mermaid_cpp/fixtures/fcose-layout.tsv',expected=rows.join('\n')+'\n';
if(writing)writeFileSync(file,expected);else assert.equal(readFileSync(file,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`fCoSE: ${cases.length} original compound graphs and both layout passes checked.`);
