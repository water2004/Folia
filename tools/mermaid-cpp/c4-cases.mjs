export const cases=[
 ['context','C4Context\ntitle Service context\nPerson(user, "User", "Uses the service")\nSystem(api, "Service", "Handles requests")\nSystem_Ext(mail, "Mail", "Sends messages")\nRel(user, api, "Uses", "HTTPS")\nRel(api, mail, "Sends")'],
 ['container','C4Container\nPerson(user, "User")\nSystem_Boundary(sys, "Product") {\nContainer(web, "Web", "Browser", "Interface")\nContainerDb(db, "Data", "SQL", "Storage")\n}\nRel(user, web, "Visits")\nRel(web, db, "Reads", "SQL")'],
 ['component','C4Component\nContainer_Boundary(api, "API") {\nComponent(controller, "Controller", "C++", "Handles requests")\nComponentDb(store, "Store", "SQL", "Data access")\n}\nRel(controller, store, "Uses")'],
 ['dynamic','C4Dynamic\nContainer(client, "Client", "Browser")\nContainer(server, "Server", "C++")\nRel(client, server, "1. Request", "HTTPS")\nRel_Back(client, server, "2. Response")'],
 ['deployment','C4Deployment\nDeployment_Node(host, "Host", "Linux") {\nContainer(api, "API", "C++")\nContainerDb(db, "Database", "SQL")\n}\nRel(api, db, "Reads")'],
 ['nested','C4Context\nEnterprise_Boundary(org, "Organization") {\nSystem_Boundary(team, "Team") {\nSystem(service, "Service", "Application")\n}\nSystem_Ext(other, "Partner")\n}\nRel(service, other, "Calls")'],
 ['styles','C4Context\nPerson(user, "Reader")\nSystem(service, "Service")\nRel(user, service, "Uses")\nUpdateElementStyle(user, "#eeeeff", "#112233", "#556677")\nUpdateRelStyle(user, service, "#112233", "#cc4400")'],
 ['rows','C4Container\nContainer(a, "One", "C++")\nContainer(b, "Two", "C++")\nContainer(c, "Three", "C++")\nUpdateLayoutConfig($c4ShapeInRow="2", $c4BoundaryInRow="1")'],
 ['unclosed-call','C4Context\nPerson(user, "Reader"',null],
 ['unknown-command','C4Context\nUnknown(user, "Reader")',null],
];
