// Fixed parser/DB corpus for the Mindmap native family, not layout acceptance.
export const cases=[
 ['root','mindmap\n  Root'],
 ['tree','mindmap\n  Root\n    Read\n      File\n    Write'],
 ['shapes','mindmap\nroot((Root))\n  plain\n  round(Round)\n  rect[Rectangle]\n  cloud)Cloud(\n  bang))Bang((\n  hex{{Hexagon}}'],
 ['labels','mindmap\nroot["Read **source**"]\n  child["Line one\nLine two"]\n  end["Caf\u00e9 &amp; \u6587\u4ef6"]'],
 ['depth-config','---\nconfig:\n  mindmap: {colorBy: depth, padding: 14, maxNodeWidth: 180}\n---\nmindmap\n  Root\n     Read\n        File\n           Details\n     Write'],
 ['explicit-layout-class','mindmap\nRoot\n  Read\n    :::important\n  Write',{layout:'elk'}],
 ['multiple-roots','mindmap\nA\nB',{},true],
 ['unfinished-shape','mindmap\nA[Open',{},true],
];
