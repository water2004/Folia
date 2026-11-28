#!/usr/bin/env node
'use strict';

const fs=require('node:fs');
const path=require('node:path');
const crypto=require('node:crypto');
const root=path.resolve(__dirname,'../..');
const {MathJaxNewcmFont}=require(path.join(root,
  'tools/mathjax-bundle/node_modules/@mathjax/mathjax-newcm-font/cjs/svg.js'));
const {MathJaxBboldxFontExtension}=require(path.join(root,
  'tools/mathjax-bundle/node_modules/@mathjax/mathjax-bboldx-font-extension/cjs/svg.js'));
const lock=JSON.parse(fs.readFileSync(path.join(root,
  'tools/math-port/locks/font-data-source-lock.json'),'utf8'));
const inputs=[
  ['@mathjax/mathjax-newcm-font','cjs/svg.js'],
  ['@mathjax/mathjax-bboldx-font-extension','cjs/svg.js'],
];
const sourceFiles=inputs.map(([pkg,file])=>{
  const locked=lock.inputs.find(item=>item.package===pkg&&item.path===file);
  if(!locked)throw new Error(`Source lock is missing ${pkg}/${file}`);
  const folder=pkg==='@mathjax/src'?'@mathjax/src':pkg;
  const absolute=path.join(root,'tools/mathjax-bundle/node_modules',folder,file);
  const sha256=crypto.createHash('sha256').update(fs.readFileSync(absolute)).digest('hex');
  if(sha256!==locked.sha256)throw new Error(`Pinned Source hash mismatch for ${pkg}/${file}: ${sha256}`);
  return {package:pkg,file,sha256};
});

const original=MathJaxNewcmFont.dynamicExtensions;
const replacement=new Map([['replacement-marker',{files:{}}]]);
MathJaxNewcmFont.dynamicExtensions=replacement;
const descriptor=Object.getOwnPropertyDescriptor(MathJaxNewcmFont,'dynamicExtensions');
const oldRecord={files:{}};
original.set('old-marker',oldRecord);
const font=new MathJaxNewcmFont();
const result=font.addExtension(MathJaxBboldxFontExtension);
const dynamic=MathJaxNewcmFont.dynamicExtensions.get(MathJaxBboldxFontExtension.name);
const oldRecordAfter=original.get('old-marker');
const oldLateRecord={files:{}};
original.set('late-old-marker',oldLateRecord);

const observation={
  source:'MathJax 4.1.3 / fb987178c2d279a99b0db5ea02e751691703955e',
  sourceScope:{sourceLock:lock.source,files:sourceFiles},
  replacement:{
    assignedSameObject:MathJaxNewcmFont.dynamicExtensions===replacement,
    replacementDiffersFromPrevious:replacement!==original,
    descriptor:{writable:descriptor.writable,enumerable:descriptor.enumerable,configurable:descriptor.configurable},
    oldAliasRetainsRecord:oldRecordAfter===oldRecord,
    oldAliasRetainsLaterMutation:original.get('late-old-marker')===oldLateRecord,
    oldAliasHasExtension:original.has(MathJaxBboldxFontExtension.name),
    replacementKeys:[...replacement.keys()],
    addExtensionResult:{isArray:Array.isArray(result),length:result.length},
    insertedRecord:{
      identity:replacement.get(MathJaxBboldxFontExtension.name)===dynamic,
      keys:Object.keys(dynamic),
      name:dynamic.name,prefix:dynamic.prefix,sizeN:dynamic.sizeN,stretchN:dynamic.stretchN,
      fileKeys:Object.keys(dynamic.files),
    },
  },
};
const target=path.join(root,'build/math-port/font-dynamic-extension-map-original.json');
fs.mkdirSync(path.dirname(target),{recursive:true});
fs.writeFileSync(target,`${JSON.stringify(observation,null,2)}\n`);
process.stdout.write(`${path.relative(root,target)}\n`);

