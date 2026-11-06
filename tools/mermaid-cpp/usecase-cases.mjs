export const cases=[
 ['basic','usecase-beta\nactor User\nactor Admin\nUser --> Login\nLogin ..> : include Verify\n'],
 ['quoted','usecase-beta\ndirection TB\nactor "Customer Service"\n"Customer Service" --> "Sign In"\n'],
 ['mixed','usecase-beta\nactor Customer\nactor Admin\nCustomer --> Browse\nCustomer --> Checkout\nCheckout ..> : include Verify\nAdmin -- Audit\n'],
 ['invalid-header','Usecase\nA',true],
 ['invalid-empty','usecase-beta\nnot a statement',true]
].map(([name,source,failure=false])=>({name,source,failure}));
