// Root actual Source copy metadata, shallow identity and recursive keepIds behavior.
'use strict';
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict'),crypto=require('node:crypto');const root=path.resolve(__dirname,'../..'),base=path.join(root,'tools/mathjax-bundle/node_modules/@mathjax/src/cjs'),hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const pins=JSON.parse(fs.readFileSync(path.join(root,'tools/math-port/profiles/evidence/p23b-sparse-mml-source.json'))).loadedPins;assert.equal(process.versions.node,'24.13.0');for(const[f,p]of Object.entries(pins))assert.equal(hash(fs.readFileSync(path.join(base,f))),p);
const {MmlFactory}=require(path.join(base,'core/MmlTree/MmlFactory.js'));
const scalar=v=>v===undefined?'U':v===null?'N':typeof v==='boolean'?(v?'T':'F'):typeof v==='number'?'D'+v:'S'+v;
const own=a=>a?Object.keys(a.getAllAttributes()).map(k=>k+'='+scalar(a.getExplicit(k))).join(','):'N';
const cases=[];
for(const kind of ['mi','mrow','mfrac','math','mstyle','msubsup','mtable','mtr'])for(const keep of [false,true])for(const nullAttributes of [false,true]){
 const f=new MmlFactory(),n=f.create(kind,{'10':10,'2':2,id:'root',mathcolor:'blue',accent:true}),child=f.create('mi',{id:'child',mathcolor:'orange'});n.appendChild(child);n.attributes.setInherited('mathsize','big');
 n.setProperty('10',10);n.setProperty('2',2);n.setProperty('top',5);n.setProperty('nested',{v:7});if(nullAttributes)n.attributes=null;
 const copy=n.copy(keep),copiedChild=(copy.arity<0?copy.childNodes[0].childNodes:copy.childNodes)[0];
 const fields=[copy.kind,scalar(n.properties!==copy.properties),scalar(n.getProperty('nested')===copy.getProperty('nested')),Object.keys(n.properties).join(','),Object.keys(copy.properties).join(','),own(n.attributes),own(copy.attributes),scalar(copy.attributes.getInherited('mathsize')),scalar(copiedChild.attributes.getExplicit('id')),scalar(copiedChild.attributes.getExplicit('mathcolor'))];
 copy.setProperty('top',99);copy.getProperty('nested').v=9;copy.attributes.set('mathcolor','green');fields.push(scalar(n.getProperty('top')),scalar(copy.getProperty('top')),scalar(n.getProperty('nested').v),scalar(n.attributes?.getExplicit('mathcolor')),scalar(copy.attributes.getExplicit('mathcolor')),own(copy.attributes));
 cases.push({input:{kind,keep,nullAttributes},expected:fields.join('|')});
}
const data=JSON.stringify({schema:1,status:'actual original copy metadata evidence; sparse and full graph gates separate',pins,cases},null,2)+'\n',out=path.join(root,'tools/math-port/profiles/evidence/p23g-copy-metadata-source.json');if(process.argv.includes('--check'))assert.equal(fs.readFileSync(out,'utf8').replaceAll('\r\n','\n'),data);else{assert.ok(!fs.existsSync(out));fs.writeFileSync(out,data);}console.log(JSON.stringify({cases:cases.length,sha256:hash(data)}));
