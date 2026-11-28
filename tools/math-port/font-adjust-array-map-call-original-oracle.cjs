#!/usr/bin/env node
'use strict';

// Verify that FontData.adjustArrayIndices performs the ordinary list.map(...)
// call. A direct method call must not read the function object's own `call`
// property.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const root = path.resolve(__dirname, '../..');
const source = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/output/common/FontData.js');
const lockPath = path.join(root, 'tools/math-port/locks/font-data-source-lock.json');
const lock = JSON.parse(fs.readFileSync(lockPath, 'utf8'));
const relative = 'cjs/output/common/FontData.js';
const pinned = lock.inputs.find(item => item.package === '@mathjax/src' && item.path === relative);
if (!pinned) throw new Error(`FontData source lock is missing ${relative}`);
const actualHash = crypto.createHash('sha256').update(fs.readFileSync(source)).digest('hex');
if (actualHash !== pinned.sha256) throw new Error(`Pinned FontData hash mismatch: ${actualHash}`);

const {FontData} = require(source);
const originalMap = Array.prototype.map;
const events = [];
let result;
let error;
try {
  const directMap = function(...args) {
    events.push(`map:${this === input ? 'receiver' : 'wrong-receiver'}`);
    return Reflect.apply(originalMap, this, args);
  };
  Object.defineProperty(directMap, 'call', {
    configurable: true,
    get() {
      events.push('map.call:get');
      throw new Error('Array.prototype.map.call was read');
    },
  });
  Array.prototype.map = directMap;
  const input = [0, -1, 2];
  result = FontData.adjustArrayIndices(input, 3);
} catch (caught) {
  error = {name: caught && caught.name, message: caught && caught.message};
} finally {
  Array.prototype.map = originalMap;
}

const observation = {
  source: 'MathJax 4.1.3 / fb987178c2d279a99b0db5ea02e751691703955e',
  sourceFile: {file: relative, sha256: actualHash},
  operation: 'FontData.adjustArrayIndices([0,-1,2], 3) with replacement Array.prototype.map whose own call getter throws',
  events,
  result: result === undefined ? {$:'undefined'} : result,
  error: error || null,
};
const output = path.join(root, 'build/math-port/font-adjust-array-map-call-original.json');
fs.mkdirSync(path.dirname(output), {recursive: true});
fs.writeFileSync(output, `${JSON.stringify(observation, null, 2)}\n`);
process.stdout.write(`${path.relative(root, output)}\n`);
