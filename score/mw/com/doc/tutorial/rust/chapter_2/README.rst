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

Chapter 2: Defining the service interface
=========================================

In C++ the service interface is a hand-written proxy/skeleton pair. In Rust it is declared once with the `interface!`
macro. The macro generates the consumer type, the producer type and the offered producer type for the interface.

Data types
~~~~~~~~~~

Data exchanged between producer and consumer must be relocatable and plain data. This is expressed with
`#[repr(C)]` and the derives `Reloc` and `CommData`:

.. literalinclude:: ../common/com-api-gen/com_api_gen.rs
   :language: rust
   :start-after: [rust-data-types-start]
   :end-before: [rust-data-types-end]
   :caption: common/com-api-gen/com_api_gen.rs

The interface
~~~~~~~~~~~~~

The Vehicle interface has two events:

.. literalinclude:: ../common/com-api-gen/com_api_gen.rs
   :language: rust
   :start-after: [rust-vehicle-interface-start]
   :end-before: [rust-vehicle-interface-end]
   :caption: common/com-api-gen/com_api_gen.rs

From this declaration the macro generates:

- `VehicleInterface` with the interface id `VehicleInterface`
- a consumer type with the members `left_tire` and `exhaust`, which can be subscribed to
- a producer type and an offered producer type with the same members, which can be used to publish samples

The generated types are generic over the runtime `R`. The sample library defines short aliases for them:

.. literalinclude:: ../common/lib.rs
   :language: rust
   :start-after: [rust-type-aliases-start]
   :end-before: [rust-type-aliases-end]
   :caption: common/lib.rs

.. note::

   The `Id` used in the macro and the event names must match the service type and event names in
   `mw_com_config.json`, see chapter 1.
