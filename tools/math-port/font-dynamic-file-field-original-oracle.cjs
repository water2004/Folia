#!/usr/bin/env node
'use strict';

// Observe the pinned CJS DynamicFile ordinary data fields and the later
// dynamicFileName call boundary after raw replacement assignments.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const root = path.resolve(__dirname, '../..');
const {FontData} = require(path.join(root,
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/output/common/FontData.js'));
const sourceLock = JSON.parse(fs.readFileSync(path.join(root,
  'tools/math-port/locks/font-data-source-lock.json'), 'utf8'));
const sourceModules = [
  'cjs/output/common/FontData.js',
  'cjs/output/common/Direction.js',
  'cjs/util/Options.js',
  'cjs/util/Retries.js',
];
const sourceFiles = sourceModules.map(file => {
  const locked = sourceLock.inputs.find(item=>item.package==='@mathjax/src'&&item.path===file);
  if(!locked)throw new Error(`FontData lock is missing ${file}`);
  const absolute=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src',file);
  const actual=crypto.createHash('sha256').update(fs.readFileSync(absolute)).digest('hex');
  if(actual!==locked.sha256)throw new Error(`Pinned Source hash mismatch for ${file}: ${actual}`);
  return {file,sha256:actual};
});

class ProbeFontData extends FontData {}
ProbeFontData.dynamicExtensions = new Map();
const font = new ProbeFontData();
font.addExtension({name:'field-probe',ranges:[['arrow.js',{},[]]]});
const dynamic = ProbeFontData.dynamicExtensions.get('field-probe');
const file = dynamic.files['arrow.js'];
const descriptor = name => {
  const d = Object.getOwnPropertyDescriptor(file,name);
  return {writable:d.writable,enumerable:d.enumerable,configurable:d.configurable};
};
const error = body => {
  try { return {result:body()}; }
  catch (e) { return {error:{name:e.name,message:e.message}}; }
};

const replacementExtension = {key:'extension'};
const initialExtension = file.extension;
file.extension = replacementExtension;
const extensionAssignment = {
  sameValue:file.extension===replacementExtension,
  descriptor:descriptor('extension'),
  valueType:typeof file.extension,
  dynamicFileName:error(()=>font.dynamicFileName(file)),
};
file.extension = initialExtension;

const replacementFile = {key:'file'};
file.file = replacementFile;
const fileAssignment = {
  sameValue:file.file===replacementFile,
  descriptor:descriptor('file'),
  valueType:typeof file.file,
  dynamicFileName:error(()=>font.dynamicFileName(file)),
};

const deletion = {file:delete file.file,descriptor:Object.getOwnPropertyDescriptor(file,'file')||null,
  dynamicFileName:error(()=>font.dynamicFileName(file))};
file.file='replacement.js';
const reassignment = {value:file.file,descriptor:descriptor('file'),
  dynamicFileName:error(()=>font.dynamicFileName(file))};

const rawFileInputs = [];
for (const value of [null, undefined, 17]) {
  file.file=value;
  rawFileInputs.push({
    assigned: value === undefined ? 'undefined' : value,
    own: Object.hasOwn(file,'file'),
    dynamicFileName:error(()=>font.dynamicFileName(file)),
  });
}
file.file='arrow.js';

const methodOrder=[];
const customFile={
  get match(){
    methodOrder.push('match.get');
    return function(pattern){
      methodOrder.push('match.call');
      return false;
    };
  },
  get replace(){
    methodOrder.push('replace.get');
    return function(pattern,replacement){
      methodOrder.push('replace.call');
      return 'custom.js';
    };
  },
};
file.file=customFile;
const customFileMethods={
  result:error(()=>font.dynamicFileName(file)),
  methodOrder:methodOrder.slice(),
};
file.file='arrow.js';

// Ordinary member-call syntax invokes the retrieved method with its object as
// receiver; it does not read Function.prototype.call. Give both methods an own
// throwing `call` getter so that this observable boundary is pinned explicitly.
const poisonedCallOrder=[];
const methodWithPoisonedCall=(name,body)=>{
  const method=function(...args){poisonedCallOrder.push(`${name}.invoke`);return body(...args);};
  Object.defineProperty(method,'call',{configurable:true,get(){
    poisonedCallOrder.push(`${name}.call.get`);
    throw new Error(`${name}.call must not be read`);
  }});
  return method;
};
file.file={
  get match(){poisonedCallOrder.push('match.get');return methodWithPoisonedCall('match',()=>false);},
  get replace(){poisonedCallOrder.push('replace.get');return methodWithPoisonedCall('replace',()=> 'custom.js');},
};
const poisonedMethodCall={
  result:error(()=>font.dynamicFileName(file)),
  methodOrder:poisonedCallOrder.slice(),
};
file.file='arrow.js';

const receiverOrder=[];
const matchingFile={
  get match(){
    receiverOrder.push('match.get');
    return function(pattern){receiverOrder.push('match.call');return true;};
  },
  get replace(){receiverOrder.push('replace.get');throw new Error('replace must not be read');},
};
let fileRead=0;
Object.defineProperty(file,'extension',{configurable:true,enumerable:true,
  get(){receiverOrder.push('extension.get');return 'field-probe';}});
Object.defineProperty(file,'file',{configurable:true,enumerable:true,
  get(){receiverOrder.push(`file.get.${++fileRead}`);return matchingFile;}});
const matchingFileOrder={
  resultIsInput:error(()=>font.dynamicFileName(file)).result===matchingFile,
  receiverOrder:receiverOrder.slice(),
};
delete file.extension;
delete file.file;
file.extension='field-probe';
file.file='arrow.js';

const extensionFallback=[];
for(const value of [null,undefined,'']){
  file.extension=value;
  extensionFallback.push({
    assigned:value===undefined?'undefined':value,
    dynamicFileName:error(()=>font.dynamicFileName(file)),
  });
}
file.extension='field-probe';

const keyOrderBeforeDelete=Object.keys(file);
delete file.file;
file.file='arrow.js';
const keyOrderAfterReassign=Object.keys(file);

const result = {
  source:'MathJax 4.1.3 / fb987178c2d279a99b0db5ea02e751691703955e',
  sourceScope:{scope:'FontData dynamic file ordinary fields and dynamicFileName',
    sourceLock:sourceLock.source,files:sourceFiles},
  ordinaryFields:{extension:extensionAssignment,file:fileAssignment,deletion,reassignment,
    rawFileInputs,customFileMethods,poisonedMethodCall,matchingFileOrder,extensionFallback,keyOrderBeforeDelete,keyOrderAfterReassign},
};
const target=path.join(root,'build/math-port/font-dynamic-file-field-original.json');
fs.mkdirSync(path.dirname(target),{recursive:true});
fs.writeFileSync(target,`${JSON.stringify(result,null,2)}\n`);
process.stdout.write(`${path.relative(root,target)}\n`);
