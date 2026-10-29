// Compile the small, pinned theme assignment language to ordinary C++.
// This is build-time provenance tooling, not a runtime JS interpreter.
import assert from 'node:assert/strict';
import { readFileSync, writeFileSync } from 'node:fs';
import path from 'node:path';
import { parse } from './oracle/node_modules/acorn/dist/acorn.mjs';
import { root, src } from './config-reference.mjs';
const constants=Object.fromEntries(parse(src('themes/erDiagram-oldHardcodedValues.ts'),
  {ecmaVersion:2022,sourceType:'module'}).body.flatMap(n=>n.declaration.declarations.map(d=>[d.id.name,d.init.value])));
const str = s => 'u'+JSON.stringify(s);
function expression(n) {
  switch(n.type) {
    case 'Literal': return typeof n.value==='string'?`Value(${str(n.value)})`:`Value(${String(n.value)})`;
    case 'Identifier':
      if(n.name==='i') return 'Value(i)';
      if(Object.hasOwn(constants,n.name)) return `Value(${str(constants[n.name])})`;
      throw Error('Unexpected identifier '+n.name);
    case 'MemberExpression': {
      const key=n.computed?expression(n.property):`Value(${str(n.property.name)})`;
      return n.object.type==='ThisExpression'?`get(${key})`:`property(${expression(n.object)}, ${key})`;
    }
    case 'ChainExpression': return expression(n.expression);
    case 'LogicalExpression': {
      const fn={'||':'fallback','??':'nullish','&&':'both'}[n.operator]; assert(fn);
      return `${fn}(${expression(n.left)}, [&] { return ${expression(n.right)}; })`;
    }
    case 'ConditionalExpression': return `(truthy(${expression(n.test)}) ? ${expression(n.consequent)} : ${expression(n.alternate)})`;
    case 'BinaryExpression': {
      const op={'+':'add','-':'subtract','*':'multiply','!==':'different'}[n.operator]; assert(op,n.operator);
      return `${op}(${expression(n.left)}, ${expression(n.right)})`;
    }
    case 'UnaryExpression': assert(['+','-'].includes(n.operator)); return `${n.operator==='-'?'negate':'numeric'}(${expression(n.argument)})`;
    case 'ObjectExpression': return 'object({'+n.properties.map(p=>{
      assert.equal(p.type,'Property'); assert(!p.computed && !p.method);
      return `{${str(p.key.name??p.key.value)}, ${expression(p.value)}}`;
    }).join(', ')+ '})';
    case 'CallExpression':
      assert.equal(n.callee.type,'Identifier');
      assert(['adjust','lighten','darken','invert','isDark','rgba','mkBorder'].includes(n.callee.name));
      return `${n.callee.name}(${n.arguments.map(expression).join(', ')})`;
    default: throw Error('Unexpected expression '+n.type);
  }
}
function statement(n, indent='    ') {
  const block=body=>body.map(s=>statement(s,indent+'    ')).join('');
  switch(n.type) {
    case 'ExpressionStatement': {
      const e=n.expression;
      if(e.type==='CallExpression') {
        assert.equal(e.callee.object.type,'ThisExpression'); assert.equal(e.callee.property.name,'updateColors');
        return indent+'update();\n';
      }
      assert.equal(e.type,'AssignmentExpression'); assert.equal(e.operator,'=');
      assert.equal(e.left.object.type,'ThisExpression');
      const key=e.left.computed?expression(e.left.property):`Value(${str(e.left.property.name)})`;
      return indent+`set(${key}, ${expression(e.right)});\n`;
    }
    case 'ForStatement': {
      assert.equal(n.init.declarations[0].id.name,'i'); assert.equal(n.init.declarations[0].init.value,0);
      assert.equal(n.test.operator,'<'); assert.equal(n.update.operator,'++');
      const bound=n.test.right.type==='Literal'?String(n.test.right.value):'color_limit()';
      if(bound==='color_limit()') assert.equal(n.test.right.property.name,'THEME_COLOR_LIMIT');
      return indent+`for (int i = 0; i < ${bound}; ++i) {\n`+block(n.body.body)+indent+'}\n';
    }
    case 'IfStatement':
      return indent+`if (truthy(${expression(n.test)})) {\n`+block(n.consequent.body)+indent+'}'+
        (n.alternate?' else {\n'+block(n.alternate.body)+indent+'}':'')+'\n';
    default: throw Error('Unexpected statement '+n.type);
  }
}
for(const name of ['default','dark']) {
  const source=src(`themes/theme-${name}.js`);
  const ast=parse(source,{ecmaVersion:2022,sourceType:'module'});
  const cls=ast.body.find(n=>n.type==='ClassDeclaration'); assert(cls);
  const methods=cls.body.body;
  const generated=['// Generated from pinned official Mermaid 12.2.0 by generate-themes.mjs. MIT.',
    '// Ordered assignments preserve constructor/update behavior; do not edit by hand.',
    `void Theme::construct_${name}() {\n`+methods.find(n=>n.kind==='constructor').value.body.body.map(n=>statement(n)).join('')+'}',
    `void Theme::update_${name}() {\n`+methods.find(n=>n.key.name==='updateColors').value.body.body.map(n=>statement(n)).join('')+'}', ''].join('\n');
  const file=path.join(root,`third_party/mermaid-cpp/src/theme_${name}.inc`);
  if(process.argv.includes('--write')) writeFileSync(file,generated);
  else assert.equal(readFileSync(file,'utf8').replaceAll('\r\n','\n'),generated,file);
}
console.log('Pinned default/dark C++ assignment generation verified');
