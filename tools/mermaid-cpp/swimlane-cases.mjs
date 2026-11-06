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
];
