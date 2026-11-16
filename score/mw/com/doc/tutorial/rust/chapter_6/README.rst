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

Chapter 6: Fields
=================

.. warning::

   The field API is shown for demonstration. The runtime implementation is not available yet, therefore
   the code in `field_*.rs` compiles but is not called by an application.

A field carries a current value and can have a notifier, a getter and a setter, selected via the tags
`WithNotifier`, `WithGetter` and `WithSetter`:

.. literalinclude:: ../common/com-api-gen/com_api_gen.rs
   :language: rust
   :start-after: [rust-field-interface-start]
   :end-before: [rust-field-interface-end]
   :caption: common/com-api-gen/com_api_gen.rs

Consumer side
~~~~~~~~~~~~~

The consumer gets async `get_<name>()` and `set_<name>(value)` functions. They are independent of the
subscription. The notifier part is used like an event:

.. literalinclude:: field_consumer.rs
   :language: rust
   :start-after: [rust-field-consumer-start]
   :end-before: [rust-field-consumer-end]
   :caption: field_consumer.rs

Producer side
~~~~~~~~~~~~~

On the producer side all fields need an initial value before `offer()` is available. This is enforced at
compile time by the type state pattern, see `field_producer.rs`.

A single interface can combine events, fields and methods. A compile-only example is in
`common/mixed_monitor.rs`.

For the design background see the
`field design document <https://github.com/eclipse-score/communication/blob/main/score/mw/com/rust/design/design_document_field.md>`__.
