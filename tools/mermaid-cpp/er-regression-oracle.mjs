// The real locked upstream grammar/DB generates the expectations, never native code.
import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
import { JSDOM } from './oracle/node_modules/jsdom/lib/api.js';
const writeFixture = process.argv.includes('--write');
process.argv = process.argv.filter(arg => arg !== '--write');
const dom = new JSDOM('<!doctype html><body></body>');
globalThis.window = dom.window;
globalThis.document = dom.window.document;
const { frontend } = await import('./frontend-oracle.mjs');
const { hex } = await import('./config-reference.mjs');
const { default: mermaid } = await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const { cases } = JSON.parse(readFileSync(new URL('../../tests/mermaid_regression/cases/er.json', import.meta.url)));
const rows = ['# Mermaid 12.2.0 official complex ER examples, original DB projection as UTF-16 JSON.'];
for (const test of cases) {
  const site = { theme: 'default', look: 'classic', fontFamily: 'Arial', ...test.config };
  const prepared = frontend(site)(test.source);
  mermaid.initialize({ ...prepared.effective, startOnLoad: false });
  const { db } = await mermaid.mermaidAPI.getDiagramFromText(prepared.code.cleaned);
  const data = db.getData();
  const result = {
    direction: db.getDirection(),
    entities: [...db.getEntities()].map(([name, e]) => ({ name, id: e.id, alias: e.alias,
      attributes: e.attributes, classes: e.cssClasses, styles: e.cssStyles,
      compiledStyles: e.cssCompiledStyles ?? [],
      parent: data.nodes.find(n => n.id === e.id)?.parentId ?? '' })),
    relationships: db.getRelationships(),
    groups: db.getSubGraphs().map(g => ({ id: g.id, title: g.title, direction: g.dir ?? '',
      members: g.nodes, classes: g.classes, styles: g.cssStyles,
      compiledStyles: data.nodes.find(n => n.id === g.id)?.cssCompiledStyles ?? [],
      parent: data.nodes.find(n => n.id === g.id)?.parentId ?? '' })),
  };
  rows.push([test.id, hex(test.source), hex(JSON.stringify(site)), hex(JSON.stringify(result))].join('\t'));
}
const target = new URL('../../tests/mermaid_cpp/fixtures/er-regressions.tsv', import.meta.url);
const output = rows.join('\n') + '\n';
if (writeFixture) writeFileSync(target, output);
else assert.equal(readFileSync(target, 'utf8').replaceAll('\r\n', '\n'), output);
dom.window.close();
console.log(`ER: ${cases.length} official complex parser/DB regression observations checked.`);
