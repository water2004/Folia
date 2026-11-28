// Capture the pinned D3 date formatter used by Mermaid's Gantt renderer.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync,writeFileSync} from 'node:fs';
import {utcFormat} from './oracle/node_modules/.pnpm/d3-time-format@4.1.0/node_modules/d3-time-format/src/index.js';

const lock=JSON.parse(readFileSync(new URL('./source-lock.json',import.meta.url)));
for(const file of lock.ganttTime.files.filter(f=>f.source.includes('/d3-time-format/'))){
 const relative=file.source.split('/node_modules/.pnpm/')[1];
 const bytes=readFileSync(new URL('./oracle/node_modules/.pnpm/'+relative,import.meta.url));
 assert.equal(createHash('sha256').update(bytes).digest('hex'),file.sha256,file.source);
}
const dates=['1969-12-31T23:59:59Z','2000-01-01T00:00:00Z','2015-12-31T12:34:56Z',
 '2016-01-01T12:34:56Z','2020-12-31T18:09:05Z','2021-01-01T18:09:05Z',
 '2021-01-03T18:09:05Z','2021-01-04T18:09:05Z','2024-02-29T06:07:08Z',
 '2025-03-01T09:08:07Z','2025-03-02T13:02:01Z','2025-03-03T23:59:59Z'];
const formats=['%Y-%m-%d %H:%M:%S','%a %A %b %B %p','%d|%e|%-d|%_d|%0e|%-I',
 '%j %q %u %w %U %W %V %g %G %y','%x|%X|%c','%L %f %%'];
const hex=s=>Buffer.from(s,'utf16le').swap16().toString('hex');
const rows=['# Mermaid 12.2.0 / d3-time-format 4.1.0; civil input, format and result are UTF-16 hex.',
 '# UTC capture excludes timezone directives; native epoch round trips are tested independently.'];
for(const iso of dates)for(const format of formats){
 const date=new Date(iso);
 rows.push([hex(iso.slice(0,19).replace('T',' ')),hex(format),hex(utcFormat(format)(date))].join('\t'));
}
const file=new URL('../../tests/mermaid_cpp/fixtures/gantt-formats.tsv',import.meta.url);
const output=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(file,output);
else assert.equal(readFileSync(file,'utf8').replaceAll('\r\n','\n'),output);
console.log(JSON.stringify({dateCases:dates.length,formats:formats.length,checks:dates.length*formats.length}));
