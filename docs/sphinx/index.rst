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

Communication Middleware
========================

This module provides components for high-performance, safety-critical
inter-process communication (IPC) in embedded automotive systems.

**LoLa** (mw::com) is the high-level communication middleware implementing
Adaptive AUTOSAR Communication Management. It offers a publisher/subscriber
skeleton/proxy framework, service discovery, and zero-copy shared-memory
event/field transport — qualified for ASIL-B on Linux and QNX.

**Message Passing** is the low-level foundation beneath LoLa. It exposes
uni-directional n-to-1 asynchronous channels with OS-native backends
(POSIX unix-domain sockets, QNX native messaging) and no heap allocation
in the data path.

**Shared Memory** is the zero-copy transport foundation underneath LoLa. It
provides the shared-memory-safe pointer and allocator abstractions
(``OffsetPtr``, ``ManagedMemoryResource``) that back LoLa's event/field data.

.. list-table::
   :widths: 34 33 33
   :header-rows: 0

   * - :doc:`LoLa / mw::com <lola>`
     - :doc:`Message Passing <message_passing>`
     - :doc:`Shared Memory <memory_shared>`
   * - High-level AUTOSAR-aligned middleware.
       Zero-copy shared-memory transport.
       Service discovery, events, fields, methods.
     - Low-level async IPC primitives.
       POSIX + QNX backends.
       Foundation for LoLa.
     - Shared-memory-safe pointers and allocators.
       Zero-copy data foundation for LoLa.

.. toctree::
   :hidden:

   lola
   message_passing
   memory_shared
   quality_reports
   CONTRIBUTING

