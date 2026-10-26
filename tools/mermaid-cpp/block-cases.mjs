export const cases=[
 ['grid','block-beta\ncolumns 2\na["Alpha"] b["Beta"]\nc["Gamma"] d["Delta"]'],
 ['span','block-beta\ncolumns 3\na["Wide"]:2 b["Side"]\nc["Full row"]:3'],
 ['nested','block-beta\ncolumns 2\nblock:group\ncolumns 2\na["One"] b["Two"]\nc["Three"]:2\nend\nd["Outside"]'],
 ['spaces','block-beta\ncolumns 3\na["Start"] space:2\nb["End"]'],
 ['links','block-beta\ncolumns 2\na["Start"] b["Finish"]\na -- "next" --> b'],
 ['shapes','block-beta\ncolumns 3\na("Round") b{"Choice"} c["Rect"]'],
 ['arrow','block-beta\ncolumns 3\na<["Go"]>(right):2 b["Target"]'],
 ['styles','block-beta\ncolumns 2\na["Accent"] b["Plain"]\nclassDef accent fill:#ffeecc,stroke:#aa4400,color:#663300\nclass a accent\nstyle b fill:#ddeeff,stroke:#335577'],
 ['incomplete-node','block-beta\na["Incomplete"',null],
 ['missing-end','block-beta\nblock:group\na["A"]',null],
];
