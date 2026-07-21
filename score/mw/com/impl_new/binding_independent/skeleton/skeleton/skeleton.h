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

#ifndef SCORE_MW_COM_IMPL_NEW_BINDING_INDEPENDENT_SKELETON_SKELETON_SKELETON_H
#define SCORE_MW_COM_IMPL_NEW_BINDING_INDEPENDENT_SKELETON_SKELETON_SKELETON_H

namespace score::mw::com::impl
{

class Skeleton
{
  public:
    void PrepareOffer();
    void PrepareStopOffer();
    void VerifyAllMethodHandlersRegistered();
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_NEW_BINDING_INDEPENDENT_SKELETON_SKELETON_SKELETON_H
