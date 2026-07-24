Chapter 5: Sample reception - polling vs. callback
==================================================


In all previous chapters the consumer received event samples by **cyclically** calling `GetNewSamples()` – even
when no new sample was available. We call this the **polling approach**. Polling is a very reasonable strategy when:

- the provider is expected to emit new event samples **cyclically**,
- that cycle time is (roughly) known to the consumer, and
- the consumer itself is anyhow activated cyclically to do its work and then just picks up the latest sample.

But there are scenarios where polling fits poorly:

- the provider sends an event update only **occasionally**, or at least at times **not anticipated** by the consumer, or
- the consumer has **no** cyclic activation of its own and wants to react **solely** on an event update by the provider.

For these cases `score::mw::com` offers an **event-driven** (callback based) reception: the consumer registers an
`EventReceiveHandler` via `SetReceiveHandler()`. Whenever the provider sends a new sample for that event, the registered
callback is invoked (on an internal middleware thread). Inside that callback, a call to `GetNewSamples()` is **guaranteed
to provide at least one new sample**.

This chapter shows the usage of `SetReceiveHandler()` and the event-driven call to `GetNewSamples()`. To make the benefit
of the event-driven approach visible, the provider in this chapter does **not** send strictly cyclically anymore: it
waits a **randomized** amount of time (between 100 ms and 3000 ms) between two `Send()` calls.

Files/artifacts used
~~~~~~~~~~~~~~~~~~~~


The `bazel` project for this chapter is located in `score/mw/com/doc/tutorial/chapter_5`. It contains the same set of
files as chapter 4 (just adapted in content):

----------------------------------------

.. list-table::
   :header-rows: 1

   * - File Name
     - Description
   * - `BUILD <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_5/BUILD>`__
     - This file contains bazel targets for this example.
   * - `consumer/consumer.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_5/consumer/consumer.cpp>`__
     - Implementation of the service consumer app. The `main()` for the consumer
   * - `consumer/consumer.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_5/consumer/consumer.h>`__
     - Header (empty - we just always want to have cpp/h pairs) of the service consumer.
   * - `consumer/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_5/consumer/mw_com_config.json>`__
     - This file contains the configuration for `score::mw::com` for the consumer app.
   * - `consumer/logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_5/consumer/logging.json>`__
     - This file contains the configuration for the logging system used by `score::mw::com`
   * - `provider/provider.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_5/provider/provider.cpp>`__
     - Implementation of the service provider.
   * - `provider/provider.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_5/provider/provider.h>`__
     - Header (empty) of the service provider.
   * - `provider/logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_5/provider/logging.json>`__
     - This file contains the configuration for the logging system used by `score::mw::com`
   * - `provider/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_5/provider/mw_com_config.json>`__
     - This file contains the configuration for `score::mw::com` for the provider app.
   * - `hello_world_service.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_5/hello_world_service.cpp>`__
     - This file is empty as the service interface is completely defined in the header.
   * - `hello_world_service.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_5/hello_world_service.h>`__
     - This file contains the definition of the service interface.


----------------------------------------

The service interface (`hello_world_service.h`) and the configuration files are **unchanged** compared to chapter 4
(single service instance, explicit `instanceId`). All changes in this chapter are in the provider/consumer application
code.

How to run the example
~~~~~~~~~~~~~~~~~~~~~~


The steps are the same as in chapter 4. The consumer uses the **implicit** configuration loading, so it is started
**without** the `--service_instance_manifest` argument.

.. code-block:: bash

   # Build the provider and consumer targets
   bazel build //score/mw/com/doc/tutorial/chapter_5:provider-tar
   bazel build //score/mw/com/doc/tutorial/chapter_5:consumer-tar


Extract both archives (e.g. in a tmp-directory):

.. code-block:: bash

   mkdir -p /tmp/tutorial/chapter_5
   tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_5/provider-tar.tar -C /tmp/tutorial/chapter_5/
   tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_5/consumer-tar.tar -C /tmp/tutorial/chapter_5/


... then start the service-provider application in the 1st terminal:

.. code-block:: bash

   cd /tmp/tutorial/chapter_5/opt/HelloWorldServer
   bin/provider_app


and the service-consumer in the 2nd terminal:

.. code-block:: bash

   cd /tmp/tutorial/chapter_5/opt/HelloWorldClient
   bin/consumer_app


You will observe how the provider announces and sends its event updates at randomized intervals:

.. code-block:: text

   Next event update in 1508 ms.
   Sample send completed. Event "message" update sent: Hello World #0
   Next event update in 2998 ms.
   Sample send completed. Event "message" update sent: Hello World #1
   ...


... and how the consumer receives each of them **event-driven** – i.e. exactly once per sent sample, without doing
any polling itself:

.. code-block:: text

   Found HelloWorld service instance (instance id: { ... "instanceId": 1 ... }). Creating proxy.
   Subscribed to 'message' event. Waiting for event-driven sample reception ...
   Sample received (event-driven): Hello World #0
   Sample received (event-driven): Hello World #1
   ...


Provider application
~~~~~~~~~~~~~~~~~~~~


The provider offers a single service instance, exactly as in chapter 4. The only relevant change is that it no longer
sends strictly cyclically. Instead, it draws a random delay between `kMinSendDelay` and `kMaxSendDelay` before each send:

.. literalinclude:: provider/provider.cpp
   :language: cpp
   :lines: 33-113
   :caption: provider/provider.cpp


Consumer application
~~~~~~~~~~~~~~~~~~~~


The general setup is the same as in chapter 4: the consumer uses `StartFindService()` to discover the (single) service
instance, creates a proxy from the found handle and stops the discovery right away from within the find-service handler
(see chapter 4 for the details on why the first handler invocation is guaranteed to be non-empty).

The interesting part is **how** the consumer receives samples. There is **no polling loop** anymore. Instead, before it
subscribes, the consumer registers an `EventReceiveHandler` via `SetReceiveHandler()`:

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 130-144
   :caption: consumer/consumer.cpp


Only **after** the handler is registered, the consumer subscribes. Note that – in contrast to chapter 4 –
there is **no need to check the subscription state** here: the receive handler will only ever be called once a new event
sample has actually been sent by the provider.

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 160-160
   :caption: consumer/consumer.cpp


Because the consumer is **purely event-driven**, its main thread has **nothing to poll** – not even a
wake-up-and-check loop (a cyclic sleep/wake loop would somewhat contradict the whole point of this chapter, as it would
essentially be polling again). Instead, the main thread simply **blocks on a condition variable** until a termination
signal requests the shut down. The signal handler sets a flag and notifies the condition variable:

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 35-43
   :caption: consumer/consumer.cpp


.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 181-181
   :caption: consumer/consumer.cpp


Since there is no main loop anymore, there is also no need to route an "unrecoverable set-up error" back to the main
thread via a shared flag: if creating the proxy, setting the receive handler or subscribing unexpectedly fails, the
find-service handler simply **terminates the application immediately** via `std::exit(EXIT_FAILURE)`. Consequently, the
consumer no longer needs a shared-state struct at all – the only piece of state shared between the find-service
handler and the receive handler is the `std::optional<HelloWorldProxy> proxy`, captured by reference:

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 122-125
   :caption: consumer/consumer.cpp


A few things worth noting about the receive handler:

- Its type is `score::mw::com::EventReceiveHandler`, which is a `score::cpp::callback<void(void)>` – it takes no
  arguments and returns nothing. It signals "a new sample arrived"; the actual data is fetched via `GetNewSamples()`.
- The handler is registered **before** subscribing so that no early sample can be missed. (`SetReceiveHandler()` and
  `Subscribe()` are independent; the reverse order works too, but registering first is the safe habit for a purely
  event-driven consumer.)
- We store the proxy in its final location (the `proxy` optional) **before** registering the handler, so that the
  handler can safely refer to it.
- Inside the handler you may call the public APIs of the event on which it is registered (like `GetNewSamples()`). You
  **must not** call `SetReceiveHandler()` from within the handler, and – as always – API calls on a proxy /
  proxy event must not happen concurrently. Here this is naturally given: the main thread does not touch the proxy after
  the set-up.
- Inside the handler you may also call `UnsetReceiveHandler()` to unregister the handler. This is not done in this
  example, but it is a valid use case.

Configuration
~~~~~~~~~~~~~


The configuration is **identical** to chapter 4: the provider (`provider/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_5/provider/mw_com_config.json>`__)
offers a single instance with `instanceId` 1, and the consumer (`consumer/mw_com_config.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_5/consumer/mw_com_config.json>`__)
maps its `instanceSpecifier` to that type **with** the explicit `instanceId` 1.

Summary
~~~~~~~


A short summary of what we have learned in this chapter:

- There are two ways for a consumer to receive event samples: the **polling** approach (cyclically calling
  `GetNewSamples()`, as in chapters 1-4) and the **event-driven** approach (registering an `EventReceiveHandler` via
  `SetReceiveHandler()`).
- **Polling** fits well when the provider emits samples cyclically with a known cycle time and/or the consumer is
  activated cyclically anyway. **Event-driven** reception fits well when samples arrive only occasionally / unanticipated
  or when the consumer has no cyclic activation and wants to react solely on incoming events.
- The `EventReceiveHandler` is a `score::cpp::callback<void(void)>`. It is invoked on an internal middleware thread
  whenever a new sample was received. Inside the handler, a call to `GetNewSamples()` is guaranteed to provide **at least
  one** new sample.
- Register the handler with `SetReceiveHandler()` (and remove it again with `UnsetReceiveHandler()`). You may call the
  event's public APIs (e.g. `GetNewSamples()`) from within the handler, but **not** `SetReceiveHandler()`.
- For a purely event-driven consumer there is **no need to check the subscription state**: the handler is only ever
  called once a new sample has actually been sent.
