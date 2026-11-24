# Setoptions observed fields

All 96 original full-profile fields retain their package, key, input and
observed kind. `tex-setoptions-source.cjs --write-fixture` generates this table
from the same pinned Source enumeration and scalar writes. UTF-16 fields use
reversible `\uHHHH` escapes; native binding names use escaped byte fields.

The native member-pointer path identifies the original compiled field binding.
`support/setoptions_bindings.hpp` resolves only those known paths to their
original `optionCase<...>` instantiations. The native seed and match templates
remain unchanged executable C++; all original member sequences are preserved.
Unknown paths are rejected. Reference input storage remains alive for the
string views in every bound case. No callback code is loaded from the data.
