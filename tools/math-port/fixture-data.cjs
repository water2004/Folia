'use strict';

const fs = require('node:fs');
const path = require('node:path');
const assert = require('node:assert/strict');

// Wire format shared by official reference-data generators. This module never
// evaluates native output and is not needed to build or execute native tests.
const byteField = (value, listElement = false) => {
  let result = '';
  for (const byte of Buffer.from(value, 'utf8')) {
    result += byte >= 32 && byte < 127 && byte !== 92 && byte !== 35 &&
      (!listElement || byte !== 124)
      ? String.fromCharCode(byte) : `\\x${byte.toString(16).padStart(2, '0')}`;
  }
  return result;
};

const binary64 = value => {
  const buffer = Buffer.alloc(8);
  buffer.writeDoubleBE(value);
  return buffer.toString('hex');
};

const utf16Field = value => {
  let result = '';
  for (let i = 0; i < value.length; ++i) {
    const unit = value.charCodeAt(i);
    result += unit >= 32 && unit < 127 && unit !== 92 && unit !== 35
      ? value[i] : `\\u${unit.toString(16).padStart(4, '0')}`;
  }
  return result;
};

const table = (columns, records, metadata = []) => [
  '# Pinned reference observations; values exported without normalization.',
  `# ${columns.join('\t')}`,
  ...metadata.map(value => `# ${value}`),
  ...records.map(row => {
    if (row.length !== columns.length || row.some(value => /[\t\r\n]/.test(String(value))))
      throw new Error('Reference TSV row has invalid columns or unescaped separators');
    return row.join('\t');
  }),
  ''
].join('\n');

const writeTables = (directory, definitions, write = process.argv.includes('--write-fixture')) => {
  for (const [name, {columns, rows, metadata = []}] of Object.entries(definitions)) {
    const file = path.join(directory, `${name}.tsv`);
    const expected = table(columns, rows, metadata);
    if (write) {
      fs.mkdirSync(directory, {recursive: true});
      if (!fs.existsSync(file) || fs.readFileSync(file, 'utf8') !== expected)
        fs.writeFileSync(file, expected);
    } else assert.equal(fs.readFileSync(file, 'utf8'), expected,
      `Pinned reference fixture differs: ${file}; review before --write-fixture`);
  }
};

module.exports = {byteField, utf16Field, binary64, table, writeTables};
