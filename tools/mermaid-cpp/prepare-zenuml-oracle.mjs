// Development-only source oracle. The product does not depend on this package.
import assert from 'node:assert/strict';
import {existsSync, mkdirSync, readFileSync, writeFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';

const directory = resolve(process.env.FOLIA_ZENUML_SOURCE ?? 'build/mermaid-cpp/zenuml-source');
mkdirSync(directory, {recursive: true});
const archive = resolve(directory, 'core-3.49.2.tgz');
if (!existsSync(archive)) {
  const response = await fetch('https://registry.npmjs.org/@zenuml/core/-/core-3.49.2.tgz');
  assert(response.ok, `ZenUML source download: ${response.status}`);
  writeFileSync(archive, new Uint8Array(await response.arrayBuffer()));
}
assert.equal(createHash('sha512').update(readFileSync(archive)).digest('base64'),
  'qeQMapuE0XETML2mRw2MIIrMCjcZBhRDSwW10//q8XQ+VyExzkAn39VCBwxIbbPdTBt6gJnaBBixF9A4hsIxcA==');
execFileSync('tar', ['-xzf', archive, '-C', directory]);
const sourceMap = JSON.parse(readFileSync(resolve(directory, 'package/dist/zenuml.esm.mjs.map'), 'utf8'));
for (const [index, name] of sourceMap.sources.entries()) {
  if (!['positioning/', 'parser/', 'svg/', 'generated-parser/']
      .some(prefix => name.startsWith('../src/' + prefix))) continue;
  const target = resolve(directory, 'original', name.slice(3));
  mkdirSync(resolve(target, '..'), {recursive: true});
  writeFileSync(target, sourceMap.sourcesContent[index]);
}
console.log('Prepared @zenuml/core 3.49.2 (gitHead 213c239587ffa1f3dbb54d8b64468c3038ced55d).');
