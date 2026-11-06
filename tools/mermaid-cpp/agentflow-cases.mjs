export const cases=[
 ['basic','agentflow-beta TB\n  changes["Gather changes"]@{ shape: input }\n  analyse["Analyse diff"]@{ shape: task }\n  lint["run_linter"]@{ shape: tool }\n  ok["Clean?"]@{ shape: decision }\n  changes --> analyse --> lint --> ok\n'],
 ['shapes','agentflow-beta LR\n  brief["Brief"]@{ shape: input }\n  draft["Draft copy"]@{ shape: task }\n  spellcheck["spell_check"]@{ shape: tool }\n  guide["Style guide"]@{ shape: refdoc }\n  publish["Publish"]@{ shape: action }\n  brief --> draft --> spellcheck --> publish\n  draft -.- guide\n'],
 ['decision','agentflow-beta TB\n  triage["Triage"]@{ shape: decision }\n  triage -- bug --> bugfix["Bug Fix"]\n  triage -- feature --> feature_work["Feature Work"]\n  triage -- question --> answer["Answer"]\n'],
 ['nested-flow','agentflow-beta LR\n  start["Receive request"]\n  flow review["Review"]\n    intake["Intake"]\n    flow checks["Checks"]\n      policy["Policy"]\n    end\n    approval["Approval"]\n  end\n  connector queue["Work queue"]\n  finish["Complete"]\n  start --> intake --> policy --> approval --> finish\n'],
];
