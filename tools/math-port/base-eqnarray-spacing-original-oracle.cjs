'use strict';

// Focused Source observations for BaseMethods.EqnArray's spacing default.
// This supplement leaves the pinned Mathtools package corpus untouched.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const root = path.resolve(__dirname, '../..');
const sourceRoot = path.join(root, '.reference/MathJax-src');
const cjsRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const hash = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const tsPath = 'ts/input/tex/base/BaseMethods.ts';
const cjsPath = 'cjs/input/tex/base/BaseMethods.js';
const load = name => require(path.join(cjsRoot, 'cjs', name));
const BaseMethods = load('input/tex/base/BaseMethods.js').default;
const { TeX } = load('input/tex.js');
const { AbstractMathItem } = load('core/MathItem.js');
const { MmlFactory } = load('core/MmlTree/MmlFactory.js');
load('input/tex/ams/AmsConfiguration.js');
load('input/tex/mathtools/MathtoolsConfiguration.js');

function observeMethod(name, spacing, supplied) {
  let created;
  const parser = {
    options: {tagSide: 'right', tagIndent: '0.8em'},
    stack: {global: {}},
    Push() {},
    itemFactory: {
      create(kind, environment, numbered, taggable, global) {
        created = {kind, environment, numbered, taggable, global,
          properties: {}, setProperty(key, value) { this.properties[key] = value; }};
        return created;
      },
    },
  };
  const begin = {getName: () => 'align'};
  const args = [parser, begin, false, false, 'c', ''];
  if (supplied) args.push(spacing);
  BaseMethods.EqnArray(...args);
  return {name, arraydef: created.arraydef};
}

function plain(value, seen = new WeakSet()) {
  if (value === undefined) return {$: 'undefined'};
  if (value === null || typeof value !== 'object') return value;
  if (seen.has(value)) return {$: 'cycle'};
  seen.add(value);
  if (Array.isArray(value)) return value.map(item => plain(item, seen));
  const result = {};
  for (const key of Reflect.ownKeys(value)) result[String(key)] = plain(value[key], seen);
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

function compile(name, input) {
  const jax = new TeX({packages: ['base', 'ams', 'mathtools']});
  jax.setMmlFactory(new MmlFactory());
  try {
    const result = tree(jax.compile(new AbstractMathItem(input, jax, true), {}));
    return {name, input, result, error: null};
  } catch (error) {
    return {name, input, result: null, error: {
      name: error.name || error.constructor.name,
      id: error.id ?? null,
      message: String(error.message),
    }};
  }
}

const spacingCases = [
  observeMethod('omitted', undefined, false),
  observeMethod('explicit-undefined', undefined, true),
  observeMethod('null', null, true),
  observeMethod('empty-string', '', true),
  observeMethod('zero', 0, true),
  observeMethod('false', false, true),
  observeMethod('ordinary', '0em 0em', true),
];
const formulas = [
  compile('dcases', String.raw`\begin{dcases}x&x>0\\-x&x<0\end{dcases}`),
  compile('lgathered', String.raw`\begin{lgathered}a\\b\end{lgathered}`),
];
const source = 'MathJax 4.1.3 fb987178c2d279a99b0db5ea02e751691703955e';
const result = {schema: 1, source, files: [
  {ts: tsPath, tsSha256: hash(fs.readFileSync(path.join(sourceRoot, tsPath))),
    cjs: cjsPath, cjsSha256: hash(fs.readFileSync(path.join(cjsRoot, cjsPath)))},
], method: 'BaseMethods.EqnArray', spacingCases, formulas};
const output = path.join(root, 'build/math-port/base-eqnarray-spacing-original.json');
fs.mkdirSync(path.dirname(output), {recursive: true});
const bytes = Buffer.from(`${JSON.stringify(result, null, 2)}\n`);
fs.writeFileSync(output, bytes);
const lock = {schema: 1, source, sourceOutputSha256: hash(bytes), files: result.files};
const lockPath = path.join(root, 'tools/math-port/locks/base-eqnarray-spacing-source-lock.json');
fs.writeFileSync(lockPath, `${JSON.stringify(lock, null, 2)}\n`);
process.stdout.write(`${JSON.stringify({cases: spacingCases.length, formulas: formulas.length,
  output: path.relative(root, output), sha256: lock.sourceOutputSha256})}\n`);
