# MathJax reference dependencies

Run `npm ci --ignore-scripts` to restore the pinned upstream MathJax packages.
The native math build reads the upstream entity tables, and the comparison
tools in `tools/math-port/` use `entry.cjs` as their reference profile. The
comparison fixtures pin both this profile and the dependency lock by SHA-256.
