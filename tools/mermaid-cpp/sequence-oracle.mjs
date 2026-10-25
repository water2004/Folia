// Original released Sequence Jison parser and SequenceDB; test-only JS runtime.
import assert from 'node:assert/strict';
import {readFileSync, writeFileSync, readdirSync} from 'node:fs';
import {JSDOM} from './oracle/node_modules/jsdom/lib/api.js';
const dom = new JSDOM('<!doctype html><body></body>');
globalThis.window = dom.window; globalThis.document = dom.window.document;
// Use DOM CSS declaration validation for the browser color-detection branch.
window.CSS = {supports(property,value) {const style=document.createElement('span').style;style.setProperty(property,value);return !!style.getPropertyValue(property);}};
const {frontend} = await import('./frontend-oracle.mjs');
const {read, hex} = await import('./config-reference.mjs');
const {default: mermaid} = await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const chunks = new URL('./oracle/node_modules/mermaid/dist/chunks/mermaid.core/', import.meta.url);
const map = JSON.parse(readFileSync(new URL(readdirSync(chunks).find(n => n.endsWith('.mjs.map') &&
  readFileSync(new URL(n,chunks),'utf8').includes('/sequenceDb.ts')),chunks)));
for (const file of ['sequenceDb.ts','sequenceDiagram.ts']) {
  const index = map.sources.findIndex(n => n.endsWith('/'+file));
  assert(index >= 0);
  assert.equal(map.sourcesContent[index].replaceAll('\r\n','\n'),read('packages/mermaid/src/diagrams/sequence/'+file));
}
const cases = [
  ["participant-boxes","sequenceDiagram\nbox rgb(230,240,255) Client\nactor U as User\nparticipant E as Editor\nend\nbox transparent Storage\nparticipant D@{type: database} as Documents\nend\nU->>E: Save\nE->>D: Commit\nD-->>U: Saved"],
  ["participant-box-title","sequenceDiagram\nbox LightGray Document processing service\nparticipant A as Service\nend\nparticipant B as Store\nA->>+B: Save\nB-->>-A: Done"],
  ["participant-box-wrap","%%{init: {\"sequence\": {\"mirrorActors\": false}}}%%\nsequenceDiagram\nbox transparent wrap: Document processing and synchronization service across local storage\nparticipant A as Service\nend\nbox\nparticipant B as Storage\nend\nA->>B: Save\nB-->>A: Done"],
  ["unclosed-box","sequenceDiagram\nbox Client\nparticipant A"],
  ["duplicate-box","sequenceDiagram\nbox Client\nparticipant A\nend\nbox Server\nparticipant A\nend\nA->>B: Save"],
  ["typed-participants","sequenceDiagram\nparticipant U@{type: boundary, alias: Editor}\nparticipant C@{type: control} as Save service\nparticipant D@{type: database} as Storage\nU->>C: Save\nC->>D: Commit\nD-->>U: Saved"],
  ["typed-lifelines","sequenceDiagram\nparticipant E@{type: entity} as Document\nparticipant Q@{type: queue} as Pending\nparticipant C@{type: collections} as Revisions\nE->>+Q: Enqueue\nQ->>C: Append\nQ-->>-E: Done"],
  ["metadata-alias","sequenceDiagram\nparticipant A@{type: database, alias: Ignored} as Storage\nactor B@{type: boundary, alias: Editor}\nA->>B: Read\nparticipant A@{type: queue, alias: Pending}\nB->>A: Save"],
  ["metadata-multiline","sequenceDiagram\nparticipant A@{\ntype: collections\nalias: Revisions\n}\nA->>B: Read"],
  ["metadata-incomplete","sequenceDiagram\nparticipant A@{type: database\nA->>B: Read"],
  ["numbered","sequenceDiagram\nautonumber\nA->>+B: Save\nNote over B: Validate\nB->>B: Commit\nB-->>-A: Done"],
  ["number-controls","sequenceDiagram\nautonumber 10 5\nA->>B: Open\nautonumber off\nB-->>A: Ready\nautonumber\nA->>B: Save"],
  ["number-config","%%{init: {\"sequence\": {\"showSequenceNumbers\": true}}}%%\nsequenceDiagram\nA->>B: Open\nB-->>A: Ready"],
  ["participant-footers","%%{init: {\"sequence\": {\"mirrorActors\": false, \"hideUnusedParticipants\": true}}}%%\nsequenceDiagram\nparticipant Unused\nactor A as User\nparticipant B as Storage\nA->>B: Save"],
  ["number-decimals","sequenceDiagram\nautonumber 10.1 .1\nA<<->>B: Open\nB<<-->>A: Ready\nA<<->>A: Check\nautonumber 1000\nA->>B: Save"],
  ["invalid-number","sequenceDiagram\nautonumber invalid\nA->>B: Go"],
  ['request-response','sequenceDiagram\nparticipant U as User\nparticipant S as Service\nU->>S: Read document\nS-->>U: Document'],
  ['activation-note','sequenceDiagram\nactor U as User\nparticipant S as Service\nU->>+S: Save document\nNote right of S: Validate input\nS->>S: Write output\nS-->>-U: Saved'],
  ['implicit','sequenceDiagram\nClient->>Server: Open\nServer-->>Client: Ready'],
  ['empty','sequenceDiagram\n'],
  ['arrows','sequenceDiagram\nA->B: open\nB-->A: dashed open\nA-xB: cross\nB--xA: dashed cross\nA-)B: async\nB--)A: dashed async\nA<<->>B: both\nB<<-->>A: dashed both'],
  ['notes','sequenceDiagram\nNote left of A: Before\nNote right of B: After\nNote over A,B: Shared\nNote over A: Single'],
  ['nested-activation','sequenceDiagram\nA->>+B: outer\nB->>+B: inner\nB-->>-B: inner done\nB-->>-A: outer done'],
  ['explicit-activation','sequenceDiagram\nparticipant A\nactivate A\nA->>A: Work\ndeactivate A'],
  ['alias-order','sequenceDiagram\nparticipant B as Backend\nactor A as User\nA->>B: Query\nparticipant B as Storage\nB-->>A: Result'],
  ['unicode-breaks','sequenceDiagram\nactor U as 用户\nparticipant S as Service<br/>进程\nU->>S: 保存 😀<br/>完成'],
  ['wrap','sequenceDiagram\nparticipant A as wrap: Long actor\nA->>B:nowrap: Keep on one line\nNote over A,B:wrap: Several words',{sequence:{wrap:true}}],
  ['metadata','---\ntitle: Save flow\n---\nsequenceDiagram\naccTitle: Accessible sequence\naccDescr: A request and response\nA->>B: Save'],
  ['inline-title','sequenceDiagram\ntitle Request\nA->>B: Go'],
  ['comments-separators','sequenceDiagram; participant A; participant B\r\n%% Request\r\nA ->> B: Start # comment\r\nB -->> A: Done'],
  ["loop", "sequenceDiagram\nloop Retry request\nA->>B: Save\nB-->>A: Retry\nend"],
  ["alt", "sequenceDiagram\nalt Valid\nA->>B: Save\nelse Invalid\nB-->>A: Rejected\nelse Missing\nB-->>A: Not found\nend"],
  ["opt-nested", "sequenceDiagram\nopt Cache available\nloop Read twice\nA->>+B: Read\nNote over B: Loading\nB-->>-A: Data\nend\nend"],
  ["par", "sequenceDiagram\npar Read store\nA->>B: Read\nand Refresh cache\nA->>C: Refresh\nend"],
  ["par-over", "sequenceDiagram\npar_over Read\nA->>B: Read\nand Log\nA->>C: Log\nend"],
  ["critical", "sequenceDiagram\ncritical Save transaction\nA->>B: Commit\noption Timeout\nB-->>A: Retry\nend"],
  ["break-rect", "sequenceDiagram\nrect rgb(240, 240, 255)\nA->>B: Save\nbreak Cancelled\nB-->>A: Stop\nend\nend"],
  ["control-wrap", "sequenceDiagram\nloop wrap: Retry the request\nA->>B: Go\nend", {"sequence": {"wrap": true}}],
  ["unclosed-region", "sequenceDiagram\nloop Retry\nA->>B: Go"],
  ["unexpected-section", "sequenceDiagram\nopt Ready\nA->>B: Go\nelse Wait\nend"],
  ["unexpected-end", "sequenceDiagram\nA->>B: Go\nend"],
  ['entities','sequenceDiagram\nparticipant A as Editor #38; viewer\nA->>B: Save #60;document#62;\nNote over B: Value #35;1'],
  ['unfinished-message','sequenceDiagram\nA->>'],
  ['missing-message-colon','sequenceDiagram\nA->>B'],
  ['inactive-end','sequenceDiagram\nA->>-B: Invalid'],
];
const rows = ['# Original Mermaid 12.2.0 Sequence parser/DB; UTF-16 hex JSON.'];
let successes = 0;
for (const [name,source,options={}] of cases) {
  const site = {theme:'default',...options};
  let result = null;
  try {
    const prepared = frontend(site)(source);
    mermaid.initialize({...prepared.effective,startOnLoad:false});
    const {db} = await mermaid.mermaidAPI.getDiagramFromText(prepared.code.cleaned,{title:prepared.title});
    result = {title:db.getDiagramTitle(),accTitle:db.getAccTitle(),accDescription:db.getAccDescription(),
      actors:[...db.getActors()].map(([id,a])=>({id,name:a.name,description:a.description,type:a.type,wrap:a.wrap,box:a.box?db.getBoxes().indexOf(a.box):-1})),
      boxes:db.getBoxes().map(b=>({name:b.name??'',fill:b.fill,wrap:b.wrap,actors:b.actorKeys})),
      messages:db.getMessages().map(m=>({id:m.id,from:m.from??'',to:m.to??'',message:m.message,
        type:m.type,wrap:m.wrap,activate:m.activate??false,placement:m.placement??-1}))};
    ++successes;
  } catch(error) {
    assert(['unclosed-box','duplicate-box','metadata-incomplete','invalid-number','unfinished-message','missing-message-colon','inactive-end','unclosed-region','unexpected-section','unexpected-end'].includes(name),`${name}: ${error.stack}`);
  }
  rows.push([name,hex(source),hex(JSON.stringify(site)),hex(JSON.stringify(result))].join('\t'));
}
const output = new URL('../../tests/mermaid_cpp/fixtures/sequence.tsv',import.meta.url);
const expected = rows.join('\n')+'\n';
if (process.argv.includes('--write')) writeFileSync(output,expected);
else assert.equal(readFileSync(output,'utf8').replaceAll('\r\n','\n'),expected);
dom.window.close();
console.log(`Sequence: ${successes} original actor/message inputs and ${cases.length-successes} parse/DB failures checked.`);
