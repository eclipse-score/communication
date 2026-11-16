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

Chapter 5: Async reception, timeouts and streams
================================================

Instead of registering receive handlers like in C++, the Rust API exposes `async` functions. The API is independent of
the executor. The sample application drives the futures with `futures::executor::block_on()`. Any other executor or
runtime works as well, see chapter 7 for Tokio.

Async service discovery
~~~~~~~~~~~~~~~~~~~~~~~

`get_available_instances_async()` is a future that resolves once at least one instance is available. Unlike the sync
variant of chapter 4 it does not fail when the producer starts later.

.. literalinclude:: consumer.rs
   :language: rust
   :start-after: [rust-consumer-async-discovery-start]
   :end-before: [rust-consumer-async-discovery-end]
   :caption: consumer.rs

Async receive
~~~~~~~~~~~~~

`receive()` takes ownership of the sample container, waits until at least the minimum number of samples arrived and
returns the container together with the result. The container is handed back so that it can be reused in the next call.

.. literalinclude:: consumer.rs
   :language: rust
   :start-after: [rust-consumer-async-receive-start]
   :end-before: [rust-consumer-async-receive-end]
   :caption: consumer.rs

Cancellable receive
~~~~~~~~~~~~~~~~~~~

`cancellable_receive()` additionally takes a future. When this future completes, the receive is cancelled. This is the
building block for timeouts. The sample uses a simple timer thread to complete a `oneshot` channel, in real
applications a timer of the chosen runtime would be used.

.. literalinclude:: consumer.rs
   :language: rust
   :start-after: [rust-consumer-cancellable-receive-start]
   :end-before: [rust-consumer-cancellable-receive-end]
   :caption: consumer.rs

Streams
~~~~~~~

`to_stream()` turns a subscription into a `Stream` of samples, which can be consumed with `StreamExt::next()`.

.. literalinclude:: consumer.rs
   :language: rust
   :start-after: [rust-consumer-stream-start]
   :end-before: [rust-consumer-stream-end]
   :caption: consumer.rs

Running
~~~~~~~

Start the producer of chapter 3 in one terminal and the async consumer in another. The async discovery waits until the
producer offers the service, so the start order does not matter:

.. code-block:: bash

   bazel run //score/mw/com/doc/tutorial/rust/chapter_3:producer_app
   bazel run //score/mw/com/doc/tutorial/rust/chapter_5:async_consumer_app -- -e async-receive-with-timeout

The option `-e, --example-type` selects `async-receive` (default), `async-receive-with-timeout` or `streaming`.
