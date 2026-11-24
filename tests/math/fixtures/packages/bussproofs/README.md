# bussproofs reference data

These tables retain every original tree, error, runtime-error, sequential-state,
direct-operation and binding observation. Typed fixture adapters load them lazily;
tests retain their callback identity checks and original assertions.

`bindings.tsv` declares argument counts and, for Physics, optional characters and
attribute counts. `arguments.tsv` stores each argument by binding and argument
index. Physics `attributes.tsv` additionally stores the attribute name. Every
primitive has an explicit `null`, `boolean`, `number`, or `string` tag. Numbers
use exact binary64 hexadecimal bits; strings preserve UTF-16 units. The loader
checks ordering and counts before handing values to the test.

The original data and runtime readers were compared field-for-field, including
variant tags, negative-zero-sensitive double bits, and every ordered nested value.
The independent MathJax generator emits the same tables and requires explicit
`--write-fixture` to update them. Native tests consume committed data without Node.
