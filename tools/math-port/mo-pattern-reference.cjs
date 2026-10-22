// Root-owned, locked development reference. Never a native runtime service.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),npm=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const names=['pseudoScripts','primes','opPattern','mathaccents','mathaccentsWithWidth'];
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');
function compiledTree(){const entries=[];function walk(dir,rel=''){for(const e of fs.readdirSync(dir,{withFileTypes:true}).sort((a,b)=>a.name<b.name?-1:a.name>b.name?1:0)){const name=rel+e.name,p=path.join(dir,e.name);if(e.isDirectory())walk(p,name+'/');else if(/\.(js|cjs|mjs|json)$/.test(name))entries.push([name,sha(fs.readFileSync(p))]);}}walk(path.join(npm,'cjs'));return {files:entries.length,sha256:sha(JSON.stringify(entries))};}
const source='ts/core/MmlTree/MmlNodes/mo.ts',runtime='cjs/core/MmlTree/MmlNodes/mo.js';
const lockfile=path.join(root,'docs/math-port/evidence/p12b-source-lock.json');
const sourceBytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));
assert.equal(sha(sourceBytes),'2082cd372a4c21fa46c67ab3972b51bcc950f711dd3369cbb2bd7ced62c2f605');
assert.equal(process.version,'v24.13.0');assert.equal(require(path.join(npm,'package.json')).version,'4.1.3');
assert.equal(fs.readFileSync(path.join(npm,source),'utf8').replaceAll('\r\n','\n'),sourceBytes.toString().replaceAll('\r\n','\n'));
const resolved=require.resolve('@mathjax/src/js/core/MmlTree/MmlNodes/mo.js',{paths:[path.join(root,'tools/mathjax-bundle')]});
assert.equal(fs.realpathSync(resolved),fs.realpathSync(path.join(npm,runtime)));
assert.equal(sha(fs.readFileSync(resolved)),'7c68f0570d82f585eb6ca7e8bbca119b96a02666e0d4b268a413bf4035ff0b2c');
const actual={version:'4.1.3',node:process.version,source,sha256:sha(sourceBytes),runtime,runtime_sha256:sha(fs.readFileSync(resolved)),compiled_package:compiledTree()};
if(process.argv.includes('--freeze-lock'))fs.writeFileSync(lockfile,JSON.stringify(actual,null,2)+'\n');
assert.deepEqual(actual,JSON.parse(fs.readFileSync(lockfile,'utf8')));
const {MmlMo:M}=require(resolved);
const specs=names.map(name=>{
 const re=M[name];assert.ok(re instanceof RegExp);assert.equal(re.flags,'');assert.equal(re.lastIndex,0);
 assert.ok(re.source.startsWith('^['));let minimum,maximum,end;
 if(re.source.endsWith(']+$')){minimum=1;end=-3;}
 else if(re.source.endsWith(']{2,}$')){minimum=2;end=-6;}
 else {assert.ok(re.source.endsWith(']$'));minimum=1;maximum=1;end=-2;}
 const body=re.source.slice(2,end);assert.ok(body.length&&!body.includes(']')&&!body.includes('['));
 const ranges=[];for(let i=0;i<65536;i++)if(re.test(String.fromCharCode(i).repeat(minimum))){const last=ranges.at(-1);if(last&&last[1]===i-1)last[1]=i;else ranges.push([i,i]);}
 assert.ok(ranges.length);return {name,source:re.source,flags:re.flags,minimum,maximum,ranges};
});
module.exports={root,M,names,specs,lock:actual};
if(require.main===module)console.log(JSON.stringify({patterns:names,compiled_package:actual.compiled_package,mode:process.argv.includes('--freeze-lock')?'freeze':'check'}));
