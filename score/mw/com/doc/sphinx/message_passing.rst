Message Passing
===============

The message passing component provides low-level abstractions for high-performance
inter-process communication (IPC) across different platforms.

.. note::
   For detailed design documentation, refer to the Message Passing design README
   at ``score/mw/com/message_passing/design/README.md``.

Overview
--------

Message Passing provides:

* **Uni-directional n-to-1 communication**: Multiple senders to one receiver
* **Asynchronous behavior**: Non-blocking message sending
* **OS-specific implementations**: POSIX and QNX native support
* **Low-level primitives**: Foundation for higher-level communication like mw::com

Key Components
--------------

* **Sender**: Produces and sends data through uni-directional channels
* **Receiver**: Consumes data and manages communication channels
* **OS Abstraction**: Platform-specific implementations (POSIX, QNX messaging)

Supported Platforms
-------------------

* **POSIX**: Unix domain sockets for Linux systems
* **QNX**: Native QNX messaging for optimal performance

.. code-block:: cpp

   // Client connection
   auto client = UnixDomainClient::connect("/tmp/my_service");

   // Send/receive
   client.send(request);
   auto response = client.receive();

QNX Dispatch
------------

The QNX dispatch mechanism provides native message passing on QNX systems.

Features:

* Pulse messages for notifications
* Blocking messages for synchronous RPC
* Priority inheritance for real-time scheduling
* Channel-based communication

Resource Manager
~~~~~~~~~~~~~~~~

The QNX dispatch includes a resource manager for exposing services
through the file system namespace.

.. code-block:: cpp

   // Create resource manager
   auto rm = QnxDispatchServer::create("/dev/my_service");

   // Handle messages
   rm.dispatch([](const Message& msg) {
       // Process message
       return response;
   });

Timed Command Queue
-------------------

A specialized queue for time-sensitive command execution.

Features:

* Prioritized command execution
* Deadline-aware scheduling
* Non-allocating implementation for real-time systems

.. code-block:: cpp

   TimedCommandQueue queue;

   // Schedule command
   queue.schedule(command, deadline);

   // Execute ready commands
   queue.execute_ready();
