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

Shared Memory
=============

.. note::
   For detailed design documentation, refer to the Shared Memory design README
   at ``score/memory/shared/design/README.md``.

Overview
--------

The shared memory module provides an abstraction layer for allocating and
accessing data in shared memory, used as the zero-copy transport
foundation beneath LoLa (mw::com). It is an implementation detail of
``mw::com`` and is not exposed as a customer-facing API; usage of shared
memory and its allocators is fully transparent to a ``mw::com``-API user.

.. toctree::
   :hidden:

   self
   docs/sphinx/dependable_element_memory_shared_index/index
