'use strict';
// Separate actual registry/configuration evidence for the locked Folia profile.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto');
const assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const digest=file=>crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const scopePath=path.join(root,'tools/math-port/profiles/scope.json');
const scope=JSON.parse(fs.readFileSync(scopePath));
const profilePath=path.join(root,scope.profile.file);
assert.equal(digest(profilePath),scope.profile.sha256);
const src=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
const locks=[];
const load=relative=>{
  const file=path.join(src,relative);
  locks.push({file:relative,sha256:digest(file)});
  return require(file);
};
for(const match of fs.readFileSync(profilePath,'utf8').matchAll(
  /require\('@mathjax\/src\/js\/(input\/tex\/[^']+Configuration\.js)'\)/g))load(match[1]);
const {ConfigurationHandler}=load('input/tex/Configuration.js');
const {TeX}=load('input/tex.js');
const warnings=[];
const warn=console.warn;
let input;
try{
  console.warn=(...args)=>warnings.push(args.map(String).join(' '));
  input=new TeX({packages:scope.profile.tex_packages});
}finally{console.warn=warn;}
const requested=input.options.packages;
const registered=requested.map(name=>ConfigurationHandler.get(name)?.name);
assert.deepEqual(registered,requested);
assert.deepEqual(warnings,[]);
const output={scopeSha256:digest(scopePath),profileSha256:digest(profilePath),locks,
  configuredPackages:requested,registeredPackages:registered,configurationWarnings:warnings,
  configurations:[...input.configuration.configurations].map(({item,priority})=>({name:item.name,priority}))};
const destination=path.resolve(root,process.argv[2]||'build/math-port/profile-package-registry-original.json');
fs.writeFileSync(destination,JSON.stringify(output,null,2)+'\n');
console.log(JSON.stringify({destination,registered:registered.length,warnings:warnings.length,
  configurations:output.configurations.length}));
