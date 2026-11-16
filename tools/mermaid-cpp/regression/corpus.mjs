import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync, readdirSync} from 'node:fs';
import {resolve, basename} from 'node:path';
import {fileURLToPath} from 'node:url';

export const repository = resolve(fileURLToPath(new URL('../../..', import.meta.url)));
export const upstream = Object.freeze({version: '12.2.0',
  commit: '1fb7869866dd619168d840291c9c54b4f129758b'});
export const families = Object.freeze(['agentflow', 'architecture', 'block', 'c4', 'class',
  'cynefin', 'er', 'eventmodeling', 'flowchart', 'gantt', 'gitgraph', 'info', 'ishikawa',
  'journey', 'kanban', 'mindmap', 'packet', 'pie', 'quadrant', 'radar', 'railroad',
  'requirement', 'sankey', 'sequence', 'state', 'swimlane', 'timeline', 'tree-view',
  'treemap', 'usecase', 'venn', 'wardley', 'xy', 'zenuml']);
export const hash = value => createHash('sha256').update(value).digest('hex');

export function loadCorpus() {
  const directory = resolve(repository, 'tests/mermaid_regression/cases');
  const paths = readdirSync(directory).filter(name => name.endsWith('.json')).sort();
  assert.deepEqual(paths.map(name => basename(name, '.json')), families,
    'Every supported family must have a corpus; missing files must fail the gate');
  const ids = new Set(), records = [], summary = [];
  for (const path of paths) {
    const bytes = readFileSync(resolve(directory, path));
    const document = JSON.parse(bytes);
    assert.equal(document.family, basename(path, '.json'));
    assert(document.cases.length >= 8, `${path}: at least eight distinct regression cases required`);
    let complex = 0;
    const inputs = new Set();
    for (const item of document.cases) {
      assert.match(item.id, /^[a-z0-9][a-z0-9-]*$/);
      assert(!ids.has(item.id), `Duplicate case id ${item.id}`);
      ids.add(item.id);
      assert(['basic', 'intermediate', 'complex'].includes(item.level), `${item.id}: invalid level`);
      complex += item.level === 'complex';
      assert(typeof item.source === 'string' && item.source.trim(), `${item.id}: empty source`);
      assert(Array.isArray(item.features) && item.features.length, `${item.id}: missing coverage features`);
      assert(item.upstream?.path && item.upstream?.section, `${item.id}: missing official provenance`);
      assert(!Object.hasOwn(item, 'skip') && !Object.hasOwn(item, 'expectedFailure'),
        `${item.id}: valid diagram regressions cannot be silently skipped or expected to fail`);
      const input = JSON.stringify([item.source.trim(), item.config ?? {}]);
      assert(!inputs.has(input), `${item.id}: duplicate input does not add coverage`);
      inputs.add(input);
      records.push({family: document.family, ...item});
    }
    if (document.family !== 'info') assert(complex >= 2, `${path}: multiple complex regressions required`);
    summary.push({family: document.family, cases: document.cases.length, complex,
      features: [...new Set(document.cases.flatMap(item => item.features))].sort(),
      sha256: hash(bytes)});
  }
  return {upstream, records, summary};
}

// Match the app's normal request initialization. Source frontmatter and per-case
// settings retain their normal precedence, including explicit nonclassic looks.
export const configuration = item => ({theme: 'default', look: 'classic', ...item.config});

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const {records, summary} = loadCorpus();
  console.log(JSON.stringify({families: summary.length, cases: records.length,
    complex: summary.reduce((count, row) => count + row.complex, 0), coverage: summary}, null, 2));
}
