export const cases=[
 ['basic','usecase-beta\nactor User\nactor Admin\nUser --> Login\nLogin ..> : include Verify\n'],
 ['quoted','usecase-beta\ndirection TB\nactor "Customer Service"\n"Customer Service" --> "Sign In"\n'],
 ['mixed','usecase-beta\nactor Customer\nactor Admin\nCustomer --> Browse\nCustomer --> Checkout\nCheckout ..> : include Verify\nAdmin -- Audit\n'],
 ['basic-elk','---\nconfig:\n  layout: elk\n---\nusecase-beta\nactor User\nactor Admin\nUser --> Login\nLogin ..> : include Verify\n'],
 ['boundary-elk','---\nconfig:\n  layout: elk\n---\nusecase-beta\nactor Customer\nsystemBoundary sb[Checkout] @{ type: package }\nCheckout(Place order)\nend\nCustomer --> Checkout\nnote for Checkout additional requirements\n'],
 ['variants','usecase-beta\nactor Hollow(Hollow) @{ type: hollow, business: true }\nactor Awesome(Awesome) @{ type: awesome }\nItem[Review]\nHollow --o Item\nAwesome x-- Item\n'],
 ['json','usecase-beta\nactor User\njson Record @{\n  "z": "last",\n  "a": [1, true],\n  "nested": {"b": null}\n}\nUser --> Record\n'],
 ['neo','---\nconfig:\n  layout: elk\n  look: neo\n---\nusecase-beta\nactor User\nactor Admin\nUser --> Login\nLogin ..> : include Verify\n'],
 ['neo-variants','---\nconfig:\n  look: neo\n---\nusecase-beta\nactor Hollow(Hollow) @{ type: hollow }\nactor Awesome(Awesome) @{ type: awesome }\nItem[Review]\nHollow --o Item\nAwesome x-- Item\n'],
 ['neo-palette','---\nconfig:\n  layout: elk\n  look: neo\n  theme: redux-color\n  usecase:\n    colorScheme: rotate\n---\nusecase-beta\nactor Member, Person\nsystemBoundary App\n  Task[Verify]\nend\nMember --|> Person\nMember --> Task\n'],
 ['rough-basic','---\nconfig:\n  look: handDrawn\n  handDrawnSeed: 7\n---\nusecase-beta\nactor User\nactor Admin\nUser --> Login\nLogin ..> : include Verify\n'],
 ['rough-boundary','---\nconfig:\n  look: handDrawn\n  handDrawnSeed: 7\n---\nusecase-beta\nactor Customer\nsystemBoundary sb[Checkout] @{ type: package }\nCheckout(Place order)\nend\nCustomer --> Checkout\nnote for Checkout additional requirements\n'],
 ['rough-json-elk','---\nconfig:\n  layout: elk\n  look: handDrawn\n  handDrawnSeed: 7\n---\nusecase-beta\nactor User\njson Record @{ "z": "last", "a": [1, true] }\nUser --> Record\n'],
 ['invalid-header','Usecase\nA',true],
 ['invalid-empty','usecase-beta\nnot a statement',true],
 ['invalid-json','usecase-beta\njson Record @{ "a": [1,] }\n',true]
].map(([name,source,failure=false])=>({name,source,failure}));
