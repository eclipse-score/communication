..
   # *******************************************************************************
   # Copyright (c) 2026 Contributors to the Eclipse Foundation
   #
   # See the NOTICE file(s) distributed with this work for additional
   # information regarding copyright ownership.
   #
   # This program and the accompanying materials are made available under the
   # terms of the Apache License Version 2.0 which is available at
   # https://www.apache.org/licenses/LICENSE-2.0
   #
   # SPDX-License-Identifier: Apache-2.0
   # *******************************************************************************

Chapter 4: Finding and consuming a service
==========================================

Service discovery
~~~~~~~~~~~~~~~~~

`runtime.find_service::<VehicleInterface>(FindServiceSpecifier::Specific(instance_specifier))` returns a discovery
object. `get_available_instances()` returns immediately with whatever is currently offered. If the producer is not up
yet, the result is empty and the sample application fails. Start the producer of chapter 3 first. Chapter 5 shows the
async variant `get_available_instances_async()`, which waits for the service.

Each instance is a consumer builder. In the sample the first instance is taken and built. Then the events are
subscribed with a maximum number of sample slots, here 3:

.. literalinclude:: consumer.rs
   :language: rust
   :start-after: [rust-consumer-subscribe-start]
   :end-before: [rust-consumer-subscribe-end]
   :caption: consumer.rs

The discovery function:

.. literalinclude:: consumer.rs
   :language: rust
   :start-after: [rust-consumer-discovery-start]
   :end-before: [rust-consumer-discovery-end]
   :caption: consumer.rs

Synchronous reception
~~~~~~~~~~~~~~~~~~~~~

`try_receive()` is non-blocking. It fills a `SampleContainer` with up to the requested number of new samples and
returns how many were received. The sample provides access to the data via `Deref`.

.. literalinclude:: consumer.rs
   :language: rust
   :start-after: [rust-consumer-sync-receive-start]
   :end-before: [rust-consumer-sync-receive-end]
   :caption: consumer.rs

.. note::

   Calling `unsubscribe()` ends the subscription explicitly. Dropping the subscription does the same.

Running
~~~~~~~

Start the producer of chapter 3 first, then the consumer, each in its own terminal:

.. code-block:: bash

   bazel run //score/mw/com/doc/tutorial/rust/chapter_3:producer_app
   bazel run //score/mw/com/doc/tutorial/rust/chapter_4:consumer_app
