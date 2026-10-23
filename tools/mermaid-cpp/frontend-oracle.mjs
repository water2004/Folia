// Compose the original preprocessing/configuration/detection functions.
// Only detector registration is observed; no browser or renderer is simulated.
import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { pathToFileURL } from 'node:url';
import path from 'node:path';
import vm from 'node:vm';
import yaml from './oracle/node_modules/js-yaml/index.js';
import { merge } from './oracle/node_modules/es-toolkit/dist/compat/object/merge.js';
import { root, lock, src, extract, strip, withoutImports, defaults, themes,
  assign, diagramKey, sanitizeDirective, log, hex, snapshot } from './config-reference.mjs';

for (const name of ['mermaidAPI.ts', 'Diagram.ts']) {
  const file = 'packages/mermaid/src/' + name;
  if (!lock.files.some(entry => entry.path === file)) {
    assert(process.argv.includes('--write'), `Unpinned source ${file}`);
    lock.files.push({path:file, sha256:createHash('sha256').update(readFileSync(
      path.join(root,'.reference/mermaid-js',file))).digest('hex')});
  }
}
const packageRoot = path.join(root,'tools/mermaid-cpp/oracle/node_modules/es-toolkit');
for (const file of lock.packages.find(p => p.name === 'es-toolkit').files)
  assert.equal(createHash('sha256').update(readFileSync(path.join(packageRoot,file.path))).digest('hex'), file.sha256);
const sourceRoot = path.join(root,'.reference/mermaid-js/packages/mermaid/src');
const regexes = await import(pathToFileURL(path.join(sourceRoot,'diagram-api/regexes.ts')));
const { UnknownDiagramError } = await import(pathToFileURL(path.join(sourceRoot,'errors.ts')));
const orchestration = src('diagram-api/diagram-orchestration.ts');
const plugins = Object.fromEntries([...orchestration.matchAll(/^import (.+) from '([^']+)';$/gm)]
  .filter(([, , p]) => p.startsWith('../diagrams/') && !p.endsWith('/errorDiagram.js'))
  .map(([, binding, p]) => [binding.replace(/[{} ]/g,''), vm.runInNewContext(strip(withoutImports(
    src(path.posix.normalize('diagram-api/'+p).replace(/\.js$/,'.ts')))
    .replace(/^export default \w+;\n/gm,''))+'\n({id,detector,loader})')]));
const clone = value => Array.isArray(value) ? value.map(clone) : value && typeof value === 'object'
  ? Object.fromEntries(Object.entries(value).map(([k,v]) => [k,clone(v)])) : value;
const clean = value => JSON.parse(JSON.stringify(value));
const utilsSource = src('utils.ts');
function frontend(site) {
  // Each request is isolated, including the upstream global directive regexp.
  const directiveRegex = new RegExp(regexes.directiveRegex.source, regexes.directiveRegex.flags);
  const configApi = vm.runInNewContext(strip(withoutImports(src('config.ts'))) +
    '\n({reset,setSiteConfig,saveConfigFromInitialize,setDiagramConfigScope,addDirective,getConfig})',
    {config:clone(defaults),theme:themes,assignWithDepth:assign,getDiagramConfigKey:diagramKey,sanitizeDirective,log});
  configApi.saveConfigFromInitialize(clone(site));
  configApi.setSiteConfig(clone(site));
  const registry = vm.runInNewContext(strip(withoutImports(src('diagram-api/detectType.ts'))) +
    '\n({detectType,addDetector,registerLazyLoadedDiagrams})', {...regexes,directiveRegex,UnknownDiagramError,log});
  vm.runInNewContext(strip(withoutImports(orchestration))+'\naddDiagrams()', {
    ...plugins,injected:{includeLargeFeatures:true},errorDiagram:{},
    registerLazyLoadedDiagrams:registry.registerLazyLoadedDiagrams,
    registerDiagram:(id,diagram,detector)=>registry.addDetector(id,detector)});
  const utils = vm.runInNewContext(strip(
    extract(utilsSource,'const directiveWithoutOpen =','\n/**')+'\n'+
    extract(utilsSource,'export const detectDirective =','\n/**\n * Detects whether')+'\n'+
    extract(utilsSource,'export const detectInit =','\ninterface Directive')+'\n'+
    extract(utilsSource,'export function cleanAndMerge','\nexport default')) +
    '\n({detectInit,detectDirective,removeDirectives,cleanAndMerge})',
    {directiveRegex,detectType:registry.detectType,assignWithDepth:assign,sanitizeDirective,log,merge});
  const extractFrontMatter = vm.runInNewContext(strip(withoutImports(src('diagram-api/frontmatter.ts'))) +
    '\nextractFrontMatter', {yaml,matchFrontMatter:regexes.matchFrontMatter});
  const cleanupComments = vm.runInNewContext(strip(src('diagram-api/comments.ts'))+'\ncleanupComments');
  const preprocessDiagram = vm.runInNewContext(strip(withoutImports(src('preprocess.ts'))) +
    '\npreprocessDiagram', {extractFrontMatter,cleanupComments,utils,...utils});
  let extractedConfig;
  const processAndSetConfigs = vm.runInNewContext(strip(extract(src('mermaidAPI.ts'),
    'function processAndSetConfigs(', '\n/**'))+'\nprocessAndSetConfigs',
    {preprocessDiagram:text=>{
      const processed=preprocessDiagram(text);
      // Observe extraction before addDirective mutates this same JS object.
      // The owned native API intentionally retains this pre-sanitization data
      // separately from the effective configuration consumed by the renderer.
      extractedConfig=clone(processed.config);
      return processed;
    },configApi,detectType:registry.detectType});
  return text => {
    const processed = processAndSetConfigs(text);
    // Diagram.fromText reads an owned getConfig copy and detects again before
    // loading a parser. Record that actual configuration, including ELK writes.
    const effective = configApi.getConfig();
    const type = registry.detectType(processed.code.cleaned,effective);
    return {...processed,config:extractedConfig,type,effective:clean(effective)};
  };
}
const cases = [];
const add = (name,text,site={theme:'default'}) => cases.push({name,text,site});
add('pie','pie\n"Tea": 3\n"Coffee": 2');
add('packet-dark','packet\n0-7: "Header"',{theme:'dark'});
add('frontmatter','---\r\ntitle: Drinks\r\nconfig: {pie: {showData: true}}\r\n---\r\n%% note\r\npie\r\n"Tea": 3');
add('precedence','---\nconfig: {theme: dark, pie: {textPosition: 0.6}}\n---\n%%{init: {"theme":"default","pie":{"textPosition":0.8}}}%%\npie');
add('wrap','%%{wrap}%%\npie');
add('nowrap','%%{nowrap}%%\npie');
add('wrap-sequence','%%{nowrap}%%\n%%{wrap}%%\n%%{nowrap}%%\npie');
add('nowrap-sequence','%%{nowrap}%%\n%%{nowrap}%%\npie');
add('legacy-init','%%{init: {"config":{"showData":true}}}%%\npie');
add('multiple-init','%%{init: {"theme":"dark"}}%%\n%%{initialize: {"pie":{"showData":true}}}%%\npie');
add('secure-site','%%{init: {"securityLevel":"loose","maxTextSize":1,"theme":"dark"}}%%\npie');
add('scoped-appearance','pie',{theme:'default',pie:{theme:'dark',look:'classic'}});
add('scoped-override','%%{init: {"pie":{"theme":"default"}}}%%\npie',{theme:'dark',pie:{theme:'dark'}});
add('display-mode','---\ndisplayMode: compact\nconfig: {gantt: {axisFormat: "%Y"}}\n---\ngantt');
add('title-body','---\ntitle: Heading\n---\npie\ntitle Body');
add('attributes','pie\n"<b title="hello">Tea</b>": 1');
add('unicode','---\ntitle: 中文 😀\n---\npie\n"咖啡": 2');
add('indented-frontmatter','  ---\n  title: Indented\n  ---\n\n%% note\npie');
add('elk','flowchart-elk TD\nA-->B');
add('elk-init','%%{init:{"config":{"curve":"linear"}}}%%\nflowchart-elk TD\nA-->B');
for (const family of ['graph TD\nA-->B','classDiagram','stateDiagram-v2','sequenceDiagram','erDiagram',
  'mindmap','journey','timeline','gantt','requirementDiagram','gitGraph','C4Context','sankey-beta',
  'quadrantChart','block-beta','kanban','architecture-beta','radar-beta','treemap-beta','xychart-beta'])
  add('family-'+cases.length,family);
for (const [name,text] of Object.entries({empty:'',unknown:'not a diagram',
  'incomplete-fence':'---\ntitle: incomplete', 'bad-yaml':'---\nconfig: [\n---\npie',
  'incomplete-directive':'%%{init: {"theme":\npie',
  'bad-directive':'%%{init: {bad}}%%\npie',
  'bad-init-before-wrap':'%%{wrap}%%\n%%{init: {bad}}%%\npie',
  'bad-init-after-wrap':'%%{init: {bad}}%%\n%%{wrap}%%\npie',
  'unknown-with-init':'%%{init:{"theme":"dark"}}%%\nnot a diagram'})) add(name,text);

const values=[], ids=new Map();
const intern=value=>{ const s=snapshot(value); if(!ids.has(s)){ids.set(s,values.length);values.push(s);} return ids.get(s); };
const json=value=>hex(JSON.stringify(value).replace(/[\u2028\u2029'%]/g,c=>'\\u'+c.charCodeAt(0).toString(16).padStart(4,'0')));
const rows=cases.map(({name,text,site})=>{
  let output;
  try {
    const result=frontend(site)(text);
    output=[hex(result.code.raw),hex(result.code.cleaned),hex(result.code.withComments),
      result.code.frontmatterLineOffset,result.title===undefined?'U':hex(result.title),
      intern(result.config),hex(result.type),intern(result.effective)].join('\t');
  } catch(error) {
    assert(['Error','TypeError','YAMLException','UnknownDiagramError'].includes(error.name), error.stack);
    output='E';
  }
  return [name,hex(text),json(site),output].join('\t');
});
const fixture='# Original composed frontend: name/source/site/raw/cleaned/withComments/lineOffset/title/config/type/effective\n'+
  values.map((v,i)=>'# value\t'+i+'\t'+v).join('\n')+'\n'+rows.join('\n')+'\n';
const output=path.join(root,'tests/mermaid_cpp/fixtures/frontend.tsv');
if(process.argv.includes('--write')) {
  writeFileSync(output,fixture);
  writeFileSync(path.join(root,'docs/mermaid-cpp/source-lock.json'),JSON.stringify(lock,null,2)+'\n');
} else assert.equal(readFileSync(output,'utf8').replaceAll('\r\n','\n'),fixture);
console.log(`Verified ${cases.length} original composed frontend cases`);
