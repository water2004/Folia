import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath,pathToFileURL} from 'node:url';
function repositoryRoot(start=path.dirname(fileURLToPath(import.meta.url))) {
  for(let current=path.resolve(start);current!==path.dirname(current);current=path.dirname(current))
    if(fs.existsSync(path.join(current,'tools/math-port/README.md'))&&
       fs.existsSync(path.join(current,'tools/mathjax-bundle/node_modules/@mathjax/src/package.json')))return current;
  return null;
}
export async function resolve(specifier, context, nextResolve) {
  if (context.parentURL?.endsWith('/ts/output/common/Wrappers/mo.ts')) {
    const root=repositoryRoot();
    if(!root)throw new Error('CommonMo loader cannot locate repository root');
    const fixtureRoot=process.env.FOLIA_COMMON_SOURCE_FIXTURE_ROOT||path.join(root,'tests/math/fixtures/common-source');
    const stubPath=path.join(fixtureRoot,'common-mo-source-stubs.mjs');
    return {url:pathToFileURL(stubPath).href,shortCircuit:true};
  }
  return nextResolve(specifier, context);
}
