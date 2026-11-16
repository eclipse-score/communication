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

score::mw::com - Rust APIs Tutorial
===================================

This part contains the Rust API example and tutorial of `score::mw::com` (crate `score_com`), based on the
**Vehicle Monitor** scenario.

The concepts (service orientation, proxies and skeletons, the LoLa binding) are explained in the
:doc:`C++ tutorial introduction <../README>`. They apply to Rust unchanged. In the Rust API a proxy is called
*consumer* and a skeleton is called *producer*.

Directory layout
----------------

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Path
     - Purpose
   * - `common/com-api-gen`
     - Service interface definitions created with the `interface!` macro, used by all chapters
   * - `common/lib.rs`
     - Runtime and logging setup, type aliases
   * - `common/etc`
     - `mw_com_config.json` and `logging.json`
   * - `chapter_N`
     - Document and application of chapter N

Tutorial chapters
-----------------

Follow the chapters in order. Later chapters build on earlier ones.

.. toctree::
   :maxdepth: 1

   Chapter 1: Running the Vehicle Monitor <chapter_1/README>
   Chapter 2: Defining the service interface <chapter_2/README>
   Chapter 3: Offering a service as producer <chapter_3/README>
   Chapter 4: Finding and consuming a service <chapter_4/README>
   Chapter 5: Async reception, timeouts and streams <chapter_5/README>
   Chapter 6: Fields <chapter_6/README>
   Chapter 7: Methods <chapter_7/README>
   Chapter 8: Using the Tokio runtime <chapter_8/README>
