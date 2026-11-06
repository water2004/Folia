export const cases = [
  ['basic', 'wardley-beta\ntitle Example\nanchor User [1.0, 0.1]\ncomponent API [0.7, 0.65]\ncomponent Database [0.4, 0.9]\nAPI --> Database'],
  ['links', 'wardley-beta\ncomponent A [0.8, 0.2]\ncomponent B [0.5, 0.6]\nA -.-> B\nA +> B\nA +< B\nA +<> B'],
  ['evolution', 'wardley-beta\nevolution Genesis@0.15 -> Custom Built@0.42 -> Product@0.72 -> Commodity@1.0\ncomponent Service [0.8, 0.55] (buy) inertia\nevolve Service 0.9\nnote "Market" [0.4, 0.7]'],
  ['pipeline', 'wardley-beta\ncomponent Service [0.8, 0.55]\npipeline Service {\n  component Design [0.2]\n  component Build [0.45]\n  component Operate [0.7]\n}\nannotations [0.85, 0.12]\nannotation 1, [0.9, 0.8] "Scale later"\naccelerator "Automation" [0.6, 0.7]\ndeaccelerator "Legacy" [0.4, 0.25]'],
  ['strategies', 'wardley-beta\ncomponent Built [0.4, 0.2] (build)\ncomponent Bought [0.55, 0.4] (buy)\ncomponent Vendor [0.7, 0.6] (outsource)\ncomponent Market [0.85, 0.8] (market)'],
  ['quoted-labels', 'wardley-beta\ncomponent "API Server" [0.8, 0.35] label [-12,14]\ncomponent "Data Store" [0.5, 0.75]\n"API Server" +\'customer\'> "Data Store"'],
];
export const invalid = [
  'component A [0.8, 0.5]',
  'wardley-beta\ncomponent A [0.8 0.5]',
  'wardley-beta\ncomponent A [1, 0.5]',
  'wardley-beta\npipeline Parent {\ncomponent Item [0.5]',
];
