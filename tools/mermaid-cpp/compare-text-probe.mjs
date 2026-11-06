// Regression ceilings for this 96-case Windows corpus, not a universal
// browser/DirectWrite equivalence claim. Native outline bounds differ from
// Chromium's hinted glyph bounds even when advances nearly agree.
import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
const directory=resolve(process.argv[2]??'build/mermaid-cpp');
const expected=readFileSync(resolve(directory,'browser-text.tsv'),'utf8').trimEnd().split(/\r?\n/);
const rows=readFileSync(resolve(directory,'native-text.tsv'),'utf8').trimEnd().split(/\r?\n/);
assert.equal(rows.length,96);
assert.equal(rows.length,expected.length);
const summary={cases:rows.length,advanceCeiling:0.0625,boundsCeiling:2.5,maxAdvanceError:0,maxBoundsError:0};
for(let i=0;i<rows.length;++i) {
 const f=rows[i].split('\t');
 assert.equal(f.length,8);
 assert.equal(f.slice(0,5).join('\t'),expected[i],'Input/order mismatch');
 const [browserBounds,browserAdvance,advance,ink,bounds]=f.slice(3).map(Number);
 assert([browserBounds,browserAdvance,advance,ink,bounds].every(n=>Number.isFinite(n)&&n>=0));
 const advanceError=Math.abs(browserAdvance-advance),boundsError=Math.abs(browserBounds-bounds);
 summary.maxAdvanceError=Math.max(summary.maxAdvanceError,advanceError);
 summary.maxBoundsError=Math.max(summary.maxBoundsError,boundsError);
 assert(advanceError<=summary.advanceCeiling,`Advance regression at row ${i}: ${advanceError}`);
 assert(boundsError<=summary.boundsCeiling,`Bounds regression at row ${i}: ${boundsError}`);
 assert(bounds>=advance);
}
writeFileSync(resolve(directory,'text-comparison.json'),JSON.stringify(summary,null,2)+'\n');
console.log(JSON.stringify(summary));
