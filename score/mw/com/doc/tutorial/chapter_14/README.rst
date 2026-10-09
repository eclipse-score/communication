Chapter 14: Writing a gateway application (advanced)
====================================================


In all previous chapters, provider and consumer lived in the same `LoLa` domain: the same OS instance, talking
through shared memory that both of them can open. In the :doc:`introduction <../README>` we mentioned that
`score::mw::com` can reach beyond a single OS instance via a *gateway*: a dedicated application in each domain
that relays service instances to the other domain.

This chapter shows how to write such a gateway application for a hypervisor setup, where two VMs on the same ECU
each run their own `LoLa` domain.

.. note::

   This is an advanced chapter. Unlike the previous chapters, it cannot give you a ready-to-run example: a
   realistic gateway has to make shared memory created in one VM visible in another VM and has to exchange control
   messages between the VMs. Both depend on your hypervisor and OS, so `score::mw::com` cannot provide a generic
   implementation.

   What `score::mw::com` provides is the gateway *logic* as a library. You write the application around it and the
   transport layer for your platform. The application in this chapter is complete, but it uses the bundled sample
   transport, which leaves the hypervisor-specific parts unimplemented (see
   :ref:`chapter_14_sample_transport_limits`).

Prerequisites: :doc:`chapter 1 <../chapter_1/README>` and :doc:`chapter 2 <../chapter_2/README>` (configuration),
plus a basic understanding of shared-memory mechanisms of your hypervisor.

Files/artifacts used
~~~~~~~~~~~~~~~~~~~~


The ``bazel`` project for this chapter is located in
`score/mw/com/doc/tutorial/chapter_14 <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_14/>`__
and contains the following files:

----------------------------------------

.. list-table::
   :header-rows: 1

   * - File Name
     - Description
   * - `BUILD <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_14/BUILD>`__
     - Bazel targets: the gateway binary and one deployable package per gateway role (source and destination).
   * - `gateway_application_main.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_14/gateway_application_main.cpp>`__
     - ``main()``: command line parsing, signal handlers and `score::mw::com` runtime initialization.
   * - `gateway_application_runner.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_14/gateway_application_runner.h>`__ / `.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_14/gateway_application_runner.cpp>`__
     - ``RunGatewayApplication()``: creates, sets up and starts the gateway, then waits for shutdown.
   * - `etc/mw_com_config_source_gateway.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_14/etc/mw_com_config_source_gateway.json>`__ / `etc/mw_com_config_destination_gateway.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_14/etc/mw_com_config_destination_gateway.json>`__
     - Regular `score::mw::com` configuration of each gateway.
   * - `etc/mw_com_gateway_config_source_gateway.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_14/etc/mw_com_gateway_config_source_gateway.json>`__ / `etc/mw_com_gateway_config_destination_gateway.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_14/etc/mw_com_gateway_config_destination_gateway.json>`__
     - Gateway configuration: which services to forward or accept, and which transport layer to use.
   * - `etc/mw_com_gateway_sample_transport_config_source_gateway.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_14/etc/mw_com_gateway_sample_transport_config_source_gateway.json>`__ / `etc/mw_com_gateway_sample_transport_config_destination_gateway.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_14/etc/mw_com_gateway_sample_transport_config_destination_gateway.json>`__
     - Configuration of the sample transport layer (addresses and ports of the control channel).
   * - `logging.json <https://github.com/eclipse-score/communication/blob/main/score/mw/com/doc/tutorial/chapter_14/logging.json>`__
     - `mw::log` configuration of the gateway process.

How a gateway works
~~~~~~~~~~~~~~~~~~~


Each `LoLa` domain runs one gateway process. Together, the gateway processes form one logical gateway, connected by a
*transport layer*. For each forwarded service instance, a gateway plays one of two roles:

- The **source gateway** runs in the domain where the service instance is originally provided. It finds the instance
  with a ``GenericProxy`` and tells the other side about it: that it exists, which events it has, and when new
  event data arrives.
- The **destination gateway** runs in the domain that wants to use the service instance. It creates a
  ``GenericSkeleton`` (the *forwarding skeleton*) as a local stand-in and offers it. Local consumers find and use it
  like any other service instance; they don't know that the original provider lives in another VM.

One gateway process can be the source for some service instances and the destination for others at the same time.

For a memory-sharing gateway, which is what this chapter targets, event data is not copied: the forwarding skeleton
opens the shared-memory objects that the original provider created in the source domain. The gateways only exchange
small control messages (service offered, service gone, consumer subscribed, new data available).

The detailed message sequences are described in the
`gateway README <https://github.com/eclipse-score/communication/blob/main/score/mw/com/gateway/README.md>`__.

What you get and what you write
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


.. list-table::
   :header-rows: 1

   * - Part
     - Provided by
     - Responsibility
   * - ``GatewayApplication``
     - `score::mw::com` (library)
     - Service discovery, generic proxies and skeletons, allow-list check, forwarding of subscriptions and updates.
   * - ``Transport`` implementation
     - You
     - Control messages between the gateways, and making shared memory accessible across VMs.
   * - Registration in ``TransportFactory``
     - You
     - Maps the transport layer ID from the configuration to your ``Transport`` implementation.
   * - The application (this chapter's code)
     - You
     - `score::mw::com` runtime initialization, configuration loading, lifecycle and shutdown.
   * - Configuration files
     - You
     - Three files per gateway (see :ref:`chapter_14_configuration`).

Step 1: Implement the transport layer for your hypervisor
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


The transport layer is the part that depends on your platform. It implements the abstract class ``Transport`` from
`transport_layer/transport.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/gateway/transport_layer/transport.h>`__.
In short:

.. code-block:: cpp

   class Transport
   {
     public:
       virtual bool IsMemorySharingSupported() const = 0;
       virtual score::Result<void> Setup() = 0;
       virtual void Shutdown() = 0;

       // Called on the source gateway, to be forwarded to the destination gateway.
       virtual score::Result<void> ProvideService(InstanceSpecifier, std::vector<EventInfo>) = 0;
       virtual score::Result<void> OfferService(InstanceSpecifier) = 0;
       virtual score::Result<void> StopOfferService(InstanceSpecifier) = 0;
       virtual score::Result<void> NotifyUpdate(InstanceSpecifier, impl::ServiceElementType, std::string) = 0;

       // Called on the destination gateway, to be forwarded to the source gateway.
       virtual score::Result<void> RegisterUpdateNotification(InstanceSpecifier, impl::ServiceElementType, std::string) = 0;
       virtual score::Result<void> UnregisterUpdateNotification(InstanceSpecifier, impl::ServiceElementType, std::string) = 0;
   };

Your implementation has two jobs:

1. **Control channel.** Each call above must reach the gateway on the other side. There, your transport receives
   the message and calls the corresponding method of ``GatewayCore``
   (`gateway_core.h <https://github.com/eclipse-score/communication/blob/main/score/mw/com/gateway/gateway_application/gateway_core.h>`__)
   on the ``GatewayCore&`` it was constructed with. That ``GatewayCore`` is the ``GatewayApplication`` of that
   side. For calls that return a ``Result``, the outcome on the remote side should be returned to the caller.
2. **Shared memory.** When ``IsMemorySharingSupported()`` returns ``true``, the destination gateway's forwarding
   skeleton opens the shared-memory objects created by the original provider in the source domain. Your transport
   must make sure those objects are accessible in the destination VM *before* it calls
   ``GatewayCore::ProvideService()``, because that call creates the skeleton.

The second job is what makes the transport hypervisor-specific: how shared memory is exported from one VM and mapped
in another differs between hypervisors.

.. _chapter_14_sample_transport_limits:

The sample transport and its limits
-----------------------------------


`score::mw::com` ships a sample transport (ID ``sample_hypervisor``) in
`transport_layer/sample <https://github.com/eclipse-score/communication/blob/main/score/mw/com/gateway/transport_layer/sample/>`__,
which this chapter's application uses. It is a good starting point for your own implementation:

- It implements the **control channel** completely, over TCP sockets, which most hypervisors offer between VMs. It
  handles request/acknowledge pairs with a configurable timeout.
- It does **not** implement the **shared-memory** part. These three functions in
  `sample_hypervisor_transport.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/gateway/transport_layer/sample/sample_hypervisor_transport.cpp>`__
  contain ``TODO`` markers and fail a precondition check, which terminates the process:

  - ``ResolveShmPaths()``: map an ``InstanceSpecifier`` to the names of its shared-memory objects in your
    hypervisor's shared-memory technology.
  - ``GetShmSizes()``: determine the sizes of those objects on the source side, so the destination side can verify
    them.
  - ``PreCreateInterVmSharedMemory()``: make the objects accessible in the destination VM.

.. warning::

   With the sample transport unchanged, both gateways start up and connect to each other, but the source gateway
   terminates as soon as it discovers one of its forwarded service instances, because ``ProvideService()`` calls
   ``GetShmSizes()``. To forward services, implement the three functions above for your hypervisor, or write your
   own ``Transport``.

Registering your transport
--------------------------


``GatewayApplication`` does not create the transport itself; it asks ``TransportFactory::Create()`` in
`transport_factory.cpp <https://github.com/eclipse-score/communication/blob/main/score/mw/com/gateway/transport_layer/transport_factory.cpp>`__
to create the transport for the ID in the gateway configuration. Add a branch for your ID there, which parses your
transport's own configuration file and constructs your implementation. An ID without a branch terminates the
process.

Step 2: Write the application
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


With a transport in place, the application itself is short. It is split into ``main()`` and a reusable
``RunGatewayApplication()``, so that the gateway can also be embedded into an existing process without going
through ``main()``.

main()
------


The application takes two arguments: the path to its `score::mw::com` configuration and the path to its gateway
configuration. Before doing anything else, it installs handlers for ``SIGINT`` and ``SIGTERM``, which are used to
shut the gateway down cleanly:

.. literalinclude:: gateway_application_main.cpp
   :language: cpp
   :lines: 27-34
   :caption: gateway_application_main.cpp


Next, it initializes the `score::mw::com` runtime with the given configuration, as you have seen since
:doc:`chapter 2 <../chapter_2/README>`. This has to happen before the gateway is created: the gateway creates proxies
and skeletons, and both need the deployment information from that configuration. Then it hands over to
``RunGatewayApplication()``:

.. literalinclude:: gateway_application_main.cpp
   :language: cpp
   :lines: 36-42
   :caption: gateway_application_main.cpp


RunGatewayApplication()
-----------------------


``RunGatewayApplication()`` first parses the gateway configuration and creates the ``GatewayApplication`` from it.
``ParseGatewayConfig()`` terminates the process if the file is missing or invalid:

.. literalinclude:: gateway_application_runner.cpp
   :language: cpp
   :lines: 42-43
   :caption: gateway_application_runner.cpp


Starting the gateway takes two calls, and their order matters:

- ``Setup()`` creates the transport via ``TransportFactory`` and calls ``Transport::Setup()``, which connects to the
  gateway on the other side. After this, the gateway can receive requests from the other side, so a destination
  gateway is fully operational at this point. The sample transport's ``Setup()`` blocks until the other gateway is
  connected, so the two gateways can be started in any order.
- ``Start()`` starts service discovery for every entry in ``forwarded-services``. Whenever one of these service
  instances appears or disappears, the gateway informs the other side through the transport. For a gateway that
  only receives services, the list is empty and ``Start()`` does nothing.

.. literalinclude:: gateway_application_runner.cpp
   :language: cpp
   :lines: 45-57
   :caption: gateway_application_runner.cpp


All further work happens in callbacks from service discovery and from the transport, so the calling thread only
waits until the signal handler requests a shutdown:

.. literalinclude:: gateway_application_runner.cpp
   :language: cpp
   :lines: 27-38
   :caption: gateway_application_runner.cpp

.. literalinclude:: gateway_application_runner.cpp
   :language: cpp
   :lines: 59-67
   :caption: gateway_application_runner.cpp


When ``RunGatewayApplication()`` returns, ``gateway_app`` is destroyed. Its destructor first shuts down the transport,
so that no callback stays blocked waiting for the other side, then invalidates all pending callbacks and finally
stops offering all forwarding skeletons.

.. _chapter_14_configuration:

Step 3: Configure both sides
~~~~~~~~~~~~~~~~~~~~~~~~~~~~


Each gateway process needs three configuration files. In this chapter, the source gateway forwards two service
instances and the destination gateway receives them.

.. list-table::
   :header-rows: 1

   * - File
     - Passed via
     - Content
   * - ``mw_com_config_*.json``
     - first command line argument
     - Regular `score::mw::com` configuration: service types and instances.
   * - ``mw_com_gateway_config_*.json``
     - second command line argument
     - Which service instances to forward or accept, and which transport layer to use.
   * - Transport configuration
     - ``config-path`` in the gateway configuration
     - Transport-specific. For ``sample_hypervisor``: addresses and ports of the control channel.

Gateway configuration
---------------------


The source gateway lists the instance specifiers it forwards. The destination gateway lists the instance specifiers
it accepts. ``expected-received-services`` is an allow-list: the destination gateway rejects any service instance the
other side tries to provide that is not listed here.

.. literalinclude:: etc/mw_com_gateway_config_source_gateway.json
   :language: json
   :caption: etc/mw_com_gateway_config_source_gateway.json

.. literalinclude:: etc/mw_com_gateway_config_destination_gateway.json
   :language: json
   :caption: etc/mw_com_gateway_config_destination_gateway.json


``transport-layer.id`` selects the transport in ``TransportFactory``. ``config-path`` is passed unchanged to the
transport, so it is **not** resolved relative to the gateway configuration file. Here it is an absolute path that
matches where the packages of this chapter install their configuration (``/opt/<app>/etc``).

Transport configuration
-----------------------


For the sample transport, each side configures the address of the other side and a pair of ports, which are mirrored
between the two sides. Adapt ``remote-ip`` to the addresses of your VMs.

.. literalinclude:: etc/mw_com_gateway_sample_transport_config_source_gateway.json
   :language: json
   :caption: etc/mw_com_gateway_sample_transport_config_source_gateway.json

.. literalinclude:: etc/mw_com_gateway_sample_transport_config_destination_gateway.json
   :language: json
   :caption: etc/mw_com_gateway_sample_transport_config_destination_gateway.json


score::mw::com configuration
----------------------------


On the source side, the gateway uses this configuration to find the forwarded service instances. On the destination
side, the forwarding skeleton reads the deployment of each received service instance from it, so **every received
service instance must be configured on the destination side**, or creating the forwarding skeleton fails.

The ``HelloWorldService`` instance below is the one provided by :doc:`chapter 2 <../chapter_2/README>`, extended by
two settings for sharing it between VMs:

.. literalinclude:: etc/mw_com_config_destination_gateway.json
   :language: json
   :lines: 47-70
   :caption: etc/mw_com_config_destination_gateway.json


- ``interVmSupport`` creates and opens the shared-memory objects of this instance under names that can be shared
  between VMs. Since it changes the names, it must be set in the configuration of **every** application that uses
  this instance: the original provider and the source gateway, and on the destination side the gateway and all
  consumers.
- ``interVmForwarded`` tells the forwarding skeleton to open the shared-memory objects created in the source domain,
  instead of creating its own. It requires ``interVmSupport``.

Step 4: Build and deploy
~~~~~~~~~~~~~~~~~~~~~~~~


The chapter builds one binary and packages it twice, once per role, together with that role's configuration:

.. code-block:: bash

   bazel build //score/mw/com/doc/tutorial/chapter_14:source_gateway-tar
   bazel build //score/mw/com/doc/tutorial/chapter_14:destination_gateway-tar

Deploy the source package to the VM that runs the original provider and the destination package to the VM with the
consumers, then start one gateway in each VM:

.. code-block:: bash

   # In the source VM
   /opt/SourceGateway/bin/gateway_application_bin \
       /opt/SourceGateway/etc/mw_com_config_source_gateway.json \
       /opt/SourceGateway/etc/mw_com_gateway_config_source_gateway.json

   # In the destination VM
   /opt/DestinationGateway/bin/gateway_application_bin \
       /opt/DestinationGateway/etc/mw_com_config_destination_gateway.json \
       /opt/DestinationGateway/etc/mw_com_gateway_config_destination_gateway.json

`mw::log` finds ``logging.json`` in ``../etc`` relative to the binary. Stop a gateway with ``Ctrl+C`` or ``SIGTERM``.

Remember the :ref:`limits of the sample transport <chapter_14_sample_transport_limits>`: until you implement its
shared-memory part, the gateways connect, but cannot forward a service instance yet.

Summary
~~~~~~~


To bring `score::mw::com` communication across VMs on your hypervisor:

1. Implement ``Transport`` for your hypervisor, starting from the sample transport. Its control channel can be
   reused; the shared-memory functions have to be written for your platform.
2. Register your transport ID in ``TransportFactory::Create()``.
3. Write the application as shown in this chapter: initialize the runtime, then ``Setup()``, ``Start()`` and wait
   for shutdown.
4. Configure both sides: forwarded and accepted service instances, the transport, and ``interVmSupport`` /
   ``interVmForwarded`` for every forwarded instance.
