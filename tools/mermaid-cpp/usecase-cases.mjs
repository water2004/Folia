export const cases=[
 ['basic','usecase-beta\nactor User\nactor Admin\nUser --> Login\nLogin ..> : include Verify\n'],
 ['quoted','usecase-beta\ndirection TB\nactor "Customer Service"\n"Customer Service" --> "Sign In"\n'],
 ['mixed','usecase-beta\nactor Customer\nactor Admin\nCustomer --> Browse\nCustomer --> Checkout\nCheckout ..> : include Verify\nAdmin -- Audit\n'],
 ['basic-elk','---\nconfig:\n  layout: elk\n---\nusecase-beta\nactor User\nactor Admin\nUser --> Login\nLogin ..> : include Verify\n'],
 ['boundary-elk','---\nconfig:\n  layout: elk\n---\nusecase-beta\nactor Customer\nsystemBoundary sb[Checkout] @{ type: package }\nCheckout(Place order)\nend\nCustomer --> Checkout\nnote for Checkout additional requirements\n'],
 ['variants','usecase-beta\nactor Hollow(Hollow) @{ type: hollow, business: true }\nactor Awesome(Awesome) @{ type: awesome }\nItem[Review]\nHollow --o Item\nAwesome x-- Item\n'],
 ['invalid-header','Usecase\nA',true],
 ['invalid-empty','usecase-beta\nnot a statement',true]
].map(([name,source,failure=false])=>({name,source,failure}));
