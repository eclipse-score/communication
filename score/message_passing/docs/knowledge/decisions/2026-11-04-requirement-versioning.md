# Distinguish Semantic Changes from Same-Parent Re-Pins

Status: proposed; recovered review convention for confirmation in this migration
Date: 2026-11-04
Scope: authored requirement versions and downstream parent-version pins

## Context

Earlier component reviews distinguished a requirement's own semantic change
from accepting a newer version of the same upstream requirement. Preserving
that distinction avoids a version cascade caused solely by refreshed pins.
This record carries that convention forward for review; it is not a new rule
proven by the validator or a replacement for the pinned schema.

## Proposed Convention

- Bump a record's version when its description, safety classification, or
  parent identity changes.
- Review downstream consequences and refresh accepted parent-version pins.
- A pin-only refresh to the same parent identity does not by itself require
  bumping the referencing record's own version.
- Changing the parent identity is a semantic change even if the description
  remains unchanged. Do not use this exception for a rewritten requirement.

## Consequences

Every re-pin still needs review of refinement and ASIL compatibility; there is
no automatic acceptance of a changed upstream contract. Confirm this recovered
convention with the requirements reviewer before applying it as mandatory
policy, especially if current tooling/project guidance has changed.

## Evidence and Review Boundary

[Current requirement records](../../../dependability/requirements/component_requirements.trlc)
and [their parents](../../../dependability/requirements/feature_requirements.trlc)
provide concrete inputs for a review. The schema's version/pinning rules are in
the resolved tooling dependency described in [tooling](../tooling.md).
The earlier rationale is recovered design context, not a newly reproduced
traceability check or a linked approval PR. This is why the status is proposed.