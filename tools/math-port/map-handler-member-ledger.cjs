// Capture the actual pinned TypeScript AST, including object-literal methods.
'use strict';
const fs=require('node:fs'),path=require('node:path'),cp=require('node:child_process'),crypto=require('node:crypto'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const source='ts/input/tex/MapHandler.ts',fresh=JSON.parse(cp.execFileSync(process.execPath,[path.join(__dirname,'member-ledger.cjs'),source],{encoding:'utf8'}));
const ts=require(path.join(root,'build/math-port-tooling/node_modules/typescript')),bytes=fs.readFileSync(path.join(root,'.reference/MathJax-src',source));
const ast=ts.createSourceFile(source,bytes.toString('utf8'),ts.ScriptTarget.Latest,true,ts.ScriptKind.TS);
const variable=ast.statements.flatMap(s=>ts.isVariableStatement(s)?Array.from(s.declarationList.declarations):[]).find(d=>d.name.getText(ast)==='MapHandler');
assert.ok(variable&&ts.isObjectLiteralExpression(variable.initializer));
fresh.files[0].object_methods=variable.initializer.properties.map(m=>{assert.ok(ts.isMethodDeclaration(m)&&m.body);return {owner:'MapHandler',name:m.name.getText(ast),line:ast.getLineAndCharacterOfPosition(m.getStart(ast)).line+1,end_line:ast.getLineAndCharacterOfPosition(m.end).line+1,member_sha256:crypto.createHash('sha256').update(m.getText(ast)).digest('hex')};});
process.stdout.write(JSON.stringify(fresh)+'\n');
