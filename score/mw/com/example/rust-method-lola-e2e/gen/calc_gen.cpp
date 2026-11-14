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
#include "calc_gen.h"
#include "score/mw/com/rust/score_com_cpp_bridge/register_interface.h"

BEGIN_EXPORT_MW_COM_INTERFACE(CalcInterface, ::score::mw::com::CalcProxy, ::score::mw::com::CalcSkeleton)
EXPORT_MW_COM_METHOD(add)
EXPORT_MW_COM_METHOD(mix)
EXPORT_MW_COM_METHOD(ping)
EXPORT_MW_COM_METHOD(reset)
EXPORT_MW_COM_METHOD(last)
EXPORT_MW_COM_METHOD(slow)
END_EXPORT_MW_COM_INTERFACE()

EXPORT_MW_COM_TYPE(Operands, ::score::mw::com::Operands)
EXPORT_MW_COM_TYPE(Total, ::score::mw::com::Total)
EXPORT_MW_COM_TYPE(Wide, ::score::mw::com::Wide)
