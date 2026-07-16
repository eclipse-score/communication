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

This module provides two components for high-performance, safety-critical
inter-process communication (IPC) in embedded automotive systems.

**LoLa** (mw::com) is the high-level communication middleware implementing
Adaptive AUTOSAR Communication Management. It offers a publisher/subscriber
skeleton/proxy framework, service discovery, and zero-copy shared-memory
event/field transport — qualified for ASIL-B on Linux and QNX.

**Message Passing** is the low-level foundation beneath LoLa. It exposes
uni-directional n-to-1 asynchronous channels with OS-native backends
(POSIX unix-domain sockets, QNX native messaging) and no heap allocation
in the data path.

.. list-table::
   :widths: 50 50
   :header-rows: 0

   * - :doc:`LoLa / mw::com <lola>`
     - :doc:`Message Passing <message_passing>`
   * - High-level AUTOSAR-aligned middleware.
       Zero-copy shared-memory transport.
       Service discovery, events, fields, methods.
     - Low-level async IPC primitives.
       POSIX + QNX backends.
       Foundation for LoLa.

.. toctree::
   :hidden:

   lola
   message_passing
   quality_reports
   CONTRIBUTING

