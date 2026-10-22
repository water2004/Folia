'use strict';
// Exercise the pinned transpiled Configuration helpers through their actual
// package and tuple loops, recording effects rather than reimplementing them.
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const source='ts/input/tex/Configuration.ts';
const scope=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/scope.json'),'utf8'));
const pin=scope.files.find(f=>f.source===source);
const bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));
assert.equal(crypto.createHash('sha256').update(bytes).digest('hex'),pin.source_sha256);
const base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src');
const bundledTs=fs.readFileSync(path.join(base,source));
assert.equal(bytes.toString().replaceAll('\r\n','\n'),bundledTs.toString().replaceAll('\r\n','\n'));
const cjsPath=path.join(base,'cjs/input/tex/Configuration.js'),cjsBytes=fs.readFileSync(cjsPath);
const cjsSha256=crypto.createHash('sha256').update(cjsBytes).digest('hex');
const {ParserConfiguration,ConfigurationHandler}=require(cjsPath);
const cjsSource=cjsBytes.toString('utf8');
const helperEnd=cjsSource.indexOf('\nvar __read');
assert.ok(helperEnd>0,'locked CJS __values helper is present');
const sourceValues=new Function(cjsSource.slice(0,helperEnd)+'\nreturn __values;')();
const rows=[];
function observe(label,fn){const log=[];let outcome;const warn=console.warn;console.warn=(...args)=>log.push('warn:'+args.join(' '));try{const value=fn(log);outcome={value:value===undefined?'undefined':value};}catch(e){outcome={error:e.name+'|'+e.message};}finally{console.warn=warn;}rows.push({label,outcome,log});}
function config(parser='tex',preprocessors=[],postprocessors=[]){return {priority:1,parser,handler:{},fallback:{},items:{},tags:{},options:{},nodes:{},preprocessors,postprocessors};}
function withPackage(name,cfg,fn){ConfigurationHandler.set(name,cfg);try{return fn();}finally{ConfigurationHandler.set(name,undefined);}}
function configurationWithPre(tuple,log){
  const name='p55-pre';ConfigurationHandler.set(name,config('tex',[tuple]));
  const pc=new ParserConfiguration([name]);
  const jax={preFilters:{add(...args){log.push('add:'+args.join(','));}},postFilters:{add(){}}};
  return {pc,jax};
}

observe('array-iterator-dynamic-prototype-method',log=>withPackage('p55-default',config(),()=>{
  const original=Array.prototype[Symbol.iterator];
  try{Array.prototype[Symbol.iterator]=function(){log.push('iterator:call');const array=this;let i=0;return {next(){log.push('next:'+i);if(i>=array.length)return {done:true};return {value:array[i++],done:false};}};};
    const pc=new ParserConfiguration(['p55-default']);return pc.configurations.items.length;
  }finally{Array.prototype[Symbol.iterator]=original;}
}));

observe('iterator-method-call-property-is-dynamic',log=>withPackage('p55-call',config(),()=>{
  const packages=[];
  packages.slice=function(){return {reverse(){return {[Symbol.iterator]:function(){throw new Error('direct-call');}};}};};
  const iterable=packages.slice().reverse(),method=iterable[Symbol.iterator];let next=false;
  method.call=function(receiver){log.push('call:'+String(receiver===iterable));return {next(){if(next)return {done:true};next=true;return {value:'p55-call',done:false};}};};
  packages.slice=function(){return {reverse(){return iterable;}};};
  const pc=new ParserConfiguration(packages);return pc.configurations.items.length;
}));

observe('truthy-iterator-object-call-method',log=>withPackage('p55-call-object',config(),()=>{
  const packages=[];let next=false;
  const iteratorMethod={call(receiver){log.push('call:'+String(receiver===iterable));return {next(){if(next)return {done:true};next=true;return {value:'p55-call-object',done:false};}};}};
  const iterable={[Symbol.iterator]:iteratorMethod};
  packages.slice=function(){return {reverse(){return iterable;}};};
  const pc=new ParserConfiguration(packages);return pc.configurations.items.length;
}));

observe('iterator-method-missing-call-is-typeerror',log=>withPackage('p55-no-call',config(),()=>{
  const packages=[];const method=function(){throw new Error('direct-call');};method.call=undefined;
  packages.slice=function(){return {reverse(){return {[Symbol.iterator]:method};}};};
  new ParserConfiguration(packages);return 'unexpected';
}));

observe('custom-iterator-abrupt-close',log=>withPackage('p55-wrong',config('other'),()=>{
  const packages=['ignored'];let i=0;
  Object.defineProperty(packages,'slice',{value(){log.push('slice');return {reverse(){log.push('reverse');return {[Symbol.iterator](){log.push('iterator');return {next(){log.push('next');return {value:'p55-wrong',done:false};},return(){log.push('return');return {};}};}};}};}});
  new ParserConfiguration(packages);return 'unexpected';
}));

observe('custom-iterator-next-error-close',log=>withPackage('p55-never',config(),()=>{
  const packages=[];packages.slice=function(){return {reverse(){return {[Symbol.iterator](){let first=true;return {next(){log.push('next');if(first){first=false;return {value:'p55-never',done:false};}throw new Error('next-fault');},return(){log.push('return');return {};}};}};}};};
  new ParserConfiguration(packages);return 'unexpected';
}));

observe('arraylike-values-fallback-live',log=>withPackage('p55-arraylike',config(),()=>{
  const packages=[];packages.slice=function(){return {reverse(){let length=1;return {get length(){log.push('length');return length;},get 0(){log.push('get0');length=2;return 'p55-arraylike';},get 1(){log.push('get1');return 'late';}};}};};
  const pc=new ParserConfiguration(packages);return pc.configurations.items.length;
}));

observe('symbol-iterator-getter-before-arraylike-length',log=>withPackage('p55-throw',config(),()=>{
  const packages=[];packages.slice=function(){return {reverse(){return new Proxy({length:1},{get(target,key,receiver){log.push('get:'+String(key));if(key===Symbol.iterator)throw new Error('iterator-get-fault');return Reflect.get(target,key,receiver);}});}};};
  new ParserConfiguration(packages);return 'unexpected';
}));

observe('null-values-helper-read-error',()=>withPackage('p55-null',config(),()=>{
  const packages=[];packages.slice=function(){return {reverse(){return null;}};};
  new ParserConfiguration(packages);return 'unexpected';
}));

observe('tuple-destructure-close',log=>withPackage('p55-tuples',config('tex',[[1,2]]),()=>{
  const pc=new ParserConfiguration(['p55-tuples']);
  const tuple={[Symbol.iterator](){log.push('tuple:iterator');let i=0;return {next(){log.push('tuple:next'+i);return i++<2?{value:i===1?'proc':7,done:false}:{value:'tail',done:false};},return(){log.push('tuple:return');return {};}};}};
  pc.configurations.items[0].item.preprocessors=[tuple];
  const jax={preFilters:{add(...args){log.push('add:'+args.join(','));}},postFilters:{add(){}}};pc.config(jax);return 'ok';
}));

observe('tuple-destructure-close-throw-priority',log=>withPackage('p55-tuples-throw',config(),()=>{
  const pc=new ParserConfiguration(['p55-tuples-throw']);
  const tuple={[Symbol.iterator](){return {next(){return {value:'proc',done:false};},return(){log.push('tuple:return');throw new Error('close-fault');}};}};
  pc.configurations.items[0].item.preprocessors=[tuple];
  const jax={preFilters:{add(){throw new Error('callback-fault');}},postFilters:{add(){}}};pc.config(jax);return 'unexpected';
}));

observe('tuple-arraylike-read-fallback',log=>{
  const tuple={get 0(){log.push('tuple:get0');return 'proc';},get 1(){log.push('tuple:get1');return 9;}};
  const {pc,jax}=configurationWithPre(tuple,log);pc.config(jax);return 'ok';
});

observe('tuple-next-abrupt-keeps-original-over-return',log=>{
  const tuple={[Symbol.iterator](){return {next(){log.push('tuple:next');throw new Error('next-fault');},return(){log.push('tuple:return');throw new Error('close-fault');}};}};
  const {pc,jax}=configurationWithPre(tuple,log);pc.config(jax);return 'unexpected';
});
observe('tuple-next-error-after-yield-keeps-priority-over-return',log=>{
  const tuple={[Symbol.iterator](){let index=0;return {
    next(){log.push('tuple:next'+index);if(index++===0)return {value:'proc',done:false};throw new Error('next-fault');},
    return(){log.push('tuple:return');throw new Error('close-fault');}
  };}};
  const {pc,jax}=configurationWithPre(tuple,log);pc.config(jax);return 'unexpected';
});

observe('function-prototype-call-metadata',()=>{
  const descriptor=Object.getOwnPropertyDescriptor(Function.prototype,'call');
  return Function.prototype.call.length+':'+descriptor.enumerable+':'+descriptor.writable+':'+descriptor.configurable;
});

observe('function-prototype-call-getter-is-live',log=>withPackage('p55-call-getter',config(),()=>{
  const packages=[];let consumed=false;
  const method=function(){throw new Error('direct invocation bypassed call getter');};
  const iterable={[Symbol.iterator]:method};
  packages.slice=function(){return {reverse(){return iterable;}};};
  const original=Object.getOwnPropertyDescriptor(Function.prototype,'call');
  Object.defineProperty(Function.prototype,'call',{configurable:true,get(){
    log.push('get-call:'+String(this===method));
    return function(receiver){log.push('invoke-call:'+String(this===method)+':'+String(receiver===iterable));
      return {next(){if(consumed)return {done:true};consumed=true;return {value:'p55-call-getter',done:false};}};};
  }});
  try { return new ParserConfiguration(packages).configurations.items.length; }
  finally { Object.defineProperty(Function.prototype,'call',original); }
}));

observe('function-prototype-call-deleted-is-not-bypassed',log=>withPackage('p55-call-deleted',config(),()=>{
  const packages=[];
  const method=function(){throw new Error('direct invocation bypassed deletion');};
  packages.slice=function(){
    const original=Object.getOwnPropertyDescriptor(Function.prototype,'call');
    delete Function.prototype.call;
    return {reverse(){return {[Symbol.iterator]:method};}};
  };
  const original=Object.getOwnPropertyDescriptor(Function.prototype,'call');
  try { new ParserConfiguration(packages);return 'unexpected'; }
  finally { Object.defineProperty(Function.prototype,'call',original); }
}));

observe('arraylike-fallback-iterator-object-shape',()=>{
  const iterator=sourceValues({length:0});
  const descriptor=Object.getOwnPropertyDescriptor(iterator,'next');
  return String(Object.getPrototypeOf(iterator)===Object.prototype)+':'+
    String(descriptor.enumerable)+':'+typeof iterator[Symbol.iterator];
});

observe('configuration-handler-method-metadata',()=>{
  return Object.keys(ConfigurationHandler).map(name=>{
    const descriptor=Object.getOwnPropertyDescriptor(ConfigurationHandler,name);
    return name+':'+ConfigurationHandler[name].length+':'+descriptor.enumerable+':'+
      descriptor.writable+':'+descriptor.configurable;
  }).join('/');
});

observe('configuration-handler-live-keys-iterator',()=>{
  const prefix='p55-live-key-'+Math.random();
  ConfigurationHandler.set(prefix+'-a',{id:'a'});
  const iterator=ConfigurationHandler.keys();
  const first=iterator.next();
  ConfigurationHandler.set(prefix+'-b',{id:'b'});
  let added;
  do { added=iterator.next(); } while(!added.done&&added.value!==prefix+'-b');
  const done=iterator.next();
  ConfigurationHandler.set(prefix+'-c',{id:'c'});
  const afterDone=iterator.next();
  return String(first.value===prefix+'-a'&&first.done===false)+':'+
    String(added.value===prefix+'-b'&&added.done===false)+':'+
    String(done.done)+':'+String(afterDone.done);
});

observe('configuration-handler-overwrite-keeps-map-order',()=>{
  const prefix='p55-overwrite-'+Math.random();
  const old={id:'old'},replacement={id:'new'},other={id:'other'};
  ConfigurationHandler.set(prefix+'-a',old);
  ConfigurationHandler.set(prefix+'-b',other);
  ConfigurationHandler.set(prefix+'-a',replacement);
  const iterator=ConfigurationHandler.keys();
  const a=iterator.next(),b=iterator.next();
  return String(ConfigurationHandler.get(prefix+'-a')===replacement)+':'+
    String(a.value===prefix+'-a')+':'+String(b.value===prefix+'-b');
});

const candidates={
  'array-iterator-dynamic-prototype-method':'source_values_resolves_current_symbol_iterator_method',
  'iterator-method-call-property-is-dynamic':'source_helper_uses_current_function_call_property',
  'truthy-iterator-object-call-method':'source_helper_accepts_object_with_callable_call_property',
  'iterator-method-missing-call-is-typeerror':'source_helper_method_call_error',
  'custom-iterator-abrupt-close':'source_for_of_body_error_closes_current_iterator',
  'custom-iterator-next-error-close':'source_for_of_next_error_closes_previous_step',
  'arraylike-values-fallback-live':'source_values_arraylike_fallback_reads_dynamic_length_and_get',
  'symbol-iterator-getter-before-arraylike-length':'source_values_symbol_getter_precedes_length_fallback',
  'null-values-helper-read-error':'source_values_null_fails_during_symbol_lookup',
  'tuple-destructure-close':'source_read_bounded_iterator_closes_after_requested_values',
  'tuple-destructure-close-throw-priority':'source_read_close_error_propagates_without_prior_error',
  'tuple-arraylike-read-fallback':'source_read_without_iterator_returns_original_identity',
  'tuple-next-abrupt-keeps-original-over-return':'source_read_iterator_error_precedes_close_error'
  ,'tuple-next-error-after-yield-keeps-priority-over-return':'source_read_iterator_error_after_yield_precedes_close_error'
  ,'function-prototype-call-metadata':'source_function_prototype_call_metadata'
  ,'function-prototype-call-getter-is-live':'source_helper_gets_inherited_function_call'
  ,'function-prototype-call-deleted-is-not-bypassed':'source_helper_does_not_bypass_deleted_call'
  ,'arraylike-fallback-iterator-object-shape':'source_arraylike_helper_iterator_is_plain_own_next'
  ,'configuration-handler-method-metadata':'source_configuration_handler_methods_metadata'
  ,'configuration-handler-live-keys-iterator':'source_configuration_handler_keys_is_live'
  ,'configuration-handler-overwrite-keeps-map-order':'source_configuration_handler_set_overwrites_in_place'
};
for(const row of rows)row.nativeCandidate=candidates[row.label];
const output=path.join(root,'docs/math-port/evidence/parser-iterator-source.json');
const data=JSON.stringify({node:process.version,source,source_sha256:pin.source_sha256,cjs:'cjs/input/tex/Configuration.js',cjs_sha256:cjsSha256,observations:rows},null,2)+'\n';
assert.ok(process.argv.includes('--check'),'Frozen Source owner is check-only');
assert.equal(fs.readFileSync(output,'utf8').replaceAll('\r\n','\n'),data,'Frozen iterator Source profile changed');
console.log(JSON.stringify({node:process.version,observations:rows.length,mode:'check'}));
