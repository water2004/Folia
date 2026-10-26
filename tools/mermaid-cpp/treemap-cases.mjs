const config=(value,source)=>`%%{init: ${JSON.stringify({treemap:value})}}%%\n${source}`;
export const cases=[
 ['flat','treemap-beta\n"Fruit"\n  "Apples": 6\n  "Pears": 4\n  "Berries": 2'],
 ['multiple','treemap-beta\n"North"\n  "A": 7\n  "B": 3\n"South"\n  "C": 5\n  "D": 5'],
 ['nested','treemap-beta\n"Company"\n  "Sales"\n    "Direct": 12\n    "Online": 8\n  "Services": 10'],
 ['small-long',config({nodeWidth:40,nodeHeight:25},'treemap-beta\n"A section with a very long title"\n  "A long label that must fit": 20\n  "Tiny": 1\n  "Other": 3')],
 ['configured',config({nodeWidth:64,nodeHeight:36,padding:6,diagramPadding:12,showValues:false},'treemap-beta\ntitle Revenue\n"Items"\n  "A": 4\n  "B": 2')],
 ['currency',config({valueFormat:'$0,0'},'treemap-beta\n"Budget"\n  "Build": 12500\n  "Review": 2500')],
 ['classes','treemap-beta\nclassDef emphasis fill:#ffeecc,stroke:#aa4400,color:#663300\n"Group":::emphasis\n  "Marked": 8:::emphasis\n  "Plain": 4'],
 ['metadata','treemap-beta\ntitle Resource allocation\naccTitle: Allocation\naccDescr: Team work\n"Engineering"\n  "Build": 6\n  "Support": 4'],
 ['missing-value','treemap-beta\n"A":',null],
 ['unquoted','treemap-beta\nA: 2',null],
];
