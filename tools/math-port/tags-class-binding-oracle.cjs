'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..'),installed=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const ts='ts/input/tex/Tags.ts',cjs='cjs/input/tex/Tags.js';
const source=fs.readFileSync(path.join(root,'.reference/MathJax-src',ts));
assert.equal(fs.readFileSync(path.join(installed,ts),'utf8').replaceAll('\r\n','\n'),source.toString('utf8').replaceAll('\r\n','\n'));
const {AbstractTags,NoTags,AllTags}=require(path.join(installed,cjs));
const observations=[];
class ProbeTags extends AbstractTags {
  constructor(){
    super();
    observations.push({actualClass:this.constructor===ProbeTags,counter:this.counter,
      configuration:this.configuration,tagEnv:this.currentTag.env,
      historyLength:this.history.length,stackLength:this.stack.length});
    this.derived=true;
  }
}
const first=new ProbeTags(),second=new ProbeTags();
// Exercise the actual exported Source TagsFactory service.
const {TagsFactory}=require(path.join(installed,cjs));
TagsFactory.add('native-probe',ProbeTags);
const made=TagsFactory.create('native-probe');
const cases={phase:observations,prototype:{
  constructor:Object.getPrototypeOf(ProbeTags)===AbstractTags,
  instance:Object.getPrototypeOf(ProbeTags.prototype)===AbstractTags.prototype,
  madeClass:made.constructor===ProbeTags,baseInstance:made instanceof AbstractTags,
  noTags:Object.getPrototypeOf(NoTags)===AbstractTags,allTags:Object.getPrototypeOf(AllTags)===AbstractTags},
  fresh:{instance:first!==second,ids:first.ids!==second.ids,tag:first.currentTag!==second.currentTag,
    history:first.history!==second.history,enTag:first.enTag!==second.enTag},
  metadata:{name:ProbeTags.name,length:ProbeTags.length},derived:made.derived};
assert.equal(cases.phase.length,3);
assert.ok(Object.values(cases.prototype).every(Boolean));assert.ok(Object.values(cases.fresh).every(Boolean));
const hash=bytes=>crypto.createHash('sha256').update(bytes).digest('hex');
const result={schema:'tags-class-binding-v1',hashes:{[ts]:hash(source),[cjs]:hash(fs.readFileSync(path.join(installed,cjs)))},cases};
const output=path.join(root,'tools/math-port/profiles/evidence/tags-class-binding-source.json');
if(process.argv.includes('--check')){assert.deepEqual(result,JSON.parse(fs.readFileSync(output,'utf8')));console.log('Pinned Tags subclass allocation, initialization and identities agree');}
else fs.writeFileSync(output,JSON.stringify(result,null,2)+'\n');
