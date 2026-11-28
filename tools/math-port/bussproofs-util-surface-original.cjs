'use strict';

// Supplementary pinned view of BussproofsUtil exports and Configuration links.
// Kept separate so the locked 31-case corpus and its whole-output hash stay
// unchanged.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const root = path.resolve(__dirname, '../..');
const cjsRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const lock = JSON.parse(fs.readFileSync(path.join(root,
  'tools/math-port/locks/bussproofs-source-lock.json'), 'utf8'));
const hash = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
for (const file of lock.files) {
  assert.equal(hash(path.join(root, '.reference/MathJax-src', file.ts)), file.tsSha256, `Source ${file.ts}`);
  assert.equal(hash(path.join(cjsRoot, file.cjs)), file.cjsSha256, `Source ${file.cjs}`);
}
const load = name => require(path.join(cjsRoot, 'cjs', name));
const {BussproofsConfiguration} = load('input/tex/bussproofs/BussproofsConfiguration.js');
const util = load('input/tex/bussproofs/BussproofsUtil.js');
const exportNames = ['setProperty', 'getProperty', 'removeProperty', 'balanceRules',
  'makeBsprAttributes', 'saveDocument', 'clearDocument'];
const utilExports = exportNames.map(name => {
  const fn = util[name];
  assert.equal(typeof fn, 'function', `missing Source export ${name}`);
  return {name, functionName: fn.name, length: fn.length};
});
const nameFor = fn => exportNames.find(name => util[name] === fn) || null;
const processors = phase => (BussproofsConfiguration[phase] || []).map(([fn, priority]) => ({
  export: nameFor(fn), functionName: fn.name, length: fn.length, priority,
}));
const output = {
  source: lock.source,
  utilExports,
  configuration: {
    preprocessors: processors('preprocessors'),
    postprocessors: processors('postprocessors'),
  },
};
const destination = path.join(root, 'build/math-port/bussproofs-util-surface-original.json');
fs.mkdirSync(path.dirname(destination), {recursive: true});
const bytes = Buffer.from(`${JSON.stringify(output, null, 2)}\n`);
fs.writeFileSync(destination, bytes);
process.stdout.write(`${JSON.stringify({exports: utilExports.length,
  preprocessors: output.configuration.preprocessors.length,
  postprocessors: output.configuration.postprocessors.length,
  sha256: crypto.createHash('sha256').update(bytes).digest('hex'),
  output: path.relative(root, destination)})}\n`);
