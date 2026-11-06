const config=(value,source)=>`%%{init: ${JSON.stringify({gitGraph:value})}}%%\n${source}`;
const branching='commit id: "A"\nbranch feature\ncommit id: "B"\ncheckout main\ncommit id: "C"\nmerge feature id: "M"';
// Mermaid 12.2.0, docs/syntax/examples.md.
const officialExample = `gitGraph:
    commit "Ashish"
    branch newbranch
    checkout newbranch
    commit id:"1111"
    commit tag:"test"
    checkout main
    commit type: HIGHLIGHT
    commit
    merge newbranch
    commit
    branch b2
    commit`;
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

const options = String.raw`gitGraph LR: commit id:"discarded" id:'A' "discarded" msg:"Fix: \"quotes\" %% inside" type:NORMAL tag:'v one'tag:"v two"
  branch "feature lane" order:2
  commit type: REVERSE id:'B' msg:'keep\nline'
  switch main %% inline comment
  commit id:"C"
  merge "feature lane" tag:'merged' id:"M" type:HIGHLIGHT tag:"release"
  branch release/1.0 order: 1
  commit id:"R"
  checkout "feature lane"
  cherry-pick tag:"" parent:"A" parent:'C' tag:"backport" id:'M'`;

export const parserCases = [
 ['official-example',officialExample],
 ['compact-options',options],
 ['metadata','gitGraph TB :\n  title  A   title\n  accTitle : Branch history\n  accDescr {\n    First line\n\n    Second   line\n  }\n  commit id:"A"'],
 ['unknown-option','gitGraph\ncommit oops:"x"',null],
 ['unquoted-id','gitGraph\ncommit id:A',null],
 ['missing-tag','gitGraph\ncommit tag:',null],
 ['separated-colon','gitGraph\ncommit id : "A"',null],
 ['unclosed-quote','gitGraph\ncommit msg:"A',null],
 ['invalid-type','gitGraph\ncommit type:unknown',null],
 ['quoted-type','gitGraph\ncommit type:"NORMAL"',null],
 ['commit-parent','gitGraph\ncommit parent:"A"',null],
 ['merge-message','gitGraph\n'+branching+' msg:"extra"',null],
 ['cherry-type',options+' type:NORMAL',null],
 ['cherry-missing-id',options.replace(" id:'M'",''),null],
 ['cherry-wrong-parent',options.replace("parent:'C'","parent:'R'"),null],
 ['branch-fraction','gitGraph\nbranch feature order:1.5',null],
 ['branch-negative','gitGraph\nbranch feature order:-1',null],
 ['branch-trailing-order-text','gitGraph\nbranch feature order:1x',null],
 ['checkout-option','gitGraph\ncheckout main order:1',null],
 ['direction-missing-colon','gitGraph TB\ncommit',null],
];
