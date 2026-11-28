'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');

function findRepositoryRoot(start = __dirname) {
  for (let current = path.resolve(start); current !== path.dirname(current); current = path.dirname(current)) {
    if (fs.existsSync(path.join(current, 'tools/math-port/README.md')) &&
        fs.existsSync(path.join(current, 'tools/mathjax-bundle/node_modules/@mathjax/src/package.json'))) return current;
  }
  return null;
}
const rootArg = process.argv.indexOf('--repo-root');
if (rootArg >= 0 && (!process.argv[rootArg + 1] || process.argv.length !== rootArg + 2))
  throw new Error('usage: node mml-native-table-source-witness.cjs [--repo-root <repository>]');
const repo = rootArg >= 0 ? path.resolve(process.argv[rootArg + 1]) : findRepositoryRoot();
assert.ok(repo, 'pass --repo-root <repository> or run this script from within the repository');
const pkg = path.join(repo, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const cjs = path.join(pkg, 'cjs/core/MmlTree');
const pkgInfo = JSON.parse(fs.readFileSync(path.join(pkg, 'package.json'), 'utf8'));
assert.equal(pkgInfo.version, '4.1.3', 'behavior oracle package must be pinned MathJax 4.1.3');
const expectedHashes = {
  'MmlFactory.js': 'd0c4e25237d04a9d21d12ee44776e46d30777e7f284db6f4f3ed1cc625b752d3',
  'MmlNodes/mtable.js': '89f9cf650c72e146c544efdb640108f6f0b496d74f6bd7258a5ecaf28a284516',
  'MmlNodes/mtr.js': 'd6820d1a7f2ad6cb8ea714c0e73d25993987174d8cf51820fac742204df2bf47',
  'MmlNodes/mtd.js': '0210a0d98d9098fcdab7362302841a77625341bf1459582e72f57f39314d9699',
  'MmlNodes/maligngroup.js': 'ec7d583f79bd49dfdd5b69630cf7f52d56e6e34f9f9c7e0963b0e1933cc4176c',
  'MmlNodes/malignmark.js': '9dba71cd21e25a0c43e58e13c02f4ec9629b1b7d4c87991cf62e12415e6a1ad7',
};
for (const [file, expected] of Object.entries(expectedHashes)) {
  const actual = crypto.createHash('sha256').update(fs.readFileSync(path.join(cjs, file))).digest('hex');
  assert.equal(actual, expected, `pinned Source hash ${file}`);
}
const { MmlFactory } = require(path.join(cjs, 'MmlFactory.js'));
const { MmlMtable } = require(path.join(cjs, 'MmlNodes/mtable.js'));
const { MmlMtr, MmlMlabeledtr } = require(path.join(cjs, 'MmlNodes/mtr.js'));
const { MmlMtd } = require(path.join(cjs, 'MmlNodes/mtd.js'));
const { MmlMaligngroup } = require(path.join(cjs, 'MmlNodes/maligngroup.js'));
const { MmlMalignmark } = require(path.join(cjs, 'MmlNodes/malignmark.js'));

const factory = new MmlFactory();
assert.equal(factory.getNodeClass('mtable'), MmlMtable);
assert.equal(factory.getNodeClass('mtr'), MmlMtr);
assert.equal(factory.getNodeClass('mlabeledtr'), MmlMlabeledtr);
assert.equal(factory.getNodeClass('mtd'), MmlMtd);
assert.equal(factory.getNodeClass('maligngroup'), MmlMaligngroup);
assert.equal(factory.getNodeClass('malignmark'), MmlMalignmark);

const rows = [];
function record(name, actual, expected) {
  assert.deepEqual(actual, expected, name);
  rows.push({ name, actual });
}
function cell(factory) { return factory.create('mtd', {}, [factory.create('mrow')]); }
function capture(node) {
  const calls = [];
  const original = node.setInheritedAttributes;
  node.setInheritedAttributes = function(attributes, display, level, prime) {
    const picked = {};
    for (const key of ['rowalign', 'columnalign', 'data-break-align', 'data-vertical-align', 'indentalign', 'indentshiftfirst', 'groupalign', 'mathcolor'])
      if (Object.hasOwn(attributes, key)) picked[key] = attributes[key];
    calls.push({ attributes: structuredClone(picked), display, level, prime });
    return original.call(this, attributes, display, level, prime);
  };
  return calls;
}
function tree(node) {
  return { kind: node.kind, children: (node.childNodes || []).map(tree) };
}
function inherited(node, keys) {
  const out = {};
  for (const key of keys) {
    const value = node.attributes.getInherited(key);
    if (value !== undefined) out[key] = value;
  }
  return out;
}

{
  const first = cell(factory), second = cell(factory);
  const firstCalls = capture(first), secondCalls = capture(second);
  const row = factory.create('mtr', {}, [first, second]);
  const table = factory.create('mtable', {
    rowalign: 'baseline top', columnalign: 'left right', 'data-break-align': 'middle bottom'
  }, [row]);
  table.setInheritedAttributes({}, false, 0, false);
  record('table-row-cell alignment and inherited origin pairs', {
    first: firstCalls[0].attributes,
    second: secondCalls[0].attributes,
    effectiveFirst: inherited(first, ['rowalign', 'columnalign', 'data-vertical-align']),
    effectiveSecond: inherited(second, ['rowalign', 'columnalign', 'data-vertical-align']),
  }, {
    first: {
      rowalign: ['mtr', 'baseline'], columnalign: ['mtr', 'left'],
      'data-break-align': ['mtr', 'top'], 'data-vertical-align': ['mtr', 'middle']
    },
    second: {
      rowalign: ['mtr', 'baseline'], columnalign: ['mtr', 'right'],
      'data-break-align': ['mtr', 'top'], 'data-vertical-align': ['mtr', 'bottom']
    },
    effectiveFirst: { rowalign: 'baseline', columnalign: 'left', 'data-vertical-align': 'middle' },
    effectiveSecond: { rowalign: 'baseline', columnalign: 'right', 'data-vertical-align': 'bottom' }
  });
}

{
  const row = factory.create('mtr');
  const table = factory.create('mtable', {
    indentalign: 'explicit-right', indentshift: 'explicit-shift', rowalign: 'baseline'
  }, [row]);
  const incoming = {
    indentalign: ['math', 'inherited-center'],
    indentshiftfirst: ['mstyle', '1em']
  };
  table.setInheritedAttributes(incoming, false, 0, false);
  record('table indent attributes move from explicit into inherited layer', {
    explicitIndent: {present: table.attributes.hasExplicit('indentalign'), value: table.attributes.getExplicit('indentalign') ?? null},
    inheritedIndent: table.attributes.getInherited('indentalign'),
    explicitShift: {present: table.attributes.hasExplicit('indentshift'), value: table.attributes.getExplicit('indentshift') ?? null},
    inheritedFirstShift: table.attributes.getInherited('indentshiftfirst')
  }, {
    explicitIndent: {present: false, value: null}, inheritedIndent: 'inherited-center',
    explicitShift: {present: false, value: null}, inheritedFirstShift: '1em'
  });
}

{
  const label = cell(factory), value = cell(factory);
  const labelCalls = capture(label), valueCalls = capture(value);
  const row = factory.create('mlabeledtr', {}, [label, value]);
  const table = factory.create('mtable', {
    side: 'left', columnalign: 'center right', 'data-break-align': 'baseline bottom', rowalign: 'top'
  }, [row]);
  table.setInheritedAttributes({}, false, 0, false);
  record('labeled row reserves label column and vertical alignment', {
    arity: row.arity,
    label: labelCalls[0].attributes,
    value: valueCalls[0].attributes
  }, {
    arity: 1,
    label: {
      rowalign: ['mlabeledtr', 'top'], columnalign: ['mlabeledtr', 'left'],
      'data-break-align': ['mlabeledtr', 'top'], 'data-vertical-align': ['mlabeledtr', 'top']
    },
    value: {
      rowalign: ['mlabeledtr', 'top'], columnalign: ['mlabeledtr', 'center'],
      'data-break-align': ['mlabeledtr', 'top'], 'data-vertical-align': ['mlabeledtr', 'baseline']
    }
  });
}

{
  const table = factory.create('mtable', { rowalign: 'baseline' }, [
    factory.create('mi', {}, [factory.create('text', {}, [])]),
    cell(factory),
    factory.create('mo', {}, [factory.create('text', {}, [])])
  ]);
  table.verifyTree({checkArity: false, checkAttributes: false, checkMathvariants: false,
    fullErrors: false, fixMmultiscripts: true, fixMtables: true});
  record('verifyTree fixMtables=true groups malformed children and wraps non-cells', tree(table), {
    kind: 'mtable', children: [{kind: 'mtr', children: [
      {kind: 'mtd', children: [{kind: 'inferredMrow', children: [{kind: 'mi', children: [{kind: 'text', children: []}]}]}]},
      {kind: 'mtd', children: [{kind: 'inferredMrow', children: [{kind: 'mrow', children: []}]}]},
      {kind: 'mtd', children: [{kind: 'inferredMrow', children: [{kind: 'mo', children: [{kind: 'text', children: []}]}]}]}
    ]}]
  });
}

{
  const table = factory.create('mtable', {}, [factory.create('mi', {}, [factory.create('text', {}, [])])]);
  table.verifyTree({checkArity: false, checkAttributes: false, checkMathvariants: false,
    fullErrors: false, fixMmultiscripts: true, fixMtables: false});
  record('verifyTree fixMtables=false retains merror repair node in row cell', tree(table), {
    kind: 'mtable', children: [{kind: 'mtr', children: [{kind: 'mtd', children: [
      {kind: 'inferredMrow', children: [{kind: 'merror', children: [
        {kind: 'inferredMrow', children: [{kind: 'mi', children: [{kind: 'text', children: []}]}]}
      ]}]}
    ]}]}]
  });
}

{
  const malformed = factory.create('mi', {}, [factory.create('text', {}, [])]);
  const row = factory.create('mtr', {}, [malformed]);
  row.verifyTree({checkArity: false, checkAttributes: false, checkMathvariants: false,
    fullErrors: false, fixMmultiscripts: true, fixMtables: false});
  record('row verify repairs a non-cell child and honors fixMtables=false', tree(row), {
    kind: 'mtr', children: [{kind: 'mtd', children: [{kind: 'inferredMrow', children: [
      {kind: 'merror', children: [{kind: 'inferredMrow', children: [
        {kind: 'mi', children: [{kind: 'text', children: []}]}
      ]}]}
    ]}]}]
  });
}

{
  const child = factory.create('mi', {}, [factory.create('text', {}, [])]);
  const childCalls = capture(child);
  const group = factory.create('maligngroup', {groupalign: '{left}', mathcolor: 'purple'}, [child]);
  group.setInheritedAttributes({}, false, 0, false);
  const mark = factory.create('malignmark');
  record('maligngroup explicit inheritance and malignmark defaults', {
    groupalign: childCalls[0].attributes.groupalign,
    mathcolor: childCalls[0].attributes.mathcolor,
    mark: {kind: mark.kind, arity: mark.arity, spacelike: mark.isSpacelike, edge: mark.attributes.get('edge')}
  }, {
    groupalign: ['maligngroup', '{left}'], mathcolor: ['maligngroup', 'purple'],
    mark: {kind: 'malignmark', arity: 0, spacelike: true, edge: 'left'}
  });
}

{
  const cellNode = cell(factory);
  cellNode.verifyTree({checkArity: false, checkAttributes: false, checkMathvariants: false,
    fullErrors: false, fixMmultiscripts: true, fixMtables: true});
  const rowNode = factory.create('mtr', {}, [cell(factory)]);
  const tableNode = factory.create('mtable', {}, [rowNode]);
  record('static node defaults and native class hierarchy', {
    defaults: {
      tableSide: MmlMtable.defaults.side,
      rowBreakAlign: MmlMtr.defaults['data-break-align'],
      cellRowSpan: MmlMtd.defaults.rowspan,
      group: MmlMaligngroup.defaults.groupalign,
      markEdge: MmlMalignmark.defaults.edge
    },
    hierarchy: {
      labelledIsRow: rowNode.isKind('mtr'), labelledKind: factory.create('mlabeledtr').kind,
      tableKind: tableNode.kind, cellArity: cellNode.arity
    }
  }, {
    defaults: {tableSide: 'right', rowBreakAlign: 'top', cellRowSpan: 1, group: '_inherit_', markEdge: 'left'},
    hierarchy: {labelledIsRow: true, labelledKind: 'mlabeledtr', tableKind: 'mtable', cellArity: -1}
  });
}

const classFiles = ['MmlFactory.js', 'MmlNodes/mtable.js', 'MmlNodes/mtr.js', 'MmlNodes/mtd.js',
  'MmlNodes/maligngroup.js', 'MmlNodes/malignmark.js'];
const evidence = {
  oracle: {package: '@mathjax/src', version: pkgInfo.version, cjsRoot: path.relative(repo, cjs).replaceAll('\\', '/')},
  cjs: classFiles.map(file => {
    const absolute = path.join(cjs, file);
    const bytes = fs.readFileSync(absolute);
    return {path: path.relative(repo, absolute).replaceAll('\\', '/'), bytes: bytes.length,
      sha256: crypto.createHash('sha256').update(bytes).digest('hex')};
  }),
  passed: rows.length,
  cases: rows
};
const output = path.join(repo, 'build/math-port/typed-mml-table/source-witness.json');
fs.mkdirSync(path.dirname(output), { recursive: true });
fs.writeFileSync(output, JSON.stringify(evidence, null, 2) + '\n');
process.stdout.write(JSON.stringify({ output: path.relative(repo, output), ...evidence }, null, 2) + '\n');
