// Execute the original renderer's private constraint builders unchanged.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {stripTypeScriptTypes} from 'node:module';
const base='.reference/mermaid-js/packages/mermaid/src/diagrams/architecture/';
const types=await import('data:text/javascript;base64,'+Buffer.from(stripTypeScriptTypes(readFileSync(base+'architectureTypes.ts','utf8'),{mode:'transform'})).toString('base64'));
const source=readFileSync(base+'architectureRenderer.ts','utf8');
const code=stripTypeScriptTypes(source.slice(source.indexOf('function getAlignments('),source.indexOf('function layoutArchitecture(')),{mode:'transform'});
const {getAlignments,getRelativeConstraints}=new Function('architectureGroupAlignmentKey','ArchitectureDirectionName','getOppositeArchitectureDirection',code+'\nreturn {getAlignments,getRelativeConstraints};')(types.architectureGroupAlignmentKey,types.ArchitectureDirectionName,types.getOppositeArchitectureDirection);
const decode=s=>s==='-'?'':s.match(/.{4}/g).map(x=>String.fromCharCode(parseInt(x,16))).join('');
const hex=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i).toString(16).padStart(4,'0')).join('');
const rows=['# Original Architecture renderer alignment/relative constraints; source, site, result as UTF-16 JSON.'];
for(const row of readFileSync('tests/mermaid_cpp/fixtures/architecture.tsv','utf8').trim().split(/\r?\n/).filter(s=>!s.startsWith('#'))) {
 const [name,source,site,json]=row.split('\t'),data=JSON.parse(decode(json));if(!data)continue;
 const db={getNode:id=>{const n=data.nodes.find(n=>n.id===id);return n?{...n,in:n.parent||undefined}:null;},getConfigField:key=>data.config[key]};
 const maps=data.spatialMaps.map(m=>new Map(m)),groups=new Map(data.groupAlignments);
 const result={alignment:getAlignments(db,maps,groups,data.hints),relative:getRelativeConstraints(maps,db,data.hints)};
 rows.push([name,source,site,hex(JSON.stringify(result))].join('\t'));
}
const expected=rows.join('\n')+'\n',file='tests/mermaid_cpp/fixtures/architecture-constraints.tsv';
if(process.argv.includes('--write'))writeFileSync(file,expected);else assert.equal(readFileSync(file,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`Architecture: ${rows.length-1} original alignment/relative-constraint projections checked.`);
