const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
function findRepositoryRoot(start = __dirname) {
  for (let current = path.resolve(start); current !== path.dirname(current); current = path.dirname(current)) {
    if (fs.existsSync(path.join(current, 'tools/math-port/README.md')) &&
        fs.existsSync(path.join(current, 'tools/mathjax-bundle/node_modules/@mathjax/src/package.json'))) return current;
  }
  return null;
}
const rootArg = process.argv.indexOf('--repo-root');
if (rootArg >= 0 && (!process.argv[rootArg + 1] || process.argv.length !== rootArg + 2)) {
  throw new Error('usage: node mml-native-consumers-source-witness.cjs [--repo-root <repository>]');
}
const root = rootArg >= 0 ? path.resolve(process.argv[rootArg + 1]) : findRepositoryRoot();
if (!root) throw new Error('pass --repo-root <repository> or run this script from within the repository');
const sourceRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax/src');
const { MmlFactory } = require(path.join(sourceRoot, 'cjs/core/MmlTree/MmlFactory.js'));

const tsRoot = path.join(sourceRoot, 'ts/core');
const paths = {
  registry: path.join(tsRoot, 'MmlTree/MML.ts'),
  node: path.join(tsRoot, 'MmlTree/MmlNode.ts'),
  factory: path.join(tsRoot, 'MmlTree/MmlFactory.ts'),
  htmlNode: path.join(tsRoot, 'MmlTree/MmlNodes/HtmlNode.ts'),
  texNodeFactory: path.join(sourceRoot, 'ts/input/tex/NodeFactory.ts'),
};
const hash = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');
const registry = MmlFactory.defaultNodes;
const factory = new MmlFactory();
const classes = Object.entries(registry).sort(([a], [b]) => a.localeCompare(b));
const kinds = classes.map(([kind]) => kind);
const entries = classes.map(([kind, Constructor]) => {
  const instance = new Constructor(factory);
  if (kind === 'XML' || kind === 'html') {
    instance.setXML({ sourceKind: kind }, { clone: value => ({ ...value }) });
  }
  let sourceCopy;
  let copyError;
  try {
    sourceCopy = instance.copy();
  } catch (error) {
    copyError = `${error?.name ?? 'Error'}: ${error?.message ?? String(error)}`;
  }
  return {
    kind,
    constructor: Constructor.name,
    constructedKind: instance.kind,
    sourceCopyConstructor: sourceCopy?.constructor?.name ?? null,
    sourceCopyKind: sourceCopy?.kind ?? null,
    sourceCopyDistinct: sourceCopy ? sourceCopy !== instance : null,
    copyError: copyError ?? null,
  };
});

const result = {
  upstream: { version: '4.1.3', commit: 'fb987178c2d279a99b0db5ea02e751691703955' },
  packageVersion: require(path.join(sourceRoot, 'package.json')).version,
  sourceHashes: Object.fromEntries(Object.entries(paths).map(([key, file]) => [key, hash(file)])),
  registryCount: classes.length,
  kindSetIsUnique: new Set(kinds).size === 43,
  entries,
};
if (result.packageVersion !== '4.1.3' || result.registryCount !== 43 || !result.kindSetIsUnique) {
  throw new Error(`Pinned MML registry check failed: ${result.packageVersion}, ${result.registryCount}`);
}
const output = path.join(root, 'build/math-port/typed-mml-consumers/source-witness.json');
fs.mkdirSync(path.dirname(output), { recursive: true });
fs.writeFileSync(output, JSON.stringify(result, null, 2) + '\n');
process.stdout.write(JSON.stringify({ output: path.relative(root, output), ...result }, null, 2) + '\n');
