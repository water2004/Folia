'use strict';
const assert=require('node:assert/strict');
const {execFileSync}=require('node:child_process');
const fs=require('node:fs');
const path=require('node:path');
function findRoot(start=__dirname){for(let current=path.resolve(start);current!==path.dirname(current);current=path.dirname(current))if(fs.existsSync(path.join(current,'tools/math-port/profiles/scope.json'))&&fs.existsSync(path.join(current,'tools/mathjax-bundle/node_modules/@mathjax/src/package.json')))return current;return null;}
const rootArg=process.argv.indexOf('--repo-root');
assert.ok(rootArg<0?process.argv.length===2:(process.argv[rootArg+1]&&process.argv.length===rootArg+2),'usage: node common-containers-source-witness.cjs [--repo-root <repository>]');
const root=rootArg<0?findRoot():path.resolve(process.argv[rootArg+1]);
assert.ok(root,'pass --repo-root <repository> or run from the repository');
for(const script of ['common-containers-method-source-witness.cjs','common-containers-geometry-source-witness.cjs'])
  execFileSync(process.execPath,[path.join(__dirname,script),'--repo-root',root],{cwd:root,stdio:'inherit'});
const evidenceDir=path.join(root,'build/math-port/common-source');
const method=JSON.parse(fs.readFileSync(path.join(evidenceDir,'containers-method-source.json'),'utf8'));
const geometry=JSON.parse(fs.readFileSync(path.join(evidenceDir,'containers-geometry-source.json'),'utf8'));
assert.equal(geometry.length,6,'expected six locked renderer geometry observations');
const summary={version:method.version,methodChecks:method.checks.length,rendererCases:geometry.length,
  evidence:['containers-method-source.json','containers-geometry-source.json']};
fs.writeFileSync(path.join(evidenceDir,'containers-source-witness.json'),JSON.stringify(summary,null,2)+'\n');
process.stdout.write(`verified containers Source methods and ${geometry.length} actual MathML/TeX SVG/NewCM cases\n`);
