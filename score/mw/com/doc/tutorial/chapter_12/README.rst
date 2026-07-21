Chapter 12: Unit testing with score::mw::com mocks
==================================================


In this chapter we show how to unit test `score::mw::com` based code in classic **gtest/gmock** style.
The chapter is based on the HelloWorld provider/consumer applications from :doc:`chapter 2 <../chapter_2/README>`, but
both applications were adapted to improve unit-testability.

Why we introduced ProxyComponent / SkeletonComponent
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


The important idea is to separate:

1. the **runtime/bootstrap** part (service discovery, static `Create(...)`, wiring), and
2. the **business interaction** part (subscribe/get samples or allocate/send samples).

For the consumer, this split is done via `ProxyComponent`, which takes an already created `HelloWorldProxy`:

.. literalinclude:: consumer/proxy_component.h
   :language: cpp
   :lines: 24-39
   :caption: consumer/proxy_component.h


For the provider, this split is done via `SkeletonComponent`, which takes an already created `HelloWorldSkeleton`:

.. literalinclude:: provider/skeleton_component.h
   :language: cpp
   :lines: 25-45
   :caption: provider/skeleton_component.h


This enables focused unit tests with injected mocks for the proxy/skeleton objects.

Current mocking boundaries (important)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


At the time of writing, mocking support starts **after** proxy/skeleton creation:

- **No service-discovery mocking support** for
  `Proxy::FindService()` / `Proxy::StartFindService()` in this tutorial context.
- **No mocking support for static `Create(...)` APIs** on proxy and skeleton in this chapter's application flow.

Therefore, both applications are structured so that testable logic begins at:

- consumer: after `HelloWorldProxy::Create(...)`, where `ProxyComponent` is constructed.
- provider: after `HelloWorldSkeleton::Create(...)`, where `SkeletonComponent` is constructed.

You can see that cut in the consumer application:

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 44-67
   :caption: consumer/consumer.cpp


... and on provider side:

.. literalinclude:: provider/provider.cpp
   :language: cpp
   :lines: 36-43
   :caption: provider/provider.cpp


Including the public test types (AoU-compliant)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


The unit tests only include **public** `score::mw::com` headers. In particular, they must **not** include headers from
``score/mw/com/impl/*`` directly and must **not** use the ``score::mw::com::impl`` namespace, because these are
implementation details and are not part of the public API (relying on them would violate the Assumptions of Use / AoU
of `score::mw::com`).

All mocking types the tests need are therefore pulled in via the single public header
`score/mw/com/test_types.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/test_types.h>`__.
It exposes public aliases in the ``score::mw::com`` namespace - e.g. ``ProxyEventMock``, ``NamedProxyEventMock``,
``ProxyWrapperClassTestView``, ``SkeletonEventMock``, ``NamedSkeletonEventMock``, ``SkeletonWrapperClassTestView`` and
``SkeletonBaseMock`` - as well as helpers such as ``MakeFakeSamplePtr`` and ``MakeFakeSampleAllocateePtr``. These
aliases resolve to the corresponding ``score::mw::com::impl`` types, so the tests can use them through the public
``score::mw::com`` namespace without ever depending on impl headers or the impl namespace.

.. literalinclude:: consumer/proxy_component_test.cpp
   :language: cpp
   :lines: 13-22
   :caption: consumer/proxy_component_test.cpp (public includes)


The provider-side test uses the same approach:

.. literalinclude:: provider/skeleton_component_test.cpp
   :language: cpp
   :lines: 13-23
   :caption: provider/skeleton_component_test.cpp (public includes)


Consumer side unit test approach (ProxyComponent)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


The `ProxyComponent` implementation encapsulates subscribe/get-samples behavior:

.. literalinclude:: consumer/proxy_component.cpp
   :language: cpp
   :lines: 17-48
   :caption: consumer/proxy_component.cpp


Its unit tests use `ProxyWrapperClassTestView` and injected event mocks:

.. literalinclude:: consumer/proxy_component_test.cpp
   :language: cpp
   :lines: 24-49
   :caption: consumer/proxy_component_test.cpp


Test cases then follow the **given/expect/when/then** style:

.. literalinclude:: consumer/proxy_component_test.cpp
   :language: cpp
   :lines: 51-110
   :caption: consumer/proxy_component_test.cpp


Provider side unit test approach (SkeletonComponent)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


The `SkeletonComponent` implementation encapsulates offer/allocate/send behavior:

.. literalinclude:: provider/skeleton_component.cpp
   :language: cpp
   :lines: 27-69
   :caption: provider/skeleton_component.cpp


Its unit tests use `SkeletonWrapperClassTestView`, `NamedSkeletonEventMock`, `SkeletonEventMock`, and
`SkeletonBaseMock`:

.. literalinclude:: provider/skeleton_component_test.cpp
   :language: cpp
   :lines: 29-52
   :caption: provider/skeleton_component_test.cpp


... with given/expect/when/then style test cases:

.. literalinclude:: provider/skeleton_component_test.cpp
   :language: cpp
   :lines: 54-122
   :caption: provider/skeleton_component_test.cpp


Bazel test targets
~~~~~~~~~~~~~~~~~~


Both unit tests are integrated as dedicated `cc_unit_test` targets:

.. literalinclude:: BUILD
   :language: python
   :lines: 136-153
   :caption: BUILD


You can run them via:

.. code-block:: bash

   bazel test //score/mw/com/doc/tutorial/chapter_12:proxy_component_test
   bazel test //score/mw/com/doc/tutorial/chapter_12:skeleton_component_test


Where to look for broader mocking capabilities
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


For an overview of currently supported mocking features (beyond this chapter's specific example), inspect the mocking
tests under:

- `score/mw/com/impl/mocking/*_test.cpp`

Good entry points are, for example:

- `skeleton_wrapper_class_test_view_test.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/impl/mocking/skeleton_wrapper_class_test_view_test.cpp>`__
- `proxy_wrapper_class_test_view_test.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/impl/mocking/proxy_wrapper_class_test_view_test.cpp>`__
- `skeleton_event_mock_test.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/impl/mocking/skeleton_event_mock_test.cpp>`__
- `proxy_event_mock_test.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/impl/mocking/proxy_event_mock_test.cpp>`__


Files/artifacts used
~~~~~~~~~~~~~~~~~~~~


The `bazel` project for this chapter is located in `score/mw/com/doc/tutorial/chapter_12`.

----------------------------------------

.. list-table::
   :header-rows: 1

   * - File Name
     - Description
   * - `BUILD <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/BUILD>`__
     - Bazel targets for provider/consumer apps and unit tests.
   * - `hello_world_service.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/hello_world_service.h>`__
     - Service interface and proxy/skeleton aliases.
   * - `hello_world_service.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/hello_world_service.cpp>`__
     - Translation unit for the service interface.
   * - `consumer/consumer.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/consumer/consumer.cpp>`__
     - Consumer app wiring (discovery/create + ProxyComponent usage).
   * - `consumer/proxy_component.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/consumer/proxy_component.h>`__
     - Consumer-side wrapper around the proxy.
   * - `consumer/proxy_component.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/consumer/proxy_component.cpp>`__
     - ProxyComponent implementation.
   * - `consumer/proxy_component_test.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/consumer/proxy_component_test.cpp>`__
     - Unit tests using proxy-side mocking infrastructure.
   * - `provider/provider.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/provider/provider.cpp>`__
     - Provider app wiring (create + SkeletonComponent usage).
   * - `provider/skeleton_component.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/provider/skeleton_component.h>`__
     - Provider-side wrapper around the skeleton.
   * - `provider/skeleton_component.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/provider/skeleton_component.cpp>`__
     - SkeletonComponent implementation.
   * - `provider/skeleton_component_test.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/provider/skeleton_component_test.cpp>`__
     - Unit tests using skeleton-side mocking infrastructure.
   * - `consumer/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/consumer/mw_com_config.json>`__
     - Consumer runtime configuration.
   * - `provider/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/provider/mw_com_config.json>`__
     - Provider runtime configuration.
   * - `consumer/logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/consumer/logging.json>`__
     - Consumer logging configuration.
   * - `provider/logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_12/provider/logging.json>`__
     - Provider logging configuration.


----------------------------------------

Summary
~~~~~~~


A short summary of what this chapter showed:

- Chapter 12 is based on chapter 2 HelloWorld apps, but refactored for unit-testability.
- `ProxyComponent` and `SkeletonComponent` isolate proxy/skeleton interaction logic into small testable units.
- Tests only depend on the public `score::mw::com` API: mocking types are included via `score/mw/com/test_types.h`,
  so no `score/mw/com/impl/*` headers and no `score::mw::com::impl` namespace are used (AoU-compliant).
- Unit tests use `ProxyWrapperClassTestView` (consumer side) and `SkeletonWrapperClassTestView` (provider side), with
  injected event/skeleton mocks in gtest/gmock style.
- Current limitation: service discovery and static `Create(...)` calls are not mocked in this chapter flow, so tests
  start after proxy/skeleton instances already exist.
- For deeper coverage of supported mocking features, inspect `score/mw/com/impl/mocking/*_test.cpp`.
