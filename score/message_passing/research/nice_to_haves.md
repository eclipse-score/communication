# Message Passing — Nice to Haves

Long-lived, shared across all actualization cycles. Possible future improvements noticed in
passing, not (yet) inconsistencies or defects — see `backlog.md` for those.

## From the 2026-08-31 baseline snapshot

- `client-server.md` calls out passing shared-memory region handles between processes (native in
  Unix Domain Socket messaging; `shm_create_handle()` on QNX) as a possible future extension, out
  of scope for the first release.
- `client-server.md` calls out a watchdog-friendly notification interface (paired client-side
  arm / server-side disarm callbacks around `Notify`) as a possible future extension to give
  timing guarantees for notification messages, which today have none.
- `client-server.md` calls out larger shared thread pools serving multiple `Server`/
  `ClientConnection` instances concurrently as a possible future extension beyond the current
  single-thread-or-single-pair-of-threads model.
