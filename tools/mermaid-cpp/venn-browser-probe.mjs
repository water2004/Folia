import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { existsSync, mkdirSync, readFileSync, realpathSync, writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { dirname, resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { chromium } from './oracle/node_modules/playwright-core/index.mjs';
import { cases, invalid } from './venn-cases.mjs';

import inspectVenn from './oracle/observations/venn.mjs';
export {inspectVenn};

export const edge = process.env.FOLIA_BROWSER
  ?? 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe';

function verifyLayout(mermaidFile) {
  const {vennLayout: lock, stateBundle} = JSON.parse(readFileSync('docs/mermaid-cpp/source-lock.json', 'utf8'));
  const fromMermaid = createRequire(realpathSync(mermaidFile));
  // This package has no package.json export and its require export targets an
  // absent file. Search Node's module directories from Mermaid's real path.
  const packageFile = fromMermaid.resolve.paths(lock.package)
    .map(directory => resolve(directory, lock.package, 'package.json'))
    .find(existsSync);
  assert(packageFile, 'Mermaid Venn layout dependency missing');
  const installed = JSON.parse(readFileSync(packageFile, 'utf8'));
  assert.equal(installed.version, lock.version);
  for (const {path, sha256} of lock.files) {
    const bytes = readFileSync(resolve(dirname(packageFile), path));
    assert.equal(createHash('sha256').update(bytes).digest('hex'), sha256, path);
  }
  const {path, sourceMap, sha256} = stateBundle.files.find(file => file.path.endsWith('/rough.esm.js'));
  const map = JSON.parse(readFileSync(resolve(dirname(mermaidFile), 'chunks/mermaid.esm', sourceMap), 'utf8'));
  const source = map.sourcesContent[map.sources.indexOf(path)];
  assert.equal(createHash('sha256').update(source).digest('hex'), sha256, path);
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
  const directory = resolve(process.argv[2] ?? 'build/mermaid-cpp/venn-browser');
  mkdirSync(directory, {recursive: true});
  const mermaidFile = resolve('tools/mermaid-cpp/oracle/node_modules/mermaid/dist/mermaid.esm.mjs');
  verifyLayout(mermaidFile);
  const url = pathToFileURL(mermaidFile).href;
  const html = '<!doctype html><meta charset="utf-8"><body></body>';
  const file = resolve(directory, 'probe.html');
  writeFileSync(file, html);
  const browser = await chromium.launch({
    executablePath: edge, headless: true, args: ['--allow-file-access-from-files'],
  });
  let rows;
  try {
    const page = await browser.newPage();
    await page.goto(pathToFileURL(file).href);
    await page.evaluate(async url => {
      const {default: mermaid} = await import(url);
      globalThis.mermaid = mermaid;
    }, url);
    await page.evaluate('globalThis.inspectVenn = ' + inspectVenn.toString());
    rows = await page.evaluate(async ({cases, invalid}) => {
      const rows = [];
      for (const theme of ['default', 'dark', 'redux-color', 'redux-dark-color']) {
        mermaid.initialize({startOnLoad: false, fontFamily: 'Arial', look: 'classic', theme});
        for (const [name, source] of cases) {
          if (theme.startsWith('redux') && name !== 'styled') continue;
          const random = Math.random;
          const sets = name === 'four' || name === 'four-mds' ? 4 : name === 'five-disjoint' ? 5 : 0;
          let count = 0;
          let state = 1;
          if (sets) {
            Math.random = () => {
              // Venn.js uses 10 starts of two coordinates per set, and
              // Mermaid calls the optimizer twice. Replay the native stream.
              if (count++ % (20 * sets) === 0) state = 1;
              state = (Math.imul(state, 1664525) + 1013904223) >>> 0;
              return state / 4294967296;
            };
          }
          let svg;
          try {
            ({svg} = await mermaid.render('venn' + rows.length, source));
          } finally {
            Math.random = random;
          }
          if (sets && count !== 40 * sets) {
            throw new Error('Unexpected Venn layout random draw count for ' + name + ': ' + count);
          }
          const host = document.createElement('div');
          host.innerHTML = svg;
          document.body.append(host);
          rows.push({name: 'venn-' + name + '-' + theme, svg, ...inspectVenn(host)});
          host.remove();
        }
        for (const source of invalid) {
          if (await mermaid.parse(source, {suppressErrors: true})) throw new Error('Invalid source parsed: ' + source);
        }
      }
      return rows;
    }, {cases, invalid});
  } finally {
    await browser.close();
  }
  assert.equal(rows.length, cases.length * 2 + 2);
  writeFileSync(resolve(directory, 'original.json'), JSON.stringify(rows, null, 2));
  for (const row of rows) writeFileSync(resolve(directory, row.name + '.svg'), row.svg);
  console.log('Captured ' + rows.length + ' Mermaid 12.2.0 Venn SVGs in ' + directory);
}
