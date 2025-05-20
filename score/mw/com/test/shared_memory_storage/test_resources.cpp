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

#include "score/mw/com/test/shared_memory_storage/test_resources.h"

namespace bmw::mw::com::test
{

    bool operator==(const BigDataServiceElementData &lhs, const BigDataServiceElementData &rhs) noexcept
    {
        return (
            (lhs.service_element_element_fq_ids == rhs.service_element_element_fq_ids) &&
            (lhs.service_element_type_meta_information_addresses == rhs.service_element_type_meta_information_addresses));
    }

    bool operator!=(const BigDataServiceElementData &lhs, const BigDataServiceElementData &rhs) noexcept
    {
        return !(lhs == rhs);
    }

    std::ostream &operator<<(std::ostream &output_stream, const BigDataServiceElementData &service_element_data)
    {
        output_stream << "map_api_lanes_stamped Type meta information: "
                      << service_element_data.service_element_type_meta_information_addresses[0]
                      << ".\n dummy_data_stamped Type meta information: "
                      << service_element_data.service_element_type_meta_information_addresses[1]
                      << ".\n map_api_lanes_stamped ElementFqId: "
                      << service_element_data.service_element_element_fq_ids[0].ToString()
                      << ".\n dummy_data_stamped ElementFqId: "
                      << service_element_data.service_element_element_fq_ids[1].ToString() << ".\n";
        return output_stream;
    }

} // namespace bmw::mw::com::test
