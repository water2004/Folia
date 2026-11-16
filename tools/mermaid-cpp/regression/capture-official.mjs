// Capture complete SVGs from the pinned, unmodified official renderers.
// Packet and Info have no DOM measurement dependency. Other families require Chromium;
// an unavailable browser is a failed gate, never replaced with estimated boxes.
import assert from 'node:assert/strict';
import {mkdirSync, readFileSync, writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {JSDOM} from '../oracle/node_modules/jsdom/lib/api.js';
import {chromium} from '../oracle/node_modules/playwright-core/index.mjs';
import {loadCorpus, configuration, repository, hash} from './corpus.mjs';

const argument = name => process.argv.find(value => value.startsWith(`--${name}=`))?.slice(name.length + 3);
const output = resolve(argument('output') ?? 'build/mermaid-regression/official-svg');
const {upstream, records, summary} = loadCorpus();
const selected = argument('family');
assert(!selected || summary.some(item => item.family === selected), 'Unknown family selection');
const cases = records.filter(item => !selected || item.family === selected);
mkdirSync(output, {recursive: true});
const packagePath = resolve(repository, 'tools/mermaid-cpp/oracle/node_modules/mermaid');
assert.equal(JSON.parse(readFileSync(resolve(packagePath, 'package.json'))).version, upstream.version);
const results = [];
const measurementFreeFamilies = new Set(['packet', 'info']);
const corpusHashes = Object.fromEntries(summary.map(row => [row.family + '.json', row.sha256]));
const dom = new JSDOM('<!doctype html><html><body></body></html>');
globalThis.window = dom.window;
globalThis.document = dom.window.document;
globalThis.Option = dom.window.Option;
globalThis.CSSStyleSheet = dom.window.CSSStyleSheet;
const {frontend} = await import('../frontend-oracle.mjs');
const {default: mermaidNode} = await import('../oracle/node_modules/mermaid/dist/mermaid.core.mjs');

function save(item, svg, observation) {
  assert(svg.includes('<svg'), item.id + ': renderer did not return SVG');
  const svgFile = item.id + '.svg';
  writeFileSync(resolve(output, svgFile), svg);
  results.push({id: item.id, family: item.family, ok: true, sourceSha256: hash(item.source),
    svgFile, svgSha256: hash(svg), ...observation});
}
function fail(item, error, stage) {
  results.push({id: item.id, family: item.family, ok: false, stage, error: String(error.stack ?? error)});
}

const vennSets = new Map();
for (const item of cases) {
  if (measurementFreeFamilies.has(item.family)) {
    try {
      mermaidNode.initialize({...configuration(item), startOnLoad: false});
      const {svg} = await mermaidNode.render(item.id, item.source);
      save(item, svg, {renderer: 'official-mermaid-jsdom', geometryMeasurements: 'none-required', browser: false});
    } catch (error) { fail(item, error, `official-${item.family}-svg`); }
  } else if (item.family === 'venn') {
    // Only supply a deterministic random stream, exactly as the existing Venn
    // oracle does. The official parser determines the set count; no case IDs or
    // fixture coordinates participate in layout or comparison.
    const prepared = frontend(configuration(item))(item.source);
    mermaidNode.initialize({...prepared.effective, startOnLoad: false});
    const diagram = await mermaidNode.mermaidAPI.getDiagramFromText(prepared.code.cleaned);
    vennSets.set(item.id, new Set(diagram.db.getSubsetData().flatMap(row => row.sets)).size);
  }
}
dom.window.close();

let browser;
let browserVersion;
const browserCases = cases.filter(item => !measurementFreeFamilies.has(item.family));
try {
  if (browserCases.length) {
    const executablePath = argument('browser') ?? process.env.FOLIA_BROWSER ??
      (process.platform === 'win32' ? 'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe' : '/usr/bin/chromium');
    browser = await chromium.launch({executablePath, headless: true,
      args: ['--allow-file-access-from-files']});
    browserVersion = browser.version();
    const context = await browser.newContext({viewport: {width: 1600, height: 1200},
      screen: {width: 1600, height: 1200}, deviceScaleFactor: 1, timezoneId: 'UTC'});
    const page = await context.newPage();
    const mermaidUrl = pathToFileURL(resolve(packagePath, 'dist/mermaid.esm.mjs')).href;
    const zenumlUrl = pathToFileURL(resolve(repository,
      'build/mermaid-cpp/zenuml-source/package/dist/zenuml.esm.mjs')).href;
    const html = '<!doctype html><meta charset="utf-8"><title>Folia official Mermaid SVG capture</title>' +
      '<style>html,body{margin:0;padding:0}#diagram{width:1600px}</style><div id="diagram"></div>' +
      '<script type="module">' +
      `window.mermaid=(await import(${JSON.stringify(mermaidUrl)})).default;` +
      `window.zenumlUrl=${JSON.stringify(zenumlUrl)};window.oracleReady=true;</script>`;
    const harness = resolve(output, 'capture.html');
    writeFileSync(harness, html);
    await page.goto(pathToFileURL(harness).href);
    await page.waitForFunction(() => window.oracleReady, undefined, {timeout: 30000});
    for (const item of browserCases) {
      try {
        const captured = await page.evaluate(async ({item, config, sets}) => {
          const host = document.querySelector('#diagram');
          host.replaceChildren();
          const random = Math.random;
          let state = 1, draws = 0;
          if (item.family === 'venn' && sets >= 4) Math.random = () => {
            if (draws++ % (20 * sets) === 0) state = 1;
            state = (Math.imul(state, 1664525) + 1013904223) >>> 0;
            return state / 4294967296;
          };
          let svg;
          try {
            if (item.family === 'zenuml') {
              const {renderToSvg} = await import(window.zenumlUrl);
              ({svg} = renderToSvg(item.source.replace(/^\s*zenuml/, '')));
            } else {
              window.mermaid.initialize({...config, startOnLoad: false});
              ({svg} = await window.mermaid.render(item.id, item.source, host));
            }
          } finally { Math.random = random; }
          host.innerHTML = svg;
          await document.fonts.ready;
          const root = host.querySelector('svg');
          if (!root) throw new Error('Official renderer produced no SVG');
          const view = root.viewBox.baseVal;
          return {svg, view: {x: view.x, y: view.y, width: view.width, height: view.height},
            availableWidth: screen.availWidth, randomDraws: draws};
        }, {item, config: configuration(item), sets: vennSets.get(item.id) ?? 0});
        assert(captured.view.width > 0 && captured.view.height >= 0, item.id + ': invalid viewport');
        const {svg, ...observation} = captured;
        save(item, svg, {renderer: 'official-browser', browser: true, ...observation});
      } catch (error) { fail(item, error, 'official-browser-svg'); }
    }
  }
} catch (error) {
  for (const item of browserCases.filter(item => !results.some(row => row.id === item.id)))
    fail(item, error, 'browser-unavailable');
} finally {
  if (browser) await browser.close();
}
const report = {schema: 1, stage: 'official-svg', upstream, coverage: summary, corpusHashes,
  browserVersion, geometryTolerance: 1.5, results,
  passed: results.filter(row => row.ok).length, total: cases.length};
writeFileSync(resolve(output, 'report.json'), JSON.stringify(report, null, 2) + '\n');
console.log(`${report.passed}/${report.total} official SVG captures succeeded`);
for (const item of results.filter(row => !row.ok)) console.error(item.id + ': ' + item.stage);
if (report.passed !== report.total) process.exitCode = 1;
