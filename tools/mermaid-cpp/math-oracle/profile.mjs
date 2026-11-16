// Independent test host for the committed MathJax profile. No native output is
// used to construct expectations, and no historical math-port lock is changed.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync, readdirSync} from 'node:fs';
import {createRequire} from 'node:module';
import {dirname, join, resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

export const directory = dirname(fileURLToPath(import.meta.url));
export const root = resolve(directory, '../../..');
export const sha256 = bytes => createHash('sha256').update(bytes).digest('hex');
export const fileHash = path => sha256(readFileSync(path));

export function moduleTree(directory) {
  const rows = [];
  function walk(path, prefix = '') {
    for (const item of readdirSync(path, {withFileTypes: true}).sort((a, b) =>
      a.name < b.name ? -1 : a.name > b.name ? 1 : 0)) {
      const name = prefix + item.name;
      if (item.isDirectory()) walk(join(path, item.name), name + '/');
      else if (/\.(?:js|cjs|mjs|json)$/.test(name)) rows.push([name, fileHash(join(path, item.name))]);
    }
  }
  walk(directory);
  return {files: rows.length, sha256: sha256(JSON.stringify(rows))};
}

export function verifyLock() {
  const lock = JSON.parse(readFileSync(join(directory, 'source-lock.json'), 'utf8'));
  assert.equal(lock.schema, 1);
  for (const file of lock.files) assert.equal(fileHash(join(root, file.path)), file.sha256, file.path);
  for (const dependency of lock.mathjax.dependencies) {
    const packageRoot = join(root, 'tools/mathjax-bundle/node_modules', dependency.name);
    const metadata = JSON.parse(readFileSync(join(packageRoot, 'package.json'), 'utf8'));
    assert.equal(metadata.version, dependency.version, dependency.name);
    assert.deepEqual(moduleTree(join(packageRoot, dependency.modules)), dependency.tree, dependency.name);
  }
  return lock;
}

let loaded = false;
export function loadMathJaxProfile() {
  assert.equal(loaded, false, 'One MathJax profile per process: its registries are shared');
  const lock = verifyLock();
  loaded = true;
  const profilePath = join(root, 'tools/mathjax-bundle/entry.cjs');
  const require = createRequire(profilePath);
  const dynamicModules = [];
  globalThis.FoliaLoadMathJaxModule = name => {
    const specifier = String(name);
    assert.match(specifier, /^@mathjax\/mathjax-(newcm-font|bbm-font-extension|bboldx-font-extension|dsfont-font-extension)\/js\//);
    dynamicModules.push(specifier);
    require(specifier);
  };
  const module = {exports: {}};
  // Execute the exact committed profile, including its real font modules. The
  // fragment contract changes only overflow policy; the editor profile stays as is.
  new Function('require', 'module', 'exports', readFileSync(profilePath, 'utf8') +
    '\nmodule.exports = {input, output, document, adaptor, mathjax};')(require, module, module.exports);
  const profile = module.exports;
  profile.output.options.displayOverflow = 'overflow';
  profile.output.options.linebreaks = {...profile.output.options.linebreaks, inline: false};
  return {...profile, dynamicModules, provenance: lock};
}
