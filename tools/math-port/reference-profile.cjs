// Root-owned development oracle. Never linked to, or invoked by, native runtime.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const {createRequire} = require('node:module');
const cp = require('node:child_process');
const root = path.resolve(__dirname,'../..');
const profilePath = path.join(root,'tools/mathjax-bundle/entry.cjs');
const req = createRequire(profilePath);
const sha = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
let created = false;

function moduleTreeHash(directory) {
  const records=[];
  function walk(local,relative='') {
    for (const entry of fs.readdirSync(local,{withFileTypes:true}).sort((a,b)=>a.name<b.name?-1:a.name>b.name?1:0)) {
      const name=relative+entry.name,absolute=path.join(local,entry.name);
      if (entry.isDirectory()) walk(absolute,name+'/');
      else if (/\.(js|cjs|mjs|json)$/.test(name)) records.push([name,sha(fs.readFileSync(absolute))]);
    }
  }
  walk(directory);
  return {files:records.length,sha256:sha(JSON.stringify(records))};
}

function createReference() {
  assert.equal(created,false,'One reference profile per process: font static extensions and global registries are shared');
  created = true;
  const scope = JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/scope.json'),'utf8'));
  const commit = cp.execFileSync('git',['rev-parse','HEAD'],{cwd:path.join(root,'.reference/MathJax-src'),encoding:'utf8'}).trim();
  assert.equal(commit,scope.upstream.commit);
  const sourcePackage = req('@mathjax/src/package.json');
  assert.equal(sourcePackage.version,scope.upstream.version);
  const entry = fs.readFileSync(profilePath);
  assert.equal(sha(entry),scope.profile.sha256,'Production profile changed; root must review scope');
  const lock = fs.readFileSync(path.join(root,'tools/mathjax-bundle/package-lock.json'));
  assert.equal(sha(lock),scope.dependency_lock_sha256);
  const compiled = [];
  for (const file of scope.files) {
    const source = fs.readFileSync(path.join(root,'.reference/MathJax-src',file.source));
    assert.equal(sha(source),file.source_sha256,`Pinned source changed: ${file.source}`);
    const installedSource = fs.readFileSync(path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src',file.source));
    assert.equal(installedSource.toString('utf8').replaceAll('\r\n','\n'),source.toString('utf8').replaceAll('\r\n','\n'));
    const cjs = file.source.replace(/^ts\//,'cjs/').replace(/\.ts$/,'.js');
    const compiledPath = path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src',cjs);
    if (fs.existsSync(compiledPath)) compiled.push([cjs,sha(fs.readFileSync(compiledPath))]);
  }
  const dependencyModules={};
  for (const font of scope.font_packages) {
    assert.equal(req(font.package+'/package.json').version,font.version);
    dependencyModules[font.package]=moduleTreeHash(path.join(path.dirname(req.resolve(font.package+'/package.json')),'cjs'));
  }
  // The mhchem syntax package delegates to this external parser via #mhchem.
  // A relative-TypeScript-import inventory alone does not pin its behavior.
  const chemistryRoot=path.join(root,'tools/mathjax-bundle/node_modules/mhchemparser');
  const chemistryPackage=JSON.parse(fs.readFileSync(path.join(chemistryRoot,'package.json'),'utf8'));
  const chemistryLock=JSON.parse(lock).packages['node_modules/mhchemparser'];
  assert.equal(chemistryPackage.version,chemistryLock.version);
  dependencyModules.mhchemparser={version:chemistryPackage.version,integrity:chemistryLock.integrity,
    ...moduleTreeHash(path.join(chemistryRoot,'esm'))};
  const loadedFonts = [];
  globalThis.FoliaLoadMathJaxModule = name => {
    // Use the same synchronous lazy-loading contract as Folia, with the pinned
    // executable font modules rather than transformed offline bundle files.
    const specifier = String(name);
    assert.match(specifier,/^@mathjax\/mathjax-(newcm-font|bbm-font-extension|bboldx-font-extension|dsfont-font-extension)\/js\//);
    loadedFonts.push(specifier);
    req(specifier);
  };
  const module = {exports:{}};
  const compile = new Function('require','module','exports',entry.toString('utf8')+
    '\nmodule.exports={input,output,document,adaptor,mathjax};');
  compile(req,module,module.exports);
  return {...module.exports, loadedFonts, require:req, provenance:{
    upstream:scope.upstream, profile_sha256:scope.profile.sha256,
    dependency_lock_sha256:scope.dependency_lock_sha256,
    compiled_source_sha256:sha(JSON.stringify(compiled)),
    dependency_modules:dependencyModules,
    packages:scope.profile.tex_packages, svg_options:scope.profile.svg_options,
    font_packages:scope.font_packages.map(({package,version,integrity})=>({package,version,integrity})),
    node:process.version
  }};
}
module.exports={createReference};
