// Values observed through the actual D3 formatter bundled in Mermaid 12.2.0.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync,readdirSync,writeFileSync} from 'node:fs';
const lock=JSON.parse(readFileSync(new URL('./source-lock.json',import.meta.url)));
assert.equal(lock.commit,'1fb7869866dd619168d840291c9c54b4f129758b');
assert.equal(JSON.parse(readFileSync(new URL('./oracle/node_modules/mermaid/package.json',import.meta.url))).version,'12.2.0');
const renderer=lock.files.find(file=>file.path==='packages/mermaid/src/diagrams/treemap/renderer.ts');
assert(renderer,'Treemap renderer source is not locked');
assert.equal(createHash('sha256').update(readFileSync(new URL('../../.reference/mermaid-js/'+renderer.path,import.meta.url))).digest('hex'),renderer.sha256);
const chunks=new URL('./oracle/node_modules/mermaid/dist/chunks/mermaid.esm/',import.meta.url);
let bundle;
for(const file of readdirSync(chunks).filter(f=>f.endsWith('.mjs.map'))){
 const map=JSON.parse(readFileSync(new URL(file,chunks)));
 if(!map.sources.some(s=>s.endsWith('/d3-format/src/formatTypes.js')))continue;
 for(const entry of lock.treemapLayout.files.filter(f=>f.source.startsWith('d3-format@'))){
  const suffix='/d3-format/'+entry.source.split('/').slice(1).join('/');
  const index=map.sources.findIndex(s=>s.endsWith(suffix));
  assert(index>=0,suffix);
  assert.equal(createHash('sha256').update(map.sourcesContent[index]).digest('hex'),entry.sha256,entry.source);
 }
 bundle=new URL(file.replace(/\.map$/,''),chunks);break;
}
assert(bundle,'Pinned Mermaid D3 format bundle was not found');
const {format}=await import(bundle);
const values=[0,Number.MIN_VALUE,1e-30,1e-25,1e-24,1e-12,1e-7,1e-6,
 .00000123456789,.00009995,.004,.005,.125,.25,.35,.5,1.005,1.25,2.55,
 2.5,3.5,8.5,9.5,25,250,1250,2499.5,999.5,999.95,999999.5,
 1234.567,12345.6789,1000000,1e18+128,1e21,1.2345678901234568e25,1e30,1e100,Number.MAX_VALUE];
const formats=[',','$0,0','.1f','.2f','$.2f','$.1%','%','.0%',',.2%','.30%',
 '.3g','.6g','.12g','.21g','.30g','.0g','.2e','.0e','.30e','.3r','.3p',
 '.3s','.6s','.3~s','+.3~g','+,.2f','020,.2f','0=+16,.2f','*>16,.2f',
 '^18.3~g','<18.2e','n','d','#b','#o','#x','#X','c','~g','$,.4f',
 '$.3g','$.3s','>+$16,.2f','banana','.f','08,'];
const hex=s=>Buffer.from(s,'utf16le').swap16().toString('hex')||'-';
const rows=['# Mermaid 12.2.0 bundled d3-format value labels; value, UTF-16 format, UTF-16 result.',
 '# Formatting wrapper is the exact branch structure from Treemap renderer.ts.'];
for(const specification of formats){
 let valueFormat;
 try{
  if(specification==='$0,0')valueFormat=v=>'$'+format(',')(v);
  else if(specification.startsWith('$')&&specification.includes(',')){
   const precision=/\.\d+/.exec(specification)?.[0]??'';
   const formatter=format(','+precision);
   valueFormat=v=>'$'+formatter(v);
  }else if(specification.startsWith('$')){
   const formatter=format(specification.slice(1)||'');valueFormat=v=>'$'+formatter(v);
  }else valueFormat=format(specification);
 }catch{valueFormat=format(',');}
 for(const value of values)rows.push([value,hex(specification),hex(value?valueFormat(value):'')].join('\t'));
}
const output=rows.join('\n')+'\n';
const file=new URL('../../tests/mermaid_cpp/fixtures/treemap-values.tsv',import.meta.url);
if(process.argv.includes('--write'))writeFileSync(file,output);
else assert.equal(readFileSync(file,'utf8').replaceAll('\r\n','\n'),output);
console.log(JSON.stringify({values:values.length,formats:formats.length,checks:values.length*formats.length}));
