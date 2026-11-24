'use strict';

// Serializes only pinned upstream observations. Native tests use the matching
// typed C++ reader; this generator never consumes Folia's candidate output.
const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');
const {byteField, utf16Field, binary64} = require('./fixture-data.cjs');
const schema = require('./mathml-observation-schema.json');

function serialize(name, values) {
  const rows = [];
  const put = (key, type, value) => rows.push([key, type, value].join('\t'));
  function field(type, key, value) {
    assert.notEqual(value, undefined, `Missing Source observation ${key}`);
    if (Array.isArray(type)) {
      if (type[0] === 'optional') {
        field('boolean', key + '.present', value !== null);
        if (value !== null) field(type[1], key + '.value', value);
      } else {
        assert.ok(Array.isArray(value), `Expected list ${key}`);
        if (type[0] === 'array') assert.equal(value.length, type[2], key);
        field('integer', key + '.count', value.length);
        value.forEach((item, i) => field(type[1], `${key}[${i}]`, item));
      }
    } else if (type === 'bytes') {
      assert.equal(typeof value, 'string', key); put(key, type, byteField(value));
    } else if (type === 'utf16') {
      assert.equal(typeof value, 'string', key); put(key, type, utf16Field(value));
    } else if (type === 'binary64') {
      assert.equal(typeof value, 'number', key);
      // Original C++ generation emitted String(number), whose -0 spelling is 0.
      put(key, type, binary64(Object.is(value, -0) ? 0 : value));
    } else if (type === 'boolean') {
      assert.equal(typeof value, 'boolean', key); put(key, type, +value);
    } else if (type === 'integer') {
      assert.ok(value === 'max' || (Number.isSafeInteger(value) && value >= 0), key);
      put(key, type, value);
    } else if (schema.types[type]?.[0] === 'enum') {
      const members = schema.types[type][1];
      assert.ok(members.includes(value), key); put(key, 'enum', members.indexOf(value));
    } else {
      assert.ok(schema.types[type], `Unknown observation schema ${type}`);
      for (const [member, memberType] of schema.types[type])
        field(memberType, key + '.' + member, value[member]);
    }
  }
  for (const [key, type] of schema.roots[name]) field(type, key, values[key]);
  return '# Pinned MathJax 4.1.3 observations; exact fields from the original compiled fixture.\n' +
    '# field\ttype\tvalue\n' + rows.join('\n') + '\n';
}

function writeObservations(name, values, write = process.argv.includes('--write-fixture')) {
  const target = path.join(__dirname, '../../tests/math/fixtures/scenes', name, 'observations.tsv');
  const expected = serialize(name, values);
  if (write) {
    fs.mkdirSync(path.dirname(target), {recursive: true});
    if (!fs.existsSync(target) || fs.readFileSync(target, 'utf8') !== expected)
      fs.writeFileSync(target, expected);
  } else assert.equal(fs.readFileSync(target, 'utf8'), expected,
    `Independent Source observations differ: ${name}; review before --write-fixture`);
  return target;
}

function attribute(name, value) {
  const kind = {string: 'text', number: 'number', boolean: 'boolean'}[typeof value];
  assert.ok(kind, `Unsupported Source attribute ${name}`);
  return {name, kind, text: kind === 'text' ? value : '',
    number: kind === 'number' ? value : 0, boolean: kind === 'boolean' ? value : false};
}
function flatMml(nodes, attributes) {
  return {
    nodes: nodes.map(n => ({kind: n.kind, is_inferred: n.inferred,
      parent: n.parent === null || typeof n.parent === 'string' ? 'max' : n.parent,
      attribute_first: n.first, attribute_count: n.count, text: n.text,
      xml_payload_index: 'max'})),
    attributes: attributes.map(a => ({name: a.name, kind: a.kind,
      text: a.text ?? '', number: a.number ?? 0, boolean: a.boolean ?? false}))
  };
}
function treeMml(root) {
  const nodes = [], attributes = [], payloads = [];
  function visit(node, parent) {
    const at = nodes.length, first = attributes.length;
    const own = Object.entries(node.attributes || {});
    attributes.push(...own.map(([name, value]) => attribute(name, value)));
    let xml = 'max';
    if (node.kind === 'XML') {
      assert.ok(node.xml?.serialized, 'Missing Source XML payload');
      xml = payloads.length; payloads.push(node.xml.serialized);
    }
    nodes.push({kind: node.kind, is_inferred: node.isInferred, parent,
      attribute_first: first, attribute_count: own.length,
      text: node.kind === 'text' ? node.text || '' : '', xml_payload_index: xml});
    node.children.forEach(child => visit(child, at));
  }
  visit(root, 'max');
  return {nodes, attributes, payloads};
}
function sourceCase(record) {
  return {name: record.name, metrics: record.metrics,
    ...treeMml(record.mml), source_svg: record.svg};
}
module.exports = {serialize, writeObservations, flatMml, treeMml, sourceCase};
