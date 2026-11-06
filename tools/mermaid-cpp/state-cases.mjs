export const cases=[
 ['basic','stateDiagram-v2\n[*] --> Idle\nIdle --> Running\nRunning --> [*]'],
 ['aliases','stateDiagram-v2\nstate "Waiting for input" as Waiting\nWaiting --> Done'],
 ['labels','stateDiagram-v2\nIdle --> Active: begin\nActive --> Idle: stop'],
 ['left-right','stateDiagram-v2\ndirection LR\n[*] --> First\nFirst --> Second\nSecond --> [*]'],
 ['composite','stateDiagram-v2\n[*] --> Ready\nstate Ready {\n[*] --> Waiting\nWaiting --> Working: run\nWorking --> [*]\n}\nReady --> Done'],
 ['nested','stateDiagram-v2\nstate Outer {\nstate Inner {\nA --> B\n}\nInner --> C\n}'],
 ['styles','stateDiagram-v2\nIdle --> Busy\nstyle Idle fill:#ffeecc,stroke:#884400,color:#112233'],
 ['style-classes','stateDiagram-v2\nclassDef hot fill:#c00,color:#fff,stroke:#333,stroke-width:2px\nA:::hot --> B:::hot\nB --> C\nclass C hot'],
 ['fork-join','stateDiagram-v2\nstate Fork <<fork>>\nstate Join [[join]]\n[*] --> Fork\nFork --> A\nFork --> B\nA --> Join\nB --> Join\nJoin --> [*]'],
 ['fork-join-lr','stateDiagram-v2\ndirection LR\nstate Fork <<fork>>\nstate Join <<join>>\nA --> Fork\nFork --> B\nB --> Join\nJoin --> C'],
 ['floating-note','stateDiagram-v2\nA --> B\nnote "unattached text" as Floating'],
 ['configured','---\ntitle: State lifecycle\nconfig:\n  state:\n    nodeSpacing: 65\n    rankSpacing: 70\n---\nstateDiagram-v2\nA --> B: continue'],
 ['inline-notes','stateDiagram-v2\nA --> B\nnote left of A : left annotation\nnote right of B : right annotation'],
 ['block-note','stateDiagram-v2\nA --> B\nnote right of B\nA multi-line\nstate annotation\nend note'],
 ['concurrent-regions','stateDiagram-v2\nstate Active {\nA --> B\n--\nC --> D\n--\nE --> F\n}'],
 ['concurrent-initial-states','stateDiagram-v2\nstate Active {\n[*] --> A\n--\n[*] --> B\n}'],
 ['concurrent-trailing-divider','stateDiagram-v2\nstate Active {\nA --> B\n--\n}',false,false],
 ['unclosed','stateDiagram-v2\nstate Outer {\nA --> B',null],
 ['missing-target','stateDiagram-v2\nA -->',null]
];

export const elkCases=[
 ['elk-default','stateDiagram\n[*] --> Idle\nIdle --> Running\nRunning --> [*]'],
 ...cases.filter(([name])=>['basic','aliases','labels','left-right','composite','nested',
  'styles','style-classes','fork-join','fork-join-lr','floating-note','configured','inline-notes',
  'block-note','concurrent-regions','concurrent-initial-states'].includes(name))
  .map(([name,source])=>['elk-'+name,source]),
];
