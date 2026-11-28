'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {root,MmlFactory,kinds,classes,hashes,core}=require('./simple-mml-reference.cjs');
function buildProfile(){

const scenarios=[];let current=null;
function encode(v){
 if(v===undefined)return ['$undefined'];
 if(v===null||typeof v==='string'||typeof v==='boolean')return v;
 if(typeof v==='number'){if(Number.isNaN(v))return ['$number','NaN'];if(v===Infinity)return ['$number','Infinity'];if(v===-Infinity)return ['$number','-Infinity'];if(Object.is(v,-0))return ['$number','-0'];return v;}
 if(Array.isArray(v))return ['$array',v.length,Object.keys(v).map(k=>[k,encode(v[k])])];
 assert.equal(typeof v,'object');return ['$object',Object.keys(v).map(k=>[k,encode(v[k])])];
}
function record(kind,value){assert.ok(current);current.observations.push({kind,expected:encode(value)});}
function input(value){record('input',value);}
function matrix(rows){record('matrix',rows);}
function startCase(name){current={name,input:{scenario:name,dimensions:name.split('_')},observations:[]};scenarios.push(current);}
function finish(){
 assert.equal(cases,106);assert.equal(rawRows,1605);assert.equal(assertions,7667);
 const counts={scenarios:scenarios.length,sourceAssertions:assertions,matrixRows:rawRows,equal:0,deep:0,input:0,call:0,matrix:0};
 for(const s of scenarios)for(const o of s.observations)++counts[o.kind];
 const runtime=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/locks/p12b-source-lock.json'),'utf8'));
 const profile={schema:'folia.math.simple-mml-source.v1',source:{node:process.version,package:'@mathjax/src',version:'4.1.3',pins:hashes,runtime},migration:{originalToolSha256:'25391161b4b90826d3f1c26c30e390eaa85f6f64ea336885467154e1be176308',frozenFixtureSha256:'6d585447047dc2e82addf1d6bb39a2483e89f9bedf07dccbbe463dc5c1e17849',note:'The one-time migration checks the complete original C++ fixture, every ordered Source scenario/observation, all matrix cells and all 296 recursive legacy value records. No native results generate this profile.'},counts,scenarios};
 return {profile,counts};
}
let enabled=false,phase=false,mode='',events=[],calls=[],target=null,late=null,returned=null,phaseBag=null,merged=null,errorOptions=null;
const note=s=>{if(enabled)events.push(s);},clone=v=>Array.isArray(v)?v.map(clone):v&&typeof v==='object'?Object.fromEntries(Object.keys(v).map(k=>[k,clone(v[k])])):v;
function fail(){throw Object.assign(new Error('witness'),{name:'WitnessError'});}function reset(){enabled=phase=false;mode='';events=[];calls=[];target=late=returned=phaseBag=merged=errorOptions=null;}
class Record extends core.AbstractMmlNode{get kind(){return 'record';}setTeXclass(prev){note('child/'+this.label+'/'+(prev?'prev':'null'));if(mode==='child-throw')fail();if(mode==='child-rebind')target.childNodes=[late];if(mode==='child-empty')target.childNodes=[];if(mode==='child-null-array')target.childNodes=null;return mode==='child-return-prev'?prev:returned;}
 setInheritedAttributes(a,d,l,p){note('inherit/'+this.label);calls.push({a,d,l,p});if(mode==='inherit-mutate')a.seen=['record',this.label];if(mode==='inherit-rebind')target.childNodes=[late];if(mode==='inherit-append'&&this.label==='first')target.childNodes.push(late);if(mode==='inherit-throw')fail();}}
function proxy(T){return class extends T{get attributes(){note('attributes');return this.savedAttrs;}set attributes(v){this.savedAttrs=v;}get texClass(){note('tex.read');return super.texClass;}set texClass(v){note('tex.write');super.texClass=v;if(enabled&&mode==='class-rebind')this.childNodes=[late];}get prevClass(){note('prev.read');return this.savedPrev;}set prevClass(v){note('prev.write');this.savedPrev=v;}get prevLevel(){note('level.read');return this.savedLevel;}set prevLevel(v){note('level.write');this.savedLevel=v;}getProperty(k){note('get/'+k);const v=super.getProperty(k);if(enabled&&mode==='get-rebind')this.childNodes=[late];return v;}
 getPrevClass(prev){note('getPrev/'+(prev?'prev':'null'));if(mode==='prev-throw')fail();super.getPrevClass(prev);if(mode==='prev-rebind')this.childNodes=[late];}
 updateTeXclass(child){note('update/'+(child?child.label||child.kind:'none'));if(mode==='update-throw')fail();super.updateTeXclass(child);}
 addInheritedAttributes(a,b){note('merge');if(mode==='merge-throw')fail();if(mode==='merge-rebind')this.childNodes=[late];if(mode==='merge-mutate')a.extra=['origin','raw'];merged=mode==='merge-return-input'?a:super.addInheritedAttributes(a,b);return merged;}
 mError(message,opts,short=false){note('error/'+message+'/'+short);errorOptions=opts;if(mode==='error-throw')fail();opts.observed=true;return returned;}
 setChildren(c){if(phase){note('phase/'+String(this.texclass));phaseBag=this.properties;}return super.setChildren(c);}
 hook(a,d,l,p){return super.setChildInheritedAttributes(a,d,l,p);}
};}
const probes={};for(const k of kinds)probes[k]=proxy(classes[k]);
function factory(){const f=new MmlFactory();f.setNodeClass('record',Record);for(const k of kinds)f.setNodeClass('probe-'+k,probes[k]);return f;}
function setup(k){reset();const f=factory(),n=f.create('probe-'+k),a=f.create('record'),b=f.create('record'),prev=f.create('record');a.label='first';b.label='late';prev.label='previous';a.texClass=2;a.prevClass='first-prev';a.prevLevel='first-level';b.texClass=5;b.prevClass='late-prev';b.prevLevel='late-level';prev.texClass=1;prev.attributes.set('scriptlevel','2');n.childNodes=[a];a.parent=n;b.parent=n;target=n;late=returned=b;return{f,n,a,b,prev};}
function state(n){return{tex:n.texClass,prev:n.prevClass,level:n.prevLevel,properties:clone(n.properties),children:n.childNodes?.map(c=>c?.label||c?.kind)};}
function tree(n){if(!n)return null;return{kind:n.kind,text:n instanceof core.TextNode?n.getText():undefined,properties:clone(n.properties),tex:n.texClass,attributes:clone(n.attributes?.getAllAttributes()),inherited:clone(n.attributes?.getAllInherited()),children:n.childNodes?.map(tree)};}

let cases=0,assertions=0,rawRows=0;
function begin(name){reset();++cases;startCase(name);}
function end(){enabled=false;current=null;}
function eq(value){++assertions;record('equal',value);}
function prop(value){++assertions;record('deep',value);}
function observe(fn){enabled=true;events=[];let error='';try{fn();}catch(e){error=e.name;}enabled=false;record('call',{error,events});eq(error);prop(events);}
begin('defaults_getters_fields');
{
    const f = factory();
    for (const k of kinds) {
        const n = f.create(k);
        for (const key of ['kind', 'arity', 'isToken', 'isEmpty', 'isSpacelike', 'linebreakContainer', 'notParent'])
            eq(n[key]);
        prop(n.texClass);
        prop(classes[k].defaults);
        eq(classes[k].defaults === Object.getPrototypeOf(classes[k]).defaults);
        if (n.arity < 0) {
            eq(n.childNodes[0].isInferred);
        }
    }
}
end();
for (const k of kinds) {
    begin('constructor_' + k);
    const f = factory();
    phase = true;
    let n;
    observe(() => n = f.create('probe-' + k));
    phase = false;
    prop(state(n));
    eq(phaseBag === n.properties);
    end();
}
const vboxes = [undefined, null, false, true, -1, -0, 0, 1, 2, NaN, Infinity, '', '0', 'false', [], [0], {}, { value: false }];
for (const k of ['menclose', 'mpadded'])
    for (const change of ['', 'child-rebind', 'child-empty', 'child-null-array', 'child-throw', 'child-return-prev', 'get-rebind', 'prev-rebind', 'prev-throw', 'class-rebind', 'update-throw', 'empty', 'null-child', 'null-array'])
        for (const previous of [false, true]) {
            begin('class_' + k + '_' + (change || 'normal') + '_' + previous);
            const s = setup(k);
            mode = change;
            if (k === 'mpadded')
                s.n.setProperty('vbox', true);
            if (change === 'empty')
                s.n.childNodes = [];
            if (change === 'null-child')
                s.n.childNodes = [null];
            if (change === 'null-array')
                s.n.childNodes = null;
            let result = null;
            observe(() => result = s.n.setTeXclass(previous ? s.prev : null));
            eq(result === s.n);
            eq(result === s.b);
            eq(result === (previous ? s.prev : null));
            prop(state(s.n));
            prop(state(s.a));
            prop(state(s.b));
            end();
        }
begin('raw_vbox_matrix');
{
    const rows = [];
    for (const v of vboxes) {
        const s = setup('mpadded');
        s.n.setProperty('vbox', v);
        let result = null, error = '';
        try {
            result = s.n.setTeXclass(s.prev);
        }
        catch (e) {
            error = e.name;
        }
        rows.push({ input: v, error, self: result === s.n, returned: result === s.b, n: state(s.n), a: state(s.a) });
    }
    matrix(rows);
    assertions += rows.length * 5;
    rawRows += rows.length;
}
end();
const glyphInputs = [undefined, null, false, true, 0, 1, '', 'valid', [], [''], {}];
begin('raw_glyph_attribute_matrix');
{
    const rows = [];
    for (const src of glyphInputs)
        for (const family of glyphInputs)
            for (const index of glyphInputs) {
                const s = setup('mglyph'), opts = { checkAttributes: true, checkMathvariants: true };
                s.n.attributes.setList({ src, fontfamily: family, index, unknown: 'raw', mathvariant: 'bogus' });
                enabled = true;
                events = [];
                let error = '';
                try {
                    s.n.verifyAttributes(opts);
                }
                catch (e) {
                    error = e.name;
                }
                enabled = false;
                rows.push({ src, family, index, error, trace: clone(events), opts: clone(opts), alias: errorOptions === opts });
            }
    matrix(rows);
    assertions += rows.length * 4;
    rawRows += rows.length;
}
end();
for (const change of ['error-throw', 'attributes-null', 'valid-no-checks', 'invalid-no-checks']) {
    begin('glyph_' + change);
    const s = setup('mglyph'), opts = { checkAttributes: false, checkMathvariants: false };
    mode = change;
    if (change === 'attributes-null')
        s.n.attributes = null;
    else
        s.n.attributes.setList({ src: change === 'valid-no-checks' ? 'image' : '', fontfamily: '', index: '' });
    observe(() => s.n.verifyAttributes(opts));
    prop(opts);
    end();
}
for (const change of ['defaults', 'inherited-src', 'inherited-family-index', 'inherited-family-empty-index', 'explicit-empty-src', 'explicit-missing-src', 'explicit-family-empty', 'explicit-index-empty']) {
    begin('glyph_lookup_' + change);
    const s = setup('mglyph'), opts = { checkAttributes: true, checkMathvariants: true };
    let inherited = {}, explicit = {};
    if (change === 'inherited-src')
        inherited = { src: 'image' };
    if (change.includes('family'))
        inherited = { fontfamily: 'font', index: '42' };
    if (change === 'inherited-family-empty-index')
        inherited.index = '';
    if (change === 'explicit-empty-src' || change === 'explicit-missing-src') {
        inherited = { src: 'image' };
        explicit = { src: change === 'explicit-empty-src' ? '' : undefined };
    }
    if (change === 'explicit-family-empty')
        explicit = { fontfamily: '' };
    if (change === 'explicit-index-empty') {
        inherited = { fontfamily: 'font', index: '42' };
        explicit = { index: '' };
    }
    for (const [k, v] of Object.entries(inherited))
        s.n.attributes.setInherited(k, v);
    s.n.attributes.setList(explicit);
    for (const [k, v] of Object.entries(inherited))
        input(v);
    input(explicit);
    prop(s.n.attributes.getList('src', 'fontfamily', 'index'));
    observe(() => s.n.verifyAttributes(opts));
    prop(opts);
    eq(errorOptions === opts);
    end();
}
const inh = [undefined, null, false, true, -1, -0, 0, 1, 2, NaN, Infinity, '', '2', [], [2], {}];
for (const change of ['', 'merge-return-input', 'merge-mutate', 'merge-rebind', 'merge-throw', 'inherit-rebind', 'inherit-append', 'inherit-mutate', 'inherit-throw', 'attributes-null', 'null-child', 'null-array']) {
    begin('group_hook_' + (change || 'normal'));
    const s = setup('maligngroup'), a = { color: ['ancestor', 'red'] };
    s.n.childNodes = [s.a, s.b];
    s.n.attributes.setList({ groupalign: 'right', mathcolor: 'blue', style: 'color:red', displaystyle: true, scriptlevel: '2' });
    mode = change;
    if (change === 'attributes-null')
        s.n.attributes = null;
    if (change === 'null-child')
        s.n.childNodes = [null];
    if (change === 'null-array')
        s.n.childNodes = null;
    input(a);
    observe(() => s.n.hook(a, 'display', '2', 'prime'));
    eq(calls.length);
    for (const [i, c] of calls.entries()) {
        prop({ a: c.a, d: c.d, l: c.l, p: c.p });
        eq(c.a === a);
        eq(c.a === merged);
    }
    prop(a);
    end();
}
begin('raw_group_inheritance_matrix');
{
    const rows = [];
    for (const d of inh)
        for (const l of inh) {
            const s = setup('maligngroup'), a = { shared: ['ancestor', 'value'] };
            s.n.hook(a, d, l, 'prime');
            rows.push({ d, l, a: calls[0].a, display: calls[0].d, level: calls[0].l, prime: calls[0].p, alias: calls[0].a === a });
        }
    matrix(rows);
    assertions += rows.length * 6;
    rawRows += rows.length;
}
end();
for (const k of kinds)
    for (const size of [0, 1, 2]) {
        begin('real_outer_' + k + '_' + size);
        const f = factory(), cs = Array.from({ length: size }, () => f.create('mi', {}, [f.create('text').setText('x')])), n = f.create(k, { mathcolor: 'blue' }, cs), parent = f.create('mrow', {}, [n]);
        observe(() => n.setInheritedAttributes({ mathsize: ['math', 'large'] }, true, '2', true));
        prop(tree(parent));
        end();
    }
for (const full of [false, true]) {
    begin('real_glyph_error_' + full);
    const f = factory(), n = f.create('mglyph'), parent = f.create('mrow', {}, [n]);
    n.attributes.setList({ src: '', fontfamily: '', index: '' });
    observe(() => n.verifyAttributes({ fullErrors: full }));
    prop(tree(parent));
    end();
}
return finish();
}
if(require.main===module)process.stdout.write(JSON.stringify(buildProfile().profile)+'\n');
module.exports={buildProfile};
