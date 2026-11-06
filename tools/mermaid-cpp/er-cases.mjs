export const cases=[
 ['basic','erDiagram\nCUSTOMER ||--o{ ORDER : places'],
 ['attributes','erDiagram\nCUSTOMER {\nint id\nstring name\n}\nORDER {\nint id\nfloat total\n}\nCUSTOMER ||--o{ ORDER : places'],
 ['keys-comments','erDiagram\nUSER {\nint id PK "Unique identifier"\nstring email UK\nint team_id FK "Owning team"\n}\nTEAM {\nint id PK\n}\nTEAM ||--o{ USER : includes'],
 ['cardinality','erDiagram\nA ||--|| B : exactly\nB |o--o{ C : optional\nC }|--|{ D : relates'],
 ['non-identifying','erDiagram\nPERSON ||..o{ ADDRESS : uses'],
 ['aliases','erDiagram\nACCOUNT["User account"] {\nint id PK\n}\nSESSION["Login session"]\nACCOUNT ||--o{ SESSION : owns'],
 ['left-right','erDiagram\ndirection LR\nCUSTOMER ||--|{ ORDER : places\nORDER ||--o{ ITEM : contains'],
 ['configured','---\ntitle: Orders and items\nconfig:\n  er:\n    diagramPadding: 12\n    entityPadding: 8\n    nodeSpacing: 90\n    rankSpacing: 100\n---\nerDiagram\nORDER {\nint id PK\n}\nORDER ||--o{ ITEM : contains'],
 ['unclosed','erDiagram\nENTITY {\nint id',null],
 ['missing-target','erDiagram\nENTITY ||--o{ : missing',null]
];

