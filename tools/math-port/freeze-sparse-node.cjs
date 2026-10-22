'use strict';
// The independent sparse-child Source owner checks every field of all 111 rows.
const fs=require('node:fs'),path=require('node:path'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
assert.ok(process.argv.includes('--check'));require('./sparse-child-oracle.cjs');
const source=JSON.parse(fs.readFileSync(path.join(root,'docs/math-port/evidence/p23-sparse-child-source.json'),'utf8'));const selected=source.cases.filter(c=>/^(setChildren-|copy-|appendChild-|replaceChild-|removeChild-|childIndex-)/.test(c.name));assert.equal(selected.length,64);console.log(JSON.stringify({selectedSourceRows:selected.length,fullOwnerRows:source.cases.length,mode:'check'}));
