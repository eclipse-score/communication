# Assumed System Scope for Service Discovery

This directory captures assumptions that are external to the service discovery implementation and therefore not guaranteed by this component itself.

## Scope Boundary

The service discovery dependable element specifies daemon behavior, protocol behavior, and registry behavior.
The following capabilities are assumed to be provided by the platform or operating environment:

1. Same-host inter-process message passing between daemon and client processes.
2. Operating-system process identity information (UID and PID) for each daemon-facing session.
3. Monotonic time for lease and liveness supervision.

## Why These Are Assumptions

These capabilities are required to satisfy integrity partitioning, stale-entry cleanup, and deterministic daemon interaction but are not implementable within this component boundary.
If any assumption is violated, service discovery correctness and safety guarantees can degrade.

## Assumptions of Use

Assumptions of use in aous.trlc define integration obligations for startup ordering and single-authority deployment.
Integrators shall ensure those conditions are met in the target system configuration.
