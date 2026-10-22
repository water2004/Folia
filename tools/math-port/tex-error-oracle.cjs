// Execute the pinned original TexError; expectations never use Native output.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=(process.env.FOLIA_MATH_ROOT||path.resolve(__dirname,'../..')),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
for(const [file,sha] of [['ts/input/tex/TexError.ts','30f398ff1910edecf029b1e4dde846bef10990539f60c43a4ef2da8783ba7ac8'],['cjs/input/tex/TexError.js','a5713111ff2ab646740da13ca6273cb5f2a7ceff9d0fb6a2e116185e4994e64f']]){
  const bytes=fs.readFileSync(path.join(file.startsWith('ts/')?path.join(root,'.reference/MathJax-src'):base,file));assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),sha);
  if(file.startsWith('ts/'))assert.equal(fs.readFileSync(path.join(base,file),'utf8').replaceAll('\r\n','\n'),bytes.toString().replaceAll('\r\n','\n'));
}
const TexError=require(path.join(base,'cjs/input/tex/TexError.js')).default;
const args=['first',-0,1e21,undefined,null,false,['a',null,['b','c']],{x:1},'ninth','tenth'];

const inputs=new Set(['','%','%%','%x','%1','%0','%00','%01','%10','%11','%9999999999999999999999999999','%{1}','%{8}','%{9}','%{90}','%{09}','%{10}','%{0}','%{1','%{plural:%1|a|b}','%{plural:%1|}','%{plural:%1|%{2}}','%{plural:%1|%}x}','%{plural:%1|%}}','%{plural:%1|abc}def}','%{PLURAL:%1|a}','%{foo:%0|%1}']);
for(const end of ['\n','\r','\u2028','\u2029','\ud800','\udc00','\ud83d\ude00','\0']){
  inputs.add('%'+end);inputs.add('%{plural:%1|a'+end+'b}');inputs.add('%{plural:%1|a}'+end);
}
for(let i=0;i<120;i++){inputs.add('%'+i);inputs.add('%{'+i+'}');}
let seed=0x4013;const rand=n=>{seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;return (seed>>>0)%n;};
const atoms=['%','{','}','|',':','1','9','0','a','z','\n','\r','\u2028','\u2029','\ud800'];
for(let i=0;i<1800;i++){let s=i%3===0?'%{plural:%1|':i%3===1?'%{':'prefix%';for(let j=0,n=rand(24);j<n;j++)s+=atoms[rand(atoms.length)];if(i%2)s+='}';inputs.add(s);}


const rows=[...inputs].map(input=>({input,parts:input.split(TexError.pattern),message:new TexError('ID',input,...args).message}));

// Frozen original Source JSON remains byte-identical; only renderer-only cpp fields are projected.
assert.ok(process.argv.includes('--check'),'Source-only oracle supports read-only --check');
const frozenProfilePath=path.join(root,"docs/math-port/evidence/p40-tex-error-source.json");
const frozenBytes=fs.readFileSync(frozenProfilePath);
function sourceFields(value){if(Array.isArray(value))return value.map(sourceFields);if(value&&typeof value==='object')return Object.fromEntries(Object.entries(value).filter(([key])=>key!=='cpp').map(([key,v])=>[key,sourceFields(v)]));return value;}
const actualSourceProfile=rows;
assert.deepStrictEqual(sourceFields(actualSourceProfile),sourceFields(JSON.parse(frozenBytes)),'Original Source inputs/expectations/pins changed');
console.log(JSON.stringify({sourceCases:rows.length,profile:"docs/math-port/evidence/p40-tex-error-source.json",profileSha256:crypto.createHash('sha256').update(frozenBytes).digest('hex'),mode:'check'}));
