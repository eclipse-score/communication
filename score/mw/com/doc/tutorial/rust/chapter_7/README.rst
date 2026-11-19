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

Chapter 7: Methods
==================

.. warning::

   The method API is shown for demonstration. The runtime implementation is not available yet, therefore
   the code in `method_*.rs` compiles but is not called by an application.

Methods use the `Method(...)` syntax, symmetric to `Event<T>` and `Field<T, ...>`:

.. literalinclude:: ../common/com-api-gen/com_api_gen.rs
   :language: rust
   :start-after: [rust-methods-interface-start]
   :end-before: [rust-methods-interface-end]
   :caption: common/com-api-gen/com_api_gen.rs

Producer side
~~~~~~~~~~~~~

The producer registers one handler per method on the producer returned by `init()`. A handler returns a
future, so the application chooses the executor. Forgetting a handler is a compile error, because `offer()`
only becomes available after all handlers are registered.

.. literalinclude:: method_producer.rs
   :language: rust
   :start-after: [rust-method-producer-start]
   :end-before: [rust-method-producer-end]
   :caption: method_producer.rs

Handlers can be called concurrently by the runtime, so shared data needs synchronization.

Consumer side
~~~~~~~~~~~~~

The consumer calls methods as async functions, see `method_consumer.rs`.

A single interface can combine events, fields and methods. A compile-only example is in
`common/mixed_monitor.rs`.

For the design background see the
`method design document <https://github.com/eclipse-score/communication/blob/main/score/mw/com/rust/design/design_document_method.md>`__.
