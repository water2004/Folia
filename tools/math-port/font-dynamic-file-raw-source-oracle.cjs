#!/usr/bin/env node
'use strict';

// Source observation for raw DynamicFile fields, live extension-map lookup,
// and ordinary function invocation. This is independent of the registered
// G90 inputs and does not modify the pinned package.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const root = path.resolve(__dirname, '../..');
const {FontData} = require(path.join(root,
  'tools/mathjax-bundle/node_modules/@mathjax/src/cjs/output/common/FontData.js'));
const lock = JSON.parse(fs.readFileSync(path.join(root,
  'tools/math-port/locks/font-data-source-lock.json'), 'utf8'));
const files = ['cjs/output/common/FontData.js', 'cjs/output/common/Direction.js',
  'cjs/util/Options.js', 'cjs/util/Retries.js'];
const hashes = files.map(file => {
  const entry = lock.inputs.find(x => x.package === '@mathjax/src' && x.path === file);
  if (!entry) throw new Error(`Missing locked Source input ${file}`);
  const actual = crypto.createHash('sha256').update(fs.readFileSync(path.join(
    root, 'tools/mathjax-bundle/node_modules/@mathjax/src', file))).digest('hex');
  if (actual !== entry.sha256) throw new Error(`Source hash mismatch for ${file}`);
  return {file, sha256: actual};
});

class ProbeFontData extends FontData {}
const order = [];
ProbeFontData.dynamicExtensions = new Map();
const font = new ProbeFontData();
font.addExtension({name: 'extension', ranges: [['arrow.js', {}, []]]});
const dynamicOwner = ProbeFontData.dynamicExtensions.get('extension');
dynamicOwner.prefix = './old';
const originalMap = new Map([['extension', dynamicOwner]]);
Object.defineProperty(originalMap, 'get', {configurable: true, get() {
  order.push('map.get');
  const fn = function(key) { order.push(`map.invoke:${key}`); return Map.prototype.get.call(this, key); };
  Object.defineProperty(fn, 'call', {get() { order.push('map.call.get'); throw Error('unexpected function.call read'); }});
  return fn;
}});
ProbeFontData.dynamicExtensions = originalMap;
const file = originalMap.get('extension').files['arrow.js'];
order.length = 0;

let extensionReads = 0;
let fileReads = 0;
const fileRecord = {
  get extension() { order.push(`extension.get:${++extensionReads}`); return 'extension'; },
  get file() {
    order.push(`file.get:${++fileReads}`);
    return {
      get match() { order.push('file.match.get'); return function(pattern) {
        order.push('file.match.invoke'); return false;
      }; },
      get replace() { order.push('file.replace.get'); return function(pattern, replacement) {
        order.push('file.replace.invoke'); return 'custom.js';
      }; }
    };
  }
};
Object.defineProperty(file, 'extension', {configurable: true, get() { return fileRecord.extension; }});
Object.defineProperty(file, 'file', {configurable: true, get() { return fileRecord.file; }});
let result;
let error;
try { result = font.dynamicFileName(file); }
catch (e) { error = {name: e.name, message: e.message}; }
const firstOrder = order.slice();

// Replacing the class's Map is observable on the next invocation. The old
// object remains unchanged for existing aliases; the new call reads the new
// map and its current get method.
const replacementMap = new Map([['extension', {prefix: './new'}]]);
ProbeFontData.dynamicExtensions = replacementMap;
const replacement = {};
Object.defineProperty(replacement, 'match', {value: () => false});
Object.defineProperty(replacement, 'replace', {value: () => 'new.js'});
const replacementFile = {extension: 'extension', file: replacement};
let replacementResult;
let replacementError;
try { replacementResult = font.dynamicFileName(replacementFile); }
catch (e) { replacementError = {name: e.name, message: e.message}; }
const mapWasReplaced = ProbeFontData.dynamicExtensions === replacementMap;

// A getter may return a different key at each Source read. The first read is
// only the truthiness check; the second read is passed to Map.get after the
// dynamicExtensions property and its get method have been read.
const changingOrder = [];
let changingExtensionReads = 0;
const changingMap = new Map([['second-key', {prefix: './second'}]]);
Object.defineProperty(changingMap, 'get', {configurable: true, get() {
  changingOrder.push('map.get');
  return function(key) {
    changingOrder.push(`map.invoke:${key}`);
    return Map.prototype.get.call(this, key);
  };
}});
ProbeFontData.dynamicExtensions = changingMap;
const changingRecord = {
  get extension() {
    changingExtensionReads++;
    changingOrder.push(`extension.get:${changingExtensionReads}`);
    return changingExtensionReads === 1 ? 'first-key' : 'second-key';
  },
  file: 'relative.js'
};
let changingResult;
let changingError;
try { changingResult = font.dynamicFileName(changingRecord); }
catch (e) { changingError = {name: e.name, message: e.message}; }
const changingExtension = {
  result: changingResult === undefined ? null : changingResult,
  error: changingError || null,
  reads: changingExtensionReads,
  order: changingOrder.slice()
};

const rawReturnOrder = [];
let rawReturnFileReads = 0;
const matchingFile = {
  get match() {
    rawReturnOrder.push('match.get');
    return function() { rawReturnOrder.push('match.call'); return true; };
  },
  get replace() { rawReturnOrder.push('replace.get'); throw new Error('replace must not be read'); }
};
const rawReturnRecord = {
  extension: 'second-key',
  get file() { rawReturnOrder.push(`file.get:${++rawReturnFileReads}`); return matchingFile; }
};
let rawReturnValue;
let rawReturnError;
try { rawReturnValue = font.dynamicFileName(rawReturnRecord); }
catch (e) { rawReturnError = {name: e.name, message: e.message}; }
const rawReturnIdentity = {
  resultIsInput: rawReturnValue === matchingFile,
  error: rawReturnError || null,
  fileReads: rawReturnFileReads,
  order: rawReturnOrder
};

// Verify ordinary descriptor replacement against a real Source DynamicFile.
// The first accessor replaces the writable native-backed data field; the
// following data definition is then the sole property value authority.
font.addExtension({name: 'descriptor', ranges: [['descriptor.js', {}, []]]});
const descriptorFile = ProbeFontData.dynamicExtensions.get('descriptor').files['descriptor.js'];
const descriptorOrder = [];
Object.defineProperty(descriptorFile, 'file', {configurable: true, enumerable: true,
  get() { descriptorOrder.push('file.get'); return 'accessor.js'; }});
const accessorValue = descriptorFile.file;
Object.defineProperty(descriptorFile, 'file', {configurable: true, enumerable: true,
  writable: true, value: 'replaced.js'});
const afterDataReplacement = descriptorFile.file;
const deleted = delete descriptorFile.file;
const absentAfterDelete = !Object.prototype.hasOwnProperty.call(descriptorFile, 'file');
descriptorFile.file = 'assigned.js';
const descriptorReplacement = {
  accessorValue,
  afterDataReplacement,
  deleted,
  absentAfterDelete,
  afterAssignment: descriptorFile.file,
  descriptor: Object.getOwnPropertyDescriptor(descriptorFile, 'file'),
  order: descriptorOrder
};

const throwingOrder = [];
const throwingRecord = {
  get extension() { throwingOrder.push('extension.get'); return 'second-key'; },
  get file() { throwingOrder.push('file.get'); throw new Error('file getter failed'); }
};
let throwingError;
try { font.dynamicFileName(throwingRecord); }
catch (e) { throwingError = {name: e.name, message: e.message}; }
const throwingGetter = {error: throwingError || null, order: throwingOrder};

const output = {
  source: lock.source,
  hashes,
  first: {result: result === undefined ? null : result, error: error || null,
    extensionReads, fileReads, order: firstOrder},
  mapReplacement: {
    replaced: mapWasReplaced,
    oldMapStillHasOriginal: originalMap.get('extension').prefix === './old',
    result: replacementResult === undefined ? null : replacementResult,
    error: replacementError || null,
  },
  changingExtension,
  rawReturnIdentity,
  descriptorReplacement,
  throwingGetter
};
const outputPath = path.join(root, 'build/math-port/font-dynamic-file-raw-source.json');
fs.mkdirSync(path.dirname(outputPath), {recursive: true});
fs.writeFileSync(outputPath, `${JSON.stringify(output, null, 2)}\n`);
process.stdout.write(`${path.relative(root, outputPath)}\n`);
