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

#ifndef SCORE_MW_COM_IMPL_SERVICE_ELEMENT_MAP_VIEW_FACTORY_H
#define SCORE_MW_COM_IMPL_SERVICE_ELEMENT_MAP_VIEW_FACTORY_H

#include "score/mw/com/impl/service_element_map_view.h"

namespace score::mw::com::impl
{

/// \brief Factory clas template for creation of ServiceElementMapView<ElementType> instances.
/// \details ServiceElementMapView<ElementType> instances are used/returned from GenericProxy/GenericSkeleton to the
/// user. We don't want the user to be able to create ServiceElementMapView<ElementType> on its own, thus the ctor is
/// private.
/// Therefore, we have this factory, which is a friend to ServiceElementMapView<ElementType> and not accessible by the
/// users.
template <typename ElementType>
class ServiceElementMapViewFactory
{
  public:
    using map_type = typename ServiceElementMapView<ElementType>::map_type;

    /// \brief Create factory method to create a ServiceElementMapView<ElementType> instance
    /// @param element_map underlying map for which the view gets created
    /// @return read-only view to the map.
    static ServiceElementMapView<ElementType> Create(map_type& element_map)
    {
        return ServiceElementMapView<ElementType>{element_map};
    }
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_SERVICE_ELEMENT_MAP_VIEW_FACTORY_H
