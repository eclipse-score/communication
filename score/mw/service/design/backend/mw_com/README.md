# mw::com Backend Details

This document refers to mw::com-specific design details that are not generic.

## Service Discovery

### Find Strategies

As mentioned in the generic design document, the user is supposed to provide a Strategy class that will determine how to
connect to the desired service. The mw::com backend provides standard classes (see below), but the user can also
implement custom strategies if needed.

#### SingleInstantiationStrategy

This strategy is used when exactly one remote mw::com proxy instance is expected.
Implementation reference:
[`single_instantiation_strategy.h`](../../../backend/mw_com/single_instantiation_strategy.h).

For usage patterns, refer to
[`single_instantiation_strategy_test.cpp`](../../../backend/mw_com/single_instantiation_strategy_test.cpp).

#### MultipleInstantiationStrategy

This strategy is used when multiple remote mw::com proxy instances are expected during an application's lifecycle.
Implementation reference:
[`multiple_instantiation_strategy.h`](../../../backend/mw_com/multiple_instantiation_strategy.h).

For usage patterns, refer to
[`multiple_instantiation_strategy_test.cpp`](../../../backend/mw_com/multiple_instantiation_strategy_test.cpp).


The runtime integration and callback/lifetime handling are implemented by
[`proxy_holder.h`](../../../backend/mw_com/proxy_holder.h).

## Service Export

For service offering, the mw::com backend uses the generic `ProvidedServicesContainer` workflow. A user-defined
mw::com service implements the `ManagedService` interface directly, holding the generated Skeleton as a member
(composition), rather than inheriting from it:

* `Start()` forwards to the Skeleton's `OfferService()` and propagates its `Result<void>`.
* `Stop()` forwards to the Skeleton's `StopOfferService()`.

The service is then registered via `ProvidedServicesContainer::Emplace()`, which constructs it and calls
`Start()` immediately, only keeping the service in the container if offering succeeded.

See the generic design document ([../../README.md](../../README.md)).
