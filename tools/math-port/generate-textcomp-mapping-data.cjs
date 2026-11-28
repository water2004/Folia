'use strict';
// Generate the native static table from the already locked TextcompMappings.ts.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..');
const source=path.join(root,'.reference/MathJax-src/ts/input/tex/textcomp/TextcompMappings.ts');
const scope=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/scope.json'),'utf8'));
const lock=scope.files.find(item=>item.source==='ts/input/tex/textcomp/TextcompMappings.ts');
if(!lock)throw new Error('TextcompMappings.ts is absent from the locked Source scope');
const contents=fs.readFileSync(source,'utf8');
const digest=crypto.createHash('sha256').update(contents).digest('hex');
if(digest!==lock.source_sha256)throw new Error('TextcompMappings.ts does not match the locked Source hash');
const declarations=[],rows=[];
for(const match of contents.matchAll(/^\s*([A-Za-z][A-Za-z0-9]*):\s*\[Insert,\s*('(?:\\.|[^'])*')(?:,\s*TexConstant\.Variant\.([A-Z]+))?\],?\s*$/gm)){
  const text=Function(`return ${match[2]}`)();
  const units=[];for(let i=0;i<text.length;i++)units.push(`0x${text.charCodeAt(i).toString(16).padStart(4,'0')}`);
  const storage=`textcomp_${rows.length}`;
  declarations.push(`static constexpr char16_t ${storage}[]{${units.join(', ')}};`);
  const variant=match[3]==='OLDSTYLE'?'oldstyle':match[3]==='BOLDOLDSTYLE'?'boldoldstyle':'';
  rows.push(`    {"${match[1]}", std::u16string_view(${storage}, ${units.length}), "${variant}"},`);
}
if(rows.length<100)throw new Error(`Expected the complete Source table; parsed only ${rows.length} mappings`);
const output=`// Generated from locked TextcompMappings.ts; Source SHA256 ${digest}; ${rows.length} entries.\n${declarations.join('\n')}\nstatic constexpr Entry entries[]{\n${rows.join('\n')}\n};\n`;
const dest=path.join(root,'src/math/input/tex/textcomp/mapping_data.inc');
fs.writeFileSync(dest,output);
process.stdout.write(JSON.stringify({entries:rows.length,sha256:digest,destination:path.relative(root,dest)})+'\n');
