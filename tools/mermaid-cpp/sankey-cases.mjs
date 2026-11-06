const configured=(config,source)=>`%%{init: ${JSON.stringify({sankey:config})}}%%\n${source}`;
const split='sankey-beta\nInput,Left,6\nInput,Right,4\nLeft,Output,6\nRight,Output,4';
const uneven='sankey-beta\nA,B,5\nA,C,3\nB,D,5\nC,E,3\nA,E,2';
export const cases=[
 ['split',split],
 ['align-left',configured({nodeAlignment:'left'},uneven)],
 ['align-right',configured({nodeAlignment:'right'},uneven)],
 ['align-center',configured({nodeAlignment:'center'},uneven)],
 ['align-justify',configured({nodeAlignment:'justify'},uneven)],
 ['quoted-values',configured({showValues:true,prefix:'$',suffix:' USD',linkColor:'source'},'sankey-beta\n"Raw, input","Part ""A""",12.345\n"Part ""A""",Output,12.345')],
 ['outlined',configured({width:480,height:320,nodeWidth:18,nodePadding:20,labelStyle:'outlined',nodeColors:{Input:'#ff6600',Output:'#006699'},linkColor:'target'},split)],
 ['duplicate-links',configured({linkColor:'#8844aa'},'sankey-beta\nA,B,3\nA,B,2\nB,C,5')],
 ['disconnected',configured({showValues:false},'sankey-beta\nA,B,5\nC,D,2')],
 ['missing-column','sankey-beta\nA,B',null],
 ['unfinished-quote','sankey-beta\n"A,B,2',null],
 ['circular','sankey-beta\nA,B,2\nB,A,2','layout'],
];
