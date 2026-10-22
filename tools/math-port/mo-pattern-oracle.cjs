// Pinned MathJax Source observations, checked against statically retained original gold.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
assert.ok(process.argv.includes('--check'),'This oracle is read-only; pass --check.');
assert.ok(!process.argv.includes('--freeze-lock'),'Source pins are immutable.');
const tools=process.env.FOLIA_MATH_PORT_TOOLS||__dirname;
const {root,M,names,specs,lock}=require(path.join(tools,'mo-pattern-reference.cjs'));
const goldPath=process.env.FOLIA_MO_GOLD||path.join(root,'docs/math-port/evidence/p12b-mo-pattern-complete-source.json');
const gold=JSON.parse(fs.readFileSync(goldPath,'utf8'));
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');
assert.equal(gold.schema,'mo-pattern-complete-static-gold/v2');
assert.deepEqual(lock,gold.sourceLock.pins);
assert.equal(sha(fs.readFileSync(path.join(root,gold.sourceLock.path))),gold.sourceLock.sha256);
assert.equal(names.length,gold.patterns.length);
const units=text=>Array.from({length:text.length},(_,i)=>text.charCodeAt(i));
let sourceQueries=0,masks=0,membersCount=0,productBits=0,sampleCount=0;
function observed(id,text){sourceQueries++;return !!text.match(M[names[id]]);}
function mask(fn){let s='';for(let i=0;i<65536;i+=4){let n=0;for(let j=0;j<4;j++)if(fn(String.fromCharCode(i+j)))n|=1<<j;s+=n.toString(16);}return s;}
for(const [i,s]of specs.entries()){
 const first=s.ranges[0][0],a=String.fromCharCode(first);
 const variants=[c=>c,c=>c+c,c=>c+c+c,c=>a+c,c=>c+a,c=>a+c+a];
 const utf16_masks=variants.map((make,shape)=>{masks++;return {shape,first_unit:first,mask:mask(c=>observed(i,make(c)))};});
 const members=[];for(let x=0;x<65536;x++)if(observed(i,String.fromCharCode(x).repeat(s.minimum)))members.push(x);
 let product='';for(const x of members)for(const y of members)for(const tail of ['',String.fromCharCode(first),'\n','\r\n','\u2028','\u2029','\0'])product+=observed(i,String.fromCharCode(x,y)+tail)?'1':'0';
 const samples=['',a,a+a,a+a+a,a.repeat(512),a.repeat(4096),a+'\n',a+'\r',a+'\r\n',a+'\u2028',a+'\u2029','\n'+a,'\r'+a,'\0'+a,a+'\0',a+'x'+a,'abc','aA','Aa','aa','zZ','Az9','alpha','ALPHA','áá','ＡＡ','KK','İİ','ßß','²²','\u2032\u2033','\ud800','\udfff','\ud800\udc00','😀','\ud800'+a,a+'\udc00','^','→','→←','^\u0301','"\u2018','\u00aa\u208e',' '+a+a,a+a+' ','\t'+a+a,a+a+'\t','\n'+a+a,a+a+'\n','\r\n'+a+a,a+a+'\r\n'];
 const actual={name:s.name,source_units:units(M[s.name].source),source:M[s.name].source,flags:M[s.name].flags,
 minimum:s.minimum,maximum:s.maximum===undefined?null:s.maximum,ranges:s.ranges,utf16_masks,members,product,
 samples:samples.map(text=>({input_units:units(text),matches:observed(i,text)}))};
 assert.deepEqual(actual,gold.patterns[i],s.name);
 assert.equal(M[s.name].lastIndex,0,s.name+' remains stateless');
 membersCount+=members.length;productBits+=product.length;sampleCount+=samples.length;
}
const callbacks=[];
for(const text of ["*","'",'ab','^','→','',"'''",'a1'])for(let i=0;i<names.length;i++)callbacks.push({pattern_index:i,input_units:units(text),matches:observed(i,text)});
assert.deepEqual(callbacks,gold.independent_callback_observations);
assert.equal(sourceQueries,2342369);
console.log(JSON.stringify({mode:'check',patterns:names.length,originalCases:16,unit_domain_masks:masks,
 members:membersCount,productBits,samples:sampleCount,independentTruthValues:callbacks.length,actual_source_queries:sourceQueries}));
