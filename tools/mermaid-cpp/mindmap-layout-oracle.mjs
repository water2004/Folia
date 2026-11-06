// Fixed measured boxes through the original Mindmap CoSE layout consumer.
// The isolated Cytoscape archive must match the original Mermaid lockfile.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {createRequire} from 'node:module';
import {fileURLToPath} from 'node:url';
const require=createRequire(import.meta.url);
const lock=JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json',import.meta.url)));
for(const file of lock.coseTree.files) {
 const slash=file.source.indexOf('/'),pkg=file.source.slice(0,slash),name=pkg.slice(0,pkg.indexOf('@'));
 const directory=pkg.startsWith('cytoscape-cose-bilkent@')?pkg+'_cytoscape@3.34.4':pkg;
 const source=new URL('./oracle/node_modules/.pnpm/'+directory+'/node_modules/'+name+'/'+file.source.slice(slash+1),import.meta.url);
 assert.equal(createHash('sha256').update(readFileSync(source)).digest('hex'),file.sha256,file.source);
}

const isolated=new URL('../../build/mermaid-cpp/mindmap-layout-oracle/',import.meta.url);
assert.equal(createHash('sha512').update(readFileSync(new URL('cytoscape-3.34.0.tgz',isolated))).digest('base64'),
 '62rNSrioXw93uliKFBwjukeQyeWwH2PqDrTac31r2P6464u3AUvTk0xS4LVvT251g7IgkFunrI48ZEZGjywSOg==');
const cytoscape=require(fileURLToPath(new URL('package/dist/cytoscape.cjs.js',isolated)));
const plugin=require('./oracle/node_modules/.pnpm/cytoscape-cose-bilkent@4.1.0_cytoscape@3.34.4/node_modules/cytoscape-cose-bilkent');
cytoscape.use(plugin);
assert.equal(cytoscape.version,'3.34.0');
const decode=s=>s==='-'?'':s.match(/.{4}/g).map(x=>String.fromCharCode(parseInt(x,16))).join('');
const hex=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i).toString(16).padStart(4,'0')).join('');
const inputs=readFileSync(new URL('../../tests/mermaid_cpp/fixtures/mindmap.tsv',import.meta.url),'utf8').trim().split(/\r?\n/).filter(s=>!s.startsWith('#'));
const rows=['# Mindmap CoSE-Bilkent 4.1.0 + Cytoscape 3.34.0: measured boxes, edges and positioned nodes as UTF-16 JSON.'];
for(const row of inputs) {
 const [name,,,data]=row.split('\t'),input=JSON.parse(decode(data));
 if(!input||input.layout!=='cose-bilkent')continue;
 const nodes=input.nodes.map((n,i)=>({id:n.id,width:60+11*i,height:30+3*i}));
 const edges=input.edges.map(e=>({id:e.id,source:e.start,target:e.end}));
 // No renderer is involved: only node dimensions consumed by the original
 // plugin are supplied. Edge SVG geometry remains a separate measured gate.
 const cy=cytoscape({headless:true,styleEnabled:true,elements:[...nodes.map(n=>({group:'nodes',data:n,position:{x:0,y:0}})),...edges.map(e=>({group:'edges',data:e}))]});
 cy.nodes().forEach(n=>{n.layoutDimensions=()=>({w:n.data('width'),h:n.data('height')});});
 const layout=cy.layout({name:'cose-bilkent',quality:'proof',styleEnabled:false,animate:false});
 layout.run();
 const result={iterations:layout.layout.totalIterations,nodes:cy.nodes().map(n=>({id:n.id(),x:n.position('x'),y:n.position('y')}))};
 rows.push([name,hex(JSON.stringify({nodes,edges})),hex(JSON.stringify(result))].join('\t'));
 cy.destroy();
}
const file=new URL('../../tests/mermaid_cpp/fixtures/mindmap-layout.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(file,expected);else assert.equal(readFileSync(file,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`Mindmap CoSE: ${rows.length-1} original measured-tree layouts checked.`);
