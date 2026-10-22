// Root independent actual-CJS observations, frozen before implementation.
'use strict';
const fs = require('node:fs'), path = require('node:path'), assert = require('node:assert/strict');
const { root, MmlFactory, kinds, classes, core } = require(path.join(path.resolve(__dirname, '../..'), 'tools/math-port/multiscript-reference.cjs'));
const scenarios = [];
let current = null;
function encode(v) {
    if (v === undefined)
        return ['$undefined'];
    if (v === null)
        return ['$null'];
    if (typeof v === 'string')
        return ['$utf16', Array.from({ length: v.length }, (_, i) => v.charCodeAt(i))];
    if (typeof v === 'boolean')
        return v;
    if (typeof v === 'number') {
        if (Number.isNaN(v))
            return ['$number', 'NaN'];
        if (v === Infinity)
            return ['$number', 'Infinity'];
        if (v === -Infinity)
            return ['$number', '-Infinity'];
        if (Object.is(v, -0))
            return ['$number', '-0'];
        return v;
    }
    if (Array.isArray(v)) {
        const entries = [];
        for (const k of Object.keys(v))
            entries.push([k, encode(v[k])]);
        return ['$array', v.length, entries];
    }
    if (v && typeof v === 'object' && v.constructor && v.constructor.name !== 'Object')
        return ['$instance', v.constructor.name, v.label ?? null, Boolean(v.marker), Array.isArray(v.childNodes) ? v.childNodes.map(encode) : null];
    return ['$object', Object.keys(v).map(k => [k, encode(v[k])])];
}
function input(kind, value) { assert.ok(current, 'input outside case: ' + kind); current.inputs.push({ kind, value: encode(value) }); }
const emitterValues = [];
let expectedEmission = null;
function sourceContains(haystack, needle) { if (JSON.stringify(haystack) === JSON.stringify(needle))
    return true; if (Array.isArray(haystack))
    return haystack.some(x => sourceContains(x, needle)); if (haystack && typeof haystack === 'object')
    return Object.keys(haystack).some(k => sourceContains(haystack[k], needle)); return false; }
function startCase(name) { current = { name, inputs: [{ kind: 'case', value: encode(name) }], expected: [] }; scenarios.push(current); }
function recordExpected(kind, value) { assert.ok(current, 'expected value outside case: ' + kind); current.expected.push({ ordinal: assertions, kind, value: encode(value) }); }
let enabled = false, multiscriptEvents = [], calls = [], mode = '', target = null, late = null, base = null, old = null, serial = 0, changingOptions = null, created = [];
const note = s => { if (enabled)
    multiscriptEvents.push(s); };
const clone = v => Array.isArray(v) ? v.map(clone) : v && typeof v === 'object' ? Object.fromEntries(Object.keys(v).map(k => [k, clone(v[k])])) : v;
function reset() { enabled = false; multiscriptEvents = []; calls = []; mode = ''; target = late = base = old = changingOptions = null; serial = 0; created = []; }
function error(n, message, options, short) { note('error/' + message + '/' + Boolean(short)); if (mode === 'throw')
    throw Object.assign(new Error('witness'), { name: 'WitnessError' }); if (mode === 'fix-mutate') {
    note('error-options-alias/' + String(options === changingOptions));
    note('error-fix/' + String(options.fixMmultiscripts));
    options.fixMmultiscripts = true;
} return n; }
function receive(n, a, d, l, p) {
    note('inherit/' + n.label);
    calls.push({ label: n.label, a, snapshot: clone(a), d, l, p });
    if (n.label === 'base') {
        if (mode === 'rebind')
            target.childNodes = [n, late];
        if (mode === 'grow') {
            target.childNodes.push(late);
            mode = '';
        }
        if (mode === 'null-next')
            target.childNodes[1] = null;
    }
    if (n.label === 'sub' && mode === 'alias') {
        a.shared[1] = 'callback';
        a.added = 'visible';
    }
    if (n.label === 'sub' && mode === 'sub-rebind')
        target.childNodes = [base, n, late];
    if (n.label === 'sub' && mode === 'sub-grow') {
        target.childNodes.push(late);
        mode = '';
    }
}
class Record extends core.AbstractMmlNode {
    get kind() { note('kind/' + this.label); return this.marker ? 'mprescripts' : 'record'; }
    isKind(k) { note('isKind/' + this.label + '/' + k); if (k === 'mprescripts') {
        if (this.marker && mode === 'marker-rebind')
            target.childNodes = [base];
        if (this.label === 'sub' && mode === 'verify-rebind')
            target.childNodes = [base, this, late];
        if (this.label === 'sub' && mode === 'verify-grow') {
            target.childNodes.push(late);
            mode = '';
        }
        return Boolean(this.marker);
    } return super.isKind(k); }
    setInheritedAttributes(a, d, l, p) { receive(this, a, d, l, p); }
    verifyTree(o) { note('verify/' + this.label); }
    mError(m, o, s = false) { return error(this, m, o, s); }
}
class RecordingNone extends classes.none {
    setInheritedAttributes(a, d, l, p) { receive(this, a, d, l, p); }
    verifyTree(o) { note('verify/' + this.label); return super.verifyTree(o); }
    mError(m, o, s = false) { return error(this, m, o, s); }
}
class Probe extends classes.mmultiscripts {
    get kind() { note('kind/owner'); return 'mmultiscripts'; }
    hook(a, d, l, p) { input('inheritance-arguments', { a, d, l, p }); return super.setChildInheritedAttributes(a, d, l, p); }
    verify(o) { input('verify-options', o); return super.verifyChildren(o); }
    mError(m, o, s = false) { return error(this, m, o, s); }
    appendChild(c) { note('append/' + (c?.label || 'null')); const result = super.appendChild(c); if (mode === 'append-swap')
        this.childNodes = [late]; return result; }
}
const parentProbes = {};
for (const k of ['mprescripts', 'none'])
    parentProbes[k] = class extends classes[k] {
        verifyAttributes(o) { note('super-attributes'); return super.verifyAttributes(o); }
        verifyChildren(o) { note('super-children'); return super.verifyChildren(o); }
        mError(m, o, s = false) { return error(this, m, o, s); }
    };
class Factory extends MmlFactory {
    constructor() { super(); this.setNodeClass('probe', Probe); this.setNodeClass('record', Record); this.setNodeClass('none', RecordingNone); for (const k of ['mprescripts', 'none'])
        this.setNodeClass('parent-' + k, parentProbes[k]); }
    create(k, ...args) {
        if (enabled && k === 'none') {
            note('factory/none/' + args.length);
            if (mode === 'factory-null')
                return null;
            if (mode === 'factory-rebind')
                target.childNodes = [base, late];
        }
        input('factory-create', { kind: k, args });
        const n = super.create(k, ...args);
        if (k === 'none')
            n.label = 'none' + serial++;
        return n;
    }
}
class TraceFactory extends MmlFactory {
    create(k, ...a) { note('factory/' + k + '/' + a.length); input('trace-factory-create', { kind: k, args: a }); const n = super.create(k, ...a); if (enabled)
        created.push(n); return n; }
}
class InputFactory extends MmlFactory {
    create(k, ...a) { input('mml-factory-create', { kind: k, args: a }); return super.create(k, ...a); }
}
const createdValues = () => created.map(n => ({ kind: n.kind, explicit: clone(n.attributes?.getAllAttributes()) }));
function setup(shape) { input('setup-shape', shape); reset(); const f = new Factory(), n = f.create('probe'); target = n; const ns = []; for (let i = 0; i < shape.length; i++) {
    let s = f.create('record');
    s.label = i === 0 ? 'base' : shape[i] === 'p' ? 'mark' + i : i === 1 ? 'sub' : 'script' + i;
    s.marker = shape[i] === 'p';
    s.parent = n;
    ns.push(s);
} n.childNodes = ns; base = ns[0]; late = f.create('record'); late.label = 'late'; late.parent = n; old = ns; return { f, n, ns }; }
function rows() { return calls.map(c => ({ label: c.label, a: c.snapshot, d: c.d, l: c.l, p: c.p })); }
function shapeSnapshot(n) { if (!n)
    return null; return { kind: n.kind, label: n.label, parent: n.parent?.kind, properties: clone(n.properties), explicit: clone(n.attributes?.getAllAttributes()), inherited: clone(n.attributes?.getAllInherited()), children: n.childNodes?.map(shapeSnapshot) }; }
let count = 0, assertions = 0;
function begin(name) { reset(); count++; startCase(name); }
function end() { enabled = false; }
function pp(v) { assertions++; recordExpected('deep', v); }
function eq(v) { assertions++; recordExpected('equal', v); }
function observe(fn, expr) { input('callback-mode', mode); enabled = true; multiscriptEvents = []; let name = ''; try {
    fn();
}
catch (e) {
    name = e.name;
} enabled = false; {
} eq(name); pp(multiscriptEvents); }
begin('defaults_accessors_class_identity');
{
    const f = new InputFactory();
    for (const k of kinds) {
        const n = f.create(k), T = classes[k];
        for (const prop of ['kind', 'arity', 'notParent', 'linebreakContainer'])
            eq(n[prop]);
        pp(n.texClass);
        pp(T.defaults);
    }
    eq(classes.mmultiscripts.defaults === f.getNodeClass('msubsup').defaults);
    eq(classes.none.defaults === classes.mprescripts.defaults);
    eq(classes.none.defaults === core.AbstractMmlNode.defaults);
}
end();
const shapes = ['b', 'bs', 'bss', 'bsss', 'bp', 'bps', 'bpss', 'bsp', 'bsps', 'bssps', 'bpp', 'bspps', 'bsspsss'];
for (const shape of shapes) {
    begin('normalization_' + shape);
    const s = setup(shape), a = { shared: ['origin', 'before'] };
    observe(() => s.n.hook(a, true, 2, false));
    pp(rows());
    pp(shapeSnapshot(s.n));
    eq(s.n.childNodes === old);
    for (const c of calls)
        eq(c.a === a);
    end();
}
begin('raw_inheritance_matrix');
{
    const raw = [];
    for (const shape of ['bss', 'bsp', 'bsps', 'bsss'])
        for (const d of [false, true, 'raw', null])
            for (const l of [-1, 0, '2', null, NaN])
                for (const prime of [false, true, 0, 'raw', null, []]) {
                    const s = setup(shape), a = {};
                    s.n.hook(a, d, l, prime);
                    raw.push({ shape, d, l, prime, rows: rows() });
                }
    assert.equal(raw.length, 480);
    assert.equal(new Set(raw.map(r => r.shape)).size, 4);
    assert.equal(new Set(raw.map(r => JSON.stringify(encode(r.d)))).size, 4);
    assert.equal(new Set(raw.map(r => JSON.stringify(encode(r.l)))).size, 5);
    assert.equal(new Set(raw.map(r => JSON.stringify(encode(r.prime)))).size, 6);
    assert.equal(new Set(raw.map(r => JSON.stringify(encode([r.shape, r.d, r.l, r.prime])))).size, 480);
    for (const row of raw)
        assert.deepEqual(Object.keys(row), ['shape', 'd', 'l', 'prime', 'rows']);
    input('raw-inheritance-matrix', raw.map(({ shape, d, l, prime, rows }) => ({ shape, d, l, prime, rows })));
    for (const row of raw) {
        assertions++;
        recordExpected('matrix-row', row.rows);
    }
}
end();
for (const [shape, change] of [['bss', 'rebind'], ['bss', 'grow'], ['bssss', 'sub-rebind'], ['bss', 'sub-grow'], ['bs', 'alias'], ['bsp', 'marker-rebind'], ['bsp', 'factory-rebind'], ['bs', 'append-swap'], ['bsp', 'factory-null'], ['bs', 'factory-null'], ['bss', 'null-next'], ['', ''], ['bs', 'null-array']]) {
    begin('callback_' + count);
    const s = setup(shape), a = { shared: ['origin', 'before'] };
    mode = change;
    if (change === 'null-array')
        s.n.childNodes = null;
    observe(() => s.n.hook(a, false, '2', 'raw'));
    pp(rows());
    pp(a);
    pp(shapeSnapshot(s.n));
    eq(s.n.childNodes === old);
    end();
}
for (const [shape, change] of [['bssss', 'verify-rebind'], ['bs', 'verify-grow']]) {
    begin(change);
    const s = setup(shape), o = { fixMmultiscripts: true };
    mode = change;
    observe(() => s.n.verify(o));
    pp(shapeSnapshot(s.n));
    end();
}
for (const shape of ['b', 'bs', 'bss', 'bsp', 'bpp', 'bspps', 'bssps'])
    for (const fix of [undefined, false, true, 'raw', 0, []])
        for (const change of ['', 'throw']) {
            begin('verify_flow_' + count);
            const s = setup(shape), o = { fixMmultiscripts: fix };
            mode = change;
            observe(() => s.n.verify(o));
            pp(shapeSnapshot(s.n));
            end();
        }
// Dedicated returning-error witness changes the options handle during the scan.
begin('captured_fix_mutated_after_first_error');
{
    const s = setup('bspps'), o = { fixMmultiscripts: false };
    mode = 'fix-mutate';
    changingOptions = o;
    observe(() => s.n.verify(o));
    pp(o);
}
end();
for (const k of ['mprescripts', 'none'])
    for (const parent of [null, 'mrow', 'mmultiscripts', 'inferredMrow'])
        for (const opts of [null, {}, { checkArity: true }, { checkAttributes: true }])
            for (const withChild of [false, true]) {
                begin('parent_verification_' + count);
                const f = new Factory(), n = f.create('parent-' + k, { bad: 'value' }, withChild ? [f.create('record')] : []);
                if (withChild)
                    n.childNodes[0].label = 'child';
                input('parent-context', { node: k, parent, logicalParent: parent === 'inferredMrow' ? 'mmultiscripts' : null });
                if (parent)
                    n.parent = f.create(parent);
                if (parent === 'inferredMrow')
                    n.parent.parent = f.create('mmultiscripts');
                input('verify-tree-options', opts);
                observe(() => n.verifyTree(opts));
                pp(shapeSnapshot(n));
                end();
            }
for (const k of ['mprescripts', 'none'])
    for (const parent of ['mrow', 'mmultiscripts', 'merror', 'inferredMrow'])
        for (const opts of [null, { fullErrors: false }, { fullErrors: true }]) {
            begin('real_parent_partial_' + count);
            const f = new TraceFactory(), n = f.create(k), holder = f.create(parent, {}, [n]);
            n.parent = holder;
            input('parent-context', { node: k, parent, logicalParent: parent === 'inferredMrow' ? 'mmultiscripts' : null });
            if (parent === 'inferredMrow')
                holder.parent = f.create('mmultiscripts');
            input('verify-tree-options', opts);
            observe(() => n.verifyTree(opts));
            pp(shapeSnapshot(holder));
            pp(createdValues());
            end();
        }
// Real source error-tree construction, distinct from returning-error probes.
for (const shape of ['b', 'bs', 'bsp', 'bpp', 'bspps'])
    for (const fix of [false, true])
        for (const full of [false, true]) {
            begin('real_errors_' + count);
            const f = new InputFactory(), ns = [...shape].map(c => f.create(c === 'p' ? 'mprescripts' : 'mi')), n = f.create('mmultiscripts', {}, ns), holder = f.create('mrow', {}, [n]), o = { fixMmultiscripts: fix, fullErrors: full };
            input('verify-tree-options', o);
            observe(() => n.verifyTree(o));
            pp(shapeSnapshot(holder));
            end();
        }
for (const shape of ['bss', 'bsp', 'bsps']) {
    begin('real_outer_inheritance_' + shape);
    const f = new InputFactory(), ns = [...shape].map(c => f.create(c === 'p' ? 'mprescripts' : 'mi')), n = f.create('mmultiscripts', {}, ns);
    input('outer-inheritance-arguments', { attributes: { mathcolor: ['math', 'blue'] }, display: true, level: 0, prime: false });
    observe(() => n.setInheritedAttributes({ mathcolor: ['math', 'blue'] }, true, 0, false));
    pp(shapeSnapshot(n));
    end();
}

assert.equal(count,226);assert.equal(assertions,1304);assert.equal(scenarios.length,226);const ordered=scenarios.flatMap(s=>s.expected);assert.equal(ordered.length,1304);for(let i=0;i<ordered.length;i++)assert.equal(ordered[i].ordinal,i+1);
const profile={schema:'folia.math.multiscript-source.v1',source:{node:process.version,package:'@mathjax/src',version:'4.1.3',typescript:'5.9.3',module:{path:'ts/core/MmlTree/MmlNodes/mmultiscripts.ts',sha256:'bb8d8d2180ef483b86b0b1570b919ba424ed54db1f30ef6e0e697cfab0bf5cf7'},locks:{p12bPath:'docs/math-port/evidence/p12b-source-lock.json',p12bSha256:'e7ea07818f7d6aa1d1dda7c2cc1c5cb32e68596f0b84a1801351485624eaa369',moRuntime:'cjs/core/MmlTree/MmlNodes/mo.js',moRuntimeSha256:'7c68f0570d82f585eb6ca7e8bbca119b96a02666e0d4b268a413bf4035ff0b2c',compiledPackageFiles:424,compiledPackageSha256:'773bf3ae1c075baa36c02b6958f7e3bed5fb116d0818ecf99bb70c5337d9e537'},loader:{multiscriptReferenceSha256:'1c3063d12886bfa1e3593b5b669370631c81e4fd76aea0ffff385469bb22f212',arithmeticReferenceSha256:'a3691e9fcba94abbe89397fff6f90ca2d5f29cead0ef58d334cce9cdc1251458'}},migration:{legacyOracleSha256:'f3e9eb1d8f486747f7a9841418b336ac1dc1900fbe46123520a3aec97ccd3d16',legacyFixtureSha256:'c6b4ad31c98fe0e9c3956264c255bbe8e5fd4abc511ea7dea79523ea169dc914'},counts:{sourceCases:226,sourceAssertions:1304,staticExpectSites:825,rawMatrixRows:480,sourceValueRecords:1170,nativeOnlyCases:0},scenarios};
assert.ok(process.argv.includes('--check'),'Source-only oracle is check-only');
assert.deepEqual(JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/multiscript-source-profile.json'),'utf8')),profile);
console.log(JSON.stringify({...profile.counts,mode:'check'}));

// Independently frozen original parent-verification projection (88 cases / 288 values).
const parentProfile=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/multiscript-parent-source-profile.json'),'utf8'));
const parentNames=new Set(parentProfile.scenarios.map(s=>s.name));
const parentScenarios=profile.scenarios.filter(s=>parentNames.has(s.name));
assert.equal(parentScenarios.length,88);
assert.equal(parentScenarios.reduce((n,s)=>n+s.expected.length,0),288);
assert.deepStrictEqual(parentScenarios,parentProfile.scenarios);
