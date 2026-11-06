export const cases = [
  ['two', 'venn-beta\nset A\nset B\nunion A,B[Both]'],
  ['three', 'venn-beta\nset A[Alpha]\nset B[Beta]\nset C[Gamma]\n'
    + 'union A,B[AB]\nunion A,C[AC]\nunion B,C[BC]\nunion A,B,C[All]'],
  ['implicit', 'venn-beta\nset A\nset B\nset C\nunion A,B,C[Shared]'],
  ['styled', 'venn-beta\ntitle Intersecting circles\nset "A one"[First] : 18\nset B[Second] : 11\n'
    + 'union "A one",B[Common] : 4\nstyle "A one" fill:#ff0000,stroke:#804020,'
    + 'stroke-width:4,fill-opacity:0.3,color:#290000\nstyle B,"A one" fill:#aabbcc,color:#121212'],
  ['text', '---\nconfig:\n  venn:\n    width: 640\n    height: 420\n    padding: 20\n'
    + '    useDebugLayout: true\n    useMaxWidth: false\n---\nvenn-beta\nset A[One]\n'
    + '  text first[First item]\n  text second[Second item]\nset B[Two]\n'
    + 'union A,B[Both]\ntext B,A item[Shared item]\nstyle item color:#ee6655'],
  ['disjoint', 'venn-beta\nset A: 14\nset B: 8\nset C: 5'],
];
export const invalid = ['venn-beta\nset A\nunion A,B', 'venn-beta\nset A\nunion A'];
