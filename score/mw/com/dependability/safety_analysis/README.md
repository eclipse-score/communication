# Communication Assumptions of Use Forwarding

This package exposes `//score/mw/com/dependability/safety_analysis:aous` for external Config Management consumption.

## Why public?

Config Management needs to resolve Communication `ScoreReq.AoU` records from `aou.trlc` in its own `component_requirements` `derived_from` references. Per the pinned `@score_tooling//bazel/rules/rules_score/docs/user_guide/assumptions_of_use.rst` (score_tooling 2.3.1), a received AoU is handled by adding `Package.Record@version` to a `CompReq.derived_from` and listing the defining `assumptions_of_use` target in the `component_requirements` target's `deps`. That dependency must be visible across repositories, hence the public alias.

## Forwarding semantics

The pinned tooling provides two forwarding mechanisms:

* Automatic forwarding (own AoUs): a dependable element's own `assumptions_of_use` are automatically forwarded to every element listing it in `deps`.
* Chain-forwarding (received AoUs): a dependable element can selectively forward received AoUs via an `aou_forwarding` YAML with a mandatory justification.

The existing `//score/mw/com/dependability:mw_com` dependable element already declares `assumptions_of_use = ["//score/mw/com/dependability/safety_analysis:aous"]`. External consumers that depend on `mw_com` receive those AoUs automatically. Consumers that need to reference the AoU directly in their `component_requirements` can list the public `aous` target.

## Safety analysis dependency

`aous_internal` depends on `:mw_com_safety_analysis` to resolve `mw_com_fta.<RootCause>` references in `aou.trlc`. This processing is not duplicated by the public alias. Do not recreate a parallel safety analysis target or copy AoU IDs.

## Draft API addition

The public `aous` alias is a draft API addition for Config Management and requires offline engineering acceptance. The internal `aous_internal` target and `aous_internal_test` remain restricted.

## Regression coverage

`aou_forwarding_test/component_requirements.trlc` simulates a consumer `CompReq` deriving from `Communication.MonotonicSemiDynamicMemoryAllocation@1` and depends on the public `aous` target. This exercises the tooling deps/provider path. Cross-repository verification is a separate obligation: the production Config Management provider requirements at e82ec2750d7e9a9dd18edbfe6f5a78be57c22d80 resolve this alias and pass their native TRLC test after alignment with score_tooling 2.3.1. Its complete dependable element still requires migration of legacy placeholder safety artifacts and review of received-AoU handling. The fixture does not supply those engineering decisions.
