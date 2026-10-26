// Original implicit hierarchical port dummy and container-boundary export.
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {readFileSync,writeFileSync,realpathSync} from 'node:fs';
import {createRequire} from 'node:module';
import path from 'node:path';
import vm from 'node:vm';
const require=createRequire(realpathSync(new URL('./oracle/node_modules/mermaid/package.json',import.meta.url)));
const base=path.dirname(require.resolve('elkjs/package.json'));
const lock=JSON.parse(readFileSync(new URL('../../docs/mermaid-cpp/source-lock.json',import.meta.url)));
for(const file of lock.elkCycle.files)
 assert.equal(createHash('sha256').update(readFileSync(path.join(base,file.path))).digest('hex'),file.sha256,file.path);
const context=vm.createContext({module:{exports:{}},exports:{},setTimeout,clearTimeout});context.global=context;
vm.runInContext(readFileSync(path.join(base,'lib/elk-worker.js'),'utf8'),context);
const original=vm.runInContext(`(direction,outgoing,border,minWidth,minHeight) => {
 $clinit_LayeredOptions();$clinit_InternalProperties_1();$clinit_PortSide();$clinit_PortConstraints();$clinit_Direction_0();$clinit_GraphProperties();
 const g=new LGraph(),properties=new MapPropertyHolder();$setProperty(properties,PORT_BORDER_OFFSET,border);
 const dummy=createExternalPortDummy(properties,FREE,UNDEFINED_5,outgoing?1:-1,null,new KVector(),new KVector(),[RIGHT_6,LEFT_6,DOWN_1,UP_1][direction],g);
 $add_3(g.layerlessNodes,dummy);$getProperty(g,GRAPH_PROPERTIES).add_2(EXTERNAL_PORTS);
 const side=$getProperty(dummy,EXT_PORT_SIDE),port=dummy.ports.array[0];
 const constraint=side===WEST_2?0:side===EAST_2?1:side===NORTH_3?2:3;
 assertConstraint();
 function assertConstraint(){if(side===WEST_2||side===EAST_2){if($getProperty(dummy,LAYERING_LAYER_CONSTRAINT_0)!==(side===WEST_2?FIRST_SEPARATE_0:LAST_SEPARATE_0))throw Error('constraint');}
  else if($getProperty(dummy,IN_LAYER_CONSTRAINT)!==(side===NORTH_3?TOP_1:BOTTOM_0))throw Error('constraint');}
 dummy.pos.x_0=20;dummy.pos.y_0=30;
 g.size_0.x_0=100;g.size_0.y_0=60;g.padding.left=24;g.padding.right=24;g.padding.top_0=40;g.padding.bottom=24;g.offset.x_0=2;g.offset.y_0=-3;
 $clinit_ContentAlignment();$setProperty_0(g,CONTENT_ALIGNMENT,of_2(V_TOP,[H_CENTER]));
 $resizeGraphNoReallyIMeanIt_0(g,new KVector_1(148,124),new KVector_1(Math.max(148,minWidth),Math.max(124,minHeight)));
 const before=[dummy.pos.x_0,dummy.pos.y_0];const exported=getExternalPortPosition(g,dummy,0,0);
 return [side.ordinal,port.side.ordinal,constraint,dummy.size_0.x_0,dummy.size_0.y_0,port.pos.x_0,port.pos.y_0,
  g.size_0.x_0,g.size_0.y_0,g.offset.x_0,g.offset.y_0,...before,exported.x_0,exported.y_0,dummy.pos.x_0,dummy.pos.y_0].join(',');
}`,context);
const cases=[];
for(let direction=0;direction<4;++direction)for(const outgoing of [0,1])cases.push([`direction-${direction}-${outgoing}`,direction,outgoing,5,0,0]);
cases.push(['title-min-width',0,1,10,220,0],['title-min-height',2,1,10,220,180]);
const rows=['# ELK compound port geometry; name,direction,outgoing,border,minimum width,height,side/inner side/constraint/size/port anchor/content size/offset/pre-export position/exported port/final dummy position.'];
for(const [name,dir,out,border,w,h]of cases)rows.push([name,dir,out,border,w,h,original(dir,out,border,w,h)].join('\t'));
const fixture=new URL('../../tests/mermaid_cpp/fixtures/elk-compound-ports.tsv',import.meta.url),expected=rows.join('\n')+'\n';
if(process.argv.includes('--write'))writeFileSync(fixture,expected);
else assert.equal(readFileSync(fixture,'utf8').replaceAll('\r\n','\n'),expected);
console.log(`ELK compound ports: ${cases.length} original boundary/constraint/resize observations checked.`);
