export const cases=[
 ['columns','kanban\n  todo[To do]\n    a[Design]\n    b[Build]\n  done[Done]\n    c[Release]'],
 ['metadata','kanban\n  doing[Doing]\n    task[Review]@{ assigned: "Ada", ticket: "PROJ-12", priority: "High" }\n    next[Ship]@{ assigned: "Lin", priority: "Low" }'],
 ['priorities','kanban\n  queue[Queue]\n    a[One]@{ priority: "Very High" }\n    b[Two]@{ priority: "Medium" }\n    c[Three]@{ priority: "Very Low" }'],
 ['empty-column','kanban\n  todo[Todo]\n  done[Done]\n    task[Ready]'],
 ['wrapped','kanban\n  backlog[A longer backlog heading]\n    task[A card description that wraps across several lines in the column]'],
 ['configured','%%{init: {"kanban":{"sectionWidth":250}}}%%\nkanban\n  work[Work]\n    task[Configured width]\n  done[Done]'],
 ['unclosed-node','kanban\n  todo[Todo',null],
 ['bad-metadata','kanban\n  todo[Todo]\n    task[Task]@{ priority: [ }',null],
];
