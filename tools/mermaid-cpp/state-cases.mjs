export const cases=[
 ['basic','stateDiagram-v2\n[*] --> Idle\nIdle --> Running\nRunning --> [*]'],
 ['aliases','stateDiagram-v2\nstate "Waiting for input" as Waiting\nWaiting --> Done'],
 ['labels','stateDiagram-v2\nIdle --> Active: begin\nActive --> Idle: stop'],
 ['left-right','stateDiagram-v2\ndirection LR\n[*] --> First\nFirst --> Second\nSecond --> [*]'],
 ['composite','stateDiagram-v2\n[*] --> Ready\nstate Ready {\n[*] --> Waiting\nWaiting --> Working: run\nWorking --> [*]\n}\nReady --> Done'],
 ['nested','stateDiagram-v2\nstate Outer {\nstate Inner {\nA --> B\n}\nInner --> C\n}'],
 ['styles','stateDiagram-v2\nIdle --> Busy\nstyle Idle fill:#ffeecc,stroke:#884400,color:#112233'],
 ['configured','---\ntitle: State lifecycle\nconfig:\n  state:\n    nodeSpacing: 65\n    rankSpacing: 70\n---\nstateDiagram-v2\nA --> B: continue'],
 ['unclosed','stateDiagram-v2\nstate Outer {\nA --> B',null],
 ['missing-target','stateDiagram-v2\nA -->',null]
];
