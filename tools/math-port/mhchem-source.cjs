'use strict';

// Pinned MathJax Source witness for the complete native Mhchem package.
// Regenerate only the checked test fixture with --write-fixture.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const assert = require('node:assert/strict');
const vm = require('node:vm');

const root = path.resolve(__dirname, '../..');
const sourceOutput = path.join(root, 'build/math-port/isolated/mhchem-package/original.json');
const sourceSupplementOutput = path.join(root,
    'build/math-port/isolated/mhchem-package/supplement.json');
const sourceLock = path.join(root, 'docs/math-port/evidence/mhchem-source-lock.json');
const fixturePath = path.join(root, 'tests/math/fixtures/packages/mhchem');
const sourceTestPath = '.reference/MathJax-src/testsuite/tests/input/tex/Mhchem.test.ts';
const expectedSourceTestSha256 = 'e0194d5d0e5931d18c3a237956cfee8065d901fbc7f5a934c835090506451dad';
const expectedCorpusSha256 = 'cba985c6d840c604b1f5d2a07926d0a4438c58d9cdb4a02ad7d4f0620a1f0c56';
const expectedSourceSha256 = '088ce21b19cad12a87bc3b1c6ceb245652b9589edc30b246d61584cd0c5eab85';
const writeFixture = process.argv.includes('--write-fixture');
const sha = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');

function makeCorpus() {
    const test = fs.readFileSync(path.join(root, sourceTestPath), 'utf8');
    const cases = [];
    const tests = /it\(('(?:\\.|[^'\\])*'|"(?:\\.|[^"\\])*"),\s*\(\)\s*=>\s*\{([\s\S]*?)\n  \}\);/g;
    for (const match of test.matchAll(tests)) {
        const input =
            /(?:tex2mml|expectTexError)\(\s*('(?:\\.|[^'\\])*'|"(?:\\.|[^"\\])*")/.exec(
                match[2]);
        assert(input, `Unrecognized original Mhchem test: ${match[1]}`);
        const section = [...test.slice(0, match.index).matchAll(/describe\('([^']+)'/g)].at(-1)[1];
        cases.push({
            name: `original/${section}/${vm.runInNewContext(match[1])}`,
            input: vm.runInNewContext(input[1]),
            packages: section === 'Mhchem-Ams' ? ['base', 'ams', 'mhchem'] : ['base', 'mhchem'],
            options: {},
        });
    }
    assert.equal(cases.length, (test.match(/\bit\(/g) || []).length,
        'Every original test must be retained');
    const add = (name, input, ams = true) => cases.push({
        name: `extra/${name}`, input, packages: ams ? ['base', 'ams', 'mhchem'] :
            ['base', 'mhchem'], options: {},
    });
    for (const arrow of ['->', '<-', '<->', '<-->', '<=>', '<=>>', '<<=>', '\u2192',
        '\u27f6', '\u21cc']) {
        add(`arrow-top-${arrow}`, `\\ce{A ${arrow}[H2O] B}`);
        add(`arrow-bottom-${arrow}`, `\\ce{A ${arrow}[][H2O] B}`);
        add(`arrow-both-${arrow}`, `\\ce{A ${arrow}[{top label}][{bottom label}] B}`);
        add(`arrow-math-${arrow}`, `\\ce{A ${arrow}[$x_i$][$y^2$] B}`);
        add(`arrow-label-base-${arrow}`, `\\ce{A ${arrow}[H2O] B}`, false);
    }
    for (const [name, input] of [
        ['two-chem-commands', String.raw`\ce{H2O}+\ce{CO2}`],
        ['two-unit-commands', String.raw`\pu{12 m/s}+\pu{3 kg}`],
        ['chem-remainder', String.raw`x+\ce{H2O}+y`],
        ['nested-chem', String.raw`\ce{CH4 + $\ce{2 O2}$ -> CO2 + 2 H2O}`],
        ['frac-chem', String.raw`\ce{\frac{H2O}{2 CO2}}`],
        ['overset-chem', String.raw`\ce{\overset{a}{H2O}}`],
        ['underset-chem', String.raw`\ce{\underset{b}{CO2}}`],
        ['underbrace-chem', String.raw`\ce{\underbrace{H2O}_{water}}`],
        ['greek-chem', String.raw`\ce{\alpha-Fe2O3 + \beta-Ni(OH)2}`],
        ['color-chem', String.raw`\ce{\color{red}{H2O} + \color{blue}{CO2}}`],
        ['isotopes', String.raw`\ce{^{14}_{6}C -> ^{14}_{7}N + e-}`],
        ['hydrate', String.raw`\ce{CuSO4 * 5H2O}`],
        ['oxidation', String.raw`\ce{Fe^{III}2O3}`],
        ['entity-and-escape', String.raw`\ce{\{A\} + B}`],
        ['units-exponent', String.raw`\pu{1.23e-4 kg m2 s-2}`],
        ['units-plusminus', String.raw`\pu{1.23 +- 0.01 m}`],
        ['units-uncertainty', String.raw`\pu{1.23(4)e5 J}`],
        ['units-number-slash', String.raw`\pu{1/2 mol}`],
        ['units-fraction', String.raw`\pu{123 kJ // mol K}`],
        ['units-braces', String.raw`\pu{10 {kg m} / {s2}}`],
        ['units-micro', String.raw`\pu{2.5 \mu m}`],
        ['units-tex', String.raw`\pu{1 $\alpha$}`],
        ['units-missing-exponent', String.raw`\pu{1e kg}`],
        ['empty-chem', String.raw`\ce{}`],
        ['empty-unit', String.raw`\pu{}`],
        ['invalid-bond', String.raw`\ce{A\bond{x}B}`],
        ['invalid-bond-nested', String.raw`\ce{A\bond{unknown}B}`],
        ['incomplete-superscript', String.raw`\ce{H^}`],
        ['incomplete-subscript', String.raw`\ce{H_}`],
        ['incomplete-arrow', String.raw`\ce{A ->[}`],
        ['unclosed-chemical-group', String.raw`\ce{(H2O}`],
        ['missing-chem-argument', String.raw`\ce`],
        ['missing-unit-argument', String.raw`\pu`],
        ['missing-closing-argument', String.raw`\ce{H2O`],
        ['unsupported-tex-command', String.raw`\ce{\mhchemUnknown{X}}`],
        ['unknown-outer-command', String.raw`\mhchemUnknown{H2O}`],
    ]) add(name, input);
    const characters = ['tripledash', 'mhchemBondTD', 'mhchemBondTDD', 'mhchemBondDTD',
        'mhchemlongleftarrow', 'mhchemlongrightarrow', 'mhchemlongleftrightarrow',
        'mhchemlongrightleftharpoons', 'mhchemlongRightleftharpoons',
        'mhchemlongLeftrightharpoons', 'mhchemlongleftrightarrows', 'mhchemrightarrow',
        'mhchemleftarrow', 'mhchemleftrightarrow'];
    for (const name of characters) add(`character-${name}`, `A\\${name} B`);
    const stretch = ['mhchemxrightarrow', 'mhchemxleftarrow', 'mhchemxleftrightarrow',
        'mhchemxleftrightarrows', 'mhchemxrightleftharpoons',
        'mhchemxRightleftharpoons', 'mhchemxLeftrightharpoons'];
    for (const name of stretch) add(`macro-${name}`, `A\\${name}[below]{above} B`);

    const replacementCases = [];
    const replace = (name, input, rules) => replacementCases.push({ name, input, rules });
    replace('empty', '', []);
    replace('plain', 'A + B', []);
    replace('rule-1',
        String.raw`\underset{\lower2mu{below}}{\overset{above}{\longleftrightarrows}}`, [1]);
    replace('rule-1-global',
        String.raw`\underset{\lower2mu{b}}{\overset{a}{\longleftrightarrows}}` +
        String.raw`+\underset{\lower2mu{d}}{\overset{c}{\longleftrightarrows}}`, [1]);
    replace('rule-1-empty-labels',
        String.raw`\underset{\lower2mu{}}{\overset{}{\longleftrightarrows}}`, [1]);
    replace('rule-2', String.raw`\overset{top}{\longrightleftharpoons}`, [2]);
    replace('rule-2-global',
        String.raw`\overset{a}{\longRightleftharpoons}+\overset{b}{\longLeftrightharpoons}`, [2]);
    replace('rule-2-newline', String.raw`\overset{a` + '\n' + String.raw`b}{\longrightarrow}`, []);
    replace('rule-3', String.raw`\rlap{\lower.1em{-}}\raise.1em{\tripledash}`, [3]);
    replace('rule-4', String.raw`\rlap{\lower.2em{-}}\rlap{\raise.2em{\tripledash}}-`, [4]);
    replace('rule-5', String.raw`\rlap{\lower.2em{-}}\rlap{\raise.2em{-}}\tripledash`, [5]);
    replace('rule-5-wildcard',
        String.raw`\rlap{\lower.2em{-}}\rlap{\raiseX2em{-}}\tripledash`, [5]);
    replace('bonds-global', String.raw`\rlap{\lower.1em{-}}\raise.1em{\tripledash}` +
        String.raw`+\rlap{\lower.1em{-}}\raise.1em{\tripledash}` +
        String.raw`+\rlap{\lower.2em{-}}\rlap{\raise.2em{\tripledash}}-` +
        String.raw`+\rlap{\lower.2em{-}}\rlap{\raise.2em{-}}\tripledash`, [3, 4, 5]);
    for (const prefix of ['', 'long', 'x', 'xlong'])
        for (const direction of ['left', 'right', 'leftright', 'Leftright', 'rightleft',
            'Rightleft'])
            for (const suffix of ['arrow', 'harpoons']) {
                const name = prefix + direction + suffix;
                replace(`rule-6-${name}`, `\\${name}`, [6]);
            }
    replace('rule-6-prefix', String.raw`\rightarrowExtra\longleftarrows\xrightarrowX`, [6]);
    replace('all-rules',
        String.raw`\underset{\lower2mu{b}}{\overset{a}{\longleftrightarrows}}` +
        String.raw`+\overset{t}{\longrightleftharpoons}` +
        String.raw`+\rlap{\lower.1em{-}}\raise.1em{\tripledash}` +
        String.raw`+\rlap{\lower.2em{-}}\rlap{\raise.2em{\tripledash}}-` +
        String.raw`+\rlap{\lower.2em{-}}\rlap{\raise.2em{-}}\tripledash+\rightarrow`,
        [1, 2, 3, 4, 5, 6]);
    const mutationCases = ['ce-macro-replaced', 'character-parser-replaced',
        'character-arrow-removed', 'character-arrow-false', 'character-arrow-null',
        'character-arrow-custom', 'macro-arrow-removed', 'character-false-macro-fallback',
        'registry-maps-replaced', 'registry-shadow-added-arrow',
        'exported-methods-reassigned', 'exported-utils-reassigned', 'exported-extra-keys',
        'exported-machine-removed'];
    const mutationInput = String.raw`\rightarrow+\leftarrow+\leftrightarrow+\longrightarrow+` +
        String.raw`\longleftarrow+\longleftrightarrow+\longleftrightarrows+` +
        String.raw`\longrightleftharpoons+\longRightleftharpoons+\longLeftrightharpoons+` +
        String.raw`\xrightarrow{top}+\xleftarrow{top}+\xleftrightarrow{top}+` +
        String.raw`\xleftrightarrows{top}+\xrightleftharpoons{top}+` +
        String.raw`\xRightleftharpoons{top}+\xLeftrightharpoons{top}`;
    return {
        schema: 1,
        sourceTests: { path: sourceTestPath, sha256: expectedSourceTestSha256 },
        originalCaseCount: cases.filter(item => item.name.startsWith('original/')).length,
        cases, replacementCases, mutationCases, mutationInput,
    };
}

function plain(value, seen = new WeakSet()) {
    if (value === undefined) return { $: 'undefined' };
    if (value === null || typeof value === 'string' || typeof value === 'boolean') return value;
    if (typeof value === 'number')
        return Number.isNaN(value) ? { $: 'NaN' } : Object.is(value, -0) ? { $: '-0' } :
            Number.isFinite(value) ? value : { $: value < 0 ? '-Infinity' : 'Infinity' };
    if (typeof value === 'function')
        return { $: 'function', name: value.name || '', length: value.length };
    if (typeof value === 'symbol') return { $: 'symbol' };
    if (typeof value !== 'object') throw new Error(`Unserializable Source value: ${typeof value}`);
    if (seen.has(value)) return { $: 'cycle' };
    seen.add(value);
    try {
        if (Array.isArray(value))
            return Array.from({ length: value.length }, (_, i) =>
                Object.hasOwn(value, i) ? plain(value[i], seen) : { $: 'hole' });
        if (value instanceof Map)
            return { $: 'map', entries: [...value].map(([key, item]) =>
                [plain(key, seen), plain(item, seen)]) };
        const result = {};
        for (const key of Reflect.ownKeys(value))
            result[typeof key === 'symbol' ? `symbol:${key.description || ''}` : key] =
                plain(value[key], seen);
        return result;
    } finally {
        seen.delete(value);
    }
}

function sourceTree(node) {
    if (node == null) return null;
    const attributes = node.attributes;
    return {
        kind: node.kind,
        attrs: plain(attributes?.getAllAttributes?.() ?? {}),
        inherited: plain(attributes?.getAllInherited?.() ?? {}),
        defaults: plain(attributes?.getAllDefaults?.() ?? {}),
        globals: plain(attributes?.getAllGlobals?.() ?? {}),
        properties: plain(node.getAllProperties?.() ?? {}),
        ...(typeof node.getText === 'function' ? { text: node.getText() } : {}),
        children: (node.childNodes || []).map(sourceTree),
    };
}

function captureSource(corpus, lockedSource) {
    const cjsRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
    const load = name => require(path.join(cjsRoot, name));
    const { TeX } = load('input/tex.js');
    const { MmlFactory } = load('core/MmlTree/MmlFactory.js');
    const { AbstractMathItem } = load('core/MathItem.js');
    const { MapHandler } = load('input/tex/MapHandler.js');
    load('input/tex/ams/AmsConfiguration.js');
    const mhchemModule = load('input/tex/mhchem/MhchemConfiguration.js');
    const { MhchemReplacements, MhchemMethods, MhchemUtils, MhchemConfiguration } = mhchemModule;
    const BaseMethods = load('input/tex/base/BaseMethods.js').default;
    const { AmsMethods } = load('input/tex/ams/AmsMethods.js');
    const { Macro, Token } = load('input/tex/Token.js');
    const { CommandMap, CharacterMap } = load('input/tex/TokenMap.js');
    const observations = corpus.cases.map(item => {
        let jax = null;
        let result = null;
        let error = null;
        const formattedErrors = [];
        try {
            jax = new TeX({ ...item.options, packages: item.packages, formatError(jax, error) {
                formattedErrors.push({ name: error.name || error.constructor.name,
                    id: error.id ?? null, message: String(error.message) });
                return jax.formatError(error);
            } });
            jax.setMmlFactory(new MmlFactory());
            result = sourceTree(jax.compile(new AbstractMathItem(item.input, jax, true), {}));
        } catch (caught) {
            error = { name: caught.name || caught.constructor.name, id: caught.id ?? null,
                message: String(caught.message) };
        }
        return { ...item, result, error, formattedErrors,
            packageData: jax ? plain(jax.parseOptions.packageData) : null };
    });
    const replacements = corpus.replacementCases.map(item => {
        let result = item.input;
        const stages = [];
        for (const [replacement, pattern] of MhchemReplacements) {
            result = result.replace(pattern, replacement);
            stages.push(result);
        }
        return { ...item, stages, result };
    });
    const replacementRules = [...MhchemReplacements].map(([replacement, pattern]) => ({
        pattern: pattern.source, flags: pattern.flags, replacement: plain(replacement),
    }));
    const maps = {};
    for (const name of ['mhchem', 'mhchem-chars']) {
        const map = MapHandler.getMap(name);
        assert(map, `Missing Source map ${name}`);
        maps[name] = plain(map);
    }
    const methods = plain(MhchemMethods);
    const utils = plain(MhchemUtils);
    const identities = {
        machineCe: MhchemMethods.Machine === MapHandler.getMap('mhchem').lookup('ce').func,
        machinePu: MhchemMethods.Machine === MapHandler.getMap('mhchem').lookup('pu').func,
        macroBase: MhchemMethods.Macro === BaseMethods.Macro,
        arrowAms: MhchemMethods.xArrow === AmsMethods.xArrow,
        arrowMacros: [...MapHandler.getMap('mhchem').map]
            .filter(([key]) => key.startsWith('mhchemx'))
            .map(([key, value]) => ({ name: key, same: MhchemMethods.xArrow === value.func })),
        relmoCharacterParser: MhchemUtils.relmo === MapHandler.getMap('mhchem-chars').parser,
        missingMethod: plain(MhchemMethods.mhchemUnknown),
    };
    const retainedMacros = MapHandler.getMap('mhchem');
    const retainedCharacters = MapHandler.getMap('mhchem-chars');
    const originalMethods = Object.entries(MhchemMethods);
    const originalUtils = Object.entries(MhchemUtils);
    const originalMacroEntries = [...retainedMacros.map];
    const originalCharacterEntries = [...retainedCharacters.map];
    const originalMacroParser = retainedMacros.parser;
    const originalCharacterParser = retainedCharacters.parser;
    const originalMachine = MhchemMethods.Machine;
    const originalMacro = MhchemMethods.Macro;
    const originalArrow = MhchemMethods.xArrow;
    const originalRelmo = MhchemUtils.relmo;
    const restoreMutations = () => {
        for (const key of Object.keys(MhchemMethods)) delete MhchemMethods[key];
        for (const [key, value] of originalMethods) MhchemMethods[key] = value;
        for (const key of Object.keys(MhchemUtils)) delete MhchemUtils[key];
        for (const [key, value] of originalUtils) MhchemUtils[key] = value;
        retainedMacros.map.clear();
        for (const [key, value] of originalMacroEntries) retainedMacros.add(key, value);
        retainedCharacters.map.clear();
        for (const [key, value] of originalCharacterEntries) retainedCharacters.add(key, value);
        retainedMacros.parser = originalMacroParser;
        retainedCharacters.parser = originalCharacterParser;
        MapHandler.register(retainedMacros);
        MapHandler.register(retainedCharacters);
    };
    const mutationSnapshot = () => {
        const imported = load('input/tex/mhchem/MhchemConfiguration.js');
        const ce = retainedMacros.lookup('ce');
        const pu = retainedMacros.lookup('pu');
        let result = corpus.mutationInput;
        const stages = [];
        for (const [replacement, pattern] of MhchemReplacements) {
            result = result.replace(pattern, replacement);
            stages.push(result);
        }
        return {
            methods: plain(MhchemMethods), utils: plain(MhchemUtils),
            getters: {
                Machine: plain(MhchemMethods.Machine), Macro: plain(MhchemMethods.Macro),
                xArrow: plain(MhchemMethods.xArrow),
                mhchemUnknown: plain(MhchemMethods.mhchemUnknown),
                observerExtra: plain(MhchemMethods.observerExtra),
                relmo: plain(MhchemUtils.relmo),
            },
            retainedMaps: {
                mhchem: plain(retainedMacros), 'mhchem-chars': plain(retainedCharacters),
            },
            registryMaps: { mhchem: plain(MapHandler.getMap('mhchem')),
                'mhchem-chars': plain(MapHandler.getMap('mhchem-chars')) },
            identities: {
                moduleSame: imported === mhchemModule,
                configurationSame: imported.MhchemConfiguration === MhchemConfiguration,
                methodsSame: imported.MhchemMethods === MhchemMethods,
                utilsSame: imported.MhchemUtils === MhchemUtils,
                methodsMachineOriginal: MhchemMethods.Machine === originalMachine,
                methodsMacroOriginal: MhchemMethods.Macro === originalMacro,
                methodsArrowOriginal: MhchemMethods.xArrow === originalArrow,
                utilsRelmoOriginal: MhchemUtils.relmo === originalRelmo,
                ceOriginal: ce?.func === originalMachine,
                puOriginal: pu?.func === originalMachine,
                characterParserOriginal: retainedCharacters.parser === originalRelmo,
                machineEqualsCe: MhchemMethods.Machine === ce?.func,
                relmoEqualsParser: MhchemUtils.relmo === retainedCharacters.parser,
                macrosSameAsRegistry: retainedMacros === MapHandler.getMap('mhchem'),
                charactersSameAsRegistry: retainedCharacters === MapHandler.getMap('mhchem-chars'),
            },
            replacementStages: stages, replacement: result,
        };
    };
    const applyMutation = name => {
        switch (name) {
        case 'ce-macro-replaced':
            retainedMacros.add('ce',
                new Macro('ce', BaseMethods.Macro, ['observer replacement', 0]));
            break;
        case 'character-parser-replaced': retainedCharacters.parser = BaseMethods.Macro; break;
        case 'character-arrow-removed': retainedCharacters.remove('mhchemrightarrow'); break;
        case 'character-arrow-false': retainedCharacters.add('mhchemrightarrow', false); break;
        case 'character-arrow-null': retainedCharacters.add('mhchemrightarrow', null); break;
        case 'character-arrow-custom':
            retainedCharacters.add('mhchemrightarrow', new Token('mhchemrightarrow', 'Z',
                { stretchy: false }));
            break;
        case 'macro-arrow-removed': retainedMacros.remove('mhchemxrightarrow'); break;
        case 'character-false-macro-fallback':
            retainedCharacters.add('mhchemrightarrow', false);
            retainedMacros.add('mhchemrightarrow',
                new Macro('mhchemrightarrow', BaseMethods.Macro, ['fallback', 0]));
            break;
        case 'registry-maps-replaced':
            new CommandMap('mhchem', { observerOnly: [BaseMethods.Macro, 'Q', 0],
                mhchemxrightarrow: [BaseMethods.Macro, 'R', 0] });
            new CharacterMap('mhchem-chars', BaseMethods.Macro,
                { mhchemleftarrow: 'L', observerOnly: 'Y' });
            break;
        case 'registry-shadow-added-arrow':
            retainedCharacters.remove('mhchemrightarrow');
            retainedMacros.remove('mhchemxrightarrow');
            new CommandMap('mhchem', { mhchemxrightarrow: [BaseMethods.Macro, 'R', 0] });
            new CharacterMap('mhchem-chars', BaseMethods.Macro, { mhchemrightarrow: 'R' });
            break;
        case 'exported-methods-reassigned':
            MhchemMethods.Machine = originalMacro;
            MhchemMethods.Macro = originalArrow;
            MhchemMethods.xArrow = originalMachine;
            break;
        case 'exported-utils-reassigned': MhchemUtils.relmo = originalArrow; break;
        case 'exported-extra-keys':
            MhchemMethods.observerExtra = originalMacro;
            MhchemUtils.observerExtra = originalMachine;
            break;
        case 'exported-machine-removed': delete MhchemMethods.Machine; break;
        default: throw new Error(`Unknown mutation case ${name}`);
        }
    };
    const mutations = corpus.mutationCases.map(name => {
        const before = mutationSnapshot();
        let during;
        try {
            applyMutation(name);
            during = mutationSnapshot();
        } finally {
            restoreMutations();
        }
        const after = mutationSnapshot();
        assert.deepEqual(after, before, `Mutation restoration failed: ${name}`);
        return { name, before, during, after };
    });
    const source = {
        schema: 1,
        source: {
            mathjaxVersion: lockedSource.mathjax.version,
            mathjaxCommit: lockedSource.mathjax.commit,
            parserVersion: lockedSource.dependency.version,
        },
        corpusSha256: expectedCorpusSha256,
        cases: observations,
        methods, utils, identities, replacementRules, replacements, maps, mutations,
    };
    const outputBytes = Buffer.from(JSON.stringify(source, null, 2) + '\n');
    return { source, sourceSha256: crypto.createHash('sha256').update(outputBytes).digest('hex') };
}

const lockedSource = JSON.parse(fs.readFileSync(sourceLock, 'utf8'));
for (const file of lockedSource.files) {
    assert.equal(sha(path.join(root, file.path)), file.sha256,
        `Pinned Source changed: ${file.path}`);
}
for (const file of lockedSource.generated) {
    assert.equal(sha(path.join(root, file.path)), file.sha256,
        `Pinned generated data changed: ${file.path}`);
}
assert.equal(sha(path.join(root, sourceTestPath)), expectedSourceTestSha256,
    'Pinned original Mhchem.test.ts changed');
const corpus = makeCorpus();
const corpusBytes = Buffer.from(JSON.stringify(corpus, null, 2) + '\n');
assert.equal(crypto.createHash('sha256').update(corpusBytes).digest('hex'), expectedCorpusSha256,
    'Original Mhchem tests/extras corpus changed');
const { source, sourceSha256 } = captureSource(corpus, lockedSource);
assert.equal(sourceSha256, expectedSourceSha256, 'Pinned Mhchem Source observations changed');
fs.mkdirSync(path.dirname(sourceOutput), { recursive: true });
fs.writeFileSync(sourceOutput, JSON.stringify(source, null, 2) + '\n');
const sourceSupplement = aliasObservations();
fs.writeFileSync(sourceSupplementOutput, JSON.stringify(sourceSupplement, null, 2) + '\n');
assert.equal(source.source.mathjaxVersion, '4.1.3');
assert.equal(source.source.mathjaxCommit, 'fb987178c2d279a99b0db5ea02e751691703955e');
assert.equal(source.source.parserVersion, '4.2.1');
assert.equal(source.cases.length, 220);
assert.equal(source.cases.filter(item => item.name.startsWith('original/')).length, 113);
assert.equal(source.cases.length, corpus.cases.length);
assert.equal(source.replacements.length, 63);
assert.equal(source.replacementRules.length, 6);
assert.equal(Object.keys(source.maps).length, 2);
assert.equal(source.maps.mhchem.map.entries.length, 9);
assert.equal(source.maps['mhchem-chars'].map.entries.length, 14);
assert.equal(source.mutations.length, 14);
assert.equal(source.identities.machineCe, true);
assert.equal(source.identities.machinePu, true);
assert.equal(source.identities.macroBase, true);
assert.equal(source.identities.arrowAms, true);
assert.equal(source.identities.relmoCharacterParser, true);
assert.equal(source.identities.arrowMacros.length, 7);
assert.ok(source.identities.arrowMacros.every(item => item.same));
for (const item of source.replacements) assert.equal(item.stages.length, 6, item.name);
for (const item of source.mutations) {
    assert.deepEqual(item.after, item.before, `Source mutation did not recover: ${item.name}`);
    for (const stage of ['before', 'during', 'after'])
        assert.equal(item[stage].replacementStages.length, 6, `${item.name}/${stage}`);
}

function add(value) {
    value = String(value);
    return `${value.length}:${value}`;
}
function snapshot(node) {
    if (!node) return add('null');
    let result = add(node.kind);
    const attributes = Object.entries(node.attrs || {}).sort(([a], [b]) =>
        a < b ? -1 : a > b ? 1 : 0);
    result += add(attributes.length);
    for (const [key, value] of attributes) {
        result += add(key);
        const isNull = value === null;
        result += add(isNull ? 'null' : typeof value);
        result += add(isNull ? '' : value);
    }
    const properties = node.properties || {};
    const propertyNames = ['texClass', 'unicode', 'autoOP', 'fnOP', 'fixStretchy',
        'ignore-variant', 'noDictDef', 'texprimestyle', 'in-lists', 'sub-sup',
        'open', 'close', 'mathaccent', 'movesupsub', 'subsupOK', 'variantForm'];
    const selected = propertyNames.filter(name => Object.hasOwn(properties, name));
    result += add(selected.length);
    for (const name of selected) {
        result += add(name);
        const value = properties[name];
        result += add(typeof value);
        result += add(value);
    }
    result += add(Object.hasOwn(node, 'text') ? node.text : '');
    result += add((node.children || []).length);
    for (const child of node.children || []) result += snapshot(child);
    return result;
}

// A captured `\let` alias is part of Source's macro-table behavior. Keep this
// in its own live jax sequence so the native test cannot pass by substituting
// a `\newcommand` expansion or by constructing a fresh parser per row.
function aliasObservations() {
    const cjs = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src/cjs');
    const load = name => require(path.join(cjs, name));
    const { TeX } = load('input/tex.js');
    const { MmlFactory } = load('core/MmlTree/MmlFactory.js');
    const { AbstractMathItem } = load('core/MathItem.js');
    const { Token } = load('input/tex/Token.js');
    const { NodeFactory } = load('input/tex/NodeFactory.js');
    load('input/tex/newcommand/NewcommandConfiguration.js');
    load('input/tex/ams/AmsConfiguration.js');
    load('input/tex/mhchem/MhchemConfiguration.js');
    const jax = new TeX({ packages: ['base', 'newcommand', 'ams', 'mhchem'] });
    jax.setMmlFactory(new MmlFactory());
    const aliases = [String.raw`\let\savedce=\ce \savedce{H2O}`,
        String.raw`\let\savedpu=\pu \savedpu{12 kg}`].map(input => {
        const tree = jax.compile(new AbstractMathItem(input, jax, true), {});
        return { input, snapshot: snapshot(plainTree(tree)) };
    });
    const customMap = jax.parseOptions.handlers.retrieve('mhchem-chars');
    customMap.add('mhchemCustom', new Token('mhchemCustom', '→', {
        stretchy: false, mathvariant: '-custom', texClass: 0,
    }));
    const requests = [];
    jax.parseOptions.nodeFactory.setCreators({
        token(factory, kind, definition, text) {
            if (kind === 'mo' && text === '→') requests.push({ kind, text,
                stretchy: definition.stretchy, mathvariant: definition.mathvariant,
                texClass: definition.texClass });
            return NodeFactory.createToken(factory, kind, definition, text);
        },
    });
    const creatorInput = String.raw`\mhchemCustom`;
    const creatorTree = jax.compile(new AbstractMathItem(creatorInput, jax, true), {});
    const errors = [];
    const recoveryJax = new TeX({ packages: ['base', 'ams', 'mhchem'],
        formatError(owner, error) {
            errors.push({ name: error.name || error.constructor.name, id: error.id ?? '',
                message: String(error.message) });
            return owner.formatError(error);
        },
    });
    recoveryJax.setMmlFactory(new MmlFactory());
    const recovery = [String.raw`\ce{A\bond{x}B}`, String.raw`\ce{H2O}`].map(input => {
        errors.length = 0;
        const tree = recoveryJax.compile(new AbstractMathItem(input, recoveryJax, true), {});
        const error = errors.at(-1);
        return { input, snapshot: snapshot(plainTree(tree)), errorName: error?.name || '',
            errorId: error?.id || '', errorMessage: error?.message || '' };
    });
    return {
        aliases,
        recovery,
        creator: {
            input: creatorInput,
            snapshot: snapshot(plainTree(creatorTree)),
            requests,
        },
    };
}

function plainTree(node) {
    if (!node) return null;
    const attributes = node.attributes;
    return {
        kind: node.kind,
        attrs: attributes?.getAllAttributes?.() ?? {},
        properties: node.getAllProperties?.() ?? {},
        ...(typeof node.getText === 'function' ? { text: node.getText() } : {}),
        children: (node.childNodes || []).map(plainTree),
    };
}

const {utf16Field, byteField, writeTables} = require('./fixture-data.cjs');
const stageColumns = name => Array.from({length: 6}, (_, index) => `${name}${index}_utf16`);
writeTables(fixturePath, {
  cases: {
    columns: ['name_utf16', 'input_utf16', 'snapshot_utf16', 'errorName_utf16',
      'errorId_utf16', 'errorMessage_utf16', 'formattedErrorName_utf16',
      'formattedErrorId_utf16', 'formattedErrorMessage_utf16', 'ams_boolean'],
    metadata: [`corpus_sha256=${expectedCorpusSha256}`],
    rows: source.cases.map(item => {
      const error = item.error || {}, formatted = item.formattedErrors[0] || {};
      return [item.name, item.input, item.result ? snapshot(item.result) : '',
        error.name || '', error.id == null ? '' : String(error.id), error.message || '',
        formatted.name || '', formatted.id == null ? '' : String(formatted.id),
        formatted.message || ''].map(utf16Field).concat(+item.packages.includes('ams'));
    })
  },
  replacements: {
    columns: ['name_utf16', 'input_utf16', 'rules_utf16', ...stageColumns('stages'), 'result_utf16'],
    rows: source.replacements.map(item => [item.name, item.input, JSON.stringify(item.rules),
      ...item.stages, item.result].map(utf16Field))
  },
  'mutation-input': {columns: ['value_utf16'], rows: [[utf16Field(corpus.mutationInput)]]},
  mutations: {
    columns: ['name_bytes', ...stageColumns('before'), ...stageColumns('during'), ...stageColumns('after')],
    rows: source.mutations.map(item => [byteField(item.name),
      ...['before', 'during', 'after'].flatMap(phase => item[phase].replacementStages.map(utf16Field))])
  },
  'untyped-mutations': {columns: ['value_bytes'],
    rows: ['character-arrow-false', 'character-arrow-null'].map(value => [byteField(value)])},
  'retired-mutations': {columns: ['value_bytes'], rows: ['exported-methods-reassigned',
    'exported-utils-reassigned', 'exported-extra-keys', 'exported-machine-removed']
    .map(value => [byteField(value)])},
  aliases: {columns: ['input_utf16', 'snapshot_utf16'],
    rows: sourceSupplement.aliases.map(item => [item.input, item.snapshot].map(utf16Field))},
  creator: {columns: ['input_utf16', 'snapshot_utf16', 'kind_utf16', 'text_utf16',
      'mathvariant_utf16', 'stretchy_boolean', 'texClass_integer'],
    rows: [[sourceSupplement.creator.input, sourceSupplement.creator.snapshot,
      sourceSupplement.creator.requests[0].kind, sourceSupplement.creator.requests[0].text,
      sourceSupplement.creator.requests[0].mathvariant].map(utf16Field).concat(
      +sourceSupplement.creator.requests[0].stretchy, sourceSupplement.creator.requests[0].texClass)]},
  recovery: {columns: ['input_utf16', 'snapshot_utf16', 'errorName_utf16', 'errorId_utf16', 'errorMessage_utf16'],
    rows: sourceSupplement.recovery.map(item => [item.input, item.snapshot, item.errorName,
      item.errorId, item.errorMessage].map(utf16Field))}
}, writeFixture);

console.log(JSON.stringify({
    accepted: true,
    sourceOutputSha256: sourceSha256,
    originalCases: 113,
    cases: source.cases.length,
    replacements: source.replacements.length,
    replacementRules: source.replacementRules.length,
    commandMaps: Object.keys(source.maps).length,
    mutations: source.mutations.length,
    fixture: path.relative(root, fixturePath).replaceAll('\\', '/'),
    fixtureFiles: Object.fromEntries(fs.readdirSync(fixturePath).filter(name => name.endsWith('.tsv'))
        .map(name => [name, sha(path.join(fixturePath, name))])),
}));
