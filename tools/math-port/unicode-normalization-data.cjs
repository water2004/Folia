// Root data generation only: the native normalization algorithm is separate.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),reference=path.join(root,'.reference/unicode16');
const source=fs.readFileSync(path.join(reference,'UnicodeData.txt'));
const hash=crypto.createHash('sha256').update(source).digest('hex');
assert.equal(hash,'ff58e5823bd095166564a006e47d111130813dcf8bf234ef79fa51a870edb48f');
const decompositions=[],classes=[],values=[];
let previous=-1;
for(const line of source.toString().trim().split(/\r?\n/)){
 const fields=line.split(';'),point=parseInt(fields[0],16),ccc=Number(fields[3]),mapping=fields[5];
 assert.equal(fields.length,15);assert.ok(point>previous);previous=point;
 assert.ok(Number.isInteger(ccc)&&ccc>=0&&ccc<=255);
 if(fields[1].endsWith(', First>')||fields[1].endsWith(', Last>'))assert.ok(!ccc&&!mapping,'Ranges cannot hide canonical records');
 if(ccc)classes.push([point,ccc]);
 if(mapping&&!mapping.startsWith('<')){const sequence=mapping.split(' ').map(x=>parseInt(x,16));assert.ok(sequence.length<=255);decompositions.push([point,values.length,sequence.length]);values.push(...sequence);}
}
const hex=n=>'0x'+n.toString(16);
let native=`// Generated Unicode 16.0 canonical decomposition/combining-class data.
// Copyright Unicode, Inc.; see LICENSE-Unicode.txt. Source SHA256 ${hash}.
export module folia.math.util.unicode_normalization_data;
import std;
export namespace folia::math::unicode16 {
struct Decomposition {char32_t point;std::uint32_t offset;std::uint8_t length;};
struct CombiningClass {char32_t point;std::uint8_t value;};
inline constexpr std::array<Decomposition,${decompositions.length}> decompositions{{
`;
native+=decompositions.map(([cp,offset,length])=>`{${hex(cp)},${offset},${length}}`).join(',\n')+'\n}};\n';
native+=`inline constexpr std::array<char32_t,${values.length}> values{{\n`+values.map(hex).join(',')+'\n}};\n';
native+=`inline constexpr std::array<CombiningClass,${classes.length}> classes{{\n`+classes.map(([cp,ccc])=>`{${hex(cp)},${ccc}}`).join(',\n')+'\n}};\n}\n';
const license=fs.readFileSync(path.join(reference,'LICENSE.txt'),'utf8');
const manifest=JSON.stringify({unicode:'16.0.0',source_url:'https://www.unicode.org/Public/16.0.0/ucd/UnicodeData.txt',sha256:hash,canonical_decompositions:decompositions.length,combining_class_records:classes.length,values:values.length,compatibility_mappings_excluded:true},null,2)+'\n';
for(const[file,data]of [['src/math/util/unicode_normalization_data.ixx',native],['src/math/util/LICENSE-Unicode.txt',license],['tools/math-port/profiles/evidence/p54-unicode-data.json',manifest]]){const target=path.join(root,file);if(process.argv.includes('--check'))assert.equal(fs.readFileSync(target,'utf8').replaceAll('\r\n','\n'),data.replaceAll('\r\n','\n'));else fs.writeFileSync(target,data);}
console.log(JSON.stringify({canonical_decompositions:decompositions.length,combining_class_records:classes.length}));
