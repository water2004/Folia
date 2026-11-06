export const cases=[
 ['basic','architecture-beta\nservice api(server)[API]\nservice store(database)[Store]\napi:R --> L:store'],
 ['groups','architecture-beta\ngroup cloud(cloud)[Cloud]\ngroup app(server)[Application] in cloud\ngroup storage(database)[Storage] in cloud\nservice api(server)[API] in app\nservice db(database)[Database] in storage\napi{group}:R -- L:db{group}'],
 ['junction','architecture-beta\nservice a(internet)[Client]\nservice b(server)[Worker]\nservice c(disk)[Files]\njunction j\na:R -- L:j\nj:T -- B:b\nj:R --> L:c'],
 ['arrow-titles','architecture-beta\nservice a "API"[Read input]\nservice b(server)[Write output]\na:R <-[request]-> L:b'],
 ['metadata-config','---\nconfig:\n  architecture: {iconSize: 64, padding: 12}\n---\narchitecture-beta\ntitle Deployment\naccTitle: Services\naccDescr: Application and storage\nservice app(server)[Application]\nservice store(disk)[Files]\napp:B --> T:store'],
 ['alignment','architecture-beta\nservice a(server)[First]\nservice b(server)[Second]\nservice c(server)[Third]\na:R -- L:b\nb:R -- L:c\nalign row a b c'],
 ['missing-node','architecture-beta\nservice a(server)[App]\na:R -- L:missing',{},true],
 ['unfinished-label','architecture-beta\nservice a(server)[Open',{},true]
];
