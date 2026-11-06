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
  ['four', 'venn-beta\nset A: 12\nset B: 11\nset C: 9\nset D: 8\nunion A,B,C,D[Shared]'],
  ['four-mds', 'venn-beta\nset A: 15\nset B: 12\nset C: 11\nset D: 9\n'
    + 'union A,B: 7\nunion A,C: 5\nunion A,D: 4\nunion B,C: 1\nunion B,D: 2\nunion C,D: 3'],
  ['five-disjoint', 'venn-beta\nset A: 15\nset B: 13\nset C: 10\nset D: 8\nset E: 6'],
  ['contained', 'venn-beta\nset A: 18\nset B: 9\nset C: 9\nunion A,B: 9\nunion A,C: 9\nunion B,C: 9'],
  ['empty-set', 'venn-beta\nset A: 12\nset B: 0\nset C: 9\nunion A,B,C\ntext B item[Zero]'],
  ['hand-drawn', '---\nconfig:\n  look: handDrawn\n  handDrawnSeed: 1\n---\n'
    + 'venn-beta\ntitle Sketch\nset A[Alpha]\nset B[Beta]\nunion A,B[Both]'],
  ['hand-drawn-styled', '---\nconfig:\n  look: handDrawn\n  handDrawnSeed: 9\n'
    + '  venn:\n    padding: 23\n---\nvenn-beta\nset A\n  text first[First]\nset B\n'
    + 'union A,B[Both]\nstyle A fill:#ff6b6b,stroke:#704040,stroke-width:"4px"\n'
    + 'style B fill:#4ecdc4\nstyle A,B fill:#ffe66d'],
  ['font-directive', '---\nconfig:\n  fontFamily: Courier New\n---\n'
    + 'venn-beta\ntitle Serif test\nset A[Alpha]\nset B[Beta]\nunion A,B[Both]'],
  ['font-precedence', '---\nconfig:\n  fontFamily: Courier New\n'
    + '  themeVariables:\n    fontFamily: Georgia\n---\n'
    + 'venn-beta\ntitle Serif test\nset A[Alpha]\nset B[Beta]\nunion A,B[Both]'],
];
export const invalid = ['venn-beta\nset A\nunion A,B', 'venn-beta\nset A\nunion A'];
