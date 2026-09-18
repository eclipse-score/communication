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

For service offering, the mw::com backend uses the generic `ProvidedServiceContainer` workflow with mw::com-specific adapters.

#### ProvidedServiceBuilder

`ProvidedServiceBuilder` wires mw::com decorators into the generic builder flow and returns
`ProvidedServices` / `ProvidedServiceContainer` objects that can be started and stopped as one unit.
Reference:
[`provided_service_builder.h`](../../../backend/mw_com/provided_service_builder.h).

#### ProvidedServiceDecorator

`ProvidedServiceDecorator<ServiceType>` adapts mw::com service implementations to the generic `ProvidedService`
interface by mapping:

* `StartService()` to `StartService()`
* `StopService()` to `StopService()`

Reference:
[`provided_service_decorator.h`](../../../backend/mw_com/provided_service_decorator.h).

For mw::com, the usual pattern is composition with generated skeletons (service holds skeleton as member) rather than
service type inheritance from generated skeletons.
