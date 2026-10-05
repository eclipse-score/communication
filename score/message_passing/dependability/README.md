# Dependability conventions

This directory holds authored requirements, design, and safety artifacts for the
message-passing dependable element. They are the current contract/model, not proof
that implementation or every trace relationship already agrees.

Use the [knowledge depot](../maintenance/README.md) for current context and sources.
Recorded human intent, authored obligations, implementation, and generated evidence
can disagree. Preserve the discrepancy and reconcile it through an authorized cycle;
neither a working note nor current code silently overrides a requirement.

## Keep artifacts focused on technical content

Feature scope, investigation history, cycle status, and acceptance belong under
`../work/<feature>/`. Enduring design intent and rationale belong in the maintained
depot or the appropriate technical artifact. Do not embed temporary work-directory
links or authoring logs in TRLC/PlantUML. Keep actual requirement rationale where the
schema calls for it.

## Assumed-system requirement convention

An `AssumedSystemReq` states a capability an integrator assessing this subsystem
would check from outside: communication model, interaction patterns, production-error
and integration-misconfiguration detectability, platform support, or access control.
Method-level behavior and internal failure handling belong below that level.
