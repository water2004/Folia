'use strict';
// Source-only observations for pinned util/Retries.ts and installed CJS.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),pkg=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const ts='.reference/MathJax-src/ts/util/Retries.ts',installed=path.join(pkg,'ts/util/Retries.ts'),cjs=path.join(pkg,'cjs/util/Retries.js');
const source=fs.readFileSync(path.join(root,ts)),sourceHash=crypto.createHash('sha256').update(source).digest('hex');
const installedBytes=fs.readFileSync(installed),installedHash=crypto.createHash('sha256').update(installedBytes).digest('hex');
const normalize=b=>b.toString('utf8').replaceAll('\r\n','\n');assert.equal(normalize(installedBytes),normalize(source),'installed Retries.ts drift');
assert.equal(sourceHash,'c517df1c4c67c8cfac65a296b0e185089fd166739d3038eebb89647f4b37c921','pinned Retries.ts hash');
const cjsBytes=fs.readFileSync(cjs),cjsHash=crypto.createHash('sha256').update(cjsBytes).digest('hex');
assert.equal(cjsHash,'0ffe30ec3d65f1d5edfdbb505e5b76cf5b66ec1a920216ab53595df8e666b549','Retries CJS hash');
const {handleRetriesFor,retryAfter}=require(cjs);
const serializeError=e=>({name:e?.name||e?.constructor?.name||'',message:String(e?.message),hasRetry:e?.retry instanceof Promise,retryResolved:e?.retry instanceof Promise?'Promise':null});
const capture=async action=>{try{return{value:await action(),error:null};}catch(e){return{value:null,error:serializeError(e)};}};
async function main(){
const rows=[];
rows.push({name:'retry-after-error-shape',...await capture(()=>{try{retryAfter(Promise.resolve());}catch(e){return serializeError(e);}})});
rows.push({name:'handle-retries-success-return',...await capture(()=>handleRetriesFor(()=>17))});
let runs=0;
rows.push({name:'handle-retries-retry-after-then-success',...await capture(()=>handleRetriesFor(()=>{if(++runs===1)retryAfter(Promise.resolve());return runs;}))});
rows.push({name:'handle-retries-direct-error',...await capture(()=>handleRetriesFor(()=>{throw new Error('direct failure');}))});
rows.push({name:'handle-retries-rejected-retry-promise',...await capture(()=>handleRetriesFor(()=>{const p=Promise.reject(new Error('retry rejection'));const e=new Error('retry');e.retry=p;throw e;}))});
const output={schema:1,source_sha256:sourceHash,installed_ts_sha256:installedHash,cjs_sha256:cjsHash,observations:rows};
const dest=path.join(root,'tools/math-port/profiles/evidence/retries-original-source.json'),serialized=JSON.stringify(output,null,2)+'\n';
if(process.argv.includes('--check'))assert.equal(fs.readFileSync(dest,'utf8').replaceAll('\r\n','\n'),serialized);else fs.writeFileSync(dest,serialized);
console.log(JSON.stringify({observations:rows.length,output:path.relative(root,dest)}));
}
main().catch(error=>{console.error(error);process.exitCode=1;});
