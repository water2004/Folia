export const cases = [
  ['indent-basic', 'treeView-beta\nproject/\n  src/\n    main.cpp\n    util.cpp\n  README.md\n'],
  ['annotations', 'treeView-beta\nproject/\n  src/ :::highlight icon(folder) ## Source code\n    main.cpp icon(file) ## Entry point\n'],
  ['quoted-names', 'treeView-beta\n"My project/"\n  "file with spaces.txt"\n  README.md\n'],
  ['box-drawing', 'treeView-beta\nRepository/\n├── src/\n│   ├── main.cpp\n│   └── util.cpp\n└── README.md\n'],
  ['box-heavy', 'treeView-beta\nRepository/\n┣━━ src/\n┃   ┗━━ main.cpp\n┗━━ README.md\n'],
  ['icons', 'treeView-beta\nproject/\n  app.ts\n  package.json\n'],
];
