'use strict';
// Actual pinned Entities, completed Source retries and all native resource inputs.
const fs = require('node:fs'), path = require('node:path'), crypto = require('node:crypto'),
      assert = require('node:assert/strict');
const root = path.resolve(__dirname, '../..'),
      cjs = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
const lock = JSON.parse(
    fs.readFileSync(path.join(root, 'docs/math-port/evidence/entities-source-lock.json'), 'utf8'));
const hash = f => crypto.createHash('sha256').update(fs.readFileSync(f)).digest('hex');
for (const [name, sha] of Object.entries(lock.source.ts))
    assert.equal(hash(path.join(root, '.reference/MathJax-src/ts/util', name)), sha);
for (const [name, sha] of Object.entries(lock.source.cjs))
    assert.equal(hash(path.join(cjs, 'util', name)), sha);
const file = path.join(cjs, 'util/Entities.js'), dir = path.join(cjs, 'util/entities') + path.sep;
function fresh() {
    for (const key of Object.keys(require.cache))
        if (key === file || key.startsWith(dir))
            delete require.cache[key];
    return require(file)
}
const names = [...'abcdefghijklmnopqrstuvwxyz', 'fr', 'opf', 'scr' ];
const base = Object.entries(fresh().entities),
      rows = base.map(([ name, value ]) => ({shard : '', name, value})), shards = [];
let overlappingEntries = 0;
const overrideCases = [], retryCases = [], numericRetryCases = [], retryInputs = [];
const baseMap = Object.fromEntries(base);
for (const name of names) {
    const api = fresh(), original = api.add;
    let entries;
    api.add = (addition, source) => {
        assert.equal(source, name);
        entries = Object.entries(addition);
        original(addition, source)
    };
    require(path.join(dir, name + '.js'));
    assert.ok(entries);
    api.add = original;
    for (const [key, value] of entries) {
        if (Object.hasOwn(api.entities, key) && Object.hasOwn(baseMap, key)) {
            ++overlappingEntries;
            const expected = baseMap[key];
            assert.equal(api.entities[key], value);
            const override = value + '#override';
            api.add({[key] : override}, 'probe');
            assert.equal(api.entities[key], override);
            delete require.cache[path.join(dir, name + '.js')];
            require(path.join(dir, name + '.js'));
            const afterOverrideReload = api.entities[key];
            assert.equal(afterOverrideReload, value);
            api.remove(key);
            assert.equal(Object.hasOwn(api.entities, key), false);
            delete require.cache[path.join(dir, name + '.js')];
            require(path.join(dir, name + '.js'));
            const afterRemoveReload = api.entities[key];
            assert.equal(afterRemoveReload, value);
            overrideCases.push(
                {name : key, shard : name, expected, afterOverrideReload, afterRemoveReload});
            const trigger = entries.find(([ entry ]) => !Object.hasOwn(baseMap, entry))[0];
            assert.ok(trigger, `no non-base trigger in ${name}`);
            retryInputs.push({shard : name, name : key, trigger, value});
        }
        rows.push({shard : name, name : key, value})
    }
    shards.push({name, size : Object.keys(api.entities).length, entries : entries.length});
}
const numeric = [
    '0',         '-0',      '65',
    'x41',       'x1F600',  'xD800',
    'xDFFF',     'x10FFFF', '1114111',
    '1114112',   '-1',      'x110000',
    'X41',       '',        '1.9',
    '0x41',      '010',     ' 65x',
    'x41suffix', 'x',       '9999999999999999999999999999999999999999999'
].map(input => {
    try {
        return { input, value: fresh().numeric(input), error: false }
    } catch (e) {
        assert.equal(e.name, 'RangeError');
        return { input, value: '', error: true }
    }
});
const {mathjax} = require(path.join(cjs, 'mathjax.js'));
const oldLoad = mathjax.asyncLoad;
let requests = [];
mathjax.asyncLoad = name => {
    requests.push(name);
    require(path.join(cjs, name));
    return Promise.resolve()
};
async function translate(api, input) {
    for (let tries = 0; tries <= 30; ++tries) {
        try {
            return { value: api.translate(input), error: false }
        } catch (e) {
            if (e.retry instanceof Promise) {
                await e.retry;
                continue
            }
            assert.equal(e.name, 'RangeError');
            return { value: '', error: true }
        }
    }
    throw Error('Source retry did not finish')
}
async function main() {
    const cases = [];
    for (const item of retryInputs) {
        const {shard, name, trigger, value} = item, retryApi = fresh();
        retryApi.options.loadMissingEntities = true;
        retryApi.add({[name] : 'changed'}, 'test-overrides');
        requests = [];
        const input = '&' + name + '; &' + trigger + ';', result = await translate(retryApi, input);
        const files = [
            ...new Set([ 'test-overrides', ...requests.map(p => path.basename(p, '.js')) ])
        ].sort();
        assert.deepEqual(files, [ 'test-overrides', shard ].sort());
        assert.equal(result.error, false);
        assert.equal(result.value, value + ' ' + retryApi.entities[trigger]);
        retryCases.push({shard, name, trigger, input, expected : result.value, files});
    }
    {
        const shard = 'n', trigger = 'nle';
        requests = [];
        const beforeApi = fresh();
        beforeApi.options.loadMissingEntities = true;
        const before = await translate(beforeApi, '&#1114112;&' + trigger + ';');
        assert.equal(before.error, true);
        assert.deepEqual(requests, []);
        numericRetryCases.push({
            trigger,
            input : '&#1114112;&' + trigger + ';',
            files : [...new Set(requests.map(p => path.basename(p, '.js'))) ].sort()
        });
        requests = [];
        const afterApi = fresh();
        afterApi.options.loadMissingEntities = true;
        const after = await translate(afterApi, '&' + trigger + ';&#1114112;');
        assert.equal(after.error, true);
        const files = [...new Set(requests.map(p => path.basename(p, '.js'))) ].sort();
        assert.deepEqual(files, [ shard ]);
        numericRetryCases.push({trigger, input : '&' + trigger + ';&#1114112;', files});
    }
    for (const [group, load, inputs] of [
             [
                 0, true,
                 [
                     '&alpha; &amp; &lt; &gt; &quot;', '&AElig; &Afr; &Ascr; &Aopf;',
                     '&Aunknown; &aUnknown;', '&Aunknown; &Afr;', '&ALPHA; &Alpha; &alpha;',
                     '&bad_name; &bad-name; &; &a &amp;&alpha', '&x &amp; &bad&beta;',
                     '&afr; &aFr; &afr;', '&#65; &#x41; &#x1F600; &#0; &#xD800;', '&#X41;',
                     '&#xZZ; &#x; &#-1;', '&#1114112;', '&AMP; &amp; &AMP;'
                 ]
             ],
             [ 1, false, [ '&alpha; &AElig; &Afr; &AMP;', '&unknown;', '&#xDFFF; &#x10FFFF;' ] ],
             [
                 2, true,
                 [
                     '&zUnknown;&zUnknown;', '&zeta; &zunknown;',
                     '&NotEqualTilde; &CounterClockwiseContourIntegral;'
                 ]
             ],
    ]) {
        const api = fresh();
        api.options.loadMissingEntities = load;
        requests = [];
        for (const input of inputs) {
            const result = await translate(api, input);
            cases.push({
                group,
                load,
                input,
                ...result,
                files : [...new Set(requests.map(p => path.basename(p, '.js'))) ].sort()
            })
        }
    }
    const mutation = fresh();
    mutation.options.loadMissingEntities = false;
    mutation.add({AddedByProbe : 'Ω', emptyByProbe : ''}, 'probe');
    assert.equal(mutation.translate('&AddedByProbe; &emptyByProbe;'), 'Ω &emptyByProbe;');
    mutation.remove('AddedByProbe');
    assert.equal(mutation.translate('&AddedByProbe;'), '&AddedByProbe;');
    const {utf16Field, utf16List, writeTables} = require('./fixture-data.cjs');
    writeTables(path.join(root, 'tests/math/fixtures/entities'), {
        "rows": {columns: ["file_utf16", "name_utf16", "value_utf16"],
            rows: rows.map(r => [utf16Field(r.shard), utf16Field(r.name), utf16Field(r.value)])},
        "shards": {columns: ["name_utf16", "tableSize_integer"],
            rows: shards.map(r => [utf16Field(r.name), r.size])},
        "overrides": {columns: ["name_utf16", "shard_utf16", "expected_utf16", "afterOverrideReload_utf16", "afterRemoveReload_utf16"],
            rows: overrideCases.map(r => [utf16Field(r.name), utf16Field(r.shard), utf16Field(r.expected), utf16Field(r.afterOverrideReload), utf16Field(r.afterRemoveReload)])},
        "retries": {columns: ["shard_utf16", "name_utf16", "trigger_utf16", "input_utf16", "expected_utf16", "files_count", "files_utf16_list"],
            rows: retryCases.map(r => [utf16Field(r.shard), utf16Field(r.name), utf16Field(r.trigger), utf16Field(r.input), utf16Field(r.expected), r.files.length, utf16List(r.files)])},
        "numeric-retries": {columns: ["trigger_utf16", "input_utf16", "files_count", "files_utf16_list"],
            rows: numericRetryCases.map(r => [utf16Field(r.trigger), utf16Field(r.input), r.files.length, utf16List(r.files)])},
        "numbers": {columns: ["input_utf16", "output_utf16", "error_boolean"],
            rows: numeric.map(r => [utf16Field(r.input), utf16Field(r.value), +!!r.error])},
        "translations": {columns: ["module_integer", "loadMissing_boolean", "input_utf16", "output_utf16", "error_boolean", "loaded_count", "loaded_utf16_list"],
            rows: cases.map(r => [r.group, +!!r.load, utf16Field(r.input), utf16Field(r.value), +!!r.error, r.files.length, utf16List(r.files)])},
    });
    console.log(JSON.stringify({
        base : base.length,
        shards : shards.length,
        rows : rows.length,
        overlappingEntries,
        overrideCases : overrideCases.length,
        retryCases : retryCases.length,
        numericRetryCases : numericRetryCases.length,
        numeric : numeric.length,
        translations : cases.length,
        completedSourceRetries : true,
        sourceAddCaptured : true,
        overrideReloadsRestored : true,
        removeReloadsRestored : true,
        retryRestartsFromInputStart : true
    }));
}
main().finally(() => mathjax.asyncLoad = oldLoad).catch(e => {
    process.stderr.write(String(e.stack || e));
    process.exitCode = 1
});
