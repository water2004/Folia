#!/usr/bin/env node
'use strict';

const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');

function options(argv) {
  const result = {check:false};
  for (let index=2; index<argv.length; ++index) {
    const arg=argv[index];
    if (arg==='--check') { result.check=true; continue; }
    if (!['--repo-root','--out-root'].includes(arg) || !argv[index+1])
      throw new Error('Usage: node operator-data.cjs [--repo-root <path>] [--out-root <path>] [--check]');
    const key=arg.slice(2);
    if (result[key]) throw new Error(`Duplicate option ${arg}`);
    result[key]=path.resolve(argv[++index]);
  }
  return result;
}
function findRepoRoot(start) {
  let current=path.resolve(start);
  for (;;) {
    if (fs.existsSync(path.join(current,'tools/math-port/locks/p11-source-lock.json')) &&
        fs.existsSync(path.join(current,'.reference/MathJax-src'))) return current;
    const parent=path.dirname(current);
    if (parent===current) throw new Error(`Could not detect repository root from ${start}`);
    current=parent;
  }
}
const args=options(process.argv);
const root=args['repo-root'] || findRepoRoot(__dirname);
const outRoot=args['out-root'] || root;
const packages = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const digest = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const p11 = JSON.parse(fs.readFileSync(path.join(root, 'tools/math-port/locks/p11-source-lock.json'), 'utf8'));
const p12 = JSON.parse(fs.readFileSync(path.join(root, 'tools/math-port/locks/p12b-source-lock.json'), 'utf8'));
assert.equal(process.version, p11.node);

function lockedSource(lock) {
  const sourcePath = path.join(root, '.reference/MathJax-src', lock.source);
  const runtimePath = path.join(packages, lock.runtime);
  assert.equal(digest(sourcePath), lock.sha256, `source hash ${lock.source}`);
  const packageSource = path.join(packages, lock.source);
  assert.equal(fs.readFileSync(packageSource,'utf8').replaceAll('\r\n','\n'),
    fs.readFileSync(sourcePath,'utf8').replaceAll('\r\n','\n'), `package source ${lock.source}`);
  const resolved = require.resolve(`@mathjax/src/js/${lock.runtime.replace(/^cjs\//, '')}`, {
    paths: [path.join(root, 'tools/mathjax-bundle')],
  });
  assert.equal(fs.realpathSync(resolved), fs.realpathSync(runtimePath));
  assert.equal(digest(resolved), lock.runtime_sha256, `runtime hash ${lock.runtime}`);
  const sourceText = fs.readFileSync(sourcePath, 'utf8').replaceAll('\r\n', '\n');
  const packageText = fs.readFileSync(runtimePath, 'utf8');
  assert.ok(sourceText.length > 0 && packageText.length > 0);
  return resolved;
}

const dictionary = require(lockedSource(p11));
const MmlMo = require(lockedSource(p12)).MmlMo;
const texClasses = new Map([[-1,'none'],[0,'ord'],[1,'op'],[2,'bin'],[3,'rel'],[4,'open'],[5,'close'],[6,'punct'],[7,'inner']]);
function texClass(value) {
  if (!texClasses.has(value)) throw new Error(`Unsupported Source TeX class ${value}`);
  return `TexClass::${texClasses.get(value)}`;
}
function number(value) {
  if (!Number.isFinite(value)) throw new Error(`Non-finite operator number ${value}`);
  if (Object.is(value, -0)) return '-0.0';
  return Number.isInteger(value) ? `${value}.0` : String(value);
}
function cppString(value, wide) {
  if (typeof value !== 'string') throw new Error(`Expected Source string, received ${typeof value}`);
  let out = wide ? 'u"' : '"';
  for (const char of value) {
    const cp = char.codePointAt(0);
    if (char === '"') out += '\\"';
    else if (char === '\\') out += '\\\\';
    else if (cp >= 0x20 && cp <= 0x7e) out += char;
    else out += cp <= 0xffff ? `\\u${cp.toString(16).padStart(4,'0')}` : `\\U${cp.toString(16).padStart(8,'0')}`;
  }
  return `${out}"`;
}
function attributeValue(value) {
  if (typeof value === 'boolean') return `AttributeValue{${value}}`;
  if (typeof value === 'number') return `AttributeValue{${number(value)}}`;
  if (typeof value === 'string') return `AttributeValue{std::u16string{${cppString(value,true)}}}`;
  if (value === null) return 'AttributeValue{AttributeNull{}}';
  throw new Error(`Unsupported operator property value ${String(value)}`);
}
function operatorDef(tuple) {
  if (!Array.isArray(tuple) || tuple.length !== 4) throw new Error('Expected four-field Source operator definition');
  const [left,right,klass,properties] = tuple;
  const entries = properties == null ? [] : Object.entries(properties).map(([key,value]) =>
    `{${cppString(key,true)},${attributeValue(value)}}`);
  return `OperatorDef{${number(left)},${number(right)},${texClass(klass)},{${entries.join(',')}}}`;
}
function codepoint(value) { return `static_cast<char32_t>(0x${value.toString(16)})`; }

function parsePattern(pattern) {
  const match = /^\^\[([\s\S]*)\](\+|\{(\d+)(?:,(\d*))?\})?\$$/.exec(pattern.source);
  if (!match) throw new Error(`Pattern is outside the finite positive-class grammar: ${pattern.source}`);
  const body = match[1];
  const chars = [...body];
  const units = [];
  for (let i = 0; i < chars.length;) {
    const first = chars[i++].codePointAt(0);
    if (chars[i] === '-' && i + 1 < chars.length) {
      ++i;
      const last = chars[i++].codePointAt(0);
      if (last < first) throw new Error(`Descending pattern range in ${pattern.source}`);
      units.push([first,last]);
    } else units.push([first,first]);
  }
  units.sort((a,b) => a[0]-b[0]);
  const merged = [];
  for (const [first,last] of units) {
    const prior = merged.at(-1);
    if (prior && first <= prior[1] + 1) prior[1] = Math.max(prior[1],last);
    else merged.push([first,last]);
  }
  const minimum = match[2] === '+' ? 1 : match[3] ? Number(match[3]) : 1;
  const maximum = match[2] === '+' || (match[2] === `{${minimum},}`) ? null :
    match[2] && match[4] !== undefined && match[4] !== '' ? Number(match[4]) : minimum;
  return {source:pattern.source, flags:pattern.flags, units:merged, minimum, maximum};
}

const names = ['pseudoScripts','primes','opPattern','mathaccents','mathaccentsWithWidth'];
const patterns = names.map(name => parsePattern(MmlMo[name]));
const suffixes = ['\n','\r','\u2028','\u2029','\r\n'];
const acceptedExamples = ["'\u00b9",'\u2019','sin','\u0301','\u2190'];
function nativeMatch(pattern,text) {
  if (text.length<pattern.minimum || (pattern.maximum!==null && text.length>pattern.maximum)) return false;
  for (let i=0;i<text.length;++i) {
    const unit=text.charCodeAt(i);
    if (!pattern.units.some(([first,last])=>first<=unit && unit<=last)) return false;
  }
  return true;
}
const patternMatchWitnesses=[];
for (let index=0;index<patterns.length;++index) {
  const regex=MmlMo[names[index]];
  for (const text of [acceptedExamples[index],...suffixes.map(suffix=>acceptedExamples[index]+suffix)]) {
    const sourceMatch=regex.test(text), nativeMatchResult=nativeMatch(patterns[index],text);
    assert.equal(nativeMatchResult,sourceMatch,`${names[index]} ${JSON.stringify(text)}`);
    patternMatchWitnesses.push({pattern:names[index],text:[...text].map(char=>char.codePointAt(0)),sourceMatch,nativeMatch:nativeMatchResult});
  }
}
const operatorRows = Object.entries(dictionary.OPTABLE).flatMap(([form,entries]) =>
  Object.entries(entries).map(([text,definition]) => ({form,text,definition})));
const records = {
  source: {operator:{path:p11.source,sha256:p11.sha256,runtime:p11.runtime,runtime_sha256:p11.runtime_sha256},
    patterns:{path:p12.source,sha256:p12.sha256,runtime:p12.runtime,runtime_sha256:p12.runtime_sha256}},
  MO:Object.entries(dictionary.MO).map(([name,definition]) => [name,definition]),
  RANGES:dictionary.RANGES,
  MMLSPACING:dictionary.MMLSPACING,
  OPTABLE:operatorRows,
  patterns:patterns.map((pattern,index) => ({name:names[index],...pattern})),
  patternMatchWitnesses,
};

const dataLines = [
  '// Generated from pinned MathJax 4.1.3 OperatorDictionary.ts by operator-data.cjs.',
  '// Apache-2.0; see src/math/LICENSE-MathJax.txt.',
  'module folia.math.core.mml_tree.operator_dictionary;',
  'import std;',
  'namespace folia::math::mml::operator_dict { namespace {',
  `const std::array<NamedOperatorDef,${records.MO.length}> mo_data{{`,
  ...records.MO.map(([name,definition],index) => `  {${cppString(name,true)},${operatorDef(definition)}}${index+1<records.MO.length?',':''}`),
  '}};',
  `const std::array<RangeDef,${records.RANGES.length}> range_data{{`,
  ...records.RANGES.map((row,index) => `  {${codepoint(row[0])},${codepoint(row[1])},${texClass(row[2])},${cppString(row[3],true)}${row[4]===undefined?'':`,std::u16string{${cppString(row[4],true)}}`}}${index+1<records.RANGES.length?',':''}`),
  '}};',
  `const std::array<std::array<double,2>,${records.MMLSPACING.length}> spacing_data{{`,
  ...records.MMLSPACING.map((row,index) => `  {{${number(row[0])},${number(row[1])}}}${index+1<records.MMLSPACING.length?',':''}`),
  '}};',
  `const std::array<OperatorEntry,${records.OPTABLE.length}> operator_data{{`,
  ...records.OPTABLE.map((row,index) => `  {${cppString(row.form,true)},${cppString(row.text,true)},${operatorDef(row.definition)}}${index+1<records.OPTABLE.length?',':''}`),
  '}};',
  '}',
  'std::span<NamedOperatorDef const> MO() { return mo_data; }',
  'std::span<RangeDef const> RANGES() { return range_data; }',
  'std::span<std::array<double,2> const> MMLSPACING() { return spacing_data; }',
  'std::span<OperatorEntry const> OPTABLE() { return operator_data; }',
  '}',
].join('\n')+'\n';

const patternLines = [
  '// Generated from pinned MathJax 4.1.3 MmlNodes/mo.ts by operator-data.cjs.',
  '// Apache-2.0; see src/math/LICENSE-MathJax.txt.',
  'module folia.math.core.mml_tree.mo_patterns;',
  'import std;',
  'namespace folia::math::mml { namespace {',
];
patterns.forEach((pattern,index) => {
  patternLines.push(`constexpr std::array<MoCodeUnitRange,${pattern.units.length}> ranges_${index}{{`);
  pattern.units.forEach(([first,last],unitIndex) => patternLines.push(`  {char16_t{0x${first.toString(16)}},char16_t{0x${last.toString(16)}}}${unitIndex+1<pattern.units.length?',':''}`));
  patternLines.push('}};');
});
patternLines.push(`const std::array<MoPatternSpec,${patterns.length}> specs{{`);
patterns.forEach((pattern,index) => patternLines.push(`  {${cppString(pattern.source,true)},${cppString(pattern.flags,false)},ranges_${index},${pattern.minimum},${pattern.maximum===null?'std::nullopt':pattern.maximum}}${index+1<patterns.length?',':''}`));
patternLines.push('}};', '}',
  'const MoPatternSpec& moPatternSpec(MoPattern kind) {',
  '  const auto index=static_cast<std::size_t>(kind);',
  '  if(index>=specs.size()) throw std::out_of_range("Invalid native MmlMo pattern");',
  '  return specs[index];',
  '}',
  '}');
const patternData = patternLines.join('\n')+'\n';
const outputs = new Map([
  ['src/math/core/mml_tree/operator_dictionary_data.cpp',dataLines],
  ['src/math/core/mml_tree/mo_pattern_data.cpp',patternData],
  ['build/math-port/operator-source-records.json',JSON.stringify(records,null,2)+'\n'],
]);
const check = args.check;
for (const [relative,contents] of outputs) {
  const file = path.join(outRoot,relative);
  if (check) assert.equal(fs.readFileSync(file,'utf8').replaceAll('\r\n','\n'),contents,`${relative} differs from pinned Source`);
  else { fs.mkdirSync(path.dirname(file),{recursive:true}); fs.writeFileSync(file,contents); }
}
console.log(JSON.stringify({mode:check?'check':'generate',mo:records.MO.length,ranges:records.RANGES.length,
  spacing:records.MMLSPACING.length,operators:records.OPTABLE.length,patterns:patterns.length},null,2));
