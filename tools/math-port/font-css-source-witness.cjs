#!/usr/bin/env node
'use strict';

// Locked behavioral witness for FontData CSS metadata. This script reads and
// executes only the repository's pinned MathJax 4.1.3 sources.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const root = path.resolve(__dirname, '../..');
const packages = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
const lock = JSON.parse(fs.readFileSync(path.join(root, 'tools/math-port/locks/font-data-source-lock.json'), 'utf8'));
if (lock.source !== 'MathJax 4.1.3, commit fb987178c2d279a99b0db5ea02e751691703955e')
  throw new Error('wrong pinned MathJax source lock');
const expectedPackageHashes = {
  'mathjax-newcm-font':'709d6e85113ef44645ad802b8424508d5f836dd61e4d1f2797b5e011a9693734',
  'mathjax-bbm-font-extension':'4330e4138451f2efd659d6a2bd4038c163e7fa98977474315b138bd5e6e2bf21',
  'mathjax-bboldx-font-extension':'9ac39ff107d6291e15e16764f45cb794cf133333ebf268281e651b54ed6ab0d1',
  'mathjax-dsfont-font-extension':'7a84404f759fcce1789a62389836245cb7aaf30c37556d1181c281d00a838478',
};
const sha = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const sourcePackages = {};
for (const [name, expected] of Object.entries(expectedPackageHashes)) {
  const dir = path.join(packages, name);
  const packageJson = JSON.parse(fs.readFileSync(path.join(dir, 'package.json'), 'utf8'));
  const cjsPath = path.join(dir, 'cjs/svg.js');
  const actual = sha(fs.readFileSync(cjsPath));
  const lockRow = lock.inputs.find(row => row.package === `@mathjax/${name}` && row.path === 'cjs/svg.js');
  if (packageJson.version !== '4.1.3' || !lockRow || lockRow.sha256 !== expected || actual !== expected)
    throw new Error(`pinned source mismatch for ${name}: ${actual}`);
  sourcePackages[name] = {version:packageJson.version, path:'cjs/svg.js', sha256:actual};
}
const hashFile = relative => sha(fs.readFileSync(path.join(packages, relative)));
const sourceFiles = {
  fontDataCjs: {path:'src/cjs/output/common/FontData.js', sha256:hashFile('src/cjs/output/common/FontData.js'), expected:'b14584e26fcb9c76d74ae4688b37e0afcc348278dc26126b4baf6662886ff63a'},
  fontDataTs: {path:'src/ts/output/common/FontData.ts', sha256:hashFile('src/ts/output/common/FontData.ts'), expected:'aec96040b5aa147276c9e144a13b9716ab1ef4d8b50f9c6562a5ac9bbc35ec00', lines:{constructor:[925,926], defineCssFonts:[1148,1155], stylesGetter:[1008,1009], getCssFont:[1407,1409], getFamily:[1415,1417]}},
  styleJsonCjs: {path:'src/cjs/util/StyleJson.js', sha256:hashFile('src/cjs/util/StyleJson.js'), expected:'dfd45381fc15fa9b8a038e8ac782550934b5327871d10534df52af02aa43f11f'},
  styleJsonTs: {path:'src/ts/util/StyleJson.ts', sha256:hashFile('src/ts/util/StyleJson.ts'), expected:'21d6fb02e99c3e1d719c099d823f1d9d2275ad005c69187e4d45024662f479f7', lines:{addStyles:[66,69]}},
};
for (const row of Object.values(sourceFiles)) if (row.sha256 !== row.expected) throw new Error(`pinned implementation source changed: ${row.path}`);
const Font = require(path.join(packages, 'mathjax-newcm-font/cjs/svg.js')).MathJaxNewcmFont;
const face = new Font();
for (const name of ['mathjax-bbm-font-extension','mathjax-bboldx-font-extension','mathjax-dsfont-font-extension']) {
  const extension = Object.values(require(path.join(packages, name, 'cjs/svg.js')))[0];
  face.addExtension(extension);
}
const sortedSourceMap = Object.fromEntries(Object.keys(face.cssFontMap).sort().map(name => [name, face.cssFontMap[name]]));
const cssFontMapSha256 = sha(Buffer.from(JSON.stringify(sortedSourceMap)));
const expectedCssFontMapSha256 = 'db47e065c8d2a277c6268c1420cfa36bc1f02d1a424c66949baf402624d021e9';
if (Object.keys(sortedSourceMap).length !== 33 || cssFontMapSha256 !== expectedCssFontMapSha256)
  throw new Error(`expected 33 pinned CSS tuples (${expectedCssFontMapSha256}), got ${Object.keys(sortedSourceMap).length} (${cssFontMapSha256})`);
const {StyleJsonSheet} = require(path.join(packages, 'src/cjs/util/StyleJson.js'));
const styleSheet = new StyleJsonSheet();
styleSheet.addStyles(face.styles);
if (face.cssFamilyPrefix !== '' || face.styles !== undefined || styleSheet.cssText !== '' ||
    JSON.stringify(face.getCssFont('missing')) !== JSON.stringify(['serif', false, false]) ||
    face.getFamily('serif') !== 'serif' || face.getFamily('') !== '')
  throw new Error('pinned NewCM CSS query or stylesheet behavior changed');
const cssFontMap = Object.fromEntries(Object.entries(sortedSourceMap).map(([name, [family, italic, bold]]) =>
  [name, {family, italic, bold}]));
const evidence = {
  source: lock.source,
  profile: 'NewCM plus BBM, Bboldx and dsfont extensions in canonical provider order',
  sourcePackages,
  sourceFiles,
  cssFamilyPrefix: face.cssFamilyPrefix,
  cssFontMapCount: Object.keys(cssFontMap).length,
  cssFontMapSha256,
  cssFontMap,
  getCssFont: {normal:face.getCssFont('normal'), unknown:face.getCssFont('missing')},
  getFamily: {serif:face.getFamily('serif'), empty:face.getFamily('')},
  styles: {sourceValue:'undefined', stylesheetCssText:styleSheet.cssText, typedEmptyIsAdditiveIdentity:true},
};
console.log(JSON.stringify(evidence, null, 2));
