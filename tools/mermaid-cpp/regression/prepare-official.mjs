// Syntax/configuration validation only. It does not claim SVG/visual parity.
import {mkdirSync, writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {execFileSync} from 'node:child_process';
import {JSDOM} from '../oracle/node_modules/jsdom/lib/api.js';
import {loadCorpus, configuration, repository} from './corpus.mjs';

const {upstream, records, summary} = loadCorpus();
const output = resolve(process.argv[2] ?? 'build/mermaid-regression/official-prepare');
mkdirSync(output, {recursive: true});
const dom = new JSDOM('<!doctype html><body></body>');
globalThis.window = dom.window;
globalThis.document = dom.window.document;
globalThis.Option = dom.window.Option;
const {frontend} = await import('../frontend-oracle.mjs');
const {default: mermaid} = await import('../oracle/node_modules/mermaid/dist/mermaid.core.mjs');
const results = [];
for (const item of records) {
  if (item.family === 'zenuml') continue;
  try {
    const prepared = frontend(configuration(item))(item.source);
    mermaid.initialize({...prepared.effective, startOnLoad: false});
    const diagram = await mermaid.mermaidAPI.getDiagramFromText(prepared.code.cleaned,
      {title: prepared.title});
    results.push({id: item.id, family: item.family, type: diagram.type, ok: true});
  } catch (error) {
    results.push({id: item.id, family: item.family, ok: false, error: String(error.stack)});
  }
}
// The pinned external ZenUML parser has its own package and real AST oracle.
try {
  execFileSync(process.execPath, ['tools/mermaid-cpp/zenuml-regression-oracle.mjs'],
    {cwd: repository, stdio: 'inherit'});
  for (const item of records.filter(item => item.family === 'zenuml'))
    results.push({id: item.id, family: item.family, type: 'zenuml', ok: true});
} catch (error) {
  for (const item of records.filter(item => item.family === 'zenuml'))
    results.push({id: item.id, family: item.family, ok: false, error: String(error)});
}
dom.window.close();
const report = {schema: 1, stage: 'official-prepare', upstream, coverage: summary,
  officialSvgCompared: false, results, passed: results.filter(item => item.ok).length,
  total: records.length};
writeFileSync(resolve(output, 'report.json'), JSON.stringify(report, null, 2) + '\n');
for (const failure of results.filter(item => !item.ok)) console.error(failure.id, failure.error);
console.log(`${report.passed}/${report.total} official syntax/configuration cases passed`);
if (report.passed !== report.total) process.exitCode = 1;
