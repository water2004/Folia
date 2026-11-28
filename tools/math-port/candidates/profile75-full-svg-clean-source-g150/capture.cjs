'use strict';
// Keep G149's continuous capture intact and build clean per-case Source
// records by running the same pinned CJS setup once in a child process per case.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const cp = require('node:child_process');
const assert = require('node:assert/strict');

const root = path.resolve(__dirname, '../../../..');
const outputPath = path.resolve(root, process.argv[2] || 'build/math-port/profile75-full-svg-source-clean-g150.json');
const smoke = process.argv.includes('--smoke');
const sourcePath = path.join(root, 'build/math-port/profile75-full-svg-source-g149.json');
const inputPath = path.join(root, 'build/math-port/common-layout-profile-root-locked.json');
const scopePath = path.join(root, 'tools/math-port/profiles/scope.json');
const profileLockPath = path.join(root, 'tools/math-port/common-layout-reference/profile-source-lock.json');
const corpusPath = path.join(root, 'tools/math-port/common-layout-reference/profile-cases.json');
const dependencyLockPath = path.join(root, 'tools/mathjax-bundle/package-lock.json');
const packageRoot = path.join(root, 'tools/mathjax-bundle/node_modules/@mathjax');
const sourceRoot = path.join(root, '.reference/MathJax-src');
const workerPath = path.join(__dirname, 'worker.cjs');
const digest = bytes => crypto.createHash('sha256').update(bytes).digest('hex');
const hashFile = file => digest(fs.readFileSync(file));
const scope = JSON.parse(fs.readFileSync(scopePath, 'utf8'));
const profileLock = JSON.parse(fs.readFileSync(profileLockPath, 'utf8'));
const original = JSON.parse(fs.readFileSync(sourcePath, 'utf8'));
const inputCapture = JSON.parse(fs.readFileSync(inputPath, 'utf8'));
const groups = ['continuous', 'fresh-session-per-case'];

assert.equal(hashFile(scopePath), profileLock.scopeHash);
assert.equal(hashFile(profileLockPath), 'acd3decb56eaeff289493133856413a704e7cc75d67fbc50de221cf716a0bf72');
assert.equal(hashFile(corpusPath), profileLock.corpusSha256);
assert.equal(hashFile(inputPath), '9f58d09411512b4682b1f0049e40d6a47844ddffb9794eda52e2700998e0d740');
assert.equal(hashFile(dependencyLockPath), scope.dependency_lock_sha256);
assert.equal(scope.upstream.version, '4.1.3');
assert.equal(scope.upstream.commit, 'fb987178c2d279a99b0db5ea02e751691703955e');
assert.equal(cp.execFileSync('git', ['rev-parse', 'HEAD'], { cwd: sourceRoot, encoding: 'utf8' }).trim(), scope.upstream.commit);
assert.equal(original.schema, 1);
assert.equal(original.source.commit, scope.upstream.commit);
assert.equal(original.source.scopeSha256, hashFile(scopePath));
assert.equal(original.source.profileSha256, hashFile(path.join(root, scope.profile.file)));
assert.equal(original.source.sourceLockSha256, hashFile(profileLockPath));
assert.equal(original.source.corpusSha256, hashFile(corpusPath));
assert.equal(original.source.caseInventorySha256, hashFile(inputPath));
assert.deepStrictEqual(original.source.dependencies, profileLock.dependencies);
for (const dependency of profileLock.dependencies)
  assert.equal(hashFile(path.join(packageRoot, dependency.file)), dependency.sha256, dependency.file);
for (const locked of inputCapture.sourceLocks) {
  const sourceLock = scope.files.find(file => file.source === locked.file);
  assert.ok(sourceLock, `missing Source lock ${locked.file}`);
  assert.equal(locked.sha256, sourceLock.source_sha256, locked.file);
  assert.equal(hashFile(path.join(sourceRoot, locked.file)), locked.sha256, locked.file);
}
assert.deepStrictEqual(original.groups, groups);
assert.equal(inputCapture.records.length, 75);
assert.equal(original.records.length, 150);

const inventory = inputCapture.records;
const selected = smoke ? [39, 53, 58, 69] : inventory.map((_, index) => index);
const cleanRows = new Array(75);
const closure = new Map();
for (const index of selected) {
  const run = cp.spawnSync(process.execPath, [workerPath, String(index)], {
    cwd: root, encoding: 'utf8', maxBuffer: 32 * 1024 * 1024, timeout: 30000,
    windowsHide: true
  });
  assert.equal(run.status, 0, `case ${index} worker failed: ${run.stderr || run.error?.message || run.stdout}`);
  const observation = JSON.parse(run.stdout);
  const row = observation.record, expected = inventory[index];
  for (const field of ['group', 'name', 'tex']) {
    const expectedValue = field === 'group' ? 'fresh-session-per-case' : expected[field];
    assert.equal(row[field], expectedValue, `case ${index}: ${field}`);
  }
  assert.equal(row.index, index, `case ${index}: index`);
  assert.equal(row.display, expected.display ?? false);
  assert.equal(row.em, expected.em ?? 16);
  assert.equal(row.ex, expected.ex ?? (expected.em ?? 16) * .5);
  assert.equal(row.width, expected.width ?? 1280);
  cleanRows[index] = row;
  for (const dependency of observation.loadedDependencies) {
    const locked = profileLock.dependencies.find(item => item.file === dependency.file);
    assert.ok(locked, `case ${index} loaded unlocked dependency ${dependency.file}`);
    assert.equal(dependency.sha256, locked.sha256, `case ${index}: ${dependency.file}`);
    const prior = closure.get(dependency.file);
    assert.ok(prior === undefined || prior === dependency.sha256, `dependency changed between workers: ${dependency.file}`);
    closure.set(dependency.file, dependency.sha256);
  }
  const continuous = original.records[index];
  if (smoke) {
    assert.equal(continuous.group, 'continuous');
    assert.deepStrictEqual({ svg: row.svg, error: row.error, mmlError: row.mmlError },
      { svg: continuous.svg, error: continuous.error, mmlError: continuous.mmlError },
      `${expected.name}: independent clean process should match the clean first output`);
  }
}

if (smoke) {
  process.stdout.write(JSON.stringify({ smoke: true, cases: selected.map(i => inventory[i].name),
    cleanProcessMatches: selected.length, parityClaim: false }) + '\n');
  process.exit(0);
}

for (let index = 0; index < 75; ++index) assert.ok(cleanRows[index], `missing fresh row ${index}`);
assert.deepStrictEqual([...closure].sort(([a], [b]) => a.localeCompare(b)),
  profileLock.dependencies.map(row => [row.file, row.sha256]).sort(([a], [b]) => a.localeCompare(b)),
  'union of clean worker dependency closures differs from the pinned lock');

const capture = {
  schema: 1,
  source: {
    ...original.source,
    runtimeBoundaries: {
      continuous: { isolation: 'shared-owner-across-cases', owner: 'one MathJax module realm and one HTMLMathDocument' },
      'fresh-session-per-case': { isolation: 'isolated-owner-per-case', owner: 'one clean process/module realm, TeX/SVG jax pair, and HTMLMathDocument per record' }
    },
    captureMethod: {
      continuousInput: path.relative(root, sourcePath).replaceAll(path.sep, '/'),
      freshWorker: path.relative(root, workerPath).replaceAll(path.sep, '/'),
      freshWorkerSha256: hashFile(workerPath),
      originalG149Sha256: hashFile(sourcePath),
      cleanProcesses: 75
    }
  },
  groups,
  records: [...original.records.slice(0, 75), ...cleanRows]
};
fs.mkdirSync(path.dirname(outputPath), { recursive: true });
fs.writeFileSync(outputPath, JSON.stringify(capture, null, 2) + '\n');
process.stdout.write(JSON.stringify({ output: outputPath, casesPerGroup: 75,
  records: capture.records.length, cleanProcesses: 75,
  continuousRecordsRetained: true, sourceDependencies: closure.size }) + '\n');
