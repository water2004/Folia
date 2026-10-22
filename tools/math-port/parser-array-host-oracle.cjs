'use strict';
// Independent observations of the ordinary Array methods needed by the
// enabled MathJax TeX package. This oracle executes native Node Array methods.
const fs = require('node:fs'), path = require('node:path'), crypto = require('node:crypto'), assert = require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const source = 'ts/input/tex/Configuration.ts';
const scope = JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/scope.json'),'utf8'));
const scoped = scope.files.find(file => file.source === source);
assert.ok(scoped);
assert.equal(crypto.createHash('sha256').update(fs.readFileSync(path.join(root,'.reference/MathJax-src',source))).digest('hex'),scoped.source_sha256);
const rows = [];
const describe = value => {
  if (value === undefined) return 'U';
  if (value === null) return 'N';
  if (typeof value === 'number' && Number.isNaN(value)) return 'NaN';
  if (typeof value === 'symbol') return 'Symbol';
  if (Array.isArray(value)) return '['+Array.from({length:value.length},(_,i)=>{
    const descriptor=Object.getOwnPropertyDescriptor(value,String(i));
    return !descriptor?'H':('get'in descriptor||'set'in descriptor)?'A':describe(descriptor.value);
  }).join(',')+']';
  if (typeof value === 'object') return 'Object';
  return String(value);
};
function observe(name,run) {
  const trace=[]; let result='U', error='ok';
  try { result=run(trace); } catch(e) { error=e.name; }
  rows.push({name,expected:error+'|'+describe(result)+'|'+trace.join('/')});
}
function metadata(name) {
  const d=Object.getOwnPropertyDescriptor(Array.prototype,name);
  return Array.prototype[name].length+':'+d.enumerable+':'+d.writable+':'+d.configurable;
}
for (const name of ['slice','reverse','includes','push','pop','join','fill','find']) observe('metadata:'+name,()=>metadata(name));
observe('method:current-own-override-is-called',trace=>{const a=[1];a.find=function(){trace.push('override:'+(this===a)+':'+arguments.length);return 'result';};return a.find(()=>false,7);});
observe('slice:negative-and-clamped',()=>[0,1,2,3].slice(-9,8));
observe('slice:holes-preserved',()=>{const a=[];a.length=4;a[1]='x';return a.slice(0);});
observe('slice:constructor-get-after-index-conversions',trace=>{const a=[1];Object.defineProperty(a,'constructor',{get(){trace.push('constructor');return Array;}});return a.slice(0,1);});
observe('slice:constructor-undefined-defaults',()=>{const a=[1];a.constructor=undefined;return a.slice();});
for (const [name,value] of [['null',null],['false',false],['zero',0]]) observe('slice:primitive-constructor:'+name,()=>{const a=[1];a.constructor=value;return a.slice();});
observe('slice:object-constructor-defaults-nullish-species',()=>{const a=[1];a.constructor={[Symbol.species]:null};return a.slice();});
observe('slice:function-constructor-defaults-missing-species',()=>{const a=[1];a.constructor=function C(){};return a.slice();});
observe('slice:custom-species-function',()=>{const a=[1];a.constructor={[Symbol.species]:function Species(n){this.length=n;}};return a.slice();});
observe('slice:species-getter-after-conversions',trace=>{const a=[1];const start={[Symbol.toPrimitive](){trace.push('start');return 0;}};function C(){};Object.defineProperty(C,Symbol.species,{get(){trace.push('species');return null;}});a.constructor=C;return a.slice(start);});
observe('slice:start-coercion-before-end',trace=>{const a=[1,2],start={[Symbol.toPrimitive](){trace.push('start');return 0;}},end={[Symbol.toPrimitive](){trace.push('end');return 1;}};return a.slice(start,end);});
observe('slice:index-get-after-conversions-and-species',trace=>{const a=[1];const start={[Symbol.toPrimitive](){trace.push('start');return 0;}};function C(){};Object.defineProperty(C,Symbol.species,{get(){trace.push('species');return null;}});a.constructor=C;Object.defineProperty(a,'0',{get(){trace.push('get0');return 1;}});return a.slice(start);});
observe('slice:numeric-bounds-constructor-species-index-order',trace=>{const a=[1];function C(){};Object.defineProperty(a,'constructor',{get(){trace.push('constructor');return C;}});Object.defineProperty(C,Symbol.species,{get(){trace.push('species');return null;}});Object.defineProperty(a,'0',{get(){trace.push('get0');return 1;}});return a.slice(0,1);});
observe('slice:defines-own-result-without-array-prototype-setter',trace=>{const old=Object.getOwnPropertyDescriptor(Array.prototype,'0');Object.defineProperty(Array.prototype,'0',{set(){trace.push('setter');},configurable:true});try{const out=[4].slice();const d=Object.getOwnPropertyDescriptor(out,'0');return [d.value,d.writable,d.enumerable,d.configurable,Object.hasOwn(out,0)];}finally{if(old)Object.defineProperty(Array.prototype,'0',old);else delete Array.prototype[0];}});
observe('reverse:both-holes',()=>{const a=[];a.length=4;a[1]='b';return a.reverse();});
observe('reverse:lower-only-hole-move',()=>{const a=[];a.length=2;a[0]='a';return a.reverse();});
observe('reverse:upper-only-hole-move',()=>{const a=[];a.length=2;a[1]='b';return a.reverse();});
observe('reverse:has-get-and-set-order',trace=>{const a=[1,2];Object.defineProperty(a,'0',{get(){trace.push('get0');return 1;},set(v){trace.push('set0:'+v);},configurable:true});Object.defineProperty(a,'1',{get(){trace.push('get1');return 2;},set(v){trace.push('set1:'+v);},configurable:true});a.reverse();return a;});
observe('reverse:lower-get-mutates-upper-before-upper-has',trace=>{const a=[1,2];Object.defineProperty(a,'0',{get(){trace.push('get0');delete a[1];return 1;},configurable:true});a.reverse();return a;});
observe('reverse:lower-set-throws-before-upper-set',trace=>{const a=[1,2];Object.defineProperty(a,'0',{set(){trace.push('set0');throw Error('setter');},configurable:true});try{a.reverse();}catch(e){trace.push(e.message);}return a;});
observe('includes:hole-matches-undefined',()=>{const a=[];a.length=2;return a.includes(undefined);});
observe('includes:zero-length-skips-symbol-from-index-conversion',()=>{const a=[];return a.includes('missing',Symbol('offset'));});
observe('includes:NaN-is-same-value-zero',()=>[NaN].includes(NaN));
observe('includes:from-index-string-coercion',trace=>{const from={[Symbol.toPrimitive](){trace.push('from');return '1';}};return [0,1,2].includes(2,from);});
observe('includes:primitive-string-from-index',()=>[0,1,2].includes(0,'1'));
for (const [name,index] of [['positive-infinity',Infinity],['negative-infinity',-Infinity],['fractional-negative',-1.5],['NaN',NaN]]) observe('includes:from-index:'+name,()=>[1,2,3].includes(1,index));
observe('includes:inherited-hole-get',trace=>{const p=Object.create(Array.prototype);Object.defineProperty(p,'0',{get(){trace.push('get0');return 7;},configurable:true});const a=[];a.length=1;Object.setPrototypeOf(a,p);return a.includes(7);});
observe('push:variadic-return-and-identity',()=>{const a=[1],x={};const n=a.push(2,x);return [n,a[2]===x,a];});
observe('push:sequential-set-partial-mutation',trace=>{const p=Object.create(Array.prototype);Object.defineProperty(p,'1',{set(){trace.push('set1');throw Error('setter');},configurable:true});const a=[];Object.setPrototypeOf(a,p);try{a.push(3,4,5);}catch(e){trace.push(e.message);}return a;});
observe('push:nonwritable-length-zero-args',trace=>{const a=[];Object.defineProperty(a,'length',{writable:false});try{return a.push();}catch(e){trace.push(e.name);return a.length;}});
observe('push:max-length-range-error-partial-write',trace=>{const a=[];a.length=0xFFFFFFFF;try{a.push(1);}catch(e){trace.push(e.name);}return [a.length,a[0xFFFFFFFF]];});
observe('pop:hole-reads-inherited-value',()=>{const p=Object.create(Array.prototype);p[0]=9;const a=[];a.length=1;Object.setPrototypeOf(a,p);const value=a.pop();return [value,a.length,Object.hasOwn(a,0)];});
observe('pop:get-delete-length-order',trace=>{const a=[4];Object.defineProperty(a,'0',{get(){trace.push('get');return 4;},configurable:true});const result=a.pop();return [result,a.length,Object.hasOwn(a,0)];});
observe('pop:delete-fails-before-length-write',trace=>{const a=[4];Object.defineProperty(a,'0',{value:4,configurable:false});try{a.pop();}catch(e){trace.push(e.name);}return [a.length,a[0]];});
observe('pop:deletes-configurable-tail-before-readonly-length-fails',trace=>{const a=[4];Object.defineProperty(a,'0',{value:4,configurable:true});Object.defineProperty(a,'length',{writable:false});try{a.pop();}catch(e){trace.push(e.name);}return [a.length,Object.hasOwn(a,0)];});
observe('pop:zero-length-still-writes-length',trace=>{const a=[];Object.defineProperty(a,'length',{writable:false});try{a.pop();}catch(e){trace.push(e.name);}return a.length;});
observe('join:nullish-empty-and-default-separator',()=>[null,undefined,0,false,'x'].join());
observe('join:holes-and-captured-length-with-primitive-separator',trace=>{const p=Object.create(Array.prototype);Object.defineProperty(p,'0',{get(){trace.push('get0');return 'p';},configurable:true});const a=[];a.length=3;a[1]=1;Object.setPrototypeOf(a,p);Object.defineProperty(a,'1',{get(){trace.push('get1');a.push(2);return 1;},configurable:true});return a.join('|');});
observe('join:separator-coerced-before-elements',trace=>{const a=[1,2],sep={[Symbol.toPrimitive](){trace.push('sep');return '|';}};Object.defineProperty(a,'0',{get(){trace.push('get0');return 1;},configurable:true});return a.join(sep);});
observe('join:symbol-element-type-error',()=>[Symbol('x')].join());
observe('join:symbol-separator-type-error',()=>[1].join(Symbol('x')));
observe('join:object-coercion',trace=>[{[Symbol.toPrimitive](){trace.push('object');return 'v';}}].join());
observe('join:nullish-elements-skip-object-conversion',trace=>[null,undefined].join({[Symbol.toPrimitive](){trace.push('separator');return '|';}}));
observe('fill:range-conversion-order',trace=>{const a=[0,0],start={[Symbol.toPrimitive](){trace.push('start');return 0;}},end={[Symbol.toPrimitive](){trace.push('end');return 1;}};return a.fill(8,start,end);});
for(const[name,start,end]of [['negative-fraction',-1.5,undefined],['NaN',NaN,undefined],['positive-infinity',Infinity,undefined],['negative-infinity',-Infinity,2]])observe('fill:range:'+name,()=>[0,0,0].fill(7,start,end));
observe('fill:sequential-set-partial-mutation',trace=>{const a=[0,0,0];Object.defineProperty(a,'1',{set(){trace.push('set1');throw Error('setter');},configurable:true});try{a.fill(7);}catch(e){trace.push(e.message);}return a;});
observe('fill:inherited-setter-stops-before-next-slot',trace=>{const p=Object.create(Array.prototype);Object.defineProperty(p,'1',{set(){trace.push('set1');throw Error('setter');},configurable:true});const a=[];a.length=3;Object.setPrototypeOf(a,p);try{a.fill(7);}catch(e){trace.push(e.message);}return a;});
observe('find:visits-holes-as-undefined',trace=>{const a=[];a.length=2;a[1]=5;return a.find(function(v,i,array){trace.push(i+':'+describe(v)+':'+arguments.length+':'+(array===a)+':'+(this===undefined));return i===0;});});
observe('find:callback-receiver-and-three-args',trace=>{const a=[1],receiver={};return a.find(function(v,i,array){trace.push(i+':'+arguments.length+':'+(this===receiver)+':'+(array===a));return true;},receiver);});
observe('find:captured-length-live-values',trace=>{const a=[1,2];return a.find((v,i)=>{trace.push(i+':'+v);if(i===0){a[1]=9;a.push(3);}return v===9;});});
observe('find:shrink-length-still-visits-captured-indices',trace=>{const a=[1,2,3];return a.find((v,i)=>{trace.push(i+':'+v);if(i===0)a.length=1;return false;});});
observe('find:inherited-hole-getter',trace=>{const p=Object.create(Array.prototype);Object.defineProperty(p,'0',{get(){trace.push('get0');return 4;},configurable:true});const a=[];a.length=1;Object.setPrototypeOf(a,p);return a.find(v=>v===4);});
observe('find:invalid-callback-before-index-get',trace=>{const a=[];a.length=1;Object.defineProperty(a,'0',{get(){trace.push('get');return 1;}});return a.find(null);});
const nativeCoverage = {
  'metadata:slice':['source_rows_0_to_7_method_metadata','matched'],
  'metadata:reverse':['source_rows_0_to_7_method_metadata','matched'],
  'metadata:includes':['source_rows_0_to_7_method_metadata','matched'],
  'metadata:push':['source_rows_0_to_7_method_metadata','matched'],
  'metadata:pop':['source_rows_0_to_7_method_metadata','matched'],
  'metadata:join':['source_rows_0_to_7_method_metadata','matched'],
  'metadata:fill':['source_rows_0_to_7_method_metadata','matched'],
  'metadata:find':['source_rows_0_to_7_method_metadata','matched'],
  'method:current-own-override-is-called':['current_method_override_is_called','matched'],
  'slice:negative-and-clamped':['slice_negative_and_clamped_bounds','matched'],
  'slice:holes-preserved':['slice_holes_and_length','matched'],
  'slice:constructor-get-after-index-conversions':['slice_constructor_get_precedes_index_get','matched'],
  'slice:constructor-undefined-defaults':['slice_constructor_undefined_defaults','matched'],
  'slice:primitive-constructor:null':['slice_primitive_constructor_throws','matched'],
  'slice:primitive-constructor:false':['slice_false_and_zero_constructors_throw','matched'],
  'slice:primitive-constructor:zero':['slice_false_and_zero_constructors_throw','matched'],
  'slice:object-constructor-defaults-nullish-species':['slice_object_constructor_nullish_species_defaults','matched'],
  'slice:function-constructor-defaults-missing-species':['slice_function_constructor_missing_species_defaults','matched'],
  'slice:custom-species-function':['slice_custom_species_is_explicit_boundary','unsupported-custom-species'],
  'slice:species-getter-after-conversions':['slice_object_bound_is_explicit_conversion_boundary','unsupported-object-ToNumber'],
  'slice:start-coercion-before-end':['slice_object_bound_is_explicit_conversion_boundary','unsupported-object-ToNumber'],
  'slice:index-get-after-conversions-and-species':['slice_object_bound_is_explicit_conversion_boundary','unsupported-object-ToNumber'],
  'slice:numeric-bounds-constructor-species-index-order':['slice_numeric_bounds_constructor_species_index_order','matched'],
  'slice:defines-own-result-without-array-prototype-setter':['slice_result_owns_data_and_bypasses_prototype_setter','matched'],
  'reverse:both-holes':['reverse_two_sided_holes','matched'],
  'reverse:lower-only-hole-move':['reverse_moves_lower_only_slot','matched'],
  'reverse:upper-only-hole-move':['reverse_moves_one_sided_slots','matched'],
  'reverse:has-get-and-set-order':['reverse_live_pair_order_and_holes','matched'],
  'reverse:lower-get-mutates-upper-before-upper-has':['reverse_lower_get_mutates_upper_before_test','matched'],
  'reverse:lower-set-throws-before-upper-set':['reverse_lower_set_throw_stops_before_upper_set','matched'],
  'includes:hole-matches-undefined':['includes_hole_matches_undefined','matched'],
  'includes:NaN-is-same-value-zero':['includes_nan_uses_samevaluezero','matched'],
  'includes:from-index-string-coercion':['includes_object_offset_is_explicit_conversion_boundary','unsupported-object-ToNumber'],
  'includes:primitive-string-from-index':['includes_from_index_string_to_number','matched'],
  'includes:zero-length-skips-symbol-from-index-conversion':['includes_zero_length_skips_symbol_conversion','matched'],
  'includes:from-index:positive-infinity':['includes_infinite_and_nan_offsets','matched'],
  'includes:from-index:negative-infinity':['includes_infinite_and_nan_offsets','matched'],
  'includes:from-index:fractional-negative':['includes_fractional_negative_offset_truncates_toward_zero','matched'],
  'includes:from-index:NaN':['includes_infinite_and_nan_offsets','matched'],
  'includes:inherited-hole-get':['includes_reads_inherited_hole','matched'],
  'push:variadic-return-and-identity':['push_variadic_identity_and_metadata','matched'],
  'push:sequential-set-partial-mutation':['push_partial_write_before_inherited_setter_throw','matched'],
  'push:nonwritable-length-zero-args':['push_empty_on_nonwritable_length_throws','matched'],
  'push:max-length-range-error-partial-write':['push_max_length_writes_tail_then_range_errors','matched'],
  'pop:hole-reads-inherited-value':['pop_hole_gets_inherited_value','matched'],
  'pop:get-delete-length-order':['pop_get_delete_then_length','matched'],
  'pop:delete-fails-before-length-write':['pop_nonconfigurable_delete_aborts_length_write','matched'],
  'pop:deletes-configurable-tail-before-readonly-length-fails':['pop_deletes_tail_before_readonly_length_failure','matched'],
  'pop:zero-length-still-writes-length':['pop_zero_length_nonwritable_length_throws','matched'],
  'join:nullish-empty-and-default-separator':['join_nullish_separator_and_primitives','matched'],
  'join:holes-and-captured-length-with-primitive-separator':['join_hole_gets_and_captured_length','matched'],
  'join:separator-coerced-before-elements':['join_object_separator_conversion_is_explicit_boundary','unsupported-object-ToString'],
  'join:symbol-element-type-error':['join_symbol_throws_and_object_conversion_boundary','matched'],
  'join:symbol-separator-type-error':['join_symbol_separator_throws','matched'],
  'join:object-coercion':['join_object_to_primitive_is_explicit_boundary','unsupported-object-ToPrimitive'],
  'join:nullish-elements-skip-object-conversion':['join_object_separator_conversion_is_explicit_boundary','unsupported-object-ToString'],
  'fill:range-conversion-order':['fill_object_bound_is_explicit_conversion_boundary','unsupported-object-ToNumber'],
  'fill:range:negative-fraction':['fill_fractional_and_infinite_bounds','matched'],
  'fill:range:NaN':['fill_nan_bounds_cover_full_range','matched'],
  'fill:range:positive-infinity':['fill_fractional_and_infinite_bounds','matched'],
  'fill:range:negative-infinity':['fill_fractional_and_infinite_bounds','matched'],
  'fill:sequential-set-partial-mutation':['fill_partial_set_via_own_accessor','matched'],
  'fill:inherited-setter-stops-before-next-slot':['fill_partial_set_via_inherited_setter','matched'],
  'find:visits-holes-as-undefined':['find_hole_callback_returns_undefined_value','matched'],
  'find:callback-receiver-and-three-args':['find_reads_holes_and_passes_exact_callback_arguments','matched'],
  'find:captured-length-live-values':['find_reads_future_values_live_captured_length','matched'],
  'find:shrink-length-still-visits-captured-indices':['find_shrink_still_gets_captured_slots','matched'],
  'find:inherited-hole-getter':['find_inherited_hole_getter','matched'],
  'find:invalid-callback-before-index-get':['find_invalid_callback_before_get','matched']
};
const nativeCoverageRows=rows.map(row=>{
  const coverage=nativeCoverage[row.name];
  assert.ok(coverage,'Missing native candidate mapping: '+row.name);
  return {sourceObservation:row.name,nativeCandidate:coverage[0],status:coverage[1]};
});
const output={stage:'Node observations for actual ordinary Array methods used by enabled MathJax TeX source',node:process.version,source:{file:source,sha256:scoped.source_sha256},methods:['slice','reverse','includes','push','pop','join','fill','find'],rows,nativeCoverage:nativeCoverageRows};
const out=path.join(root,'docs/math-port/evidence/parser-array-host-source.json');
if(process.argv.includes('--check'))assert.equal(fs.readFileSync(out,'utf8').replaceAll('\r\n','\n'),JSON.stringify(output,null,2)+'\n');
else fs.writeFileSync(out,JSON.stringify(output,null,2)+'\n');
console.log(JSON.stringify({node:process.version,observations:rows.length}));
