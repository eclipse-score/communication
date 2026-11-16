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

Chapter 8: Using the Tokio runtime
==================================

The previous chapters used `futures::executor::block_on`. In a real application the `score_com` futures can
run on any executor. This chapter shows how to use them with `Tokio`, including a
multi-threaded runtime. The code is a test target, so it is also a regression test for the async API.

`tokio_usage.rs` uses the generated types from `com_api_gen` together with the runtime, service
discovery and subscription APIs directly.

Adding Tokio to the target
~~~~~~~~~~~~~~~~~~~~~~~~~~

Add `futures` and `tokio` from the crate index to the dependencies of the Bazel target, next to `score_com`:

.. literalinclude:: BUILD
   :language: python
   :start-after: [rust-tokio-deps-start]
   :end-before: [rust-tokio-deps-end]
   :caption: BUILD

Producer: async sender
~~~~~~~~~~~~~~~~~~~~~~

The sender is an ordinary `async fn`. It waits between samples with `tokio::time::sleep`, so no thread is
blocked:

.. literalinclude:: tokio_usage.rs
   :language: rust
   :start-after: [rust-tokio-sender-start]
   :end-before: [rust-tokio-sender-end]
   :caption: tokio_usage.rs

Consumer: async receiver
~~~~~~~~~~~~~~~~~~~~~~~~

The receiver uses `receive()`, or `cancellable_receive()` with a `tokio::time::sleep` future as the timeout:

.. literalinclude:: tokio_usage.rs
   :language: rust
   :start-after: [rust-tokio-receiver-start]
   :end-before: [rust-tokio-receiver-end]
   :caption: tokio_usage.rs

Running both concurrently
~~~~~~~~~~~~~~~~~~~~~~~~~

`#[tokio::test(flavor = "multi_thread")]` starts the multi-threaded runtime. `tokio::spawn` runs the sender
and the receiver as separate tasks, possibly on different worker threads:

.. literalinclude:: tokio_usage.rs
   :language: rust
   :start-after: [rust-tokio-spawn-start]
   :end-before: [rust-tokio-spawn-end]
   :caption: tokio_usage.rs

The tests share one runtime instance, because the LoLa backend prints a warning when it is initialized more
than once. The file also contains tests for async receive with timeout and for stream based processing.

Running the tests
~~~~~~~~~~~~~~~~~

.. code-block:: bash

   # All integration tests
   bazel test //score/mw/com/doc/tutorial/rust/chapter_8:tokio-usage

   # A specific test
   bazel test //score/mw/com/doc/tutorial/rust/chapter_8:tokio-usage \
     --test_filter=receive_and_send_using_multi_thread

   # Verbose output for debugging
   bazel test //score/mw/com/doc/tutorial/rust/chapter_8:tokio-usage \
     --test_output=all --test_arg=--nocapture

.. note::

   The target is Linux only and is disabled under sanitizers until the findings in the Tokio multi-thread
   runtime are resolved, see the comment in the `BUILD` file.
