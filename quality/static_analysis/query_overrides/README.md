# Pinned MISRA location query override

This pack imports the native MISRA C++ 2.62.0 default suite, excludes only
`cpp/misra/type-aliases-declaration`, and adds the corrected query with the same
ID. The effective suite remains 218 queries. CodeQL 2.21.4 and the locked library
versions remain unchanged.

`TypeAliasesDeclaration.ql` is derived from the corresponding upstream file in
[codeql-coding-standards v2.62.0](https://github.com/github/codeql-coding-standards/blob/v2.62.0/cpp/misra/src/rules/RULE-6-9-1/TypeAliasesDeclaration.ql).
Its upstream MIT license is retained in `LICENSE.md`. Primary finding selection
and rule predicates are unchanged. A located alias links to its definition; a
locationless template alias instance links to the actual type-name use selected
by the original query. No inferred definition location or SARIF deletion is used.

The helper is inline and bound to the alias/use pair already selected by the
query. The analyzer resolves locked dependencies from the native compiled pack's
`.codeql/libraries` directory explicitly, including qtil and the common pack.
Adding a second copy of the common pack introduces ambiguous pack resolution.

The exact-scope retained database comparison preserves 501 findings and removes
281 empty URI occurrences. All primary locations/fingerprints and original located
related links are retained. Two merged messages expand as restored locations
distinguish link targets. A fresh production database also passes all 218 queries
and contains no empty file URI locations. These measurements do not establish
safety acceptance or tool qualification.
