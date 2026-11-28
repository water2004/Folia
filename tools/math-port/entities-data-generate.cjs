#!/usr/bin/env node
'use strict';

// Generate the pinned MathJax entity tables for the native Entities module.
// Generation verifies both TypeScript and installed CJS inputs and writes
// only an ignored build artifact; it never rewrites the evidence lock.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');

const repo = path.resolve(__dirname, '../..');
const source = path.join(repo, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const packageJson = require(path.join(source, 'package.json'));
assert.equal(packageJson.version, '4.1.3');
const entitiesFile = path.join(source, 'cjs/util/Entities.js');
const shardDir = path.join(source, 'cjs/util/entities');
const shardNames = [...'abcdefghijklmnopqrstuvwxyz', 'fr', 'opf', 'scr' ];
const tsShardDir = path.join(repo, '.reference/MathJax-src/ts/util/entities');
const lock = JSON.parse(
    fs.readFileSync(path.join(repo, 'tools/math-port/locks/entities-source-lock.json'), 'utf8'));
const files = [ 'Entities.js', ...shardNames.map((name) => `entities/${name}.js`) ];
const tsFiles = [ 'Entities.ts', ...shardNames.map((name) => `entities/${name}.ts`) ];
const sha = (file) => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const actualSource = {
    entitiesTs : sha(path.join(repo, '.reference/MathJax-src/ts/util/Entities.ts')),
    cjs : Object.fromEntries(files.map((file) => [file, sha(path.join(source, 'cjs/util', file))])),
    ts : Object.fromEntries(tsFiles.map(
        (file) => [file, sha(file === 'Entities.ts'
                                 ? path.join(repo, '.reference/MathJax-src/ts/util/Entities.ts')
                                 : path.join(tsShardDir, file.replace('entities/', '')))])),
};
assert.deepEqual({package : packageJson.version, source : actualSource}, lock,
                 'pinned MathJax Entities TypeScript/CJS source lock changed');

for (const key of Object.keys(require.cache)) {
    if (key === entitiesFile || (key.startsWith(shardDir + path.sep) && key.endsWith('.js')))
        delete require.cache[key];
}
const api = require(entitiesFile);
const base = Object.assign(Object.create(null), api.entities);
const shards = {};
for (const name of shardNames) {
    for (const key of Object.keys(require.cache))
        if (key === entitiesFile || key.startsWith(shardDir + path.sep))
            delete require.cache[key];
    const fresh = require(entitiesFile);
    const originalAdd = fresh.add;
    let entries;
    fresh.add = (addition, file) => {
        assert.equal(file, name, `unexpected EntityList source for ${name}`);
        assert.equal(entries, undefined, `multiple add calls for ${name}`);
        entries = Object.assign(Object.create(null), addition);
        originalAdd(addition, file);
    };
    require(path.join(shardDir, name + '.js'));
    assert.ok(entries, `no add call captured for ${name}`);
    shards[name] = entries;
}

function literal(value) {
    let out = 'u"';
    for (const char of value) {
        const cp = char.codePointAt(0);
        if (cp === 0x22)
            out += '\\"';
        else if (cp === 0x5c)
            out += '\\\\';
        else if (cp === 0x0a)
            out += '\\n';
        else if (cp === 0x0d)
            out += '\\r';
        else if (cp === 0x09)
            out += '\\t';
        else if (cp >= 0x20 && cp <= 0x7e)
            out += char;
        else if (cp <= 0xffff)
            out += `\\u${cp.toString(16).padStart(4, '0')}`;
        else
            out += `\\U${cp.toString(16).padStart(8, '0')}`;
    }
    return out + '"';
}
function emitEntries(name, table) {
    const entries = Object.keys(table)
                        .map((key) => `    {${literal(key)}, ${literal(table[key])}},`)
                        .join('\n');
    return `inline constexpr Entry ${name}[] = {\n${entries}\n};\n`;
}

let output = `// Generated from pinned MathJax 4.1.3 EntityList sources. Do not edit.\n` +
             `namespace folia::math::entities::generated {\n` +
             `struct Entry { std::u16string_view name; std::u16string_view value; };\n` +
             emitEntries('baseEntries', base);
for (const name of shardNames)
    output += emitEntries(`shard_${name}`, shards[name]);
output += `struct Shard { std::string_view name; std::span<const Entry> entries; };\n`;
output += `inline constexpr Shard shardEntries[] = {\n`;
for (const name of shardNames)
    output += `    {"${name}", shard_${name}},\n`;
output += `};\n}\n`;

const arg = process.argv.indexOf('--output');
const outputPath = arg >= 0 ? path.resolve(process.argv[arg + 1])
                            : path.join(repo, 'build/math-port/entities/entities_data.inc');
fs.mkdirSync(path.dirname(outputPath), {recursive : true});
if (!fs.existsSync(outputPath) || fs.readFileSync(outputPath, 'utf8') !== output)
    fs.writeFileSync(outputPath, output, 'utf8');
const shardRows =
    Object.values(shards).reduce((count, values) => count + Object.keys(values).length, 0);
process.stdout.write(JSON.stringify({
    output : path.relative(repo, outputPath),
    base : Object.keys(base).length,
    rows : Object.keys(base).length + shardRows,
    shardRows,
    shards : Object.fromEntries(
        Object.entries(shards).map(([ name, values ]) => [name, Object.keys(values).length]))
}) + '\n');
