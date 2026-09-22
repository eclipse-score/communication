# Work Log — feature-req-notify-split-and-crossplatform-qm

- 2026-09-22: Cycle opened. Human said "let's move to refining feature requirements"; asked to pick
  from the two deferred backlog items vs. something else; human chose both. Wrote
  `change_request.md` (Step 0). Next: Step 1 impact analysis.
- 2026-09-22: Wrote `impact_analysis.md` (Step 1) and asked for confirmation on 3 concrete
  decisions (new FeatReq names, correct-in-place vs. retire-and-replace for the two wrong
  `CompReq`s, whether the new QM FeatReq should derive into `external_component_requirements.trlc`).
  Human answered: default names accepted; retire-and-replace; wire `TransportMechanismOnLinux` to
  derive from the new QM FeatReq.
- 2026-09-22: Step 2-4 edits applied — rewrote `SynchronousUnidirectionalCommunication`/
  `AsynchronousUnidirectionalCommunication` (`@2`→`@3`), added `AsynchronousServerNotification` and
  `QMImplementationOnNonQnxOS` FeatReqs, cascaded re-pins in `component_requirements.trlc`, retired
  and replaced the two wrong Send `CompReq`s, re-pointed `IServerConnectionNotifyAPI`, wired
  `TransportMechanismOnLinux` (`external_component_requirements.trlc`) to the new QM FeatReq.
  Confirmed via repo-wide search that no FTA alias/lobster-tracing/coverage-lock entry referenced
  the retired identifiers.
- 2026-09-22: Step 6 validation — `bazel test` on requirements/assumed_system packages: 4/4 PASSED.
  `bazel build //score/message_passing/dependability:dependable_element_message_passing` succeeded;
  all warnings pre-existing and unrelated. Step 7: wrote `evidence_bundle.md`, updated
  `problem_statement.md` changelog, `backlog.md` (marked both items resolved), `nice_to_haves.md`
  (logged the empty-reply-bidirectional alternative design idea). Cycle CLOSED pending final human
  acceptance.
