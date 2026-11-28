'use strict';

// Full pinned observations for MathJax 4.1.3 ColortblConfiguration.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '../..');
const sourceRoot = path.join(root, '.reference/MathJax-src');
const cjsRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const lockPath = path.join(root, 'tools/math-port/locks/colortbl-source-lock.json');
const lock = JSON.parse(fs.readFileSync(lockPath, 'utf8'));
const shaFile = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
for (const file of lock.files) {
  assert.equal(shaFile(path.join(sourceRoot, file.ts)), file.tsSha256, `Source ${file.ts}`);
  assert.equal(shaFile(path.join(cjsRoot, file.cjs)), file.cjsSha256, `Source ${file.cjs}`);
}

const load = name => require(path.join(cjsRoot, 'cjs', name));
const {TeX} = load('input/tex.js');
const {MmlFactory} = load('core/MmlTree/MmlFactory.js');
const {AbstractMathItem} = load('core/MathItem.js');
const {MapHandler} = load('input/tex/MapHandler.js');
const {ConfigurationHandler} = load('input/tex/Configuration.js');
const {ColorArrayItem} = load('input/tex/colortbl/ColortblConfiguration.js');
load('input/tex/color/ColorConfiguration.js');
load('input/tex/colortbl/ColortblConfiguration.js');

function plain(value, seen = new WeakSet()) {
  if (value === undefined) return {$: 'undefined'};
  if (value === null || typeof value === 'string' || typeof value === 'boolean') return value;
  if (typeof value === 'number') return Number.isNaN(value) ? {$: 'NaN'} :
    Object.is(value, -0) ? {$: '-0'} : Number.isFinite(value) ? value :
      {$: value < 0 ? '-Infinity' : 'Infinity'};
  if (typeof value === 'function') return {$: 'function', name: value.name || '', length: value.length};
  if (typeof value !== 'object') return String(value);
  if (seen.has(value)) return {$: 'cycle'};
  seen.add(value);
  if (Array.isArray(value)) return Array.from({length: value.length}, (_, i) =>
    Object.hasOwn(value, i) ? plain(value[i], seen) : {$: 'hole'});
  if (value instanceof Map) return {$: 'map', entries: [...value].map(([key, item]) =>
    [plain(key, seen), plain(item, seen)])};
  const result = {};
  for (const key of Reflect.ownKeys(value)) {
    result[typeof key === 'symbol' ? `symbol:${key.description || ''}` : key] = plain(value[key], seen);
  }
  return result;
}

function tree(node) {
  return node == null ? null : {
    kind: node.kind,
    attrs: plain(node.attributes?.getAllAttributes?.() ?? {}),
    properties: plain(node.getAllProperties?.() ?? {}),
    ...(typeof node.getText === 'function' ? {text: node.getText()} : {}),
    children: (node.childNodes || []).map(tree),
  };
}

function descriptor(value) {
  const result = {configurable: value.configurable, enumerable: value.enumerable};
  if (Object.hasOwn(value, 'value')) {
    result.value = plain(value.value);
    result.writable = value.writable;
  } else {
    result.get = plain(value.get);
    result.set = plain(value.set);
  }
  return result;
}

const inputs = [
  ['cell-basic', String.raw`\begin{array}{cc}\cellcolor{red}a&b\\c&d\end{array}`, ['base', 'colortbl'], {}],
  ['cell-all-cells', String.raw`\begin{array}{cc}\cellcolor{blue}a&\cellcolor{green}b\\\cellcolor{yellow}c&\cellcolor{cyan}d\end{array}`, ['base', 'colortbl'], {}],
  ['cell-over-row-and-column', String.raw`\begin{array}{c}\rowcolor{blue}\columncolor{red}\cellcolor{green}x\\y\end{array}`, ['base', 'colortbl'], {}],
  ['row-first-cell', String.raw`\begin{array}{cc}\rowcolor{red}a&b\\c&d\end{array}`, ['base', 'colortbl'], {}],
  ['row-color-resets', String.raw`\begin{array}{c}\rowcolor{red}a\\b\\\rowcolor{blue}c\end{array}`, ['base', 'colortbl'], {}],
  ['row-color-after-cell', String.raw`\begin{array}{c}a\rowcolor{red}b\end{array}`, ['base', 'colortbl'], {}],
  ['row-color-after-empty-entry', String.raw`\begin{array}{c}&\rowcolor{red}a\end{array}`, ['base', 'colortbl'], {}],
  ['column-color-preamble', String.raw`\begin{array}{>{\columncolor{red}}c}a\\b\end{array}`, ['base', 'colortbl'], {}],
  ['column-color-before-first-cell', String.raw`\begin{array}{c}\columncolor{red}a\\b\end{array}`, ['base', 'colortbl'], {}],
  ['column-color-second-column', String.raw`\begin{array}{cc}a&\columncolor{red}b\\c&d\end{array}`, ['base', 'colortbl'], {}],
  ['column-color-same-as-preamble', String.raw`\begin{array}{c}\columncolor{red}a&\columncolor{red}b\end{array}`, ['base', 'colortbl'], {}],
  ['column-color-different-after-table-start', String.raw`\begin{array}{c}a\\\columncolor{red}b\end{array}`, ['base', 'colortbl'], {}],
  ['column-color-overlap-arguments', String.raw`\begin{array}{c}\columncolor{red}[1pt][2pt]a\end{array}`, ['base', 'colortbl'], {}],
  ['cell-color-optional-model', String.raw`\begin{array}{c}\cellcolor[rgb]{1,0,0}a\end{array}`, ['base', 'colortbl'], {}],
  ['cell-color-custom-definition', String.raw`\definecolor{custom}{rgb}{0,0.5,1}\begin{array}{c}\cellcolor{custom}a\end{array}`, ['base', 'color', 'colortbl'], {}],
  ['cell-color-unknown-name', String.raw`\begin{array}{c}\cellcolor{noSuchColor}a\end{array}`, ['base', 'colortbl'], {}],
  ['cell-color-invalid-model', String.raw`\begin{array}{c}\cellcolor[noSuchModel]{x}a\end{array}`, ['base', 'colortbl'], {}],
  ['cell-color-outside-array', String.raw`\cellcolor{red}x`, ['base', 'colortbl'], {}],
  ['row-color-outside-array', String.raw`\rowcolor{red}x`, ['base', 'colortbl'], {}],
  ['column-color-outside-array', String.raw`\columncolor{red}x`, ['base', 'colortbl'], {}],
  ['cell-color-missing-argument', String.raw`\begin{array}{c}\cellcolor`, ['base', 'colortbl'], {}],
  ['row-color-missing-argument', String.raw`\begin{array}{c}\rowcolor`, ['base', 'colortbl'], {}],
  ['column-color-missing-argument', String.raw`\begin{array}{c}\columncolor`, ['base', 'colortbl'], {}],
  ['colored-array-framed', String.raw`\begin{array}{|c|}\hline\cellcolor{red}a\\\hline\end{array}`, ['base', 'colortbl'], {}],
  ['colored-array-fenced', String.raw`\left(\begin{array}{c}\rowcolor{red}a\\b\end{array}\right)`, ['base', 'colortbl'], {}],
  ['colored-array-in-mstyle', String.raw`\scriptstyle\begin{array}{c}\cellcolor{red}x\end{array}`, ['base', 'colortbl'], {}],
  ['color-package-explicit', String.raw`\begin{array}{c}\cellcolor{red}x\end{array}`, ['base', 'color', 'colortbl'], {}],
  ['color-package-not-listed', String.raw`\begin{array}{c}\cellcolor{red}x\end{array}`, ['base', 'colortbl'], {}],
  ['column-color-top-row-repeat', String.raw`\begin{array}{cc}\columncolor{red}a&\columncolor{red}b\end{array}`, ['base', 'colortbl'], {}],
];

const cases = inputs.map(([name, input, packages, settings]) => {
  let jax = null;
  let result = null;
  let error = null;
  try {
    jax = new TeX({...settings, packages});
    jax.setMmlFactory(new MmlFactory());
    const item = new AbstractMathItem(input, jax, true);
    result = tree(jax.compile(item, {}));
  } catch (caught) {
    error = {name: caught.name || caught.constructor.name, id: caught.id ?? null,
      message: String(caught.message)};
  }
  return {name, input, packages, settings: plain(settings), result, error,
    packageData: jax ? plain(jax.parseOptions.packageData) : null};
});

const map = MapHandler.getMap('colortbl');
const configuration = ConfigurationHandler.get('colortbl');
const prototype = ColorArrayItem.prototype;
const classInfo = {
  name: ColorArrayItem.name,
  length: ColorArrayItem.length,
  prototype: Object.getOwnPropertyNames(prototype).map(name =>
    [name, descriptor(Object.getOwnPropertyDescriptor(prototype, name))]),
  instance: (() => {
    const item = new ColorArrayItem();
    return {kind: item.kind, isOpen: item.isOpen, color: plain(item.color),
      hasColor: item.hasColor, rowLength: item.row.length, tableLength: item.table.length};
  })(),
};
const configurationInfo = {
  name: configuration.name,
  handler: plain(configuration.handler),
  items: plain(configuration.items),
  priority: configuration.priority,
  configMethod: plain(configuration.configMethod),
  config: plain(configuration.config),
};
const output = {schema: 1, source: lock.source, files: lock.files,
  class: classInfo, configuration: configurationInfo,
  maps: {colortbl: map ? plain(map.map) : {$: 'missing-map'}}, cases};
const bytes = Buffer.from(`${JSON.stringify(output, null, 2)}\n`);
const outputSha256 = crypto.createHash('sha256').update(bytes).digest('hex');
assert.equal(outputSha256, lock.oracleOutputSha256, 'Colortbl Source oracle output');
const destination = path.join(root, 'build/math-port/colortbl-original.json');
fs.mkdirSync(path.dirname(destination), {recursive: true});
fs.writeFileSync(destination, bytes);
process.stdout.write(`${JSON.stringify({cases: cases.length, maps: Object.keys(output.maps).length,
  sourceOutputSha256: outputSha256,
  output: path.relative(root, destination)})}\n`);
