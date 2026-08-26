/*******************************************************************************
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
 *******************************************************************************/

#ifndef OUR_NAME_SPACE_SOMEINTERFACE_SOMEINTERFACE_PROXY_H
#define OUR_NAME_SPACE_SOMEINTERFACE_SOMEINTERFACE_PROXY_H

#include <our/name_space/someinterface/someinterface_common.h>

#include "score/mw/com/types.h"

namespace our::name_space::someinterface::proxy
{

namespace events
{
using Value = typename score::mw::com::impl::ProxyTrait::template Event<::our::name_space::ValueEvtType>;
}

namespace fields
{
}

using SomeInterfaceProxy = score::mw::com::AsProxy<SomeInterface>;

}  // namespace our::name_space::someinterface::proxy

#endif  // OUR_NAME_SPACE_SOMEINTERFACE_SOMEINTERFACE_PROXY_H
