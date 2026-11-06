// ECMAScript's actual Number::toString is the reference for SVG attributes
// and numeric diagnostics; binary64 bit patterns avoid decimal input rounding.
import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
const fixture = new URL('../../tests/mermaid_cpp/fixtures/number.tsv', import.meta.url);
const cases = new Map();
const buffer = new ArrayBuffer(8);
const view = new DataView(buffer);
function addBits(bits) {
  view.setBigUint64(0, bits);
  const value = view.getFloat64(0);
  cases.set(bits.toString(16).padStart(16, '0'), Number.isFinite(value) ? String(value) : '-');
}
for (const value of [0, -0, NaN, Infinity, -Infinity, Number.MAX_VALUE,
  Number.MIN_VALUE, Number.EPSILON, Number.MAX_SAFE_INTEGER,
  1000000000000000128, -1000000000000000128, 0.1, 1 / 3, Math.PI]) {
  view.setFloat64(0, value);
  addBits(view.getBigUint64(0));
}
for (let exponent = -323; exponent <= 308; ++exponent) {
  for (const scale of [1, 1.2345678901234567, -9.999999999999998]) {
    view.setFloat64(0, scale * 10 ** exponent);
    const bits = view.getBigUint64(0);
    addBits(bits);
    if (bits > 0n) addBits(bits - 1n);
    if (bits < 0xffffffffffffffffn) addBits(bits + 1n);
  }
}
let state = 0x7f4a7c15;
function next() {
  state = (Math.imul(state, 1664525) + 1013904223) >>> 0;
  return state;
}
for (let index = 0; index < 4000; ++index) {
  addBits((BigInt(next()) << 32n) | BigInt(next()));
}
const output = '# binary64-bits\tECMAScript-Number-toString-or-nonfinite\n' +
  [...cases].map(entry => entry.join('\t')).join('\n') + '\n';
if (process.argv[2] === '--write') writeFileSync(fixture, output);
else {
  assert.equal(process.argv[2], '--check', 'Use --write or --check');
  assert.equal(readFileSync(fixture, 'utf8'), output, 'Number oracle fixture is stale');
}
console.log(`Number oracle: ${cases.size} binary64 observations verified.`);
