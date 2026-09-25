/*********************************************************************************
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

#ifndef SCORE_MW_SERVICE_DETAILS_TUPLE_INDEX_FOR_TYPE_H
#define SCORE_MW_SERVICE_DETAILS_TUPLE_INDEX_FOR_TYPE_H

#include <cstdint>
#include <initializer_list>
#include <limits>
#include <tuple>

namespace score::mw::service::details
{

// Base class used for unpacking tuple element types in below class specialization
template <typename TypeToFind, std::size_t number_of_occurrence_of_type, typename Executor>
class TupleIndexForType
{
};

/// @brief Finds the index of a certain type in a tuple. In difference to std::get<>(), it supports if a type is given
/// multiple times within a tuple. In such a case, `number_of_occurrence_of_type` can be used to determine which of the
/// duplicated types shall be used.
///
/// @tparam TypeToFind The type to search for in the tuple
/// @tparam number_of_occurrence_of_type The number of matched types that shall be ignored
/// @tparam ElementsInTuple The types within the tuple
template <typename TypeToFind, std::size_t number_of_occurrence_of_type, typename... ElementsInTuple>
class TupleIndexForType<TypeToFind, number_of_occurrence_of_type, std::tuple<ElementsInTuple...>>
{
  public:
    constexpr static auto kInvalidIndex = std::numeric_limits<std::size_t>::max();

    constexpr explicit TupleIndexForType() noexcept = delete;

    constexpr static std::size_t Index() noexcept
    {
        // Due to members being not constexpr, we have a small data-encapsulation that holds the runtime data for our
        // algorithm.
        Data data{};

        // We need make this workaround, since C++17 fold-expressions are not there yet for us.
        // What do we do here? We call the std::initializer_list constructor for an integer array, in that we use the
        // parameter pack expansion to then call `Iterate` for each parameter within the pack.
        // coverity[autosar_cpp14_a8_5_3_violation] False positive: Variable not of type auto anymore
        const std::initializer_list<int> only_needed_for_expansion{0, (FindIndex<ElementsInTuple>(data), 0)...};
        static_cast<void>(only_needed_for_expansion);

        // At the end we return the found index
        return data.type_found_at_index;
    }

  private:
    // We need to be constexpr, thus not member that can be manipulated. Thus, we create a custom data class that is
    // passed down the call stack as needed
    struct Data
    {
        // coverity[autosar_cpp14_m11_0_1_violation] No harm, having public members in private struct
        std::size_t current_index_in_tuple{0U};
        // coverity[autosar_cpp14_m11_0_1_violation] No harm, having public members in private struct
        std::size_t current_number_of_types_found{0U};
        // coverity[autosar_cpp14_m11_0_1_violation] No harm, having public members in private struct
        std::size_t type_found_at_index{kInvalidIndex};
    };

    // We don't have constexpr-if yet (comes with C++17), thus we have to use SFINAE as static if
    // In this case the type we are looking for, matches the one expanded in the parameter pack
    template <typename T, std::enable_if_t<std::is_same<T, TypeToFind>::value, bool> = true>
    constexpr static void FindIndex(Data& data) noexcept
    {
        if (number_of_occurrence_of_type == data.current_number_of_types_found)
        {
            data.type_found_at_index = data.current_index_in_tuple;
        }
        data.current_number_of_types_found++;
        data.current_index_in_tuple++;
    }

    // We don't have constexpr-if yet (comes with C++17), thus we have to use SFINAE as static if
    // In this case the type we are looking for does _not_ match the one we are looking for
    template <typename T, std::enable_if_t<!std::is_same<T, TypeToFind>::value, bool> = true>
    constexpr static void FindIndex(Data& data) noexcept
    {
        static_assert(std::tuple_size_v<std::tuple<ElementsInTuple...>> <
                          std::numeric_limits<decltype(data.current_index_in_tuple)>::max(),
                      "Too many tuple elements.");
        // The static_assert above ensures that current_index_in_tuple does not exceed its maximum value.
        // coverity[autosar_cpp14_a4_7_1_violation] see above justification
        data.current_index_in_tuple++;
    }
};

}  // namespace score::mw::service::details

#endif  // SCORE_MW_SERVICE_DETAILS_TUPLE_INDEX_FOR_TYPE_H
