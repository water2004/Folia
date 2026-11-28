'use strict';
// One Source case in a clean Node process/module realm. The parent verifies the
// complete package lock and unions this process's loaded dependency closure.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');

const root = path.resolve(__dirname, '../../../..');
const packages = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
const cjsRoot = path.join(packages, 'src/cjs');
const scopePath = path.join(root, 'tools/math-port/profiles/scope.json');
const scope = JSON.parse(fs.readFileSync(scopePath, 'utf8'));
const profilePath = path.join(root, scope.profile.file);
const profileText = fs.readFileSync(profilePath, 'utf8');
const profileLockPath = path.join(root, 'tools/math-port/common-layout-reference/profile-source-lock.json');
const profileLock = JSON.parse(fs.readFileSync(profileLockPath, 'utf8'));
const corpusPath = path.join(root, 'tools/math-port/common-layout-reference/profile-cases.json');
const inputPath = path.join(root, 'build/math-port/common-layout-profile-root-locked.json');
const inputCapture = JSON.parse(fs.readFileSync(inputPath, 'utf8'));
const digest = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const hashFile = file => digest(fs.readFileSync(file));
const caseIndex = Number(process.argv[2]);
assert.ok(Number.isInteger(caseIndex) && caseIndex >= 0 && caseIndex < 75, 'expected case index 0..74');

assert.equal(scope.upstream.version, '4.1.3');
assert.equal(scope.upstream.commit, 'fb987178c2d279a99b0db5ea02e751691703955e');
assert.equal(hashFile(scopePath), profileLock.scopeHash);
assert.equal(hashFile(profilePath), scope.profile.sha256);
assert.equal(hashFile(profileLockPath), 'acd3decb56eaeff289493133856413a704e7cc75d67fbc50de221cf716a0bf72');
assert.equal(hashFile(corpusPath), profileLock.corpusSha256);
assert.equal(hashFile(inputPath), '9f58d09411512b4682b1f0049e40d6a47844ddffb9794eda52e2700998e0d740');
assert.equal(hashFile(path.join(root, 'tools/mathjax-bundle/package-lock.json')), scope.dependency_lock_sha256);
assert.equal(JSON.parse(fs.readFileSync(path.join(packages, 'src/package.json'), 'utf8')).version, '4.1.3');
assert.equal(inputCapture.records.length, 75);

const load = file => require(path.join(cjsRoot, file));
const { mathjax } = load('mathjax.js');
const { TeX } = load('input/tex.js');
const { SVG } = load('output/svg.js');
const { liteAdaptor } = load('adaptors/liteAdaptor.js');
const { RegisterHTMLHandler } = load('handlers/html.js');
const { MathJaxNewcmFont } = require(path.join(packages, 'mathjax-newcm-font/cjs/svg.js'));
for (const match of profileText.matchAll(/require\('@mathjax\/src\/js\/(input\/tex\/[^']+Configuration\.js)'\)/g)) {
  require(path.join(cjsRoot, match[1]));
}

const adaptor = liteAdaptor({ fontSize: 16 });
RegisterHTMLHandler(adaptor);
mathjax.asyncIsSynchronous = true;
mathjax.asyncLoad = name => {
  const target = path.isAbsolute(name) ? name : path.resolve(cjsRoot, name);
  assert.ok(target.startsWith(packages + path.sep), `Source requested an external module: ${name}`);
  return require(target);
};

const item = inputCapture.records[caseIndex];
const input = new TeX({ packages: scope.profile.tex_packages });
const output = new SVG({ ...scope.profile.svg_options, fontData: MathJaxNewcmFont,
  dynamicPrefix: path.join(packages, 'mathjax-newcm-font/cjs/svg/dynamic') });
for (const [name, exportName] of [
  ['bboldx', 'MathJaxBboldxFontExtension'], ['bbm', 'MathJaxBbmFontExtension'],
  ['dsfont', 'MathJaxDsfontFontExtension']
]) output.addExtension(require(path.join(packages, `mathjax-${name}-font-extension/cjs/svg.js`))[exportName]);
const document = mathjax.document('', { InputJax: input, OutputJax: output });
const em = item.em ?? 16, ex = item.ex ?? em * .5, width = item.width ?? 1280;
let svg = null, error = null;
try {
  svg = adaptor.serializeXML(document.convert(item.tex, {
    display: item.display ?? false, em, ex, containerWidth: width
  }));
} catch (caught) {
  error = { name: caught.name || caught.constructor.name, message: String(caught.message || caught) };
}
const record = {
  group: 'fresh-session-per-case', index: caseIndex, name: item.name, tex: item.tex,
  display: item.display ?? false, em, ex, width, svg,
  mmlError: svg !== null && (svg.includes('data-mjx-error=') || svg.includes('data-mml-node="merror"')),
  error
};
const loadedDependencies = Object.keys(require.cache)
  .filter(file => file.startsWith(packages + path.sep))
  .sort()
  .map(file => ({ file: path.relative(packages, file).replaceAll(path.sep, '/'), sha256: hashFile(file) }));
process.stdout.write(JSON.stringify({ record, loadedDependencies }));
