/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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

#include "vehicle_gen.h"
#include "score/mw/com/impl/rust/registry_bridge_macro.h"

// Export the Vehicle interface with FFI bindings
// VehicleInterface Id used  from BEGIN Macro to the END Macro
// So Event macro should be called in between these two macros because it uses the same Id
BEGIN_EXPORT_MW_COM_INTERFACE(VehicleInterface, ::score::mw::com::VehicleProxy, ::score::mw::com::VehicleSkeleton)
EXPORT_MW_COM_EVENT(::score::mw::com::Tire, left_tire)
EXPORT_MW_COM_EVENT(::score::mw::com::Exhaust, exhaust)

END_EXPORT_MW_COM_INTERFACE()

// Export data types - commented out due to type issue with macro
EXPORT_MW_COM_TYPE(Tire, ::score::mw::com::Tire)
EXPORT_MW_COM_TYPE(Exhaust, ::score::mw::com::Exhaust)
