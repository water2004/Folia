// Mermaid 12.2.0 requirement parser / RequirementDB oracle.
import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
import { JSDOM } from './oracle/node_modules/jsdom/lib/api.js';
import { cases } from './requirement-cases.mjs';
const write = process.argv.includes('--write');
const dom = new JSDOM('<!doctype html><body></body>');
globalThis.window = dom.window; globalThis.document = dom.window.document;
const { default: mermaid } = await import('./oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const toHex = (s) => Array.from({ length: s.length }, (_, i) => s.charCodeAt(i).toString(16).padStart(4, '0')).join('');
const rows = ['# Mermaid 12.2.0 requirement parser/DB; UTF-16 hex JSON.'];
for (const [name, source, failure] of cases) {
  let result = null;
  try {
    mermaid.initialize({ startOnLoad: false, theme: 'default', themeVariables: { fontFamily: 'Arial' } });
    const { db } = await mermaid.mermaidAPI.getDiagramFromText(source);
    result = {
      direction: db.getDirection(), title: db.getDiagramTitle(), accTitle: db.getAccTitle(), accDescription: db.getAccDescription(),
      requirements: [...db.getRequirements().values()], elements: [...db.getElements().values()], relationships: db.getRelationships(), classes: [...db.getClasses().values()],
    };
    assert.equal(failure, null, `${name}: expected syntax error`);
  } catch (error) {
    assert.notEqual(failure, null, `${name}: ${error.stack}`);
  }
  rows.push([name, toHex(source), toHex('{}'), toHex(JSON.stringify(result))].join('\t'));
}
const path = new URL('../../tests/mermaid_cpp/fixtures/requirement.tsv', import.meta.url);
const output = rows.join('\n') + '\n';
if (write) writeFileSync(path, output); else assert.equal(readFileSync(path, 'utf8').replaceAll('\r\n', '\n'), output);
dom.window.close();
console.log(`Requirement: ${cases.length} Mermaid parser/DB observations checked.`);
