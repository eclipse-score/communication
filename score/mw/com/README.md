# Communication Middleware (mw::com)

## Overview

`LoLa`/`mw::com`: A high-level communication middleware as a partial implementation of the Adaptive AUTOSAR
Communication Management specification. Also known as `ara::com` (as this is the related namespace in AUTOSAR).

## General introduction

`mw::com` and `LoLa` are **synonyms** right now. While the former just expresses the namespace (`mw::com` = middleware
communication), the latter is a hint on its technical implementation/unique selling point (`LoLa` = Low Latency).
Since `LoLa`/`mw::com` is our in-house implementation of the AUTOSAR standard, which &ndash; according to this standard
&ndash; resides under `ara::com`, the namespace similarity (`mw::com` ↔ `ara::com`) is **intentional**.

Low latency in our `mw::com` implementation is realized by basing our underlying `technical binding` on zero-copy
shared-memory based communication.
The notion of a `technical binding` also comes from the `ara::com` AUTOSAR standard:
`ara::com` just defines a standardized user-level API and its behavior (which we generally follow with our
`mw::com` implementation). **How** an implementation realizes the public API within its lower layers
(`technical binding`) is completely up to it.
It is even common, that implementations of the `ara::com` standard come up with several different `technical binding`s
(e.g. one for local and one for network communication).

## Documentation for users

If you are an adaptive application developer in the IPNEXT project, and you want to use `mw::com` to do local
interprocess communication, you will find the user documentation here: LINK TODO.

Documentation of `mw::com`s public API with usage examples can be found [here](./doc/user_facing_API_examples.md).
If you want to see an example app, which uses LoLa to establish shared-memory communication between a service and a
client you can take a look at our macro benchmark app which resides in [this](./performance_benchmarks/macro_benchmark) directory.


## Requirements

Our single source of truth for requirements is **_Codebeamer_**.

- The SW-Component requirements for `mw::com` are located [here](broken_link_c/issue/5672603).
- The safety requirements for `mw::com` are located [here](broken_link_c/issue/5823087).
- The assumptions of use (AoU) for mw::com are located [here](broken_link_c/issue/6221478).

## Useful links

- [High level architecture concept](../../docs/features/ipc/lola/ipnext_README.md)
- [Detailed design](broken_link_g/swh/ddad_platform/tree/master/aas/mw/com/design)
