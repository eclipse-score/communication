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
#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_CONFIGURATION_COMMON_RESOURCES_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_CONFIGURATION_COMMON_RESOURCES_H

#include "score/json/json_parser.h"
#include "score/json/json_writer.h"
#include "score/mw/log/logging.h"

#include <score/assert.hpp>

#include <cstddef>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>
#include <type_traits>

namespace score::mw::com::impl
{

constexpr auto kSerializationVersionKey = "serializationVersion";
constexpr auto kBindingInfoKey = "bindingInfo";
constexpr auto kBindingInfoIndexKey = "bindingInfoIndex";

// There is a template-method in service_type_deployment.cpp with the same signature, but different due to template use
// coverity[autosar_cpp14_a2_10_4_violation] False positive, see above
std::string ToHashStringImpl(const std::uint16_t instance_id, const std::size_t hash_string_size) noexcept;
std::string ToStringImpl(const json::Object& serialized_json_object) noexcept;

/// \brief Helper function to get a value from a json object.
///
/// Since json::Object::As can either return a value (e.g. for integer types, std::string_view etc.) or a
/// reference_wrapper (e.g. for json::Object), we need to return the same from this function.
template <typename T,
          typename = std::enable_if_t<!std::is_arithmetic<T>::value>,
          typename = std::enable_if_t<!std::is_same<T, std::string_view>::value, bool>>
// coverity[autosar_cpp14_a15_5_3_violation] false positive, we check before accessing the variant.
auto GetValueFromJson(const score::json::Object& json_object, std::string_view key) noexcept -> const T&
{
    const auto it = json_object.find(key);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(it != json_object.end());
    const auto json_result = it->second.As<T>();
    if (!json_result.has_value())
    {
        score::mw::log::LogFatal("lola")
            << "Failed to parse JSON configuration key '" << key
            << "'. Configuration parsing failed. Terminating.";
        std::terminate();
    }
    return json_result.value();
}

template <typename T, typename = std::enable_if_t<std::is_arithmetic<T>::value, bool>>
// coverity[autosar_cpp14_a15_5_3_violation] false positive, we check before accessing the variant.
auto GetValueFromJson(const score::json::Object& json_object, std::string_view key) noexcept -> T
{
    const auto it = json_object.find(key);
    SCORE_LANGUAGE_FUTURECPP_ASSERT(it != json_object.end());
    const auto json_result = it->second.As<T>();
    if (!json_result.has_value())
    {
        score::mw::log::LogFatal("lola")
            << "Failed to parse JSON configuration key '" << key
            << "'. Configuration parsing failed. Terminating.";
        std::terminate();
    }
    return json_result.value();
}

template <typename T, typename = std::enable_if_t<std::is_same<T, std::string_view>::value, bool>>
// coverity[autosar_cpp14_a15_5_3_violation] false positive, we check before accessing the variant.
auto GetValueFromJson(const score::json::Object& json_object, std::string_view key) noexcept -> std::string_view
{
    const auto it = json_object.find(key);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(it != json_object.end());
    const auto json_result = it->second.As<T>();
    if (!json_result.has_value())
    {
        score::mw::log::LogFatal("lola")
            << "Failed to parse JSON configuration key '" << key
            << "'. Configuration parsing failed. Terminating.";
        std::terminate();
    }
    return json_result.value();
}

template <typename element_type>
// coverity[autosar_cpp14_a15_5_3_violation] false positive, ResultToOptionalOrElse checks before accessing the variant.
auto GetOptionalValueFromJson(const score::json::Object& json_object, std::string_view key) noexcept
    -> std::optional<element_type>
{

    const auto val_candidate = json_object.find(key);
    if (val_candidate == json_object.end())
    {
        return {};
    }

    return ResultToOptionalOrElse(val_candidate->second.As<element_type>(), [key](auto) noexcept {
        score::mw::log::LogFatal("lola") << "provided key" << key
                                       << "contains a value that can not be parsed. Terminating.\n";
        std::terminate();
    });
}

template <typename VariantHeldType>
class DoConstructVariant
{
  public:
    static VariantHeldType Do(const score::json::Object& json_object, std::string_view json_variant_key) noexcept
    {
        const auto& binding_info_json_object = GetValueFromJson<json::Object>(json_object, json_variant_key);
        return VariantHeldType{binding_info_json_object};
    }
};

template <>
class DoConstructVariant<score::cpp::blank>
{
  public:
    static score::cpp::blank Do(const score::json::Object&, std::string_view) noexcept
    {
        return score::cpp::blank{};
    }
};

/// ConstructVariant dispatches to DoConstructVariant in a template class to avoid function template specialization
template <typename VariantHeldType>
auto ConstructVariant(const score::json::Object& json_object, std::string_view json_variant_key) noexcept
    -> VariantHeldType
{
    return DoConstructVariant<VariantHeldType>::Do(json_object, json_variant_key);
}

/// \brief Helper function to deserialize a variant from a json Object
///
/// Accessing the type of a variant using an index can only be done at compile time. This class generates template
/// functions for each possible type within a variant such that a variant type can be created at runtime using an index.
///
/// \param json_object The json object containing the serialized variant
/// \param variant_index The index of the variant (taken from std::variant::index() which should be serialized /
/// deserialized along with the variant itself)
/// \param json_variant_key The key for the variant in the json object
template <std::size_t VariantIndex,
          typename VariantType,
          typename std::enable_if<VariantIndex == std::variant_size<VariantType>::value>::type* = nullptr>
auto DeserializeVariant(const score::json::Object& /* json_object */,
                        std::ptrdiff_t /* variant_index */,
                        std::string_view /* json_variant_key */) noexcept -> VariantType
{
    std::terminate();
}

template <std::size_t VariantIndex,
          typename VariantType,
          typename std::enable_if<VariantIndex<std::variant_size<VariantType>::value>::type* = nullptr> auto
              DeserializeVariant(const score::json::Object& json_object,
                                 std::ptrdiff_t variant_index,
                                 std::string_view json_variant_key) noexcept -> VariantType
{
    if (static_cast<std::size_t>(variant_index) == VariantIndex)
    {
        using VariantHeldType = typename std::variant_alternative<VariantIndex, VariantType>::type;
        VariantHeldType deployment{ConstructVariant<VariantHeldType>(json_object, json_variant_key)};
        return VariantType{std::move(deployment)};
    }
    else
    {
        return DeserializeVariant<VariantIndex + 1, VariantType>(json_object, variant_index, json_variant_key);
    }
}

template <typename ServiceElementInstanceMapping>
json::Object ConvertServiceElementMapToJson(const ServiceElementInstanceMapping& input_map) noexcept
{
    json::Object service_element_instance_mapping_object{};
    for (auto it : input_map)
    {
        const auto insert_result =
            service_element_instance_mapping_object.insert(std::make_pair(it.first, it.second.Serialize()));
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(insert_result.second, "Could not insert element in map");
    }
    return service_element_instance_mapping_object;
}

template <typename ServiceElementInstanceMapping>
// coverity[autosar_cpp14_a15_5_3_violation] false positive, we check before accessing the variant.
auto ConvertJsonToServiceElementMap(const json::Object& json_object, std::string_view key) noexcept
    -> ServiceElementInstanceMapping
{
    const auto& service_element_json = GetValueFromJson<json::Object>(json_object, key);

    ServiceElementInstanceMapping service_element_map{};
    for (auto& it : service_element_json)
    {
        const score::cpp::string_view event_name_view{it.first.GetAsStringView()};
        const std::string event_name{event_name_view.data(), event_name_view.size()};
        auto event_instance_deployment_json = it.second.As<score::json::Object>().value();
        const auto insert_result =
            service_element_map.insert(std::make_pair(event_name, event_instance_deployment_json));
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(insert_result.second, "Could not insert element in map");
    }
    return service_element_map;
}

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_CONFIGURATION_COMMON_RESOURCES_H
