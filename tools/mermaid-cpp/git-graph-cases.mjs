const config=(value,source)=>`%%{init: ${JSON.stringify({gitGraph:value})}}%%\n${source}`;
const branching='commit id: "A"\nbranch feature\ncommit id: "B"\ncheckout main\ncommit id: "C"\nmerge feature id: "M"';
export const cases=[
 ['commits','gitGraph\ncommit id: "A" tag: "v1"\ncommit id: "B" type: REVERSE\ncommit id: "C" type: HIGHLIGHT tag: "ready" tag: "v2"'],
 ['merge','gitGraph\n'+branching],
 ['top-bottom','gitGraph TB:\n'+branching],
 ['bottom-top','gitGraph BT:\n'+branching],
 ['parallel',config({parallelCommits:true},'gitGraph\n'+branching)],
 ['configured',config({mainBranchName:'trunk',mainBranchOrder:2,rotateCommitLabel:false},'gitGraph\ncommit id: "A"\nbranch feature order: 1\ncommit id: "B"\ncheckout trunk\ncommit id: "C"')],
 ['cherry','gitGraph\ncommit id: "A"\nbranch feature\ncommit id: "B"\ncheckout main\ncommit id: "C"\ncherry-pick id: "B"'],
 ['cherry-merge','gitGraph\n'+branching+'\nbranch release\ncommit id: "R"\ncheckout feature\ncherry-pick id: "M" parent: "C" tag: "backport"'],
 ['duplicate-branch','gitGraph\nbranch main',null],
 ['unknown-checkout','gitGraph\ncheckout missing',null],
 ['self-merge','gitGraph\ncommit id: "A"\nmerge main',null],
];
