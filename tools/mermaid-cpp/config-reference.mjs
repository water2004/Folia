// Load original, hash-locked Mermaid sources. Dependencies are test-only.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFileSync, readdirSync } from 'node:fs';
import { stripTypeScriptTypes } from 'node:module';
import { pathToFileURL, fileURLToPath } from 'node:url';
import path from 'node:path';
import vm from 'node:vm';
import Ajv2019 from './oracle/node_modules/ajv/dist/2019.js';
import { load, JSON_SCHEMA } from './oracle/node_modules/js-yaml/dist/js-yaml.mjs';

export const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
export const lock = JSON.parse(readFileSync(path.join(root, 'docs/mermaid-cpp/source-lock.json')));
export const read = name => readFileSync(path.join(root, '.reference/mermaid-js', name), 'utf8');
for (const file of lock.files) assert.equal(createHash('sha256').update(readFileSync(
  path.join(root, '.reference/mermaid-js', file.path))).digest('hex'), file.sha256, file.path);
export const src = name => read(`packages/mermaid/src/${name}`);
export const strip = text => stripTypeScriptTypes(text).replace(/^export /gm, '');
export const withoutImports = text => text.replace(/^import[\s\S]*?;\r?\n/gm, '');
export const log = { debug() {}, warn() {}, error() {} };
export function extract(text, start, end) {
  const a = text.indexOf(start), b = text.indexOf(end, a + start.length);
  assert(a >= 0 && b > a, start);
  return text.slice(a, b);
}
const chunks = path.join(root, 'tools/mermaid-cpp/oracle/node_modules/mermaid/dist/chunks/mermaid.core');
const chunk = readdirSync(chunks).find(name => name.endsWith('.mjs') &&
  readFileSync(path.join(chunks, name), 'utf8').includes('var configKeys ='));
assert(chunk);
const map = JSON.parse(readFileSync(path.join(chunks, chunk + '.map')));
// Theme code runs from the locked official package, checked against original sources.
for (const file of lock.files.filter(f => f.path.startsWith('packages/mermaid/src/themes/'))) {
  const suffix = file.path.replace('packages/mermaid/', '/');
  const index = map.sources.findIndex(name => name.endsWith(suffix));
  assert(index >= 0, file.path);
  assert.equal(map.sourcesContent[index].replaceAll('\r\n', '\n'), read(file.path).replaceAll('\r\n', '\n'), file.path);
}
export const { themes_default: themes } = await import(pathToFileURL(path.join(chunks, chunk)));
const schemaProgram = strip(withoutImports(read('.build/jsonSchema.ts'))) + '\n({loadSchema,generateDefaults})';
const generator = vm.runInNewContext(schemaProgram, { Ajv2019, load, JSON_SCHEMA, assert });
const schema = generator.loadSchema(src('schemas/config.schema.yaml'), 'config.schema.yaml');
const defaultConfigJson = generator.generateDefaults(schema);
export const { config: defaults, configKeys } = vm.runInNewContext(
  strip(withoutImports(src('defaultConfig.ts')).replace('export default config;', '')) + '\n({config,configKeys})',
  { defaultConfigJson, theme: themes });
export const { sanitizeDirective, sanitizeCss } = vm.runInNewContext(
  strip(withoutImports(src('utils/sanitizeDirective.ts'))) + '\n({sanitizeDirective,sanitizeCss})', { configKeys, log });
export const assign = vm.runInNewContext(strip(src('assignWithDepth.ts').replace('export default assignWithDepth;', '')) + '\nassignWithDepth');
export const diagramKey = vm.runInNewContext(strip(src('diagram-api/diagramConfigKeys.ts')) + '\ngetDiagramConfigKey');
const configSource = src('config.ts');
export function secureSanitize(value, secure) {
  const fn = vm.runInNewContext(strip(extract(configSource, 'export const sanitize =', '\n/**')) + '\nsanitize',
    { siteConfig: { secure }, log });
  fn(value);
  return value;
}
export function appearance(cfg, sum, directives, site, diagram) {
  const program = strip(extract(configSource, 'const APPEARANCE_KEYS', '\n/**\n * Converts') + '\n' +
    extract(configSource, 'const resolveAppearance =', '\nconst updateCurrentConfig')) + '\nresolveAppearance';
  const resolve = vm.runInNewContext(program, { theme: themes, diagramConfigKey: diagram === null ? undefined : diagramKey(diagram),
    directives, siteConfigDelta: site, defaultConfig: defaults });
  resolve(cfg, sum);
  return cfg;
}
export const hex = text => Array.from({length:text.length}, (_,i) => text.charCodeAt(i).toString(16).padStart(4,'0')).join('') || '-';
export function snapshot(value) {
  if (value === undefined) return 'U';
  if (value === null) return 'N';
  if (typeof value === 'boolean') return value ? 'T' : 'F';
  if (typeof value === 'number') return `D${Object.is(value,-0)?'-0':String(value)};`;
  if (typeof value === 'string') return `S${hex(value)};`;
  if (Array.isArray(value)) return '[' + Array.from({length:value.length},(_,i)=>snapshot(value[i])).join('') + ']';
  return '{' + Object.keys(value).sort().map(k => `S${hex(k)};${snapshot(value[k])}`).join('') + '}';
}
