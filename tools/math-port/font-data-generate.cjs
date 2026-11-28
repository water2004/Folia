#!/usr/bin/env node
'use strict';

// Deterministically transcribe the pinned 4.1.3 NewCM CJS tables into native
// initializer code.  This runs only as a maintainer tool; native rendering
// never loads or evaluates JavaScript.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');

const root = path.resolve(__dirname, '../..');
const cli=process.argv.slice(2);
let generatedRoot=path.join(root,'build/math-port/font-generated');
let checkLock=false;
for(let i=0;i<cli.length;i++){
  if(cli[i]==='--output-dir'){
    if(!cli[i+1])throw new Error('--output-dir requires a path');
    const requested=cli[++i];
    if(!path.isAbsolute(requested))throw new Error('--output-dir requires an absolute path');
    generatedRoot=path.resolve(requested);
  } else if(cli[i]==='--check-lock')checkLock=true;
  else throw new Error(`unknown argument: ${cli[i]}`);
}
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
function resolveSourceRequire(from, request) {
  let candidate;
  if (request.startsWith('.')) candidate=path.resolve(path.dirname(from),request);
  else if (request.startsWith('@mathjax/')) candidate=path.join(packageRoot,request.slice('@mathjax/'.length));
  else return null;
  for (const file of [candidate,candidate+'.js',path.join(candidate,'index.js')])
    if(fs.existsSync(file)&&fs.statSync(file).isFile())return file;
  return null;
}
function collectCjsDependencies(startFiles) {
  const visited=new Set(), queue=[...startFiles];
  while(queue.length){
    const file=path.resolve(queue.pop());
    if(visited.has(file))continue;
    visited.add(file);
    const source=fs.readFileSync(file,'utf8');
    const requireLiteral=/require\((['"])([^'"]+)\1\)/g;
    for(const match of source.matchAll(requireLiteral)){
      const dependency=resolveSourceRequire(file,match[2]);
      if(dependency)queue.push(dependency);
      else if(match[2].startsWith('.')||match[2].startsWith('@mathjax/'))
        throw new Error(`cannot resolve pinned CJS dependency ${match[2]} from ${path.relative(root,file)}`);
    }
  }
  return [...visited].sort();
}
const newcmPackage = path.join(packageRoot, 'mathjax-newcm-font');
const packageJson = JSON.parse(fs.readFileSync(path.join(newcmPackage, 'package.json'), 'utf8'));
if (packageJson.version !== '4.1.3') throw new Error(`expected NewCM 4.1.3, got ${packageJson.version}`);
const expectedSourceSha256 = new Map([
  ['@mathjax/mathjax-newcm-font', '709d6e85113ef44645ad802b8424508d5f836dd61e4d1f2797b5e011a9693734'],
  ['@mathjax/mathjax-bbm-font-extension', '4330e4138451f2efd659d6a2bd4038c163e7fa98977474315b138bd5e6e2bf21'],
  ['@mathjax/mathjax-bboldx-font-extension', '9ac39ff107d6291e15e16764f45cb794cf133333ebf268281e651b54ed6ab0d1'],
  ['@mathjax/mathjax-dsfont-font-extension', '7a84404f759fcce1789a62389836245cb7aaf30c37556d1181c281d00a838478'],
]);
const {MathJaxNewcmFont: Font} = require(path.join(newcmPackage, 'cjs/svg.js'));
const {FontData} = require(path.join(packageRoot, 'src/cjs/output/common/FontData.js'));
const extensionPackages = ['mathjax-bbm-font-extension', 'mathjax-bboldx-font-extension', 'mathjax-dsfont-font-extension'];

function quote(value) {
  return JSON.stringify(String(value)).replace(/\u2028/g, '\\u2028').replace(/\u2029/g, '\\u2029');
}
function number(value) {
  if (!Number.isFinite(value)) throw new Error(`non-finite font number: ${value}`);
  if (Object.is(value, -0)) return '-0.0';
  return Number.isInteger(value) ? `${value}.0` : String(value);
}
function optionalNumber(value) { return value == null ? 'std::nullopt' : `std::optional<double>{${number(value)}}`; }
function vec(values) { return `std::vector<double>{${values.map(number).join(',')}}`; }
function optVec(values) { return values == null ? 'std::nullopt' : `std::optional<std::vector<double>>{${vec(values)}}`; }
function propertyArrayExpression(value) {
  if (!Array.isArray(value)) throw new Error('expected a JavaScript array');
  const slots=[];
  for(let i=0;i<value.length;i++)if(Object.hasOwn(value,i))slots.push(`a.set(${i},${propertyExpression(value[i])});`);
  return `([]{ PropertyArray a; a.resize(${value.length}); ${slots.join(' ')} return a; }())`;
}
function propertyExpression(value) {
  if (value === undefined) return 'Property(Missing{})';
  if (value === null) return 'Property(PropertyNull{})';
  if (typeof value === 'string') return `Property(std::string(${quote(value)}))`;
  if (typeof value === 'number') return `Property(${number(value)})`;
  if (typeof value === 'boolean') return `Property(${value})`;
  if (Array.isArray(value)) return `Property(${propertyArrayExpression(value)})`;
  if (typeof value === 'object') {
    const fields=Object.keys(value).map(key=>`o.setOwn(${quote(key)},${propertyExpression(value[key])});`);
    return `([]{ PropertyList o; ${fields.join(' ')} return Property(o); }())`;
  }
  throw new Error(`unsupported property value: ${typeof value}`);
}
function ranges(values) {
  return `std::vector<DynamicRange>{${values.map(value => Array.isArray(value)
    ? `DynamicRange{${value[0]},${value[1]},true}`
    : `DynamicRange{${value},${value},false}`).join(',')}}`;
}
function variantDefinitions(values) {
  return propertyArrayExpression(values);
}
function mapNumberToString(values) {
  return `{${Object.entries(values || {}).map(([key,value]) => `{${Number(key)},${number(value)}}`).join(',')}}`;
}
function mapNumberToU16(values) {
  return `{${Object.entries(values || {}).map(([key,value]) => `{${Number(key)},std::u16string(u${quote(value)})}`).join(',')}}`;
}
function remapSource(value) {
  if (typeof value === 'string') return `SmpSource{std::string(${quote(value)})}`;
  const slots=[];
  for(let i=0;i<value.length;i++)if(Object.hasOwn(value,i))slots.push(`a.set(${i},${propertyExpression(value[i])});`);
  return `SmpSource{[]{ PropertyArray a; a.resize(${value.length}); ${slots.join(' ')} return a; }()}`;
}
function glyphStatements(mapName, map, target='d.defaultChars') {
  const lines = [];
  for (const [key, tuple] of Object.entries(map)) {
    const cp = Number(key);
    if (!Array.isArray(tuple)) throw new Error(`unexpected non-array glyph ${mapName}:${key}`);
    lines.push(`  { PropertyArray g; g.resize(${tuple.length});`);
    for (let i=0;i<tuple.length;i++) if(Object.hasOwn(tuple,i)) lines.push(`    g.set(${i},${propertyExpression(tuple[i])});`);
    lines.push(`    ${target}[${quote(mapName)}].insert_or_assign(${cp},Property(g)); }`);
  }
  return lines.join('\n');
}
function delimiterStatements(delimiters, target='d.defaultDelimiters') {
  const lines=[];
  for (const [key,value] of Object.entries(delimiters)) {
    const cp=Number(key);
    lines.push(`  { auto x=std::make_shared<DelimiterData>(); x->dir=${quote(value.dir)};`);
    for(const [src,dest] of [['sizes','sizes'],['variants','variants'],['schar','schar'],['stretch','stretch'],['stretchv','stretchv']])
      if(value[src] != null)lines.push(`    x->${dest}=${optVec(value[src])};`);
    for(const [src,dest] of [['HDW','HDW'],['hd','hd'],['fullExt','fullExt']])
      if(value[src] != null)lines.push(`    x->${dest}=std::array<double,${value[src].length}>{${value[src].map(number).join(',')}};`);
    for(const keyName of ['ext','min','c'])if(value[keyName] != null)lines.push(`    x->${keyName}=${number(value[keyName])};`);
    lines.push(`    ${target}.insert_or_assign(${cp},std::move(x)); }`);
  }
  return lines.join('\n');
}
function dynamicStatements(files) {
  const lines=[];
  for(const [key,file] of Object.entries(files)) {
    lines.push(`  { DynamicFileDef x; x.file=${quote(file.file)};`);
    for(const [name,rangesValue] of Object.entries(file.variants || {}))lines.push(`    x.variants.insert_or_assign(${quote(name)},${ranges(rangesValue)});`);
    lines.push(`    x.delimiters=${ranges(file.delimiters || [])}; defs.push_back(std::move(x)); }`);
  }
  return lines.join('\n');
}
function sourceData(font) {
  const lines=[];
  // Keep each installation frame bounded. A single function with thousands of
  // glyph-local shared_ptr and option temporaries exceeds the normal Windows
  // thread stack in debug builds, even though the data itself is heap-owned.
  // Preserve Source insertion order by partitioning one flattened traversal.
  const glyphEntries=[];
  for(const [name,map] of Object.entries(font.defaultChars))
    for(const [key,tuple] of Object.entries(map))glyphEntries.push([name,key,tuple]);
  const glyphBatchSize=48;
  const glyphBatches=[];
  for(let offset=0;offset<glyphEntries.length;offset+=glyphBatchSize){
    const batch=glyphEntries.slice(offset,offset+glyphBatchSize);
    const name=`installGeneratedFontDataGlyphBatch_${glyphBatches.length}`;
    const body=batch.map(([variant,key,tuple])=>glyphStatements(variant,{[key]:tuple})).join('\n');
    glyphBatches.push(name);
    lines.push(`static void ${name}(FontDataClass& d) {\n${body}\n}`);
  }
  lines.push('void installGeneratedFontData(FontDataClass& d, std::map<std::string,std::string,std::less<>>* svgIds) {');
  lines.push(`  d.NAME=${quote(font.NAME)}; d.JAX=${quote(font.JAX)};`);
  lines.push(`  d.defaultVariants=${variantDefinitions(font.defaultVariants)};`);
  lines.push('  d.defaultChars.clear();');
  // `defaultChars` is an ordinary Source object with one own property for
  // every declared variant, including variants whose static glyph map is
  // empty.  Materialize those maps before the glyph batches so both the
  // complete key order and empty-map identity match the pinned class data.
  for(const name of Object.keys(font.defaultChars))
    lines.push(`  d.defaultChars.insert_or_assign(${quote(name)},CharMap{});`);
  for(const name of glyphBatches)lines.push(`  ${name}(d);`);
  lines.push('  d.defaultDelimiters.clear();');
  lines.push(delimiterStatements(font.defaultDelimiters));
  lines.push('  d.defaultSizeVariants=PropertyArray{' + font.defaultSizeVariants.map(value=>`Property(std::string(${quote(value)}))`).join(',') + '};');
  lines.push('  d.defaultStretchVariants=PropertyArray{' + font.defaultStretchVariants.map(value=>`Property(std::string(${quote(value)}))`).join(',') + '};');
  lines.push('  d.VariantSmp.clear();');
  for(const [name,value] of Object.entries(font.VariantSmp || {}))lines.push(`  d.VariantSmp.insert_or_assign(${quote(name)},${remapSource(value)});`);
  lines.push('  d.SmpRanges={' + (font.SmpRanges || []).map(row=>`std::array<int,3>{${row.join(',')}}`).join(',') + '};');
  for(const [source,dest] of [['SmpRemap','SmpRemapMap'],['SmpRemapGreekU','SmpRemapGreekU'],['SmpRemapGreekL','SmpRemapGreekL']])
    lines.push(`  d.${dest}=${mapNumberToString(font[source] || {})};`);
  for(const [source,dest] of [['defaultAccentMap','defaultAccentMap'],['defaultMoMap','defaultMoMap'],['defaultMnMap','defaultMnMap']])
    lines.push(`  d.${dest}=${mapNumberToU16(font[source] || {})};`);
  lines.push('  d.defaultParams=FontParameters{};');
  for(const [key,value] of Object.entries(font.defaultParams || {}))lines.push(`  d.defaultParams.${key}=${number(value)};`);
  lines.push('  d.defaultParams.extra.clear();');
  lines.push('  d.defaultCssFonts.clear();');
  for(const [name,values] of Object.entries(font.defaultCssFonts || {}))lines.push(`  d.defaultCssFonts.insert_or_assign(${quote(name)},${propertyArrayExpression(values)});`);
  lines.push('  std::vector<DynamicFileDef> defs;');
  lines.push(dynamicStatements(font.dynamicFiles || {}));
  lines.push('  d.dynamicFiles=FontDataClass::defineDynamicFiles(defs);');
  lines.push('  if(svgIds){ svgIds->clear();');
  for(const [name,id] of Object.entries(font.variantCacheIds || {}))lines.push(`    svgIds->insert_or_assign(${quote(name)},${quote(id)});`);
  lines.push('  }');
  if(font.OPTIONS && font.OPTIONS.dynamicPrefix)
    lines.push(`  if(d.optionsContext)d.optionsContext->set(d.OPTIONS,options::Key(u"dynamicPrefix"),options::Value(std::u16string(u${quote(font.OPTIONS.dynamicPrefix)})));`);
  lines.push('}');
  return lines.join('\n');
}

function extensionSource(name, value) {
  const lines = [`SvgFontExtensionData ${name}() {`, '  SvgFontExtensionData e;'];
  lines.push(`  e.name=${quote(value.name)};`);
  if (value.variants) {
    lines.push('  e.variants=ListPatch{};');
    if (Array.isArray(value.variants))
      lines.push(`  e.variants->replace=${variantDefinitions(value.variants)};`);
    else {
      if (Object.hasOwn(value.variants,'[+]')) lines.push(`  e.variants->append=${variantDefinitions(value.variants['[+]'])};`);
      if (Object.hasOwn(value.variants,'[-]')) lines.push(`  e.variants->remove=${variantDefinitions(value.variants['[-]'])};`);
    }
  }
  if (value.variantSmp) {
    lines.push('  e.variantSmp=OrderedMap<std::string,SmpSource>{};');
    const arrays=new Map();
    let arrayIndex=0;
    for (const [variant, remap] of Object.entries(value.variantSmp)) {
      let expression;
      if(Array.isArray(remap)){
        let shared=arrays.get(remap);
        if(!shared){
          shared=`smpRemap${arrayIndex++}`;
          arrays.set(remap,shared);
          lines.push(`  static PropertyArray ${shared}=${propertyArrayExpression(remap)};`);
        }
        expression=`SmpSource{${shared}}`;
      }else expression=remapSource(remap);
      lines.push(`  e.variantSmp->insert_or_assign(${quote(variant)},${expression});`);
    }
  }
  if (value.cacheIds) {
    lines.push('  e.cacheIds=std::map<std::string,std::string,std::less<>>{};');
    for (const [variant, id] of Object.entries(value.cacheIds))
      lines.push(`  e.cacheIds->insert_or_assign(${quote(variant)},${quote(id)});`);
  }
  if (value.chars) {
    lines.push('  e.chars=CharMapMap{};');
    for (const [variant, glyphs] of Object.entries(value.chars)) {
      lines.push(`  { auto& map=(*e.chars)[${quote(variant)}];`);
      for (const [key, tuple] of Object.entries(glyphs)) {
        const cp = Number(key);
        lines.push(`    { PropertyArray g; g.resize(${tuple.length});`);
        for(let i=0;i<tuple.length;i++)if(Object.hasOwn(tuple,i))lines.push(`      g.set(${i},${propertyExpression(tuple[i])});`);
        lines.push(`      map.insert_or_assign(${cp},Property(g)); }`);
      }
      lines.push('  }');
    }
  }
  for (const field of ['sizeVariants','stretchVariants']) {
    const values = value[field];
    if (!values) continue;
    lines.push(`  e.${field}=ListPatch{};`);
    if (Array.isArray(values))
      lines.push(`  e.${field}->replace=${propertyArrayExpression(values)};`);
    else {
      if (Object.hasOwn(values,'[+]')) lines.push(`  e.${field}->append=${propertyArrayExpression(values['[+]'])};`);
      if (Object.hasOwn(values,'[-]')) lines.push(`  e.${field}->remove=${propertyArrayExpression(values['[-]'])};`);
    }
  }
  lines.push('  return e;', '}');
  return lines.join('\n');
}

const output = `// Generated by tools/math-port/font-data-generate.cjs from MathJax 4.1.3 CJS.\n`+
  `// Source package: ${packageJson.name}@${packageJson.version}\n`+
  `// NewCM SVG source SHA-256: ${crypto.createHash('sha256').update(fs.readFileSync(path.join(newcmPackage,'cjs/svg.js'))).digest('hex')}\n`+
  sourceData(Font);
fs.mkdirSync(generatedRoot, {recursive:true});
const destination = path.join(generatedRoot, 'newcm_font_data.inc');

const extensionInputs = [];
for (const packageName of extensionPackages) {
  const packageDir = path.join(packageRoot, packageName);
  const manifest = JSON.parse(fs.readFileSync(path.join(packageDir, 'package.json'), 'utf8'));
  if (manifest.version !== '4.1.3') throw new Error(`expected ${packageName}@4.1.3, got ${manifest.version}`);
  const sourcePath = path.join(packageDir, 'cjs/svg.js');
  const sourceText = fs.readFileSync(sourcePath);
  const sourceHash = crypto.createHash('sha256').update(sourceText).digest('hex');
  if (sourceHash !== expectedSourceSha256.get(`@mathjax/${packageName}`))
    throw new Error(`pinned source hash mismatch for @mathjax/${packageName}: ${sourceHash}`);
  const data = Object.values(require(sourcePath))[0];
  extensionInputs.push({packageName, sha256:sourceHash, data});
}
const newcmHash=crypto.createHash('sha256').update(fs.readFileSync(path.join(newcmPackage,'cjs/svg.js'))).digest('hex');
if(newcmHash!==expectedSourceSha256.get('@mathjax/mathjax-newcm-font'))throw new Error(`pinned source hash mismatch for @mathjax/mathjax-newcm-font: ${newcmHash}`);
const extensionNames = ['Bbm','Bboldx','Dsfont'];
const extensionOutput = `// Generated by tools/math-port/font-data-generate.cjs from MathJax 4.1.3 CJS.\n`+
  extensionInputs.map((item, i) => `// ${item.packageName}/cjs/svg.js SHA-256: ${item.sha256}\n${extensionSource(`make${extensionNames[i]}Extension`, item.data)}`).join('\n\n') + '\n';
const extensionDestination = path.join(generatedRoot, 'font_extensions.inc');

// Load every pinned dynamic shard in this maintainer process and intercept the
// source's registration call. The generated native callback owns the same
// source tables; runtime loading never executes JavaScript.
const dynamicRegistration = new Map();
const originalDynamicSetup = Font.dynamicSetup;
Font.dynamicSetup = function(extension,file,variants,delimiters={},fonts=null) {
  dynamicRegistration.set(file,{extension,variants,delimiters,fonts});
  return originalDynamicSetup.call(this,extension,file,variants,delimiters,fonts);
};
const dynamicInputs=[];
for (const descriptor of Object.values(Font.dynamicFiles)) {
  const dynamicPath=path.join(newcmPackage,'cjs/svg/dynamic',`${descriptor.file}.js`);
  if(!fs.existsSync(dynamicPath))throw new Error(`missing pinned NewCM dynamic shard ${descriptor.file}`);
  const bytes=fs.readFileSync(dynamicPath);
  const digest=crypto.createHash('sha256').update(bytes).digest('hex');
  dynamicInputs.push({path:`cjs/svg/dynamic/${descriptor.file}.js`,sha256:digest});
  require(dynamicPath);
  if(!dynamicRegistration.has(descriptor.file))throw new Error(`dynamic shard did not register setup: ${descriptor.file}`);
}
Font.dynamicSetup=originalDynamicSetup;
function dynamicSetupCode(file,registration,installerName) {
  const lines=[];
  const glyphEntries=[];
  for(const [variant,map] of Object.entries(registration.variants||{}))
    for(const [key,tuple] of Object.entries(map))glyphEntries.push([variant,key,tuple]);
  const glyphBatchSize=48;
  const batches=[];
  for(let offset=0;offset<glyphEntries.length;offset+=glyphBatchSize){
    const batch=glyphEntries.slice(offset,offset+glyphBatchSize);
    const helper=`${installerName}_glyphs_${batches.length}`;batches.push(helper);
    const body=batch.map(([variant,key,tuple])=>glyphStatements(variant,{[key]:tuple},'chars')).join('\n');
    lines.push(`static void ${helper}(CharMapMap& chars) {\n${body}\n}`);
  }
  lines.push(`void ${installerName}(FontDataClass& d) {`);
  lines.push('  CharMapMap chars;');
  for(const helper of batches)lines.push(`  ${helper}(chars);`);
  lines.push('  DelimiterMap delimiters;');
  if(Object.keys(registration.delimiters||{}).length){
    const statements=delimiterStatements(registration.delimiters,'delimiters');
    if(statements)lines.push(statements.replace(/^/gm,'  '));
  }
  lines.push(`  std::vector<std::string> fonts{${(registration.fonts||[]).map(quote).join(',')}};`);
  lines.push(`  d.registerBundledDynamicSetup(${quote(registration.extension||'')},${quote(file)},`);
  lines.push('    [chars=std::move(chars),delimiters=std::move(delimiters),fonts=std::move(fonts)](FontDataClass& loadedClass) {');
  lines.push(`      loadedClass.dynamicSetup(${quote(registration.extension||'')},${quote(file)},chars,delimiters,fonts);`);
  lines.push('    });');
  lines.push('}');
  return lines.join('\n');
}
const dynamicShardNames=[...dynamicRegistration.keys()];
const dynamicShardId=(file)=>file.replace(/[^A-Za-z0-9_]/g,'_');
const dynamicShardOutputs=[];
const dynamicSetupLines=[];
for(const [file,data] of dynamicRegistration.entries()){
  const id=dynamicShardId(file),functionName=`installGeneratedNewcmDynamicShard_${id}`;
  const shardSource=`// Generated by tools/math-port/font-data-generate.cjs from pinned MathJax 4.1.3 dynamic shard ${file}.\n`+
    `// Input SHA-256: ${dynamicInputs.find(item=>item.path===`cjs/svg/dynamic/${file}.js`).sha256}\n`+
    `module folia.math.fonts.newcm_font_data;\nimport std;\nnamespace folia::math::fonts {\nusing namespace output::common;\n${dynamicSetupCode(file,data,functionName)}\n}\n`;
  const outputPath=path.join(generatedRoot,`newcm_dynamic_setup_${id}.cpp`);
  dynamicShardOutputs.push({file,outputPath,source:shardSource,functionName});
  dynamicSetupLines.push(`${functionName}(d);`);
}
const dynamicOutput=`// Generated by tools/math-port/font-data-generate.cjs from pinned MathJax 4.1.3 dynamic shards.\n`+
  dynamicInputs.map(x=>`// ${x.path} SHA-256: ${x.sha256}`).join('\n')+'\n'+
  dynamicShardOutputs.map(x=>`void ${x.functionName}(FontDataClass&);`).join('\n')+'\n'+
  'void installGeneratedNewcmDynamicSetup(FontDataClass& d) {\n'+dynamicSetupLines.join('\n')+'\n}\n';
const dynamicDestination=path.join(generatedRoot,'newcm_dynamic_setup.inc');

// Lock every CJS module actually reached while loading the four font entry
// points and the dynamic shards. This includes the MathJax common/SVG font
// implementations and every static font data table, rather than trusting only
// the package entry hashes.
const mainEntries=[
  ['mathjax-newcm-font','cjs/svg.js'],
  ...extensionPackages.map(name=>[name,'cjs/svg.js'])
];
const entryKeys=new Set(mainEntries.map(([name,file])=>`${name}/${file}`));
const dynamicKeys=new Set(dynamicInputs.map(item=>`mathjax-newcm-font/${item.path}`));
const transitiveCjs=[];
for(const file of Object.keys(require.cache)){
  const absolute=path.resolve(file), relative=path.relative(packageRoot,absolute);
  if(relative.startsWith('..')||path.isAbsolute(relative)||!relative.endsWith('.js'))continue;
  const [packageDir,...rest]=relative.split(path.sep), packageName=packageDir==='src'?'src':packageDir;
  const packagePath=rest.join('/');
  const key=`${packageName}/${packagePath}`;
  if(entryKeys.has(key)||dynamicKeys.has(key))continue;
  const bytes=fs.readFileSync(absolute);
  const manifestPath=path.join(packageRoot,packageDir,'package.json');
  const version=JSON.parse(fs.readFileSync(manifestPath,'utf8')).version;
  transitiveCjs.push({package:`@mathjax/${packageName}`,version,path:packagePath,
    sha256:crypto.createHash('sha256').update(bytes).digest('hex')});
}
transitiveCjs.sort((a,b)=>`${a.package}/${a.path}`.localeCompare(`${b.package}/${b.path}`));
const packageManifests=['mathjax-newcm-font',...extensionPackages].map(packageName=>{
  const manifestPath=path.join(packageRoot,packageName,'package.json');
  const bytes=fs.readFileSync(manifestPath), manifest=JSON.parse(bytes);
  return {package:`@mathjax/${packageName}`,version:manifest.version,path:'package.json',
    sha256:crypto.createHash('sha256').update(bytes).digest('hex')};
});

const sourceLock = {
  source: 'MathJax 4.1.3, commit fb987178c2d279a99b0db5ea02e751691703955e',
  inputs: [
    {package:'@mathjax/mathjax-newcm-font', version:'4.1.3', path:'cjs/svg.js', sha256:crypto.createHash('sha256').update(fs.readFileSync(path.join(newcmPackage,'cjs/svg.js'))).digest('hex'), variants:Object.keys(Font.defaultChars).length, glyphs:Object.values(Font.defaultChars).reduce((n,m)=>n+Object.keys(m).length,0), delimiters:Object.keys(Font.defaultDelimiters).length, dynamicFiles:Object.keys(Font.dynamicFiles).length},
    ...extensionInputs.map(({packageName,sha256,data})=>({package:`@mathjax/${packageName}`,version:'4.1.3',path:'cjs/svg.js',sha256,variants:Object.keys(data.chars).length,glyphs:Object.values(data.chars).reduce((n,m)=>n+Object.keys(m).length,0),delimiters:Object.keys(data.delimiters||{}).length})),
    ...transitiveCjs,
    ...packageManifests,
    ...dynamicInputs.map(({path,sha256})=>({package:'@mathjax/mathjax-newcm-font',version:'4.1.3',path,sha256}))
  ],
  outputs:[
    {path:'build/math-port/font-generated/newcm_font_data.inc',sha256:crypto.createHash('sha256').update(output).digest('hex'),bytes:Buffer.byteLength(output)},
    {path:'build/math-port/font-generated/font_extensions.inc',sha256:crypto.createHash('sha256').update(extensionOutput).digest('hex'),bytes:Buffer.byteLength(extensionOutput)},
    {path:'build/math-port/font-generated/newcm_dynamic_setup.inc',sha256:crypto.createHash('sha256').update(dynamicOutput).digest('hex'),bytes:Buffer.byteLength(dynamicOutput)},
    ...dynamicShardOutputs.map(x=>({path:`build/math-port/font-generated/newcm_dynamic_setup_${dynamicShardId(x.file)}.cpp`,sha256:crypto.createHash('sha256').update(x.source).digest('hex'),bytes:Buffer.byteLength(x.source)}))
  ]
};
const lockPath=path.join(root,'tools/math-port/locks/font-data-source-lock.json');
if(checkLock){
  if(!fs.existsSync(lockPath))throw new Error(`missing frozen font source lock: ${path.relative(root,lockPath)}`);
  const frozen=JSON.parse(fs.readFileSync(lockPath,'utf8'));
  if(JSON.stringify(frozen)!==JSON.stringify(sourceLock))throw new Error('generated font source/count lock differs from the frozen evidence file');
} else fs.writeFileSync(lockPath,JSON.stringify(sourceLock,null,2)+'\n','utf8');
console.log(`${checkLock?'verified':'wrote'} ${path.relative(root,lockPath)}: ${sourceLock.inputs.length} pinned source files`);
for(const [file,contents] of [[destination,output],[extensionDestination,extensionOutput],[dynamicDestination,dynamicOutput],...dynamicShardOutputs.map(x=>[x.outputPath,x.source])])
  fs.writeFileSync(file,contents,'utf8');
console.log(`${path.relative(root,destination)}: ${output.length} bytes, variants=${Object.keys(Font.defaultChars).length}, glyphs=${Object.values(Font.defaultChars).reduce((n,m)=>n+Object.keys(m).length,0)}, delimiters=${Object.keys(Font.defaultDelimiters).length}`);
console.log(`${path.relative(root,extensionDestination)}: ${extensionOutput.length} bytes; ` + extensionInputs.map(x => `${x.packageName}=${Object.values(x.data.chars).reduce((n,m)=>n+Object.keys(m).length,0)} glyphs`).join(', '));
console.log(`${path.relative(root,dynamicDestination)}: ${dynamicOutput.length} bytes, shards=${dynamicRegistration.size}`);
