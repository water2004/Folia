export const cases = [
  ['basic', 'requirementDiagram\nrequirement req1 {\nid: R1\ntext: Must be safe\nrisk: high\nverifyMethod: test\n}\n', null],
  ['types', 'requirementDiagram\nfunctionalRequirement "User authentication" {\nid: FR1\ntext: Users must sign in\nrisk: medium\nverifyMethod: demonstration\n}\ninterfaceRequirement API {\nid: IR2\ntext: Expose HTTPS\nrisk: low\nverifyMethod: inspection\n}\nperformanceRequirement Latency {\nid: PR3\ntext: Under 100 ms\nrisk: high\nverifyMethod: analysis\n}\nphysicalRequirement Device {\nid: PH4\ntext: Must fit enclosure\nrisk: low\nverifyMethod: test\n}\ndesignConstraint Budget {\nid: DC5\ntext: Under budget\nrisk: medium\nverifyMethod: inspection\n}\n', null],
  ['elements', 'requirementDiagram\nelement "Login service" {\ntype: software\ndocRef: docs/login.md\n}\nelement DB {\ntype: database\ndocRef: DB101\n}\n', null],
  ['relationships', 'requirementDiagram\nrequirement R1 {\nid: 1\ntext: First\nrisk: low\nverifyMethod: test\n}\nrequirement R2 {\nid: 2\ntext: Second\nrisk: high\nverifyMethod: analysis\n}\nelement E1 {\ntype: module\ndocRef: sourceone\n}\nR1 - contains -> R2\nR2 <- derives - E1\nR1 - verifies -> E1\n', null],
  ['direction-title-access', 'requirementDiagram\ndirection LR\naccTitle: Requirements\naccDescr: A compact model\nrequirement R {\nid: reqr\ntext: "Keep < 2s"\nrisk: low\nverifyMethod: inspection\n}\n', null],
  ['classes-style', 'requirementDiagram\nrequirement R:::critical {\nid: R1\ntext: Important\nrisk: high\nverifyMethod: test\n}\nelement E {\ntype: api\ndocRef: spec\n}\nclassDef critical fill:#ffeeee,stroke:#cc0000\nstyle E fill:#eeeeff,stroke:#0000cc\nR - satisfies -> E\n', null],
  ['comment-quoted', 'requirementDiagram\n# top comment\nrequirement "Long requirement name" {\nid: "RQ 1"\ntext: "Text with spaces and: colon"\nrisk: medium\nverifyMethod: demonstration\n}\n%% comment\n', null],
  ['unterminated', 'requirementDiagram\nrequirement R {\nid: R1\ntext: incomplete\n', 'syntax error'],
  ['unknown-relation', 'requirementDiagram\nA - unknown -> B\n', 'syntax error'],
  ['invalid-direction', 'requirementDiagram\ndirection XX\n', 'syntax error'],
];
