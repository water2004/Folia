'use strict';
const fs=require('node:fs');
const path=require('node:path');
const crypto=require('node:crypto');
const assert=require('node:assert/strict');
const args=process.argv.slice(2),rootIndex=args.indexOf('--repo-root');
if(rootIndex>=0&&(!args[rootIndex+1]||args.length!==rootIndex+2))
  throw Error('usage: node typed-mml-arithmetic-source-witness.cjs [--repo-root <repository>]');
function findRepositoryRoot(start=__dirname){
  for(let current=path.resolve(start);current!==path.dirname(current);current=path.dirname(current))
    if(fs.existsSync(path.join(current,'tools/math-port/README.md'))&&
       fs.existsSync(path.join(current,'tools/mathjax-bundle/node_modules/@mathjax/src/package.json')))return current;
  return null;
}
const repo=rootIndex>=0?path.resolve(args[rootIndex+1]):findRepositoryRoot();
assert.ok(repo,'pass --repo-root <repository> or run this script from within the repository');
const sourceRoot=path.join(repo,'tools/mathjax-bundle/node_modules/@mathjax/src');
const sourcePackage=require(path.join(sourceRoot,'package.json'));
assert.equal(sourcePackage.version,'4.1.3');
const bundle=path.join(repo,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
const hashes={
  'core/MmlTree/MmlNodes/mfrac.js':'b2e053ce68304c03ea740fb22baa5b01a28d540002faeddc30ad5720a4172770',
  'core/MmlTree/MmlNodes/msqrt.js':'5dfe5af23967d82ccc74cf3f187c49d847da21e13ea721a539cb93afcde0ef35',
  'core/MmlTree/MmlNodes/mroot.js':'f73d25a1ca793ab21cc78ae73ce6062c925f49c8a51c4484211b8098a1a6bcd8',
  'core/MmlTree/MmlNodes/mstyle.js':'49e13c923b88f6b41806c3f030a30edbfe8527beee6350e0e7067fc87707c488',
  'core/MmlTree/MmlNodes/merror.js':'cd9b48970d5d0878d3806e8ff4e4f091d2db38f5021a63aa043acfc50d952d6a',
  'core/MmlTree/MmlNodes/mpadded.js':'675aae98959b257582171d58584f220da4543deb8e0e3d3b8b07a48ab6fc23a2',
  'core/MmlTree/MmlNodes/mphantom.js':'dae156634a5a2fd1076af9abd551df195463a2b6db029635b13e9809ddb566f3',
  'core/MmlTree/MmlNodes/menclose.js':'2cea1d1819a2eb545727f37940b341ac0ce802fad4d7daa11f813ce7381f23de',
};
for(const [file,expected] of Object.entries(hashes)) {
  const actual=crypto.createHash('sha256').update(fs.readFileSync(path.join(bundle,file))).digest('hex');
  assert.equal(actual,expected,`pinned Source hash ${file}`);
}
const load=(name,key)=>require(path.join(bundle,`core/MmlTree/MmlNodes/${name}.js`))[key];
const Mfrac=load('mfrac','MmlMfrac'),Msqrt=load('msqrt','MmlMsqrt'),Mroot=load('mroot','MmlMroot');
const Mstyle=load('mstyle','MmlMstyle'),Merror=load('merror','MmlMerror'),Mpadded=load('mpadded','MmlMpadded');
const Mphantom=load('mphantom','MmlMphantom'),Menclose=load('menclose','MmlMenclose');
const meta=C=>{const n=Object.create(C.prototype);return {kind:n.kind,arity:n.arity,linebreakContainer:n.linebreakContainer,
  ...(C===Mfrac?{linebreakAlign:n.linebreakAlign}:{})};};
const calls=[];
const child=tag=>({setTeXclass(value){calls.push([tag,'texclass',value]);return null;},
  setInheritedAttributes(...values){calls.push([tag,'inherit',...values]);}});
const frac=Object.create(Mfrac.prototype);frac.getPrevClass=()=>calls.push(['frac','previous']);
frac.childNodes=[child('num'),child('den')];
frac.attributes={get:key=>({numalign:'left',denomalign:'right'})[key]};
frac.addInheritedAttributes=(current,additions)=>({current,additions});
frac.setTeXclass(null);frac.setChildInheritedAttributes({seed:1},false,0,false);
const sqrt=Object.create(Msqrt.prototype);sqrt.getPrevClass=()=>calls.push(['sqrt','previous']);sqrt.childNodes=[child('sqrt-body')];
sqrt.setTeXclass(null);sqrt.setChildInheritedAttributes({seed:2},true,3,false);
const root=Object.create(Mroot.prototype);root.getPrevClass=()=>calls.push(['root','previous']);root.childNodes=[child('root-base'),child('root-degree')];
root.setTeXclass(null);root.setChildInheritedAttributes({seed:3},true,1,false);
const style=Object.create(Mstyle.prototype);style.childNodes=[child('style-body')];
style.attributes={getExplicit:key=>({scriptlevel:' +2',displaystyle:true,'data-cramped':true})[key],getAllAttributes:()=>({mathcolor:'red'})};
style.addInheritedAttributes=(current,additions)=>({current,additions});
style.setChildInheritedAttributes({seed:4},false,1,false);
const padded=Object.create(Mpadded.prototype);padded.getPrevClass=()=>calls.push(['padded','previous']);
padded.getProperty=key=>key==='vbox';padded.texClass=null;padded.childNodes=[child('padded-body')];
padded.setTeXclass(null);
const enclosure=Object.create(Menclose.prototype);enclosure.childNodes=[child('enclosed')];
enclosure.updateTeXclass=target=>calls.push(['enclose','update',target===enclosure.childNodes[0]]);
const enclosureResult=enclosure.setTeXclass('prior');
assert.deepStrictEqual(meta(Mfrac),{kind:'mfrac',arity:2,linebreakContainer:true,linebreakAlign:''});
assert.deepStrictEqual(meta(Msqrt),{kind:'msqrt',arity:-1,linebreakContainer:true});
assert.deepStrictEqual(meta(Mroot),{kind:'mroot',arity:2,linebreakContainer:true});
assert.deepStrictEqual(meta(Merror),{kind:'merror',arity:-1,linebreakContainer:true});
assert.deepStrictEqual(meta(Mpadded),{kind:'mpadded',arity:-1,linebreakContainer:true});
assert.deepStrictEqual(meta(Mphantom),{kind:'mphantom',arity:-1,linebreakContainer:false});
assert.deepStrictEqual(meta(Menclose),{kind:'menclose',arity:-1,linebreakContainer:true});
assert.deepStrictEqual(meta(Mstyle),{kind:'mstyle',arity:-1,linebreakContainer:false});
assert.equal(Mfrac.defaults.linethickness,'medium');assert.equal(Mfrac.defaults.numalign,'center');
assert.equal(Mroot.defaults['data-vertical-align'],'bottom');assert.equal(Msqrt.defaults['data-vertical-align'],'bottom');
assert.equal(Menclose.defaults.notation,'longdiv');assert.equal(Mpadded.defaults.width,'');
const fracCalls=calls.filter(x=>x[0]==='num'||x[0]==='den');
assert.equal(fracCalls.filter(x=>x[1]==='texclass').length,2);
const inheritCalls=calls.filter(x=>x[1]==='inherit');
assert.deepStrictEqual(inheritCalls.map(x=>x.slice(3,6)),[
  [false,1,false],[false,1,true],[true,3,true],[true,1,true],[false,3,false],[true,3,true],
]);
function styleLevel(value,initial) {
  let inherited;
  const node=Object.create(Mstyle.prototype);
  node.childNodes=[{setInheritedAttributes(...args){inherited=args;}}];
  node.attributes={getExplicit:key=>key==='scriptlevel'?value:undefined,getAllAttributes:()=>({})};
  node.addInheritedAttributes=(current,additions)=>({current,additions});
  node.setChildInheritedAttributes({},false,initial,false);
  return inherited[2];
}
assert.equal(styleLevel('0x10',3),16);
assert.equal(styleLevel('\u2003+2',3),5);
assert.equal(styleLevel('-2',3),1);
assert.equal(styleLevel(1e21,3),1);
assert(Number.isNaN(styleLevel('not-a-number',3)));
assert.equal(enclosureResult,null);
const result={source:{package:sourcePackage.name,version:sourcePackage.version,hashes},classes:{mfrac:Mfrac.defaults,msqrt:Msqrt.defaults,
  mroot:Mroot.defaults,mstyle:Mstyle.defaults,merror:Merror.defaults,mpadded:Mpadded.defaults,
  mphantom:Mphantom.defaults,menclose:Menclose.defaults},metadata:[meta(Mfrac),meta(Msqrt),meta(Mroot),meta(Mstyle),meta(Merror),meta(Mpadded),meta(Mphantom),meta(Menclose)],calls};
const output=path.join(repo,'build/math-port/typed-mml-arithmetic/source-witness.json');
fs.mkdirSync(path.dirname(output),{recursive:true});
fs.writeFileSync(output,JSON.stringify(result,null,2)+'\n');
process.stdout.write(JSON.stringify({output:path.relative(repo,output),...result},null,2)+'\n');
