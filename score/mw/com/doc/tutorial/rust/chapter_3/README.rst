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

Chapter 3: Offering a service as producer
=========================================

The producer side follows three steps: build the producer from the runtime, offer it, then allocate and send samples.

Building and offering
~~~~~~~~~~~~~~~~~~~~~

`runtime.producer_builder::<VehicleInterface>(instance_specifier)` returns a builder. `build()` creates the producer
and `offer()` makes the service visible to consumers. The result of `offer()` is an *offered producer*, which gives
access to the events.

.. literalinclude:: producer.rs
   :language: rust
   :start-after: [rust-producer-offer-start]
   :end-before: [rust-producer-offer-end]
   :caption: producer.rs

Publishing samples
~~~~~~~~~~~~~~~~~~

Sending is zero-copy, like in C++. A sample slot is first allocated in shared memory, then written and finally sent:

1. `allocate()` returns an uninitialized sample (`SampleMaybeUninit`).
2. `write()` initializes it and returns a `SampleMut`.
3. `send()` publishes it to all subscribed consumers.

.. literalinclude:: producer.rs
   :language: rust
   :start-after: [rust-producer-publish-start]
   :end-before: [rust-producer-publish-end]
   :caption: producer.rs

Stop offering
~~~~~~~~~~~~~

`unoffer()` consumes the offered producer and withdraws the service. Dropping it has the same effect, but calling
`unoffer()` lets the application react to errors.

Running
~~~~~~~

.. code-block:: bash

   bazel run //score/mw/com/doc/tutorial/rust/chapter_3:producer_app

The producer publishes five samples, one per second, and then stops offering the service. Use the consumers of the
next chapters to receive them.
