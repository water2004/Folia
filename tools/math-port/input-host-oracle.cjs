// Source-only ECMAScript host observations for js_numeric_input and UTF16.
// Inputs and expectation algorithms match input-host-oracle.cjs; no C++ emitted.
'use strict';
const fs=require('node:fs'),path=require('node:path'),crypto=require('node:crypto'),assert=require('node:assert/strict');
assert.equal(process.version,'v24.13.0','Pinned reference runtime changed');
const root=path.resolve(__dirname,'../..');
const bits=n=>{const b=Buffer.alloc(8);b.writeDoubleLE(n);return b.readBigUInt64LE().toString(16).padStart(16,'0');};
const codeUnits=s=>Array.from({length:s.length},(_,i)=>s.charCodeAt(i));
let state=0x79429317;function random(){state=(Math.imul(state,1664525)+1013904223)>>>0;return state;}
const ws=[9,10,11,12,13,32,160,5760,8192,8193,8194,8195,8196,8197,8198,8199,8200,8201,8202,8232,8233,8239,8287,12288,65279];
const numbers=['',' ','\0','-0','+0','-0e999999','-0e-999999','Infinity','+Infinity','-Infinity','inf','NaN','infinity','1.','.1','+.5','-.5','1e+2','1e','1e-','1_2','1n','1px','0x','0x1','0XfF','-0x1','+0x1','0o10','0b11','0b12','0o8','0x1g','0x1.2','0x1p2','0o','0b','1e309','-1e309','1e-9999','-1e-9999','2.2250738585072014e-308','2.2250738585072012e-308','4.9406564584124654e-324','2.4703282292062327e-324','2.4703282292062328e-324','1.7976931348623157e308','1.7976931348623159e308','9007199254740993','9007199254740995','1.00000000000000011102230246251565404236316680908203125','1.00000000000000011102230246251565404236316680908203126','9'.repeat(500),'-0.'+'0'.repeat(500)+'1'];
for(const c of ws)numbers.push(String.fromCharCode(c)+'-2.5'+String.fromCharCode(c));
for(const c of [0,133,6158,8203,55296,56320,65535])numbers.push(String.fromCharCode(c)+'1','1'+String.fromCharCode(c));
for(const exponent of [51,52,53,54,63,64,1022,1023,1024,1100])for(const delta of [-3n,-2n,-1n,0n,1n,2n,3n]){const n=(1n<<BigInt(exponent))+delta;for(const[prefix,radix]of [['0x',16],['0o',8],['0b',2]])numbers.push(prefix+n.toString(radix));}
numbers.push('0x'+'0'.repeat(1500)+'20000000000001','00000000000000001','-0000.000e9999','000.e-9999');
numbers.push('0.'+'0'.repeat(600)+'1e600','1'+'0'.repeat(600)+'e-600','-0.'+'0'.repeat(600)+'1e1000','-1'+'0'.repeat(600)+'e-1000','1e'+'9'.repeat(500),'-1e-'+'9'.repeat(500));
for(let i=0;i<400;i++){let digits='';for(let j=0,n=1+random()%100;j<n;j++)digits+=random()%10;const cut=random()%(digits.length+1);numbers.push((random()%2?'-':'+')+digits.slice(0,cut)+'.'+digits.slice(cut)+'e'+(Number(random()%1401)-700));}
for(const[prefix,radix]of [['0x',16],['0o',8],['0b',2]])for(let i=0;i<100;i++){let digits='';for(let j=0,n=1+random()%1200;j<n;j++)digits+='0123456789abcdef'[random()%radix];numbers.push(prefix+digits);}
const numberRows=numbers.map(input=>({input,expected_bits:bits(Number(input))}));
const ints=['',' ','-0','-0junk','+12px','-12.9','0x10','-0x10','0Xff','0b11','0o11','08','010','123e4','zZ','Infinity','0x','0xg','1_2','9007199254740993','9007199254740995','-9'.repeat(2),'9'.repeat(500)];
ints.push('0'.repeat(1500)+'9007199254740993!', '-0'.replace('0','0'.repeat(1500))+'!', '0x'+'0'.repeat(1500)+'20000000000001!');
for(const exponent of [52,53,54,63,64,1023,1024])for(const delta of [-1n,0n,1n,2n,3n]){const n=(1n<<BigInt(exponent))+delta;ints.push(n.toString(10),'0x'+n.toString(16),n.toString(8));}
for(const c of ws)ints.push(String.fromCharCode(c)+'-012junk');for(const c of [0,133,6158,8203,55296])ints.push(String.fromCharCode(c)+'12');
const parseIntRows=[];for(const input of ints)for(const radix of [0,8,10,16])parseIntRows.push({input,radix,expected_bits:bits(parseInt(input,radix))});
for(let i=0;i<300;i++){const radix=[8,10,16][random()%3];let digits='';for(let j=0,n=1+random()%1000;j<n;j++)digits+='0123456789abcdef'[random()%radix];const input=(random()%2?'-':'')+digits+'!';parseIntRows.push({input,radix,expected_bits:bits(parseInt(input,radix))});}
const strings=['','a','a\0b','\uD800','\uDC00','\uD800\uDC00','\uD800x\uDC00','\uDC00\uD800','\uD800\uD800\uDC00','\uDBFF\uDFFF','e\u0301','a😀中'];
for(let i=0;i<100;i++){let s='';for(let j=0,n=random()%20;j<n;j++)s+=String.fromCharCode(random()%65536);strings.push(s);}
const codePointAt=(s,i)=>i<0||i>=s.length?null:s.codePointAt(i);
const utf16Rows=strings.map(input=>({input,code_point_at:Array.from({length:input.length+2},(_,i)=>codePointAt(input,i)),code_points:Array.from(input,c=>c.codePointAt(0)),utf8_bytes:[...new TextEncoder().encode(input)]}));
const valid=['','abc','a\0b','\u007f\u0080\u07ff\u0800\ud7ff\ue000\uffff','a😀中','\u{10000}\u{10ffff}','e\u0301','\ufeffx'];
for(let i=0;i<60;i++){let s='';for(let j=0,n=random()%20;j<n;j++){let cp=random()%0x110000;if(cp>=0xd800&&cp<=0xdfff)cp=0xfffd;s+=String.fromCodePoint(cp);}valid.push(s);}
const decoder=new TextDecoder('utf-8',{fatal:true,ignoreBOM:true});
const decodeRows=valid.map(input=>{const bytes=[...new TextEncoder().encode(input)];assert.equal(decoder.decode(Uint8Array.from(bytes)),input);const offsets=[];let at=0;for(const c of input){for(let j=0;j<c.length;j++)offsets.push(at);at+=new TextEncoder().encode(c).length;}offsets.push(at);return{input,bytes,expected_text:input,byte_offsets:offsets};});
const invalid=[[0x80],[0xbf],[0xc0,0x80],[0xc1,0xbf],[0xc2],[0xc2,0x20],[0xe0,0x80,0x80],[0xe0,0x9f,0xbf],[0xed,0xa0,0x80],[0xed,0xbf,0xbf],[0xe1,0x80],[0xe1,0x80,0x20],[0xf0,0x80,0x80,0x80],[0xf0,0x8f,0xbf,0xbf],[0xf4,0x90,0x80,0x80],[0xf5,0x80,0x80,0x80],[0xff],[0xfe],[0xf1,0x80,0x80],[0xf1,0x80,0x80,0x20]];
const invalidRows=[];for(const tail of invalid)for(const prefix of [[],[0x61],[0xf0,0x9f,0x98,0x80]]){const bytes=[...prefix,...tail];assert.throws(()=>new TextDecoder('utf-8',{fatal:true}).decode(Uint8Array.from(bytes)));invalidRows.push({bytes,expected_byte_offset:prefix.length});}
const output={schema:'input-host-source-values/v1',runtime:{node:process.version,unicode:process.versions.unicode},counts:{number:numberRows.length,parseInt:parseIntRows.length,utf16:utf16Rows.length,utf8_decode:decodeRows.length,utf8_invalid:invalidRows.length},number:numberRows,parseInt:parseIntRows,utf16:utf16Rows,utf8_decode:decodeRows,utf8_invalid:invalidRows};
assert.ok(process.argv.includes('--check'),'This source-only oracle is check-only');
const gold=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/input-host-source.json'),'utf8'));
assert.equal(gold.schema,'input-host-complete-static-gold/v1');
assert.equal(gold.source_fixture_sha256,'1c2b7bf427a11d8ba07132a3d070db4b77a6d46f2faa95aadb1a3760efbc3fcf');
assert.deepEqual(output.counts,{number:gold.number.length,parseInt:gold.parseInt.length,utf16:gold.utf16.length,utf8_decode:gold.utf8_decode.length,utf8_invalid:gold.utf8_invalid.length});
for(let i=0;i<gold.number.length;i++){
  const row=gold.number[i],actual=numberRows[i];
  assert.deepEqual(codeUnits(actual.input),row.input_units,`Number input ${i}`);
  assert.equal(actual.expected_bits,row.expected_binary64_bits.slice(2),`Number gold ${i}`);
}
const radix={Automatic:0,Octal:8,Decimal:10,Hex:16};
for(let i=0;i<gold.parseInt.length;i++){
  const row=gold.parseInt[i],actual=parseIntRows[i];
  assert.deepEqual(codeUnits(actual.input),row.input_units,`parseInt input ${i}`);
  assert.equal(actual.radix,radix[row.radix],`parseInt radix ${i}`);
  assert.equal(actual.expected_bits,row.expected_binary64_bits.slice(2),`parseInt gold ${i}`);
}
for(let i=0;i<gold.utf16.length;i++){
  const expected=gold.utf16[i],actual=utf16Rows[i];
  assert.deepEqual(codeUnits(actual.input),expected.input_units,`UTF-16 input ${i}`);
  assert.deepEqual(actual.code_point_at,expected.code_point_at,`codePointAt ${i}`);
  assert.deepEqual(actual.code_points,expected.code_points,`codePoints ${i}`);
  assert.deepEqual(actual.utf8_bytes,expected.utf8_bytes,`UTF-8 encode ${i}`);
}
for(let i=0;i<gold.utf8_decode.length;i++){
  const expected=gold.utf8_decode[i],actual=decodeRows[i];
  assert.deepEqual(actual.bytes,expected.bytes,`UTF-8 decode input ${i}`);
  assert.deepEqual(codeUnits(actual.expected_text),expected.expected_text_units,`UTF-8 decoded text ${i}`);
  assert.deepEqual(actual.byte_offsets,expected.expected_byte_offsets,`UTF-8 offsets ${i}`);
}
for(let i=0;i<gold.utf8_invalid.length;i++)
  assert.deepEqual(invalidRows[i],gold.utf8_invalid[i],`UTF-8 invalid input ${i}`);
console.log(JSON.stringify({mode:'check',counts:output.counts,source_gold:gold.source_fixture_sha256}));
