#!/usr/bin/env node
'use strict';

// Candidate typed initializer generator. Development-time only; native code
// consumes the generated C++ data and never evaluates JavaScript.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const {createRequire} = require('node:module');
const args = process.argv.slice(2);
const rootOption = args.indexOf('--repo-root');
if (rootOption >= 0 && !args[rootOption + 1]) throw new Error('--repo-root requires a path');
const root = rootOption >= 0 ? path.resolve(args[rootOption + 1]) : path.resolve(__dirname, '../..');
const outputDir = path.join(root, 'build/math-port/font-generated-native-raii');
const outputArgument = args.find((arg,index) => arg !== '--repo-root' && (rootOption < 0 || index !== rootOption + 1));
if (outputArgument && path.resolve(outputArgument) !== outputDir) throw new Error(`output is locked to ${outputDir}`);
const packages = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
const newcm = path.join(packages, 'mathjax-newcm-font');
const sourceLock = JSON.parse(fs.readFileSync(path.join(root, 'tools/math-port/locks/font-data-source-lock.json'), 'utf8'));
function writeIfChanged(file, value) {
  const bytes = Buffer.isBuffer(value) ? value : Buffer.from(value, 'utf8');
  try { if (fs.readFileSync(file).equals(bytes)) return false; } catch (error) { if (error.code !== 'ENOENT') throw error; }
  fs.mkdirSync(path.dirname(file), {recursive:true});
  fs.writeFileSync(file, bytes);
  return true;
}
const jsonEntries = Symbol('streaming JSON object entries');
const jsonItems = Symbol('streaming JSON array items');
const lazyJsonObject = entries => ({[jsonEntries]:entries});
const lazyJsonArray = items => ({[jsonItems]:items});
function sortCodepointKeyList(keys) {
  for (const key of keys) {
    if (!/^(0|[1-9][0-9]*)$/.test(key)) throw new Error(`non-codepoint key in Source character map: ${key}`);
    const codepoint = Number(key);
    if (!Number.isInteger(codepoint) || codepoint > 0x10FFFF)
      throw new Error(`invalid Source codepoint key: ${key}`);
  }
  return [...keys].sort((left,right) => Number(left) - Number(right));
}
function sortedCodepointKeys(chars) {
  const keys = [];
  for (const key in chars) keys.push(key);
  return sortCodepointKeyList(keys);
}
function* jsonChunks(value, depth = 0, arrayItem = false) {
  if (value === null || typeof value !== 'object') {
    const encoded = JSON.stringify(value);
    yield encoded === undefined && arrayItem ? 'null' : encoded;
    return;
  }
  if (Array.isArray(value) || value[jsonItems]) {
    const values = Array.isArray(value) ? value : value[jsonItems]();
    const iterator = values[Symbol.iterator]();
    const first = iterator.next();
    if (first.done) { yield '[]'; return; }
    yield '[';
    let item = first.value;
    let index = 0;
    while (true) {
      yield index ? ',\n' : '\n';
      yield '  '.repeat(depth + 1);
      yield* jsonChunks(item, depth + 1, true);
      index++;
      const next = iterator.next();
      if (next.done) break;
      item = next.value;
    }
    yield '\n'; yield '  '.repeat(depth); yield ']';
    return;
  }
  const entries = value[jsonEntries] ? value[jsonEntries]() : (function*(){
    for (const key of Object.keys(value)) yield [key,value[key]];
  })();
  const iterator = entries[Symbol.iterator]();
  let next;
  do { next = iterator.next(); } while (!next.done && next.value[1] === undefined);
  if (next.done) { yield '{}'; return; }
  yield '{';
  let entry = next.value;
  let first = true;
  while (true) {
    if (entry[1] !== undefined) {
      yield first ? '\n' : ',\n';
      yield '  '.repeat(depth + 1);
      yield JSON.stringify(entry[0]); yield ': ';
      yield* jsonChunks(entry[1], depth + 1);
      first = false;
    }
    next = iterator.next();
    if (next.done) break;
    entry = next.value;
  }
  yield '\n'; yield '  '.repeat(depth); yield '}';
}
function writeJsonIfChanged(file, value) {
  fs.mkdirSync(path.dirname(file), {recursive:true});
  const temporary = `${file}.stream-tmp`;
  const fd = fs.openSync(temporary, 'w');
  let pending = '';
  let byteCount = 0;
  const flush = () => {
    if (!pending) return;
    const bytes = Buffer.from(pending, 'utf8');
    let offset = 0;
    while (offset < bytes.length) offset += fs.writeSync(fd, bytes, offset, bytes.length - offset);
    byteCount += bytes.length;
    pending = '';
  };
  try {
    for (const chunk of jsonChunks(value)) {
      pending += chunk;
      if (pending.length >= 64 * 1024) flush();
    }
    pending += '\n'; flush(); fs.closeSync(fd);
    let identical = false;
    try {
      if (fs.statSync(file).size === byteCount) {
        const oldFd = fs.openSync(file, 'r');
        const newFd = fs.openSync(temporary, 'r');
        try {
          const left = Buffer.allocUnsafe(64 * 1024), right = Buffer.allocUnsafe(64 * 1024);
          let position = 0; identical = true;
          while (position < byteCount) {
            const length = Math.min(left.length, byteCount - position);
            const readLeft = fs.readSync(oldFd, left, 0, length, position);
            const readRight = fs.readSync(newFd, right, 0, length, position);
            if (readLeft !== length || readRight !== length ||
                !left.subarray(0, length).equals(right.subarray(0, length))) { identical = false; break; }
            position += length;
          }
        } finally { fs.closeSync(oldFd); fs.closeSync(newFd); }
      }
    } catch (error) { if (error.code !== 'ENOENT') throw error; }
    if (identical) { fs.unlinkSync(temporary); return false; }
    fs.copyFileSync(temporary, file); fs.unlinkSync(temporary); return true;
  } catch (error) {
    try { fs.closeSync(fd); } catch {}
    try { fs.unlinkSync(temporary); } catch {}
    throw error;
  }
}
if (sourceLock.source !== 'MathJax 4.1.3, commit fb987178c2d279a99b0db5ea02e751691703955e') throw new Error('wrong pinned source lock');
const lockHashes = new Map();
for (const entry of sourceLock.inputs) {
  const packageName = entry.package.replace(/^@mathjax\//, '');
  const relative = `${packageName}/${entry.path}`;
  const hash = crypto.createHash('sha256').update(fs.readFileSync(path.join(packages, relative))).digest('hex');
  if (hash !== entry.sha256) throw new Error(`font source lock mismatch: ${entry.package}/${entry.path}`);
  lockHashes.set(relative, hash);
}
function verifyPackage(name) {
  const dir = path.join(packages, name);
  const manifest = JSON.parse(fs.readFileSync(path.join(dir, 'package.json'), 'utf8'));
  if (manifest.version !== '4.1.3') throw new Error(`wrong pinned package version: ${name}@${manifest.version}`);
  const source = path.join(dir, 'cjs/svg.js');
  const hash = crypto.createHash('sha256').update(fs.readFileSync(source)).digest('hex');
  if (hash !== lockHashes.get(`${name}/cjs/svg.js`)) throw new Error(`pinned source lock mismatch: ${name} ${hash}`);
  return {dir, source, hash};
}
const quote = value => JSON.stringify(value).replace(/\u2028/g, '\\u2028').replace(/\u2029/g, '\\u2029');
const num = value => {
  if (!Number.isFinite(value)) throw new Error(`non-finite source font value: ${value}`);
  if (Object.is(value, -0)) return '-0.0';
  return Number.isInteger(value) ? `${value}.0` : String(value);
};
const glyphAudit = new Map();
const glyphTargetFile = new Map();
const glyphPending = new Map();
let glyphTableCounter = 0;
function tupleRows(target, variant, table) {
  const rows = [];
  for (const [code, tuple] of Object.entries(table || {})) {
    if (!Array.isArray(tuple)) throw new Error(`non-tuple glyph: ${variant}/${code}`);
    const options = tuple[3] && typeof tuple[3] === 'object' ? tuple[3] : {};
    const allowed = new Set(['p','c','ic','sk','dx','oc','smp']);
    for (const key of Object.keys(options)) if (!allowed.has(key)) throw new Error(`unsupported pinned glyph option ${key}: ${variant}/${code}`);
    for (const key of ['ic','sk','dx','oc']) if (options[key] != null && !Number.isFinite(options[key])) throw new Error(`invalid ${key} option: ${variant}/${code}`);
    if (Object.hasOwn(options,'p') && typeof options.p !== 'string') throw new Error(`invalid path option: ${variant}/${code}`);
    if (Object.hasOwn(options,'c') && typeof options.c !== 'string' && !Array.isArray(options.c)) throw new Error(`invalid composition option: ${variant}/${code}`);
    if (options.smp != null && !Number.isInteger(options.smp)) throw new Error(`invalid SMP target: ${variant}/${code}`);
    const pathDefined = Object.hasOwn(options,'p');
    const pathData = pathDefined ? options.p : '';
    const unsupported = [...pathData.matchAll(/[A-Za-z]/g)].map(match => match[0]).find(command => !'MCLQZ'.includes(command.toUpperCase()));
    if (unsupported) throw new Error(`unsupported pinned glyph path command ${unsupported}: ${variant}/${code}`);
    const compositionValues = value => {
      if (value == null) return [];
      const result=[];
      const append=part=>{
        if(typeof part==='string') for(let i=0;i<part.length;){const cp=part.codePointAt(i);result.push(cp);i+=cp>0xFFFF?2:1;}
        else if(Number.isInteger(part)&&part>=0&&part<=0x10FFFF) result.push(part);
        else throw new Error(`unsupported composition element: ${variant}/${code}`);
      };
      if(Array.isArray(value)) value.forEach(append); else append(value);
      return result;
    };
    const composition = compositionValues(options.c).join(',');
    const contentData = options.c === undefined ? 'std::nullopt' :
      `std::optional<std::string>{${quote(Array.isArray(options.c) ? options.c.map(String).join(',') : options.c)}}`;
    const serializedPath = options.p ? `M${pathData}Z` : '';
    if (!Number.isInteger(options.smp)) {
      if (compositionValues(options.c).length > 2) throw new Error(`composition exceeds locked NewCM POD capacity: ${variant}/${code}`);
      const compositionRow = [...compositionValues(options.c), ...Array(2 - compositionValues(options.c).length).fill(0)];
      const contentDefined = options.c !== undefined;
      const contentValue = Array.isArray(options.c) ? options.c.map(String).join(',') : (options.c ?? '');
      rows.push(`    {${quote(variant)},${Number(code)}, {${num(tuple[0] ?? 0)},${num(tuple[1] ?? 0)},${num(tuple[2] ?? 0)},${num(options.ic ?? 0)},${num(options.sk ?? 0)},${num(options.dx ?? 0)},${num(options.oc ?? 0)}}, ${pathDefined}, {${compositionRow.join(',')}}, ${compositionValues(options.c).length}, ${quote(pathData)}, ${quote(serializedPath)}, ${contentDefined}, ${quote(contentValue)}}`);
      const args = [quote(variant),String(Number(code)),num(tuple[0] ?? 0),num(tuple[1] ?? 0),num(tuple[2] ?? 0),num(options.ic ?? 0),num(options.sk ?? 0),num(options.dx ?? 0),num(options.oc ?? 0),String(pathDefined),`std::vector<std::uint32_t>{${composition}}`,quote(pathData),quote(serializedPath),contentData];
      const file = glyphTargetFile.get(target);
      if (!file) throw new Error('glyph table target has no generated source file');
      if (!glyphAudit.has(file)) glyphAudit.set(file, []);
      glyphAudit.get(file).push({args,values:compositionValues(options.c)});
    }
  }
  if (rows.length) {
    if (!glyphPending.has(target)) glyphPending.set(target, []);
    glyphPending.get(target).push(...rows);
  }
}
function flushGlyphRows(target) {
  const rows = glyphPending.get(target) || [];
  if (!rows.length) return;
  rows.sort((a,b) => {
    const left = a.match(/^    \{"([^"]+)",(\d+),/);
    const right = b.match(/^    \{"([^"]+)",(\d+),/);
    if (!left || !right) throw new Error('invalid generated glyph row');
    return left[1] < right[1] ? -1 : left[1] > right[1] ? 1 : Number(left[2])-Number(right[2]);
  });
  const name = `newcmGlyphRows_${glyphTableCounter++}`;
  target.push(`  static constexpr NewcmFontGlyphRow ${name}[] = {`, ...rows.map(row=>`${row},`),
    '  };', `  addGlyphRows(request,${name},std::size(${name}));`);
  glyphPending.set(target, []);
}
function delimiters(target, source) {
  for (const [code, d] of Object.entries(source || {})) {
    const allowed = new Set(['dir','sizes','variants','schar','stretch','stretchv','c','ext','min','hd','HDW','fullExt']);
    for (const key of Object.keys(d || {})) if (!allowed.has(key)) throw new Error(`unsupported pinned delimiter field ${key}: U+${Number(code).toString(16)}`);
    // These optional Source arrays have no presence bit in DelimiterRecipe.
    // Their absent and empty states therefore must not silently collapse.
    for (const key of ['variants','stretch','stretchv'])
      if (Object.hasOwn(d || {}, key) && Array.isArray(d[key]) && d[key].length === 0)
        throw new Error(`present empty delimiter ${key} has no typed presence representation: U+${Number(code).toString(16)}`);
    for (const [key,length] of [['hd',2],['HDW',3],['fullExt',2]])
      if (d[key] != null && (!Array.isArray(d[key]) || d[key].length !== length))
        throw new Error(`invalid delimiter ${key} tuple: U+${Number(code).toString(16)}`);
    if (d.c != null && (!Number.isInteger(Number(d.c)) || Number(d.c)<0))
      throw new Error(`invalid delimiter character: U+${Number(code).toString(16)}`);
    const direction = String(d.dir || '').toUpperCase();
    const dir = direction === 'V' ? 'vertical' : direction === 'H' ? 'horizontal' : 'none';
    const doubles = value => `std::vector<double>{${(value || []).map(num).join(',')}}`;
    const ids = value => {
      if (value != null && (!Array.isArray(value) || value.some(x=>!Number.isInteger(Number(x)) || Number(x)<0)))
        throw new Error(`invalid delimiter ID/index array: U+${Number(code).toString(16)}`);
      return `std::vector<std::uint32_t>{${(value || []).map(Number).join(',')}}`;
    };
    const optArr = value => value == null ? 'std::nullopt' : `std::optional<std::array<double,${value.length}>>{std::array<double,${value.length}>{${value.map(num).join(',')}}}`;
    target.push(`  { DelimiterRecipe d; d.direction=StretchDirection::${dir};`);
    if (d.sizes) target.push(`    d.sizes=${doubles(d.sizes)};`);
    for (const key of ['variants','schar','stretch','stretchv']) if (d[key]) target.push(`    d.${key}=${ids(d[key])};`);
    for (const key of ['min','ext']) if (d[key] != null) target.push(`    d.${key}=${num(d[key])};`);
    if (d.c != null) target.push(`    d.c=std::uint32_t{${Number(d.c)}};`);
    if (d.hd != null) target.push(`    d.heightDepth=${optArr(d.hd)};`);
    if (d.HDW != null) target.push(`    d.heightDepthWidth=${optArr(d.HDW)};`);
    if (d.fullExt != null) target.push(`    d.fullExtent=${optArr(d.fullExt)};`);
    target.push(`    face.delimiters.insert_or_assign(${Number(code)},std::move(d)); }`);
  }
}
function typedRemap(target, member, source) {
  for (const [code, value] of Object.entries(source || {}))
    target.push(`  face.${member}.insert_or_assign(${Number(code)},std::u16string(u${quote(value)}));`);
}
function variantSmpRemaps(target, sourceVariants) {
  for (const [name, variant] of Object.entries(sourceVariants || {})) {
    for (const cpText in (variant.chars || {})) {
      const tuple = variant.chars[cpText];
      const cp = Number(cpText), targetCp = tuple?.[3]?.smp;
      if (Number.isInteger(targetCp))
        target.push(`  face.variants[${quote(name)}].remap.emplace(${cp},GlyphReference{${quote(name)},${targetCp}});`);
    }
  }
}
function extensionSmpRemaps(target, source) {
  const greek = [null, null, source.SmpRemapGreekU || {}, source.SmpRemapGreekL || {}];
  for (const [name, raw] of Object.entries(source.VariantSmp || {})) {
    const remap = typeof raw === 'string' ? source.VariantSmp[raw] : raw;
    if (!Array.isArray(remap)) continue;
    for (const [index, low, high] of source.SmpRanges || []) {
      const base = remap[index];
      if (!base) continue;
      for (let cp = low; cp <= high; ++cp) {
        if (cp === 0x3a2) continue;
        const targetCp = Number((source.SmpRemap || {})[base + cp - low] || base + cp - low);
        target.push(`  face.variants[${quote(name)}].remap.insert_or_assign(${cp},GlyphReference{${quote(name)},${targetCp}});`);
      }
      for (const [cp, offset] of Object.entries(greek[index] || {}))
        target.push(`  face.variants[${quote(name)}].remap.insert_or_assign(${Number(cp)},GlyphReference{${quote(name)},${base + Number(offset)}});`);
    }
    for (const [cp, targetCp] of Object.entries(remap[5] || {}))
      target.push(`  face.variants[${quote(name)}].remap.insert_or_assign(${Number(cp)},GlyphReference{${quote(name)},${Number(targetCp)}});`);
  }
}
const sourceVariantGlyphCount = sourceFont => Object.values(sourceFont.variant)
  .reduce((sum, variant) => sum + Object.keys(variant.chars).length, 0);

const {source:newcmSource, hash:newcmHash} = verifyPackage('mathjax-newcm-font');
const {MathJaxNewcmFont: Font} = require(newcmSource);
const originalSetup = Font.dynamicSetup;
const registrations = new Map();
Font.dynamicSetup = function(extension, file, variants, delimiterData, fonts) {
  registrations.set(file, {extension, file, variants, delimiters:delimiterData, fonts});
  return originalSetup.call(this, extension, file, variants, delimiterData, fonts);
};
const moduleInputs = [];
for (const descriptor of Object.values(Font.dynamicFiles)) {
  const file = path.join(newcm, 'cjs/svg/dynamic', `${descriptor.file}.js`);
  const bytes = fs.readFileSync(file);
  moduleInputs.push({file:`cjs/svg/dynamic/${descriptor.file}.js`, sha256:crypto.createHash('sha256').update(bytes).digest('hex')});
  require(file);
  if (!registrations.has(descriptor.file)) throw new Error(`dynamic shard missing typed data: ${descriptor.file}`);
}
Font.dynamicSetup = originalSetup;
if (registrations.size !== 40) throw new Error(`expected 40 NewCM shards; got ${registrations.size}`);
const mathjax = require(path.join(packages, 'src/cjs/mathjax.js')).mathjax;
const sourceRequire = createRequire(path.join(packages, 'src/cjs/util/asyncLoad/node.js'));
const previousAsyncLoad = mathjax.asyncLoad, previousSync = mathjax.asyncIsSynchronous;
let sourceProviderRequests = 0, sourceProviderTraceEvents = 0;
mathjax.asyncIsSynchronous = true;
mathjax.asyncLoad = request => {
  ++sourceProviderRequests; ++sourceProviderTraceEvents;
  const moduleValue = sourceRequire(request);
  ++sourceProviderTraceEvents;
  return moduleValue;
};
const sourceFont = new Font();
sourceFont.loadDynamicFilesSync();
mathjax.asyncLoad = previousAsyncLoad;
mathjax.asyncIsSynchronous = previousSync;
if (sourceProviderRequests !== 40 || sourceProviderTraceEvents !== 80)
  throw new Error(`unexpected Source provider trace: ${sourceProviderRequests} calls / ${sourceProviderTraceEvents} events`);
const sourceBaseVariants = {...sourceFont.variant};
const sourceProfileGlyphCount = sourceVariantGlyphCount(sourceFont);

const extensionPackageNames = ['mathjax-bbm-font-extension','mathjax-bboldx-font-extension','mathjax-dsfont-font-extension'];
const extensions = [];
for (const name of extensionPackageNames) {
  const {source, hash} = verifyPackage(name);
  const data = Object.values(require(source))[0];
  sourceFont.addExtension(data);
  extensions.push({name, hash, data});
}
const expectedCssFontMapSha256 = 'db47e065c8d2a277c6268c1420cfa36bc1f02d1a424c66949baf402624d021e9';
const cssFontMap = Object.fromEntries(Object.keys(sourceFont.cssFontMap).sort()
  .map(variant => [variant, sourceFont.cssFontMap[variant]]));
const cssFontMapSha256 = crypto.createHash('sha256').update(JSON.stringify(cssFontMap)).digest('hex');
if (Object.keys(cssFontMap).length !== 33 || cssFontMapSha256 !== expectedCssFontMapSha256 ||
    sourceFont.cssFamilyPrefix !== '' ||
    JSON.stringify(sourceFont.getCssFont('missing')) !== JSON.stringify(['serif', false, false]))
  throw new Error(`unexpected pinned NewCM CSS metadata (${Object.keys(cssFontMap).length} rows, ${cssFontMapSha256})`);
const {StyleJsonSheet} = require(path.join(packages, 'src/cjs/util/StyleJson.js'));
const sourceStyleSheet = new StyleJsonSheet();
sourceStyleSheet.addStyles(sourceFont.styles);
if (sourceFont.styles !== undefined || sourceStyleSheet.cssText !== '')
  throw new Error('pinned NewCM FontData.styles is no longer undefined with no stylesheet contribution');
const sourceTupleOwner = new Map();
function rememberRows(tables) {
  for (const [variant, table] of Object.entries(tables || {}))
    for (const [cp, tuple] of Object.entries(table || {}))
      if (Array.isArray(tuple)) sourceTupleOwner.set(tuple, {variant, codepoint:Number(cp)});
}
rememberRows(Font.defaultChars);
for (const registration of registrations.values()) rememberRows(registration.variants);
for (const extension of extensions) rememberRows(extension.data.chars);
const svgAliasVariants = Object.keys(sourceFont.variant).filter(name => {
  const variant = sourceFont.variant[name];
  for (const _cp in variant.chars) return true;
  return false;
});
const svgAliasHelpers = new Map(svgAliasVariants.map(name => [name,
  `addNewcmSvgAliases_${name.replace(/[^A-Za-z0-9_]/g,'_')}`]));

const lines = [
  '// Generated by font-data-generate-typed.cjs from pinned MathJax 4.1.3.',
  `// NewCM package source SHA-256: ${newcmHash}`,
  '// Glyph outlines are normalized from 1000 design units to em and retain y-up coordinates.',
  'static void addGlyphRows(NewcmFontTables& request, NewcmFontGlyphRow const* rows, std::size_t count) { request.addRows(std::span{rows,count}); }',
];
for (const helper of svgAliasHelpers.values()) lines.push(`void ${helper}(NewcmFontTables& request);`);
const shardNames = new Map([...registrations.keys()].map(file => [file, `addNewcmShard_${file.replace(/[^A-Za-z0-9_]/g,'_')}`]));
for (const helper of shardNames.values()) lines.push(`void ${helper}(FontFaceData& face, NewcmFontTables& request);`);
lines.push('static FontFaceData makeNewcmFace(NewcmFontTables& request) {', '  FontFaceData face; face.name="MathJaxNewcm";');
lines.push(`  face.cssFamilyPrefix=${quote(sourceFont.cssFamilyPrefix)};`);
for (const [variant, [family, italic, bold]] of Object.entries(sourceFont.cssFontMap))
  lines.push(`  face.cssFontMap.insert_or_assign(${quote(variant)},CssFontData{${quote(family)},${italic},${bold}});`);
// For this pinned profile Source returns undefined; its actual stylesheet
// consumer treats that as a no-op, represented by the empty typed value.
lines.push('  face.styles.clear();');
for (const name of sourceFont.sizeVariants || []) lines.push(`  face.size_variants.push_back(${quote(name)});`);
for (const name of sourceFont.stretchVariants || []) lines.push(`  face.stretch_variants.push_back(${quote(name)});`);
for (const [key, value] of Object.entries(sourceFont.params || {})) lines.push(`  face.parameters.values.emplace(${quote(key)},${num(value)});`);
for (const [name, variant] of Object.entries(sourceFont.variant))
  lines.push(`  face.variants[${quote(name)}].name=${quote(name)}; face.variants[${quote(name)}].cache_id=${quote(variant.cacheID || '')};`);
glyphTargetFile.set(lines,'newcm_font_data_typed.inc');
for (const [variant, glyphs] of Object.entries(Font.defaultChars)) tupleRows(lines, variant, glyphs);
flushGlyphRows(lines);
variantSmpRemaps(lines, sourceBaseVariants);
for (const [name, inherit, link] of Font.defaultVariants) {
  const inh = inherit ? quote(inherit) : 'std::nullopt';
  const lnk = link ? quote(link) : 'std::nullopt';
  lines.push(`  face.variants[${quote(name)}].inherit_variant=${inh}; face.variants[${quote(name)}].link_overlay_variant=${lnk};`);
}
typedRemap(lines, 'accentRemap', sourceFont.remapChars?.accent || Font.defaultAccentMap);
typedRemap(lines, 'operatorRemap', sourceFont.remapChars?.mo || Font.defaultMoMap);
typedRemap(lines, 'numberRemap', sourceFont.remapChars?.mn || Font.defaultMnMap);
delimiters(lines, sourceFont.delimiters);
const shardDefinitions = [];
for (const [file, registration] of registrations) {
  const helper = shardNames.get(file);
  lines.push(`  ${helper}(face, request);`);
  const body = [
    'module folia.math.fonts.newcm_font_data;',
    'import folia.math.output.common.font_data;',
    'import folia.math.output.common.font_path;',
    'import folia.math.output.svg.font_data;',
    'import folia.math.output.scene.types;',
    'import std;',
    'namespace folia::math::fonts {',
    'using namespace output::common;',
    'using namespace output::svg;',
    'namespace {',
    'static void addGlyphRows(NewcmFontTables& request, NewcmFontGlyphRow const* rows, std::size_t count) { request.addRows(std::span{rows,count}); }',
    '}',
    `void ${helper}(FontFaceData& face, NewcmFontTables& request) {`,
  ];
  glyphTargetFile.set(body,`newcm_font_shard_${file.replace(/[^A-Za-z0-9_]/g,'_')}.cpp`);
  for (const [variant, glyphs] of Object.entries(registration.variants || {})) tupleRows(body, variant, glyphs);
  flushGlyphRows(body);
  body.push('}', '}');
  shardDefinitions.push({file, body});
}
lines.push(`  face.source_profile_glyph_count=${sourceProfileGlyphCount};`);
lines.push(`  face.source_dynamic_module_count=${registrations.size};`);
lines.push(`  face.source_provider_trace_events=${sourceProviderTraceEvents};`);

for (const {name,hash,data} of extensions) {
  const extensionLabel = name.replace('mathjax-','').replace('-font-extension','');
  lines.push(`  auto merge_${extensionLabel} = [&](FontFaceData& face, NewcmFontTables& request) {`);
  for (const [variant, parent] of data.variants?.['[+]'] || [])
    lines.push(`  face.variants[${quote(variant)}].inherit_variant=${quote(parent)};`);
  for (const [variant, glyphs] of Object.entries(data.chars || {})) tupleRows(lines, variant, glyphs);
  flushGlyphRows(lines);
  for (const variant of Object.keys(data.chars || {}))
    variantSmpRemaps(lines, {[variant]:sourceFont.variant[variant]});
  delimiters(lines, data.delimiters);
  const count = Object.values(data.chars || {}).reduce((sum, map) => sum + Object.keys(map).length, 0);
  lines.push('};');
  lines.push(`  merge_${extensionLabel}(face, request);`);
  lines.push(`  face.extension_glyph_counts.emplace(${quote(name)},${count});`);
}
for (const [name, helper] of svgAliasHelpers) lines.push(`  ${helper}(request);`);
const requestGlyphKeys = new Set([...glyphAudit.values()].flatMap(rows =>
  rows.map(row => JSON.stringify([row.args[0],Number(row.args[1])]))));
lines.push(`  request.setGlyphCount(${requestGlyphKeys.size});`);
lines.push('  return face;', '}');
fs.mkdirSync(outputDir, {recursive:true});
const destination = path.join(outputDir, 'newcm_font_data_typed.inc');
writeIfChanged(destination, lines.join('\n') + '\n');
const generatedFiles = [destination];
let svgAliasCount = 0;
for (const [name, helper] of svgAliasHelpers) {
  const variant = sourceFont.variant[name];
  const body = [
    'module folia.math.fonts.newcm_font_data;',
    'import folia.math.output.svg.font_data;',
    'import std;',
    'namespace folia::math::fonts {',
    'using namespace output::svg;',
    `void ${helper}(NewcmFontTables& request) {`
  ];
  const rows=[];
  for (const cpText in variant.chars) {
    const tuple = variant.chars[cpText];
    const owner = sourceTupleOwner.get(tuple);
    if (!Array.isArray(tuple)) throw new Error(`unresolved Source character row ${name}/U+${Number(cpText).toString(16)}`);
    if (Number.isInteger(tuple?.[3]?.smp)) continue;
    if (!owner) throw new Error(`Source visible glyph has no locked owner ${name}/U+${Number(cpText).toString(16)}`);
    const sourceVariant = sourceFont.variant[owner.variant];
    if (!sourceVariant) throw new Error(`missing Source variant for glyph owner ${owner.variant}`);
    const key = [name,Number(cpText)];
    const target = [owner.variant,owner.codepoint];
    if (JSON.stringify(key)!==JSON.stringify(target)) {
      rows.push(`    {${quote(key[0])},${key[1]},${quote(target[0])},${target[1]}}`);
      ++svgAliasCount;
    }
  }
  rows.sort((a,b) => Number(a.match(/,([0-9]+),/)[1])-Number(b.match(/,([0-9]+),/)[1]));
  body.push(`  static constexpr std::array<NewcmSvgAliasRow,${rows.length}> rows{{`,
    ...rows.map((row,index)=>`${row}${index+1<rows.length?',':''}`),
    '  }};',
    '  request.addAliases(rows);');
  body.push('}', '}');
  shardDefinitions.push({file:`svg_alias_${name}`,body});
}
for (const {file, body} of shardDefinitions) {
  const slug=file.replace(/[^A-Za-z0-9_]/g,'_');
  const shardPath = path.join(outputDir, `newcm_font_shard_${slug}.cpp`);
  writeIfChanged(shardPath, body.join('\n')+'\n');
  generatedFiles.push(shardPath);
}
const expectedSvgAliases=lazyJsonArray(function*(){
  for(const name of svgAliasVariants){
    const variant=sourceFont.variant[name];
    for(const cpText in variant.chars){
      const tuple=variant.chars[cpText], owner=sourceTupleOwner.get(tuple);
      if(!Array.isArray(tuple))throw new Error(`unresolved Source character row ${name}/U+${Number(cpText).toString(16)}`);
      if(Number.isInteger(tuple?.[3]?.smp))continue;
      if(!owner)throw new Error(`Source visible glyph has no locked owner ${name}/U+${Number(cpText).toString(16)}`);
      const sourceVariant=sourceFont.variant[owner.variant];
      if(!sourceVariant)throw new Error(`missing Source variant for glyph owner ${owner.variant}`);
      const key=[name,Number(cpText)], target=[owner.variant,owner.codepoint];
      if(JSON.stringify(key)!==JSON.stringify(target))yield [key,target];
    }
  }
});
// Capture the Source lookup result for every visible variant/key. This is the
// oracle for inheritance, link overlays, and SMP resolution; the comparator
// must not approximate FontData.getChar with its own table walk.
const sourceResolvedVisible = lazyJsonObject(function*(){
  for (const [name,variant] of Object.entries(sourceFont.variant)) {
    yield [name,lazyJsonObject(function*(){
      for (const cpText of sourceResolvedKeys.get(name)) {
        const requested = Number(cpText);
        const target = variant.chars[cpText]?.[3]?.smp || requested;
        // Wrapper.placeChar resolves SMP before calling getChar; retain both
        // keys and the resulting tuple so comparison reflects that exact Source flow.
        const glyph = sourceFont.getChar(name, target);
        if (!Array.isArray(glyph)) throw new Error(`Source visible glyph lookup failed ${name}/U+${Number(cpText).toString(16)}`);
        yield [cpText,{requested_codepoint:requested,target_codepoint:target,tuple:glyph}];
      }
    })];
  }
});
// Preserve the original eager lookup phase before the loadedVariants snapshot.
// FontData.getChar deletes dynamic placeholders and loads their glyph tables.
const sourceResolvedKeys = new Map();
for (const [name,variant] of Object.entries(sourceFont.variant)) {
  const keys = [];
  for (const cpText in variant.chars) {
    keys.push(cpText);
    const requested = Number(cpText);
    const target = variant.chars[cpText]?.[3]?.smp || requested;
    const glyph = sourceFont.getChar(name,target);
    if (!Array.isArray(glyph)) throw new Error(`Source visible glyph lookup failed ${name}/U+${Number(cpText).toString(16)}`);
  }
  sourceResolvedKeys.set(name,sortCodepointKeyList(keys));
}
const sourceSnapshot = {
  source_lock: 'tools/math-port/locks/font-data-source-lock.json',
  sourceProfile:{newcm_variant_glyph_keys:sourceProfileGlyphCount,dynamic_modules:registrations.size,
    provider_loader_calls:sourceProviderRequests,provider_trace_events:sourceProviderTraceEvents},
  newcm: {
    defaultParams: Font.defaultParams, defaultVariants: Font.defaultVariants,
    sizeVariants: Font.defaultSizeVariants, stretchVariants: Font.defaultStretchVariants,
    delimiters: Font.defaultDelimiters, accentMap: Font.defaultAccentMap,
    operatorMap: Font.defaultMoMap, numberMap: Font.defaultMnMap,
    variantSmp: Font.VariantSmp, smpRanges: Font.SmpRanges,
    smpRemap: Font.SmpRemap, smpRemapGreekU: Font.SmpRemapGreekU,
    smpRemapGreekL: Font.SmpRemapGreekL, defaultChars: Font.defaultChars,
    loadedVariants: lazyJsonObject(function*(){
      for (const [name,v] of Object.entries(sourceFont.variant)) {
        yield [name,{cacheID:v.cacheID,inherit:v.inherit,linked:v.linked?.length || 0,
          ownChars:v.chars,visibleChars:lazyJsonObject(function*(){
            for (const cp of sortedCodepointKeys(v.chars)) yield [cp,v.chars[cp]];
          })}];
      }
    }),
    effectiveSizeVariants: sourceFont.sizeVariants,
    effectiveStretchVariants: sourceFont.stretchVariants,
    effectiveParameters: sourceFont.params,
    effectiveDelimiters: sourceFont.delimiters,
    effectiveRemaps: sourceFont.remapChars,
    dynamic: [...registrations].map(([file,r]) => ({file,variants:r.variants,delimiters:r.delimiters})),
    providerTrace: {requests:sourceProviderRequests,events:sourceProviderTraceEvents}
  },
  sourceResolvedVisible,
  extensions: extensions.map(({name,hash,data}) => ({name,sha256:hash,data})),
  svgAliases:expectedSvgAliases
};
const expectedPath = path.join(outputDir,'font-source-expected.json');
writeJsonIfChanged(expectedPath, sourceSnapshot);
const auditFiles = fs.readdirSync(outputDir).filter(file=>glyphAudit.has(file));
const auditRows = auditFiles.flatMap(file => glyphAudit.get(file).map(row=>[file,JSON.stringify(row.args)]));
const auditFinal = new Map();
const auditPerFiles = auditFiles.map(file => {
  const rows = glyphAudit.get(file);
  for (const row of rows) auditFinal.set(JSON.stringify([row.args[0],Number(row.args[1])]),JSON.stringify(row.args));
  return {path:file,rows:rows.length,source_rows_sha256:crypto.createHash('sha256').update(JSON.stringify(rows)).digest('hex')};
});
const glyphRowAudit = {
  files:auditPerFiles, ordered_rows:auditRows.length, table_loops:glyphTableCounter,
  ordered_rows_sha256:crypto.createHash('sha256').update(JSON.stringify(auditRows)).digest('hex'),
  final_keys:auditFinal.size, final_table_sha256:crypto.createHash('sha256').update(JSON.stringify([...auditFinal])).digest('hex'),
  composition_rows:auditFiles.flatMap(file=>glyphAudit.get(file)).filter(row=>row.values.length).length,
  max_composition_size:Math.max(0,...auditFiles.flatMap(file=>glyphAudit.get(file)).map(row=>row.values.length)),
  content_defined_rows:auditFiles.flatMap(file=>glyphAudit.get(file)).filter(row=>row.args[13]!=='std::nullopt').length,
  absent_path_rows:auditFiles.flatMap(file=>glyphAudit.get(file)).filter(row=>row.args[9]==='false').length,
  empty_defined_path_rows:auditFiles.flatMap(file=>glyphAudit.get(file)).filter(row=>row.args[9]==='true'&&row.args[11]==='""').length,
  source_lock_input_count:sourceLock.inputs.length,
  source_lock_inputs_sha256:crypto.createHash('sha256').update(JSON.stringify(sourceLock.inputs)).digest('hex')
};
writeIfChanged(path.join(outputDir,'bounded-initializer-row-equivalence.json'),JSON.stringify(glyphRowAudit,null,2)+'\n');
const glyphs = Object.values(Font.defaultChars).reduce((n, m) => n + Object.keys(m).length, 0) +
  [...registrations.values()].reduce((n, r) => n + Object.values(r.variants || {}).reduce((k, m) => k + Object.keys(m).length, 0), 0);
const remappedCount = sourceProfileGlyphCount;
if (remappedCount !== 24646) throw new Error(`NewCM variant/remap profile changed: expected 24646, got ${remappedCount}`);
const sourceGlyph = (variant, codepoint) => {
  const mapping = sourceFont.variant[variant].chars[codepoint]?.[3]?.smp || codepoint;
  const glyph = sourceFont.getChar(variant, mapping);
  if (!Array.isArray(glyph)) throw new Error(`Source glyph missing: ${variant}/${codepoint}`);
  return {variant, cache_id:sourceFont.variant[variant].cacheID || '', codepoint, target_codepoint:mapping, metrics:glyph.slice(0,3)};
};
const metricWitnesses=[sourceGlyph('normal',0x41),sourceGlyph('bold',0x3B1),sourceGlyph('bold',0x1D400),
  sourceGlyph('bold-fraktur',0x20),sourceGlyph('-ds-rm',0x41),sourceGlyph('-bbm-normal',0x41)];
const metricEquals=(actual,expected)=>actual.length===expected.length && actual.every((value,index)=>value===expected[index]);
if (!metricEquals(metricWitnesses[0].metrics,[.716,0,.75]) ||
    !metricEquals(metricWitnesses[1].metrics,[.452,.008,.743]) ||
    !metricEquals(metricWitnesses[2].metrics,[.698,0,.869]) ||
    metricWitnesses[3].metrics[2] !== .383 ||
    metricWitnesses[4].target_codepoint !== 0x1D538 ||
    metricWitnesses[5].target_codepoint !== 0x1D538 ||
    metricWitnesses[4].cache_id !== '' ||
    metricWitnesses[5].cache_id !== '')
  throw new Error(`pinned Source metric witnesses changed: ${JSON.stringify(metricWitnesses)}`);
const extensionRecords = extensions.map(x=>({name:x.name,count:Object.values(x.data.chars||{}).reduce((sum,map)=>sum+Object.keys(map).length,0)}));
if (extensionRecords.some((item,index)=>item.count !== [290,391,60][index])) throw new Error(`pinned extension glyph profile changed: ${JSON.stringify(extensionRecords)}`);
const dependencyManifestPath = path.join(outputDir, 'font-data-source-dependencies.json');
const dependencyManifest = {source_lock:'tools/math-port/locks/font-data-source-lock.json',
  source_dependencies:sourceLock.inputs.map(entry=>({path:`tools/mathjax-bundle/node_modules/${entry.package.replace(/^@mathjax\//,'@mathjax/')}/${entry.path}`,sha256:entry.sha256})),
  generated_cpp:generatedFiles.map(file=>path.relative(root,file).replaceAll('\\','/'))};
writeIfChanged(dependencyManifestPath, JSON.stringify(dependencyManifest,null,2)+'\n');
const manifest = {source:'MathJax 4.1.3 commit fb987178c2d279a99b0db5ea02e751691703955e', source_lock:'tools/math-port/locks/font-data-source-lock.json', source_lock_input_count:sourceLock.inputs.length, source_dependency_manifest:path.relative(root,dependencyManifestPath).replaceAll('\\','/'), generated_cpp:dependencyManifest.generated_cpp, packages:[{name:'mathjax-newcm-font',sha256:newcmHash},...extensions.map(x=>({name:x.name,sha256:x.hash}))], modules:moduleInputs, newcm_direct_glyph_records:glyphs, newcm_source_variant_glyph_count:remappedCount, extension_glyph_records:extensionRecords, svg_effective_aliases:svgAliasCount, initializer_rows:glyphRowAudit, metric_witnesses:metricWitnesses, dynamic_modules:registrations.size, source_provider_loader_calls:sourceProviderRequests, source_provider_trace_events:sourceProviderTraceEvents, required_provider_requests:80, required_profile_glyphs:24646, generated_translation_units:generatedFiles.map(file=>({path:path.basename(file),bytes:fs.statSync(file).size,sha256:crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex')})), compile_pool_limit:2};
manifest.generated_bytes=manifest.generated_translation_units.reduce((sum,file)=>sum+file.bytes,0);
manifest.source_expected_table={path:path.basename(expectedPath),bytes:fs.statSync(expectedPath).size,sha256:crypto.createHash('sha256').update(fs.readFileSync(expectedPath)).digest('hex')};
writeIfChanged(path.join(outputDir,'font-data-typed-manifest.json'), JSON.stringify(manifest,null,2)+'\n');
console.log(`generated ${path.basename(destination)}: ${manifest.generated_bytes} bytes, NewCM source glyph records=${glyphs}, dynamic modules=${registrations.size}`);
