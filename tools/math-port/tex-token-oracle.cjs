// Root executes every actual Token/Macro member; expected values never use C++.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const {root,Token,Macro,ConfigurationType,HandlerType,TexConstant}=require('./tex-token-reference.cjs');
const cases=[];function value(x){if(x===undefined)return{type:'missing'};if(x===null)return{type:'null'};if(typeof x==='number')return{type:'number',value:Number.isNaN(x)?'NaN':Object.is(x,-0)?'-0':x===Infinity?'Infinity':x===-Infinity?'-Infinity':x};if(typeof x==='boolean')return{type:'boolean',value:x};if(typeof x==='string')return{type:'string',units:Array.from({length:x.length},(_,i)=>x.charCodeAt(i))};if(Array.isArray(x))return{type:'array',items:x.map(value)};return{type:'object',entries:Object.keys(x).map(k=>[k,value(x[k])])};}
const raw=[undefined,null,false,true,0,-0,NaN,Infinity,-Infinity,'','\\mathbb','ℝ𝔸中文','\ud800x\udfff','a\u0000b',[],[null,false,1,'x'],{}, {key:'v',nested:[1,2]}];
for(let i=0;i<raw.length;++i){const a=raw[i],t=new Token(a,raw[(i+1)%raw.length],raw[(i+2)%raw.length]);
 cases.push({name:"math_port_tex_token_raw_"+i+"",observations:[{kind:'value',value:value(t.token)},{kind:'value',value:value(t.char)},{kind:'value',value:value(t.attributes)}]});
 const m=new Macro(a,raw[(i+1)%raw.length],a);
 cases.push({name:"math_port_tex_macro_raw_"+i+"",observations:[{kind:'value',value:value(m.token)},{kind:'value',value:value(m.func)},{kind:'value',value:value(m.args)}]});
}
const attrs={nested:[1]},token={name:'input'},chr=['translation'],n=new Token(token,chr,attrs);
const flags=[n.token===token,n.char===chr,n.attributes===attrs,n.attributes===n.attributes];attrs.changed=true;chr.push('later');token.name='mutated';
cases.push({name:"math_port_tex_token_identity",observations:[{kind:'boolean',value:flags[0]},{kind:'boolean',value:flags[1]},{kind:'boolean',value:flags[2]},{kind:'boolean',value:flags[3]},{kind:'value',value:value(n.token)},{kind:'value',value:value(n.char)},{kind:'value',value:value(n.attributes)}]});
const args=[false,1,'x',null],fn=()=>{},m=new Macro(token,fn,args),other=new Macro('x',()=>{},args);
const identities=[m.func===fn,m.func!==other.func,m.args===args,m.args===m.args];args.push('mutated');
cases.push({name:"math_port_tex_macro_identity",observations:[{kind:'boolean',value:identities[0]},{kind:'boolean',value:identities[1]},{kind:'boolean',value:identities[2]},{kind:'boolean',value:identities[3]},{kind:'value',value:value(m.args)}]});
const d1=new Macro('x',fn),d2=new Macro('x',fn,undefined);const defaultFlags=[d1.args===d1.args,d1.args!==d2.args];d1.args.push('one');
cases.push({name:"math_port_tex_macro_default_fresh",observations:[{kind:'boolean',value:defaultFlags[0]},{kind:'boolean',value:defaultFlags[1]},{kind:'value',value:value(d1.args)},{kind:'value',value:value(d2.args)}]});
cases.push({name:"math_port_tex_token_omitted_attributes",observations:[{kind:'value',value:value(new Token('x','y').attributes)}]});
for(const [name,table] of Object.entries({ConfigurationType,HandlerType,TexConstant})){
cases.push({name:"math_port_tex_table_"+name+"",observations:[{kind:'value',value:value(table)}]});
}
// Native callable seam witnesses use real Token/Macro storage and all typed alternatives.
;

const report=({cases});const gold=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/tex-token-source.json'),'utf8'));assert.ok(process.argv.includes('--check'),'Source owner is check-only');assert.deepEqual(report,gold);console.log(JSON.stringify({profile:'tex-token-source.json',cases:report.cases?.length??report.rows?.length,mode:'check'}));
