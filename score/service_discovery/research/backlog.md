# Backlog

Known technical debt / deferred issues found while researching. Captured, not acted on.

- **Home of the LoLa integration glue.** The PoC's `service_discovery_compat.*` adapts the daemon
  to `mw/com`'s `impl::IServiceDiscovery`/`impl::IServiceDiscoveryClient`. Whether this adaptation
  layer should live inside `score/service_discovery/` (as today) or be moved into `score/mw/com/`
  (since a SEooC out-of-context arguably shouldn't contain `mw/com`-specific glue) is unresolved.
  Do not move code to "fix" this now — decide during architecture (Step 3) after the human weighs
  in on `problem_statement.md`'s Open Question 3.
- **Migration/coexistence of the legacy marker-file mechanism.** `score/mw/com/impl/bindings/lola/`
  currently implements creation/usage locking via flock on marker files
  (`ExclusiveFlockMutex`/`SharedFlockMutex`, see `partial_restart_path_builder.cpp`,
  `skeleton.{h,cpp}`, `proxy.h`) plus inotify-based discovery
  (`impl/bindings/lola/service_discovery/client/service_discovery_client.h`). Once the new daemon
  absorbs this responsibility, someone needs to plan removal/replacement of that binding code — out
  of scope for this cycle's problem statement, but real follow-up work once SD is implemented.
- **PoC dependability artifacts are not reusable as-is.** `score/mw/com/service_discovery/
  dependability/` has a fairly complete-looking TRLC/PlantUML set (feature reqs, component reqs,
  FMEA/FTA, unit design docs) that *passes verification* but is semantically wrong for the new
  component per the bootstrap prompt. It should not be quietly kept "for reference" beyond Step 0-2
  — once the new requirements exist, the PoC's dependability tree should eventually be removed or
  clearly marked superseded (not decided/scheduled here).
- **Batched calls beyond Register/lock ops.** The informal input's batching idea is discussed
  mainly in the context of startup-time Register/lock calls; whether `FindService`/
  `StartFindService` benefit from the same batching (e.g. resolving several service-types at once)
  is unaddressed and should be revisited once Open Question 1/2 are resolved.
