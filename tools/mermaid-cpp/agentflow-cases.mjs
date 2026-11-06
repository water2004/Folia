export const cases=[
 ['basic','agentflow-beta TB\n  changes["Gather changes"]@{ shape: input }\n  analyse["Analyse diff"]@{ shape: task }\n  lint["run_linter"]@{ shape: tool }\n  ok["Clean?"]@{ shape: decision }\n  changes --> analyse --> lint --> ok\n'],
 ['shapes','agentflow-beta LR\n  brief["Brief"]@{ shape: input }\n  draft["Draft copy"]@{ shape: task }\n  spellcheck["spell_check"]@{ shape: tool }\n  guide["Style guide"]@{ shape: refdoc }\n  publish["Publish"]@{ shape: action }\n  brief --> draft --> spellcheck --> publish\n  draft -.- guide\n'],
 ['decision','agentflow-beta TB\n  triage["Triage"]@{ shape: decision }\n  triage -- bug --> bugfix["Bug Fix"]\n  triage -- feature --> feature_work["Feature Work"]\n  triage -- question --> answer["Answer"]\n'],
 ['nested-flow','agentflow-beta LR\n  start["Receive request"]\n  flow review["Review"]\n    intake["Intake"]\n    flow checks["Checks"]\n      policy["Policy"]\n    end\n    approval["Approval"]\n  end\n  connector queue["Work queue"]\n  finish["Complete"]\n  start --> intake --> policy --> approval --> finish\n'],
 ['frame-endpoints','agentflow-beta LR\nflow F["Frame"]\n A --> B\nend\n X --> F --> Y'],
 ['frame-descendant-crossing','---\nconfig:\n  elk:\n    lineHops: false\n---\nagentflow-beta LR\nflow F["Frame"]\n A --> B\nend\n X --> F --> Y\n Z --> A'],
 ['frame-inside-outside','agentflow-beta LR\nflow F["Frame"]\n A --> F\nend\n X --> F'],
 ['shape-fallback','agentflow-beta TB\n  a["Alpha"]\n  a@{ shape: triangle }\n  b["Beta"]@{ shape: circle }\n  a --> b\n'],
];

export const diagnosticCases=[
 ['shape-fallback', cases.find(([name])=>name==='shape-fallback')[1]],
 ['unknown-shape','agentflow-beta TB\n  a@{ shape: made-up }'],
 ['shape-casing','agentflow-beta TB\n  a@{ shape: roundedRect }'],
 ['inline-removed','agentflow-beta TB\n  a((Alpha)) --> b'],
 ['mutual-containment','agentflow-beta TB\n  flow A["Flow A"]\n    a1["Task A1"]\n    a1 --> B\n  end\n  flow B["Flow B"]\n    b1["Task B1"]\n    b1 --> A\n  end'],
 ['self-containment','agentflow-beta TB\n  flow A["Flow A"]\n    a1["Task A1"]\n    a1 --> A\n  end'],
 ['global-membership','agentflow-beta TB\n  flow f["F"]\n    a --> b\n  end\n  global\n    b[Beta]\n  end'],
 ['metadata','agentflow-beta TB\n  flow f["F"]\n    a["Before"]@{ shape: input, label: After, myExtension: 42, icon: "fa:rocket" }\n    a e1@--> b\n  end\n  f@{ totallyCustomKey: opaque }\n  e1@{ instruction: "hand off", weight: 5 }'],
];
