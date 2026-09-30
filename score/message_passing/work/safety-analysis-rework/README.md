# Safety-analysis rework

Objective: reconcile message-passing failure analysis and controls with the intended
safety behavior and actual shared/QNX implementation.

Boundary: review the eight existing failure modes and FTAs, then author only agreed
controls, AoUs, and affected dependencies. An informal system-level FTA is optional.
This record migration does not authorize product changes.

Current cycle: [Ground the FTA redo](cycles/01-grounded-fta.md).

Start from the [subsystem depot](../../maintenance/README.md), especially current
safety intent and discrepancies. The feature is complete only when the agreed
analysis and downstream evidence are reviewed, durable knowledge is consolidated,
and residual work has an explicit disposition.
