export const cases = [
  ['two-lanes-lr', `swimlane-beta LR
subgraph Intake
Start
Review
end
subgraph Approval
Approve
Finish
end
Start --> Review --> Approve --> Finish
`],
  ['decisions-tb', `swimlane-beta TB
subgraph Customer
Request
Decision{Complete?}
end
subgraph Operations
Check
Reject
end
subgraph Result
Done
end
Request --> Decision
Decision -- yes --> Check --> Done
Decision -- no --> Reject
`],
  ['unassigned-default-lane', `swimlane-beta LR
subgraph Assigned
InLane
end
Outside --> InLane --> AlsoOutside
`],
  ['edge-labels', `swimlane-beta LR
subgraph First
A[Begin]
end
subgraph Second
B[Work]
end
A -- routed --> B
`],
  ['reverse-rl', `swimlane-beta RL
subgraph East
Review
Approve
end
subgraph West
Start
end
Start --> Review --> Approve
`],
  ['reverse-bt', `swimlane-beta BT
subgraph Upper
Top
Middle
end
subgraph Lower
Reply
Finish
end
Top --> Middle --> Reply --> Finish
`],
  ['styles-multiline', `swimlane-beta LR
subgraph Draft
A[First line<br/>Second line]
B{Pass?}
end
subgraph Review
C[Approved]
end
A --> B
B -- yes --> C
classDef accent fill:#ff99cc,stroke:#003366,stroke-width:5px,color:#111111,font-weight:bold,font-size:18px,font-family:Georgia
class A accent
style B fill:#fff2cc,stroke:#cc6600,stroke-width:3px,color:#222222
linkStyle 0 stroke:#ff6600,stroke-width:5px
`],
];
