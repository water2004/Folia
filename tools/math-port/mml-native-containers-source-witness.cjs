// Executable behavioral witness against pinned @mathjax/src 4.1.3.
// Run: node tools/math-port/drafts/native-raii/typed-mml-containers/typed-mml-containers-source-witness.cjs
const assert = require('node:assert/strict');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const fs = require('node:fs');
const crypto = require('node:crypto');

function findRepositoryRoot(start = __dirname) {
  for (let current = path.resolve(start); current !== path.dirname(current); current = path.dirname(current)) {
    if (fs.existsSync(path.join(current, 'tools/math-port/README.md')) &&
        fs.existsSync(path.join(current, 'tools/mathjax-bundle/node_modules/@mathjax/src/package.json'))) return current;
  }
  return null;
}
const rootArg = process.argv.indexOf('--repo-root');
if (rootArg >= 0 && (!process.argv[rootArg + 1] || process.argv.length !== rootArg + 2))
  throw new Error('usage: node mml-native-containers-source-witness.cjs [--repo-root <repository>]');
const root = rootArg >= 0 ? path.resolve(process.argv[rootArg + 1]) : findRepositoryRoot();
assert.ok(root, 'pass --repo-root <repository> or run this script from within the repository');
const sourceRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const sourcePackage = require(path.join(sourceRoot, 'package.json'));
assert.equal(sourcePackage.version, '4.1.3');
const base = path.join(sourceRoot, 'mjs/core/MmlTree');
const sourceHashes = {
  'MmlFactory.js': 'c86ff14625a199c5acf5b779c51d2ab0ec6254c3b01edd55dae816a259ffe815',
  'MmlNodes/math.js': '23e4aad7730a5594b63b188653b522dc83d0544dfe9fb4356351d5cc812b143e',
  'MmlNodes/mathchoice.js': 'b1bed378153fb4782646651d65a66f7b3f297f111efdcb6d3f50ee09b7b2f430',
  'MmlNodes/maction.js': '5c85d5e839ae92873ab7069064704ef684e484f1c386f7c28f57b00734287b89',
  'MmlNodes/semantics.js': '8b1bcd378a13183849f1ba9ee4ecd250b751fc1d417abd425c90ca62b0c848c2',
  'MmlNodes/TeXAtom.js': '22b2347df4e976f3a3001d605abf4b24ac7fedded0deeb5f08436beb8ae3e5a5',
};
for (const [file, expected] of Object.entries(sourceHashes)) {
  const actual = crypto.createHash('sha256').update(fs.readFileSync(path.join(base, file))).digest('hex');
  assert.equal(actual, expected, `pinned Source hash ${file}`);
}
const load = (name) => import(pathToFileURL(path.join(base, name)).href);

(async () => {
  const [{ MmlFactory }, { MmlMath }, { MathChoice }, { MmlMaction }, { MmlAnnotation }, { TeXAtom }] =
    await Promise.all([
      load('MmlFactory.js'),
      load('MmlNodes/math.js'),
      load('MmlNodes/mathchoice.js'),
      load('MmlNodes/maction.js'),
      load('MmlNodes/semantics.js'),
      load('MmlNodes/TeXAtom.js'),
    ]);
  const factory = new MmlFactory();

  const math = factory.create('math');
  assert.equal(math.kind, 'math');
  assert.equal(math.linebreakContainer, true);
  assert.equal(math.linebreakAlign, '');
  const displayMath = factory.create('math', { display: 'block' }, [factory.create('mi')]);
  displayMath.setInheritedAttributes([], false, 0, false);
  assert.equal(displayMath.attributes.get('displaystyle'), true);

  const choice = factory.create('MathChoice');
  assert.equal(choice.kind, 'MathChoice');
  choice.setChildren(['0', '1', '2', '3'].map(() => factory.create('mi')));
  const parent = factory.create('mrow', {}, [choice]);
  const branches = choice.childNodes;
  choice.setInheritedAttributes([], false, 1, false);
  assert.equal(parent.childNodes[0], branches[2]);
  assert.equal(branches[2].parent, parent);
  const fractionalChoice = factory.create('MathChoice');
  fractionalChoice.setChildren(['0', '1', '2', '3'].map(() => factory.create('mi')));
  const fractionalParent = factory.create('mrow', {}, [fractionalChoice]);
  fractionalChoice.setInheritedAttributes([], false, 0.5, false);
  assert.equal(fractionalParent.childNodes[0].kind, 'mrow');

  const action = factory.create('maction', {}, [factory.create('mi'), factory.create('mo')]);
  assert.equal(action.attributes.get('selection'), 1);
  assert.equal(action.selected, action.childNodes[0]);
  action.nextToggleSelection();
  assert.equal(action.selected, action.childNodes[1]);
  action.nextToggleSelection();
  assert.equal(action.selected, action.childNodes[0]);
  const numericAction = factory.create('maction', {}, [
    factory.create('mi'), factory.create('mi'), factory.create('mi'),
  ]);
  numericAction.attributes.set('selection', '\u00A00x2');
  assert.equal(numericAction.selected, numericAction.childNodes[1]);
  numericAction.nextToggleSelection();
  assert.equal(numericAction.attributes.get('selection'), 3);
  assert.equal(numericAction.selected, numericAction.childNodes[2]);
  numericAction.attributes.set('selection', '1e21');
  assert.equal(numericAction.selected, numericAction.childNodes[2]);
  numericAction.nextToggleSelection();
  assert.equal(numericAction.attributes.get('selection'), 2);
  numericAction.attributes.set('selection', 1e21);
  numericAction.nextToggleSelection();
  assert.equal(numericAction.attributes.get('selection'), 2);
  assert.equal(action instanceof MmlMaction, true);
  const tooltip = factory.create('maction', { actiontype: 'tooltip' }, [
    factory.create('mi'), factory.create('mi'),
  ]);
  tooltip.setInheritedAttributes([], true, 2, false);
  assert.equal(tooltip.childNodes[0].attributes.get('displaystyle'), true);
  assert.equal(tooltip.childNodes[0].attributes.get('scriptlevel'), 2);
  assert.equal(tooltip.childNodes[1].attributes.get('displaystyle'), false);
  assert.equal(tooltip.childNodes[1].attributes.get('scriptlevel'), 1);
  const nonToggle = factory.create('maction', { actiontype: 'statusline', selection: 2 }, [factory.create('mi')]);
  nonToggle.verifyTree({ checkArity: false, checkAttributes: true, checkMathvariants: false,
    fullErrors: false, fixMmultiscripts: false, fixMtables: false });
  assert.equal(nonToggle.attributes.hasExplicit('selection'), false);
  assert.equal(nonToggle.attributes.get('selection'), 1);

  const annotation = factory.create('annotation');
  assert.equal(annotation instanceof MmlAnnotation, true);
  assert.equal(annotation.kind, 'annotation');
  assert.equal(annotation.properties.isChars, true);
  assert.equal(factory.create('semantics').notParent, true);
  const payload = factory.create('mi');
  const annotationXml = factory.create('annotation-xml', {}, [payload]);
  annotationXml.setInheritedAttributes([], false, 3, false);
  assert.equal(annotationXml.kind, 'annotation-xml');
  assert.equal(payload.attributes.get('scriptlevel'), 0);
  const atom = factory.create('TeXAtom');
  assert.equal(atom instanceof TeXAtom, true);
  assert.equal(atom.arity, -1);

  const fenced = factory.create('mfenced', { open: ' ( ', close: ' ) ', separators: ' ; ' }, [
    factory.create('mi'), factory.create('mi'), factory.create('mi'),
  ]);
  fenced.setInheritedAttributes([], true, 0, false);
  assert.equal(fenced.open.kind, 'mo');
  assert.equal(fenced.open.parent, fenced);
  assert.equal(fenced.open.texClass, 4);
  assert.equal(fenced.open.attributes.get('fence'), true);
  assert.equal(fenced.open.attributes.get('form'), 'prefix');
  assert.equal(fenced.open.childNodes[0].kind, 'text');
  assert.equal(fenced.open.childNodes[0].text, '(');
  assert.deepEqual(fenced.separators.map((node) => node.childNodes[0].text), [';', ';']);
  assert.equal(fenced.close.parent, fenced);
  assert.equal(fenced.close.texClass, 5);
  assert.equal(fenced.childNodes.length, 3);
  assert.equal(factory.getNodeClass('math'), MmlMath);

  const output = path.join(root, 'build/math-port/typed-mml-containers/source-witness.json');
  fs.mkdirSync(path.dirname(output), {recursive: true});
  fs.writeFileSync(output, JSON.stringify({package: sourcePackage.name, version: sourcePackage.version, sourceHashes, assertions: 45, status: 'passed'}, null, 2) + '\n');
  process.stdout.write(`typed MML containers pinned Source witness: 45 assertions passed; output ${path.relative(root, output)}\n`);
})().catch((error) => {
  process.stderr.write(`${error.stack || error}\n`);
  process.exitCode = 1;
});
