// Execute Mermaid's original Markdown-to-HTML adapter and observe display runs.
import assert from 'node:assert/strict';
import {readFileSync,writeFileSync,realpathSync} from 'node:fs';
import {createHash} from 'node:crypto';
import {createRequire,stripTypeScriptTypes} from 'node:module';
import {pathToFileURL} from 'node:url';
import vm from 'node:vm';
import {JSDOM} from './oracle/node_modules/jsdom/lib/api.js';
const require=createRequire(realpathSync(new URL('./oracle/node_modules/mermaid/package.json',import.meta.url)));
const markedPath=require.resolve('marked');
const {marked}=await import(pathToFileURL(markedPath));
const lockPath='docs/mermaid-cpp/source-lock.json';
const lock=JSON.parse(readFileSync(lockPath));
const path='packages/mermaid/src/rendering-util/handle-markdown-text.ts';
const source=readFileSync('.reference/mermaid-js/'+path,'utf8');
const hash=bytes=>createHash('sha256').update(bytes).digest('hex');
assert.equal(hash(Buffer.from(source)),lock.files.find(x=>x.path===path).sha256);
const pin={version:'16.4.2',file:'lib/marked.esm.js',sha256:hash(readFileSync(markedPath))};
if(process.argv.includes('--write')) {
 lock.markdownLabels=pin;writeFileSync(lockPath,JSON.stringify(lock,null,2)+'\n');
} else assert.deepEqual(pin,lock.markdownLabels);
const original=stripTypeScriptTypes(source.replace(/^import[\s\S]*?;\r?\n/gm,''))
 .replace(/^export /gm,'');
const convert=vm.runInNewContext(original+'\nmarkdownToHTML',{marked,log:{warn(){}}});
const cases=[
 ['mixed','Read **bold** and *italic*'],
 ['underscores','Read __source document__ and _validate input_'],
 ['nested','Save **updated _data_**'],
 ['both','Save ***both***\nthen finish'],
 ['escapes','Read \\*literal\\* &amp; **bold**'],
 ['unfinished','Save file_name and *unfinished'],
 ['break','Read **source**<br/>and *write*'],
 ['html','Read <b>bold</b> and <i>italic</i>'],
];
const hex=text=>text?Array.from({length:text.length},(_,i)=>text.charCodeAt(i).toString(16).padStart(4,'0')).join(''):'-';
const rows=['# Original Mermaid markdownToHTML; input and styled non-space UTF-16 characters.'];
for(const [name,input] of cases) {
 const dom=new JSDOM(convert(input));
 const doc=dom.window.document,walker=doc.createTreeWalker(doc.body,dom.window.NodeFilter.SHOW_ELEMENT|dom.window.NodeFilter.SHOW_TEXT);
 let observed='';
 while(walker.nextNode()) {
  const node=walker.currentNode;
  if(node.nodeName==='BR') observed+='\n';
  if(node.nodeType!==3) continue;
  const strong=!!node.parentElement.closest('b,strong'),emphasis=!!node.parentElement.closest('i,em');
  for(let i=0;i<node.textContent.length;++i) if(!/\s/.test(node.textContent[i])) observed+=String(Number(strong)+2*Number(emphasis))+node.textContent[i];
 }
 rows.push([name,hex(input),hex(observed)].join('\t'));dom.window.close();
}
const target='tests/mermaid_cpp/fixtures/flowchart-label.tsv',output=rows.join('\n')+'\n';
if(process.argv.includes('--write')) writeFileSync(target,output);
else assert.equal(readFileSync(target,'utf8').replaceAll('\r\n','\n'),output);
console.log(`Checked ${cases.length} original Markdown label observations.`);
