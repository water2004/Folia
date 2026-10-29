// Bounded public ZenUML examples: docs plus Mermaid's existing nested-call e2e.
export const cases=[
 ['async','zenuml\ntitle Demo\nAlice->John: Hello John, how are you?\nJohn->Alice: Great!\nAlice->John: See you later!'],
 ['participants','zenuml\ntitle Participants\n@Actor A as Alice\n@Database B as Store\nA->B: Save\nB->A: Saved'],
 ['nested','zenuml\nA.method() {\n if(x) {\n  B.method() {\n   selfCall() { return X }\n  }\n }\n}'],
 ['creation-return','zenuml\nnew A1\nnew A2(with, parameters)\nvalue = A1.read()\nA2.write(value) { return saved }'],
 ['loop-alt','zenuml\nClient.send() {\n while(waiting) {\n  Service.poll()\n }\n if(ok) {\n  Service.accept()\n } else {\n  Service.reject()\n }\n}']
];
