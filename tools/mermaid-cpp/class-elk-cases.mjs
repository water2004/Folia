export const cases=[
 ['elk-members','classDiagram\nclass Document {\n+title String\n+save() bool\n}\nDocument --> Store : saves'],
 ['elk-inheritance','classDiagram\nBase <|-- Reader\nBase <|-- Writer\nReader --> Writer : sends'],
 ['elk-multiplicity','classDiagram\ndirection LR\nLibrary "1" o-- "*" Book : contains'],
 ['elk-feedback','classDiagram\ndirection LR\nReader "1" --> "*" Store : writes\nStore "1" --> "1" Reader : refreshes'],
 ['elk-namespace','classDiagram\nnamespace Input {\nclass Reader\nclass Source\n}\nnamespace Output {\nclass Writer\n}\nSource --> Reader : reads\nReader "1" --> "*" Writer : sends'],
];
