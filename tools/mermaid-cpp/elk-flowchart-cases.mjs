// Representative public ELK adapter inputs; original layout and classic paint.
export const cases=[
 ['elk-flat-right','flowchart LR\nA[Read] -->|next| B[Write]\nA --> C[Check]\nC --> B'],
 ['elk-flat-down','flowchart TD\nA{Ready?} -->|yes| B((Done))\nA -->|no| C[Wait]'],
 ['elk-flat-left','flowchart RL\nA[Read] --> B[Write]\nB --> C[Done]'],
 ['elk-flat-up','flowchart BT\nA[Read] --> B[Write]\nB --> C[Done]'],
 ['elk-group-right','flowchart LR\nsubgraph g[Processing]\nA[Read] --> B[Write]\nend\nB --> C[Done]'],
 ['elk-groups-down','flowchart TD\nsubgraph a[Input]\nA[Read] --> B[Check]\nend\nsubgraph b[Output]\nC[Write] --> D[Done]\nend\nB -->|save| C'],
];
