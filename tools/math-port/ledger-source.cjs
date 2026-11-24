// Generate only the actual upstream AST data. Reviewed/native acceptance is Python.
'use strict';
const fs = require('node:fs');
const path = require('node:path');
const cp = require('node:child_process');
const assert = require('node:assert/strict');
const profiles = JSON.parse(fs.readFileSync(path.join(__dirname, 'ledger-profiles.json'), 'utf8'));
const profile = profiles[process.argv[2]];
assert.ok(profile, 'Expected a known pinned ledger profile');
let inputs = profile.inputs;
if (profile.bootstrap) {
  // Preserve each original reference loader's complete TypeScript/CJS package
  // and runtime locks before sampling the official source.
  const reference = require(path.join(__dirname, profile.bootstrap));
  if (profile.input_selector === 'source') inputs = [reference.source];
  if (profile.input_selector === 'hashes') inputs = reference.hashes.map(item => item.source);
}
const generator = profile.generator || 'member-ledger.cjs';
const args = [path.join(__dirname, generator), ...(profile.generator ? [] : inputs)];
process.stdout.write(cp.execFileSync(process.execPath, args, {maxBuffer: 16 * 1024 * 1024}));
