'use strict';
// Read-only observation of the pinned SvgWrappers/defaultNodes constructor
// graph. This does not alter the locked profile oracle or its expected rows.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');

const root = path.resolve(__dirname, '../../..');
const bundle = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const scopePath = path.join(root, 'tools/math-port/profiles/scope.json');
const digest = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const scope = JSON.parse(fs.readFileSync(scopePath, 'utf8'));
const sourceLock = JSON.parse(fs.readFileSync(path.join(__dirname, 'profile-source-lock.json'), 'utf8'));
assert.equal(sourceLock.scopeHash, digest(scopePath));
const locked = sourceLock.dependencies.find(item => item.file === 'src/cjs/output/svg/Wrappers.js');
assert.ok(locked, 'SvgWrappers source must be pinned by the existing profile lock');
assert.equal(digest(path.join(bundle, 'cjs/output/svg/Wrappers.js')), locked.sha256);
const { SvgWrappers } = require(path.join(bundle, 'cjs/output/svg/Wrappers.js'));

const ids = new WeakMap();
const objects = [];
let nextId = 0;
function describeValue(value) {
  if (value === undefined) return { $: 'undefined' };
  if (value === null || typeof value === 'string' || typeof value === 'boolean') return value;
  if (typeof value === 'number') {
    if (Number.isNaN(value)) return { $: 'NaN' };
    if (value === Infinity) return { $: 'Infinity' };
    if (value === -Infinity) return { $: '-Infinity' };
    if (Object.is(value, -0)) return { $: '-0' };
    return value;
  }
  if (typeof value === 'bigint') return { $: 'BigInt', value: String(value) };
  if (typeof value === 'symbol') return { $: 'Symbol', description: value.description ?? null };
  let id = ids.get(value);
  if (id !== undefined) return { $ref: id };
  id = nextId++;
  ids.set(value, id);
  const isFunction = typeof value === 'function';
  const row = {
    id,
    type: isFunction ? 'function' : Array.isArray(value) ? 'array' : 'object',
    name: isFunction ? value.name : undefined,
    length: isFunction ? value.length : undefined,
    prototypeConstructor: isFunction && value.prototype
      ? value.prototype.constructor?.name ?? null : undefined,
    parentConstructor: isFunction ? Object.getPrototypeOf(value)?.name ?? null : undefined,
    properties: []
  };
  objects.push(row);
  for (const key of Reflect.ownKeys(value)) {
    const descriptor = Object.getOwnPropertyDescriptor(value, key);
    row.properties.push({
      key: typeof key === 'symbol' ? { $: 'Symbol', description: key.description ?? null } : key,
      enumerable: descriptor.enumerable,
      configurable: descriptor.configurable,
      writable: Object.hasOwn(descriptor, 'writable') ? descriptor.writable : null,
      value: Object.hasOwn(descriptor, 'value') ? describeValue(descriptor.value) : undefined,
      getter: descriptor.get ? describeValue(descriptor.get) : null,
      setter: descriptor.set ? describeValue(descriptor.set) : null
    });
  }
  return { $ref: id };
}
const rows = Object.keys(SvgWrappers).map((kind, index) => {
  const ctor = SvgWrappers[kind];
  assert.equal(typeof ctor, 'function', `SvgWrappers[${kind}]`);
  const ctorRef = describeValue(ctor);
  return {
    index,
    kind,
    constructor: ctorRef
  };
});
const output = {
  schema: 1,
  source: 'MathJax 4.1.3 src/cjs/output/svg/Wrappers.js',
  sourceSha256: locked.sha256,
  scopeProfileSha256: digest(path.join(root, scope.profile.file)),
  enumerableKeys: Object.keys(SvgWrappers),
  rows,
  constructorGraph: objects
};
const outputPath = process.argv[2] || path.join(root, 'build/math-port/svg-wrapper-classes-original.json');
fs.mkdirSync(path.dirname(outputPath), { recursive: true });
fs.writeFileSync(outputPath, JSON.stringify(output, null, 2) + '\n');
process.stdout.write(`${rows.length} constructors observed -> ${outputPath}\n`);
