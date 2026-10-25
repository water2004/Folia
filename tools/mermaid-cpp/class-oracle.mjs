// Original released Class Jison parser, ClassDB and ClassMember; test-only JS.
import assert from 'node:assert/strict';
import {readFileSync, writeFileSync, readdirSync} from 'node:fs';
import {JSDOM} from './oracle/node_modules/jsdom/lib/api.js';
const dom = new JSDOM('<!doctype html><body>');
globalThis.window = dom.window; globalThis.document = dom.window.document;
const {frontend} = await import('./frontend-oracle.mjs');
const {read, hex} = await import('./config-reference.mjs');
const {default:mermaid} = await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const chunks = new URL('./oracle/node_modules/mermaid/dist/chunks/mermaid.core/',import.meta.url);
const map = JSON.parse(readFileSync(new URL(readdirSync(chunks).find(n => n.endsWith('.mjs.map') &&
  readFileSync(new URL(n,chunks),'utf8').includes('/classDb.ts')),chunks)));
for (const file of ['classDb.ts','classTypes.ts']) {
  const index = map.sources.findIndex(n => n.endsWith('/'+file)); assert(index >= 0);
  assert.equal(map.sourcesContent[index].replaceAll('\r\n','\n'),read('packages/mermaid/src/diagrams/class/'+file));
}
const cases = [
  ['notes','classDiagram\nnote "Document model"\nnote for Document "Validate before saving"\nclass Document\nnote for Missing "Unattached"'],
  ['note-multiline','classDiagram\nclass `Local Store`\nnote for `Local Store` "Keep revisions\nSave locally"'],
  ['unclosed-note','classDiagram\nclass Document\nnote for Document "Incomplete'],
  ['members','classDiagram\nclass Document {\n+String title\n+save() bool\n}\nclass Store {\n+load(id) Document\n}\nDocument --> Store : persists'],
  ['relations','classDiagram\ndirection LR\nclass Shape\n<<interface>> Shape\nclass Circle\nShape <|-- Circle\nCircle "1" o-- "*" Point : contains'],
  ['member-details','classDiagram\nclass Repository~T~ {\n<<service>>\n-items List~T~\n+count int$\n#find( id, key ) T*\n~reset()$\n}\nRepository : +append(T item) void\nclass Repository'],
  ['relation-kinds','classDiagram\nA <|.. B\nB *-- C\nC ..> D : uses\nD -- E\nE <--> A\nA --|> F\nF .. G'],
  ['labels','classDiagram\nclass D["Document model"]\nclass `Local Store`\nD " 0..1 " --> " many " `Local Store` : saves'],
  ['direction-bt','classDiagram\ndirection BT\nclass A {}\nA : +read()'],
  ['direction-rl','classDiagram-v2\ndirection RL\nclass A <<interface>> {\n+read() String\n}'],
  ['metadata','---\ntitle: Document storage\n---\nclassDiagram\naccTitle: Storage model\naccDescr: Classes used to save documents\n%% comment\nclass Document\nDocument : +name String'],
  ['empty','classDiagram\n'],
  ['unclosed-body','classDiagram\nclass Document {\n+name String'],
  ['missing-endpoint','classDiagram\nDocument -->'],
  ['nested-body','classDiagram\nclass Document {\nclass Store {\n}\n}'],
];
const errors = ['empty','unclosed-body','missing-endpoint','nested-body','unclosed-note'];
const rows = ['# Original Mermaid 12.2.0 Class parser/DB; UTF-16 hex JSON.'];
for (const [name,source] of cases) {
  const site = {theme:'default'}; let result = null;
  try {
    const prepared = frontend(site)(source);
    mermaid.initialize({...prepared.effective,startOnLoad:false});
    const {db} = await mermaid.mermaidAPI.getDiagramFromText(prepared.code.cleaned,{title:prepared.title});
    const member = m => ({id:m.id,visibility:m.visibility,classifier:m.classifier,
      parameters:m.parameters??'',returnType:m.returnType??'',text:m.text,...m.getDisplayDetails()});
    result = {title:db.getDiagramTitle(),accTitle:db.getAccTitle(),accDescription:db.getAccDescription(),
      direction:db.getDirection(),classes:[...db.getClasses()].map(([id,c])=>({id,label:c.label,type:c.type,
        annotations:c.annotations,members:c.members.map(member),methods:c.methods.map(member)})),
      notes:[...db.getNotes().values()].map(n=>({id:n.id,class:n.class??'',text:n.text})),
      relations:db.getRelations().map(r=>({from:r.id1,to:r.id2,title:r.title??'',
        startLabel:r.relationTitle1,endLabel:r.relationTitle2,
        start:r.relation.type1==='none'?-1:r.relation.type1,end:r.relation.type2==='none'?-1:r.relation.type2,
        dotted:r.relation.lineType===1}))};
  } catch(error) { assert(errors.includes(name),`${name}: ${error.stack}`); }
  assert.equal(result === null,errors.includes(name),name);
  rows.push([name,hex(source),hex(JSON.stringify(site)),hex(JSON.stringify(result))].join('\t'));
}
const output = new URL('../../tests/mermaid_cpp/fixtures/class.tsv',import.meta.url);
const expected = rows.join('\n')+'\n';
if (process.argv.includes('--write')) writeFileSync(output,expected);
else assert.equal(readFileSync(output,'utf8').replaceAll('\r\n','\n'),expected);
dom.window.close();
console.log(`Class: ${cases.length-errors.length} original class/member/relation inputs and ${errors.length} parse failures checked.`);
