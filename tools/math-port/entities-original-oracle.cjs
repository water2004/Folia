#!/usr/bin/env node
'use strict';

const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');

const root = path.resolve(__dirname, '../../tools/mathjax-bundle/node_modules/@mathjax/src');
const packageJson = require(path.join(root, 'package.json'));
assert.equal(packageJson.version, '4.1.3');
const entitiesPath = path.join(root, 'cjs/util/Entities.js');
const shardDir = path.join(root, 'cjs/util/entities');
const mathjaxPath = path.join(root, 'cjs/mathjax.js');
const shardNames = [...'abcdefghijklmnopqrstuvwxyz', 'fr', 'opf', 'scr'];

function freshEntities() {
  for (const key of Object.keys(require.cache)) {
    if (key === entitiesPath || (key.startsWith(shardDir + path.sep) && key.endsWith('.js'))) {
      delete require.cache[key];
    }
  }
  return require(entitiesPath);
}
function sha(file) {
  return crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
}
function tableFor(name) {
  const api = freshEntities();
  const before = Object.assign(Object.create(null), api.entities);
  if (name !== 'base') require(path.join(shardDir, name + '.js'));
  const additions = Object.create(null);
  for (const key of Object.keys(api.entities)) if (before[key] !== api.entities[key]) additions[key] = api.entities[key];
  return { additions, full: api.entities };
}
const base = tableFor('base');
const files = ['Entities.js', ...shardNames.map((name) => `entities/${name}.js`)];
const tsFiles = ['Entities.ts', ...shardNames.map((name) => `entities/${name}.ts`)];
const data = {
  package: packageJson.version,
  source: {
    entitiesTs: sha(path.resolve(__dirname, '../../.reference/MathJax-src/ts/util/Entities.ts')),
    cjs: Object.fromEntries(files.map((f) => [f, sha(path.join(root, 'cjs/util', f))])),
    ts: Object.fromEntries(tsFiles.map((f) => [f, sha(path.resolve(__dirname, '../../.reference/MathJax-src/ts/util', f))])),
  },
  base: base.full,
  shards: {},
};
const sourceLock = JSON.parse(fs.readFileSync(
  path.resolve(__dirname, '../../tools/math-port/locks/entities-source-lock.json'), 'utf8'));
assert.deepEqual({ package: data.package, source: data.source }, sourceLock,
  'pinned MathJax Entities TypeScript/CJS source lock changed');
for (const name of shardNames) data.shards[name] = tableFor(name).additions;

// Behavioral observations keep lazy loading separate from the complete table
// manifest. They prove Source's loaded-before-request policy through requests.
const run = async (label, setup, loader = (name) => {
  const file = path.resolve(root, 'cjs', name);
  require(file);
  return Promise.resolve();
}) => {
  const mathjax = require(mathjaxPath).mathjax;
  const requests = [];
  const original = mathjax.asyncLoad;
  mathjax.asyncLoad = loader === null ? undefined : (name) => {
    requests.push(name);
    return loader(name);
  };
  let result = null;
  let retry = null;
  try {
    const api = freshEntities();
    try { result = await setup(api, requests); }
    catch (error) {
      retry = { name: error.name, message: error.message, hasRetryPromise: error.retry instanceof Promise };
      if (error.retry instanceof Promise) {
        try { await error.retry; retry.settled = 'fulfilled'; }
        catch (failure) { retry.settled = 'rejected'; retry.failure = String(failure); }
      }
    }
    return { label, requests, retry, result };
  } finally { mathjax.asyncLoad = original; }
};
function apiTranslate(input) { const api=freshEntities();api.options.loadMissingEntities=false;return api.translate(input); }
data.observations = {
  directNumeric: ['0', '65', 'x41', 'x1F600', 'xD800', 'x110000', 'X41', '', '1.9', '999999999999999999999999999999999999999999'].map((entity) => {
    try { return [entity, freshEntities().numeric(entity)]; }
    catch (error) { return [entity, { name: error.name, message: error.message }]; }
  }),
  numeric: ['&#65;', '&#x41;', '&#x1F600;', '&#0;', '&#xD800;'].map((input) => {
    try { return [input, apiTranslate(input)]; }
    catch (error) { return [input, { name: error.name, message: error.message }]; }
  }),
  replacement: ['&alpha;', '&Alpha;', '&unknown;', '&amp;', '&bad_name;', '&#xZZ;'].map((input) => {
    try { return [input, apiTranslate(input)]; }
    catch (error) { return [input, { name: error.name, message: error.message }]; }
  }),
  mutation: (() => {
    const api = freshEntities();
    const identity = api.entities;
    api.add({ AddedByProbe: 'Ω', emptyByProbe: '' }, 'probe.js');
    api.options.loadMissingEntities = false;
    const afterAdd = [api.entities === identity, api.translate('&AddedByProbe;'), api.translate('&emptyByProbe;')];
    api.remove('AddedByProbe');
    return { afterAdd, afterRemove: api.translate('&AddedByProbe;'), keysRetained: api.entities === identity };
  })(),
};
async function main() {
  data.lazy = [
    await run('unloaded-a-once', async (api) => {
      let first;
      try { api.translate('&Aunknown;'); }
      catch (error) { first = { name: error.name, hasRetryPromise: error.retry instanceof Promise }; if (error.retry instanceof Promise) await error.retry; }
      return { first, second: api.translate('&Aunknown;') };
    }),
    await run('special-fr', async (api) => {
      let first;
      try { api.translate('&Afr;'); }
      catch (error) { first = { name: error.name, hasRetryPromise: error.retry instanceof Promise }; if (error.retry instanceof Promise) await error.retry; }
      return { first, second: api.translate('&Afr;') };
    }),
    await run('rejected-a-provider', async (api) => {
      let first;
      try { api.translate('&Aunknown;'); }
      catch (error) {
        first = { name: error.name, hasRetryPromise: error.retry instanceof Promise };
        if (error.retry instanceof Promise) {
          try { await error.retry; first.retry = 'fulfilled'; }
          catch (failure) { first.retry = { name: failure.name, message: String(failure.message || failure) }; }
        }
      }
      return { first, second: api.translate('&Aunknown;') };
    }, () => Promise.reject(new Error('entity-shard-load-failed'))),
    await run('missing-provider', async (api) => {
      let first;
      try { api.translate('&Aunknown;'); }
      catch (error) {
        first = { name: error.name, hasRetryPromise: error.retry instanceof Promise };
        if (error.retry instanceof Promise) {
          try { await error.retry; first.retry = 'fulfilled'; }
          catch (failure) { first.retry = String(failure); }
        }
      }
      return { first, second: api.translate('&Aunknown;') };
    }, null),
    await run('disabled', async (api) => { api.options.loadMissingEntities = false; return api.translate('&zUnknown;'); }),
  ];
  process.stdout.write(JSON.stringify(data) + '\n');
}
main().catch((error) => { process.stderr.write(String(error?.stack || error) + '\n'); process.exitCode = 1; });
