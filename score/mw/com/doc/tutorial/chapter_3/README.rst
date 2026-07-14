Chapter 3: Finding (multiple) service instances asynchronously
==============================================================


In chapters 1 and 2 the consumer used the **synchronous** `FindService()` API in a polling loop to find the one and
only service instance offered by the provider. In this chapter we look at two closely related, more realistic scenarios:

1. The provider offers **multiple** instances of the same service (here: 3 instances of the `HelloWorldService`).
2. The consumer does not know upfront, which (or how many) instances exist. It does a **"find-any"** search and reacts
   to instances appearing (and disappearing) at runtime. To do so, it uses the **asynchronous** `StartFindService()`
   API instead of the synchronous `FindService()`.

The provider brings up its 3 instances **one after the other** (with a 5 second pause in between), so you can nicely
observe how the consumer discovers each new instance asynchronously, as soon as it becomes available.

Files/artifacts used
~~~~~~~~~~~~~~~~~~~~


The `bazel` project for this chapter is located in `score/mw/com/doc/tutorial/chapter_3`. It contains the same set of
files as chapter 2 (just adapted in content):

----------------------------------------

.. list-table::
   :header-rows: 1

   * - File Name
     - Description
   * - `BUILD`
     - This file contains bazel targets for this example.
   * - consumer/`consumer.cpp`
     - Implementation of the service consumer app. The `main()` for the consumer
   * - consumer/`consumer.h`
     - Header (empty - we just always want to have cpp/h pairs) of the service consumer.
   * - consumer/`mw_com_config.json`
     - This file contains the configuration for `score::mw::com` for the consumer app.
   * - consumer/`logging.json`
     - This file contains the configuration for the logging system used by `score::mw::com`
   * - provider/`provider.cpp`
     - Implementation of the service provider.
   * - provider/`provider.h`
     - Header (empty) of the service provider.
   * - provider/`logging.json`
     - This file contains the configuration for the logging system used by `score::mw::com`
   * - provider/`mw_com_config.json`
     - This file contains the configuration for `score::mw::com` for the provider app.
   * - `hello_world_service.cpp`
     - This file is empty as the service interface is completely defined in the header.
   * - `hello_world_service.h`
     - This file contains the definition of the service interface.


----------------------------------------

The service interface (`hello_world_service.h`) is **unchanged** compared to chapter 2. All changes in this chapter are
in the provider/consumer application code and in the configuration files.

How to run the example
~~~~~~~~~~~~~~~~~~~~~~


The steps are the same as in chapter 1 (minus the chapter adaptions in paths). Note that – unlike chapter 2
– the consumer in this chapter uses the **implicit** configuration loading again (see below), so it is started
**without** the `--service_instance_manifest` argument.

.. code-block:: bash

   # Build the provider and consumer targets
   bazel build //score/mw/com/doc/tutorial/chapter_3:provider-tar 
   bazel build //score/mw/com/doc/tutorial/chapter_3:consumer-tar


Extract both archives (e.g. in a tmp-directory):

.. code-block:: bash

   mkdir -p /tmp/tutorial/chapter_3
   tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_3/provider-tar.tar -C /tmp/tutorial/chapter_3/
   tar -xf <project dir>/bazel-bin/score/mw/com/doc/tutorial/chapter_3/consumer-tar.tar -C /tmp/tutorial/chapter_3/


... then start the service-provider application in the 1st terminal:

.. code-block:: bash

   cd /tmp/tutorial/chapter_3/opt/HelloWorldServer
   bin/provider


and the service-consumer in the 2nd terminal. Note that we do **not** pass a `--service_instance_manifest` argument
here: the consumer uses the **implicit** configuration loading, which expects the config file at the default location
`./etc/mw_com_config.json` relative to the current working directory:

.. code-block:: bash

   cd /tmp/tutorial/chapter_3/opt/HelloWorldClient
   bin/consumer


When both run, you will observe how the provider brings up its 3 instances one after the other:

.. code-block:: text

   Created and offered HelloWorld service instance: MyHelloWorldServiceInstance_1
   Created and offered HelloWorld service instance: MyHelloWorldServiceInstance_2
   Created and offered HelloWorld service instance: MyHelloWorldServiceInstance_3


... and how the consumer discovers each new instance asynchronously (roughly 5 seconds apart), and then starts to
receive samples from it:

.. code-block:: text

   Found new HelloWorld service instance (instance id: { "bindingInfo": { "instanceId": 1, ... } ... }). Creating proxy.
   Sample received from instance { ... "instanceId": 1 ... }: Hello World from MyHelloWorldServiceInstance_1 #52
   Found new HelloWorld service instance (instance id: { "bindingInfo": { "instanceId": 2, ... } ... }). Creating proxy.
   Sample received from instance { ... "instanceId": 2 ... }: Hello World from MyHelloWorldServiceInstance_2 #7
   ...


Provider application
~~~~~~~~~~~~~~~~~~~~


The provider now offers **3 instances** of the `HelloWorldService` instead of just one. In the
`provider.cpp <provider/provider.cpp>`__ file we introduce a small array of instance specifiers – one per
instance to offer:

.. literalinclude:: provider/provider.cpp
   :language: cpp
   :lines: 28-30
   :caption: provider/provider.cpp


Each of these instance specifiers is mapped in the provider configuration to its own service instance (see below). To
avoid code duplication, creating and offering a single instance was factored out into a helper `CreateAndOfferInstance()`
– its body is exactly the create-and-offer logic you already know from chapter 1 and 2.

The interesting part is the main loop. Instead of creating all instances upfront, the provider **staggers** the creation
of its instances by 5 seconds each:

.. literalinclude:: provider/provider.cpp
   :language: cpp
   :lines: 103-131
   :caption: provider/provider.cpp


Two things are worth noting here:

- We keep the offered skeletons in a `std::vector<HelloWorldSkeleton>`. `score::mw::com` skeletons are **movable**, so
  storing them in (and growing) a vector is perfectly fine. To make it explicit that no reallocation happens while we
  add our known number of instances, we `reserve()` the vector upfront.
- The `SendSample()` helper writes a payload that includes the instance specifier string (e.g.
  `Hello World from MyHelloWorldServiceInstance_2 #7`). This makes it easy to see on the consumer side, from which
  instance a received sample originates.

Consumer application
~~~~~~~~~~~~~~~~~~~~


This is where the biggest change happens. Instead of the synchronous `FindService()` polling loop from chapter 2, the
consumer now uses the **asynchronous** `StartFindService()` API in `consumer.cpp <consumer/consumer.cpp>`__.

.. note::

   Note on configuration loading: In chapter 2 the consumer used the **explicit** configuration loading (calling
   `score::mw::com::runtime::InitializeRuntime(argc, argv)` and passing `--service_instance_manifest`). In this chapter we
   go back to the **preferred, implicit** approach (see chapter 2's "Loading of configurations" section): the consumer
   ships its config as `./etc/mw_com_config.json` and no longer calls `InitializeRuntime()` nor takes any command line
   arguments. `main()` is therefore simply `int main()`.

`StartFindService()` takes a **handler** (a callback) and an `InstanceSpecifier`. It starts a **continuous** service
discovery in the background. Whenever the set of matching service instances changes (an instance becomes available or
unavailable), `score::mw::com` invokes the provided handler **asynchronously** (from an internal middleware thread),
handing over the container of **all** currently matching service handles.

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 111-147
   :caption: consumer/consumer.cpp


A few important details:

- **The handler always receives the full set of currently matching handles**, not just the newly added ones. So the
  consumer keeps a set of already-known handles (`known_handles`) to detect which handles are actually **new**. Note
  that `HandleType` is both equality- and less-than-comparable (and hashable), so it can be used directly as a key in
  `std::set`/`std::map`.
- **The handler runs on an internal middleware thread**, while our main loop (see below) reads the discovered proxies.
  We therefore protect the shared state (`discovered_instances` and `known_handles`) with a `std::mutex`.
- For each newly discovered instance we create a proxy via `HelloWorldProxy::Create(handle)` and `Subscribe()` to its
  `message` event – exactly as in the previous chapters, just now driven by the discovery callback.

Getting a portable instance-id description from a handle
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


To log *which* instance we just discovered, we need some representation of its instance id. It is important to only use
the **public** `score::mw::com` API for that – user code must **never** include anything from `score/mw/com/impl`.

The portable way to get a representation of the instance id out of a handle is:

1. `HandleType::GetInstanceId()` returns a `ServiceInstanceId`.
2. `ServiceInstanceId::Serialize()` returns a **binding-independent** JSON object (`score::json::Object`).
3. We stringify that JSON object with the public `score::json::JsonWriter` (`ToBuffer()`), and collapse the pretty-printed
   whitespace into a single line for compact logging.

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 54-60
   :caption: consumer/consumer.cpp


This yields, for the first instance, a description like
`{ "bindingInfo": { "instanceId": 1, "serializationVersion": 1 }, "bindingInfoIndex": 0, "serializationVersion": 1 }`.

.. note::

   Note: Do **not** reach into `score::mw::com::impl` to fish out e.g. the binding-specific numeric instance id. That is
   a private implementation detail. `ServiceInstanceId::Serialize()` is the only portable, binding-independent way to get
   an instance-id representation out of a handle. Using it also keeps your code working across bindings.

The main loop of the consumer then simply polls the `message` event of **every** discovered instance for new samples:

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 157-169
   :caption: consumer/consumer.cpp


Finally, before shutting down, the consumer stops the asynchronous discovery again. `StartFindService()` returns a
`FindServiceHandle`, which is passed to `StopFindService()` to terminate the discovery (after which the handler will no
longer be called):

.. literalinclude:: consumer/consumer.cpp
   :language: cpp
   :lines: 179-179
   :caption: consumer/consumer.cpp


.. note::

   Note: The `FindServiceHandle` is also handed to the handler as its second argument. This allows you to call
   `StopFindService()` **from within the handler itself** – e.g. if you only want to find the very first instance
   and then stop searching (as some of the tests in this repository do). In this chapter we want to keep discovering
   instances for the whole lifetime of the consumer, so we only stop the discovery on shutdown.

Configuration
~~~~~~~~~~~~~


Provider configuration - offering multiple instances
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


To offer 3 instances of the `HelloWorldService`, the provider configuration
`mw_com_config.json <provider/mw_com_config.json>`__ now contains **3 entries** in its `serviceInstances`
array. Each entry maps a distinct `instanceSpecifier` to a distinct `instanceId` of the **same** `serviceTypeName`:

.. literalinclude:: provider/mw_com_config.json
   :language: json
   :lines: 25-69
   :caption: provider/mw_com_config.json


The `serviceTypes` section is unchanged compared to chapter 2 – all 3 instances share the very same service type.

Consumer configuration - "find-any"
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


The magic that enables the **"find-any"** search is in the consumer configuration
`mw_com_config.json <consumer/mw_com_config.json>`__. Its single `serviceInstances` entry maps the
`instanceSpecifier` `MyHelloWorldServiceInstance` to the `HelloWorldService` type, but **deliberately leaves out the
`instanceId`** in the `instances` array:

.. literalinclude:: consumer/mw_com_config.json
   :language: json
   :lines: 25-34
   :caption: consumer/mw_com_config.json


Because no concrete `instanceId` is specified, a search using this instance specifier matches **any** instance of the
`HelloWorldService` type. This is exactly why our consumer, using a single `InstanceSpecifier`
(`MyHelloWorldServiceInstance`), discovers **all 3** instances offered by the provider.

.. note::

   Note: This is a general mechanism and not tied to `StartFindService()`. You could equally do a "find-any" search with
   the synchronous `FindService()` – it would then return a container with (in our case) up to 3 handles. The
   reason we combine it with `StartFindService()` here is that in real systems instances typically appear and disappear
   over time, and the asynchronous API lets you react to those changes without polling.

Summary
~~~~~~~


A short summary of what we have learned in this chapter:

- A provider can offer **multiple instances** of the same service type. Each instance gets its own `instanceSpecifier`
  and its own `instanceId` in the provider configuration, but they all share the same `serviceTypeName`.
- `score::mw::com` skeletons (and proxies) are **movable**, so you can conveniently keep them in containers like
  `std::vector`.
- A **"find-any"** search is configured by leaving out the `instanceId` for the instance specifier in the configuration.
  This is only possible at the consumer side. A search using that specifier then matches **any** instance of the service
  type.
- `StartFindService()` provides **asynchronous, continuous** service discovery. Its handler is called (from an internal
  middleware thread) with the **full set** of currently matching handles whenever service availability changes. Keep
  track of already-known handles yourself to detect newly appeared instances, and protect any state shared with your
  application threads.
- `StartFindService()` returns a `FindServiceHandle` that is used to stop the discovery again via `StopFindService()`
  – either on shutdown, or directly from within the handler.
- User code must **never** include from `score/mw/com/impl` (impl-namespace) directly. To obtain a portable,
  binding-independent representation of a service instance id from a handle, use the public path
  `HandleType::GetInstanceId()` → `ServiceInstanceId::Serialize()` (which yields a `score::json::Object`).
