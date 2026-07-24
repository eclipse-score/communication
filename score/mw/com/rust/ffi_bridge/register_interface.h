/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

/// \file register_interface.h
/// \brief Public entry point for COM interface registration macros of rust APIs
/// \details This is the canonical public header for registering COM interfaces in generated C++ code.
/// It provides registration macros for code generators to create FFI bindings between the Rust COM API
/// and C++ interface implementations. These macros are used in generated C++ files (not user code)
/// to register interface operations at compile time.
///
/// The registration system uses a compile-time registry that maps interface and type operations
/// to their runtime implementations, enabling the Rust COM API to work with different runtime
/// implementations (LoLa, Mock, etc.) without static coupling.
///
/// Usage is typically in generated code files:
/// ```
/// #include "score/mw/com/rust/ffi_bridge/register_interface.h"
///
/// BEGIN_EXPORT_MW_COM_INTERFACE(VehicleInterface, VehicleProxy, VehicleSkeleton)
/// EXPORT_MW_COM_EVENT(Tire, left_tire)
/// EXPORT_MW_COM_EVENT(Exhaust, exhaust)
/// END_EXPORT_MW_COM_INTERFACE()
///
/// EXPORT_MW_COM_TYPE(TireType, Tire)
/// EXPORT_MW_COM_TYPE(ExhaustType, Exhaust)
/// ```
///
#ifndef SCORE_MW_COM_RUST_REGISTER_INTERFACE_H
#define SCORE_MW_COM_RUST_REGISTER_INTERFACE_H

// Include the FFI registry infrastructure that provides all type declarations,
// helper functions, and registration macros (BEGIN_EXPORT_MW_COM_INTERFACE,
// EXPORT_MW_COM_EVENT, END_EXPORT_MW_COM_INTERFACE, EXPORT_MW_COM_TYPE).

#include "score/mw/com/impl/rust/com-api/com-api-ffi-lola/registry_bridge_macro.h"

#endif  // SCORE_MW_COM_RUST_REGISTER_INTERFACE_H
