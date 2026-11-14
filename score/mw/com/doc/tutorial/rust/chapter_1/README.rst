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

Chapter 1: Vehicle Monitor and runtime setup
============================================

The sample applications simulate a **Vehicle Monitor** that publishes two kinds of events:

- **Tire pressure data** (`left_tire`)
- **Exhaust data** (`exhaust`)

A producer application publishes tire data at regular intervals. Consumer applications subscribe to the events and log
the received samples. Producer and consumer are separate processes that communicate through the LoLa binding. The
code shared by all chapters, namely the runtime setup, the logging setup and the generated service interface, lives in
the `common` directory.

Creating the runtime
~~~~~~~~~~~~~~~~~~~~

Everything starts with a runtime. The LoLa runtime is created from a `mw_com_config.json` file, which is the same
configuration file format the C++ API uses.

.. literalinclude:: ../common/lib.rs
   :language: rust
   :start-after: [rust-main-runtime-start]
   :end-before: [rust-main-runtime-end]
   :caption: common/lib.rs

The configuration file defines the service type with its events, and the service instance
`/Vehicle/Service1/Instance`. The producer offers this instance and the consumer looks it up using the same instance specifier:

.. literalinclude:: ../common/etc/mw_com_config.json
   :language: json
   :caption: common/etc/mw_com_config.json

Applications of the tutorial
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :header-rows: 1
   :widths: 20 40 40

   * - Chapter
     - Bazel target
     - Description
   * - 3
     - `//score/mw/com/doc/tutorial/rust/chapter_3:producer_app`
     - Offers the service and publishes samples
   * - 4
     - `//score/mw/com/doc/tutorial/rust/chapter_4:consumer_app`
     - Sync discovery and sync receive
   * - 5
     - `//score/mw/com/doc/tutorial/rust/chapter_5:async_consumer_app`
     - Async discovery, async receive, timeout, stream
   * - 6
     - `//score/mw/com/doc/tutorial/rust/chapter_6:fields`
     - Compile-only field demonstration
   * - 7
     - `//score/mw/com/doc/tutorial/rust/chapter_7:methods`
     - Compile-only method demonstration
   * - 8
     - `//score/mw/com/doc/tutorial/rust/chapter_8:tokio-usage`
     - Tokio runtime, integration test

All applications accept `-s, --service-instance-manifest <PATH>` to use a different configuration file. The default is
`./score/mw/com/doc/tutorial/rust/common/etc/mw_com_config.json`.

Quick start
~~~~~~~~~~~

Run producer and consumer in two terminals:

.. code-block:: bash

   bazel run //score/mw/com/doc/tutorial/rust/chapter_3:producer_app
   bazel run //score/mw/com/doc/tutorial/rust/chapter_4:consumer_app
