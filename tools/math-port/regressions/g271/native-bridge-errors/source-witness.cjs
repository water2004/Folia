'use strict';
// Source-only error semantics witness for the fixed Folia MathJax profile.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');

const root = path.resolve(__dirname, '../../../../..');
const packages = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
const cjsRoot = path.join(packages, 'src/cjs');
const scopePath = path.join(root, 'tools/math-port/profiles/scope.json');
const scope = JSON.parse(fs.readFileSync(scopePath, 'utf8'));
const profilePath = path.join(root, scope.profile.file);
const profileText = fs.readFileSync(profilePath, 'utf8');
const lockPath = path.join(root, 'tools/math-port/common-layout-reference/profile-source-lock.json');
const lock = JSON.parse(fs.readFileSync(lockPath, 'utf8'));
const digest = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const hashFile = file => digest(fs.readFileSync(file));
assert.equal(scope.upstream.version, '4.1.3');
assert.equal(scope.upstream.commit, 'fb987178c2d279a99b0db5ea02e751691703955e');
assert.equal(hashFile(scopePath), lock.scopeHash);
assert.equal(hashFile(profilePath), scope.profile.sha256);
assert.equal(hashFile(lockPath), 'acd3decb56eaeff289493133856413a704e7cc75d67fbc50de221cf716a0bf72');
assert.equal(hashFile(path.join(root, 'tools/mathjax-bundle/package-lock.json')), scope.dependency_lock_sha256);
assert.equal(JSON.parse(fs.readFileSync(path.join(packages, 'src/package.json'), 'utf8')).version, '4.1.3');
for (const dep of lock.dependencies) {
  const file = path.join(packages, dep.file);
  assert.equal(hashFile(file), dep.sha256, `pinned dependency changed: ${dep.file}`);
}

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
const input = new TeX({ packages: scope.profile.tex_packages });
const output = new SVG({ ...scope.profile.svg_options, fontData: MathJaxNewcmFont,
  dynamicPrefix: path.join(packages, 'mathjax-newcm-font/cjs/svg/dynamic') });
for (const [name, exportName] of [
  ['bboldx', 'MathJaxBboldxFontExtension'], ['bbm', 'MathJaxBbmFontExtension'],
  ['dsfont', 'MathJaxDsfontFontExtension']
]) output.addExtension(require(path.join(packages, `mathjax-${name}-font-extension/cjs/svg.js`))[exportName]);
const document = mathjax.document('', { InputJax: input, OutputJax: output });
const cases = [
  { name: 'unknown-command-recovery', tex: '\\notacommand', expectedMerror: false },
  { name: 'structural-frac-error', tex: '\\frac{', expectedMerror: true }
];
const records = cases.map(item => {
  const svg = adaptor.serializeXML(document.convert(item.tex, { display: false, em: 16, ex: 8, containerWidth: 640 }));
  const merror = svg.includes('data-mml-node="merror"');
  assert.ok(svg.includes('<svg'), `${item.name}: missing SVG`);
  assert.equal(merror, item.expectedMerror, `${item.name}: merror expectation changed`);
  return {
    ...item,
    svgSha256: digest(svg),
    svgBytes: Buffer.byteLength(svg),
    merror,
    hasMtext: svg.includes('data-mml-node="mtext"'),
    hasMfrac: svg.includes('data-mml-node="mfrac"'),
    hasErrorAttribute: svg.includes('data-mjx-error='),
    errorEvidence: merror ? svg.match(/<g[^>]*data-mml-node="merror"[^>]*>/)?.[0] ?? null : null,
    unknownRecoveryEvidence: !merror ? svg.match(/<g[^>]*data-mml-node="mtext"[^>]*>/)?.[0] ?? null : null
  };
});
const loadedDependencies = Object.keys(require.cache)
  .filter(file => file.startsWith(packages + path.sep))
  .sort()
  .map(file => ({ file: path.relative(packages, file).replaceAll(path.sep, '/'), sha256: hashFile(file) }));
process.stdout.write(JSON.stringify({
  schema: 1,
  upstream: scope.upstream,
  profile: { file: scope.profile.file, sha256: hashFile(profilePath), texPackages: scope.profile.tex_packages,
    svgOptions: scope.profile.svg_options },
  pins: { scopeSha256: hashFile(scopePath), profileLockSha256: hashFile(lockPath),
    dependencyLockSha256: hashFile(path.join(root, 'tools/mathjax-bundle/package-lock.json')),
    lockedSourceDependencyCount: lock.dependencies.length },
  records,
  loadedDependencyCount: loadedDependencies.length,
  loadedDependencies
}, null, 2));