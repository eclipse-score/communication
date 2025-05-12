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
#ifndef SCORE_MW_COM_IMPL_INSTANCEIDENTIFIER_H
#define SCORE_MW_COM_IMPL_INSTANCEIDENTIFIER_H

#include "score/mw/com/impl/com_error.h"
#include "score/mw/com/impl/configuration/configuration.h"
#include "score/mw/com/impl/configuration/service_identifier_type.h"
#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/configuration/service_instance_id.h"
#include "score/mw/com/impl/configuration/service_type_deployment.h"
#include "score/mw/com/impl/configuration/service_version_type.h"

#include <score/string_view.hpp>

#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace score::mw::com::impl
{

/**
 * \brief Represents a specific instance of a given service
 *
 * \requirement SWS_CM_00302
 */
class InstanceIdentifier final
{
  public:
    /**
     * \brief Exception-less constructor to create InstanceIdentifier from a serialized InstanceIdentifier created with
     * InstanceIdentifier::ToString()
     */
    static score::Result<InstanceIdentifier> Create(score::cpp::string_view serialized_format) noexcept;

    InstanceIdentifier() = delete;
    ~InstanceIdentifier() noexcept = default;

    /**
     * ctor from serialized representation.
     *
     * Constructor is required by adaptive AUTOSAR Standard. But it uses exceptions, thus we will not implement it.
     *
     * explicit InstanceIdentifier(score::cpp::string_view value);
     */

    /**
     * \brief CopyAssignment for InstanceIdentifier
     *
     * \post *this == other
     * \param other The InstanceIdentifier *this shall be constructed from
     * \return The InstanceIdentifier that was constructed
     */
    InstanceIdentifier& operator=(const InstanceIdentifier& other) = default;
    InstanceIdentifier(const InstanceIdentifier&) = default;
    InstanceIdentifier(InstanceIdentifier&&) noexcept = default;
    InstanceIdentifier& operator=(InstanceIdentifier&& other) = default;

    /**
     * \brief Returns the serialized form of the unknown internals of this class as a meaningful string
     *
     * \return A non-owning string representation of the internals of this class
     */
    std::string_view ToString() const noexcept;

    /**
     * \brief Compares two instances for equality
     *
     * \param lhs The first instance to check for equality
     * \param rhs The second instance to check for equality
     * \return true if other and *this equal, false otherwise
     */
    friend bool operator==(const InstanceIdentifier& lhs, const InstanceIdentifier& rhs) noexcept;

    /**
     * \brief LessThanComparable operator
     *
     * \param lhs The first InstanceIdentifier instance to compare
     * \param rhs The second InstanceIdentifier instance to compare
     * \return true if *this is less then other, false otherwise
     */
    friend bool operator<(const InstanceIdentifier& lhs, const InstanceIdentifier& rhs) noexcept;

  private:
    const ServiceInstanceDeployment* instance_deployment_;
    const ServiceTypeDeployment* type_deployment_;

    /**
     * @brief Internal constructor to construct an InstanceIdentifier from a json-serialized InstanceIdentifier
     *
     * @param json_object Used to construct the InstanceIdentifier (no copies of json_object are made internally).
     * @param serialized_string Serialized string which the json_object is derived from. Used to set serialized_string_.
     */
    explicit InstanceIdentifier(const json::Object& json_object, std::string serialized_string) noexcept;

    /**
     * @brief internal impl. specific ctor.
     *
     * @param service identification of service
     * @param version version info
     * @param deployment deployment info
     */
    explicit InstanceIdentifier(const ServiceInstanceDeployment&, const ServiceTypeDeployment&) noexcept;

    static void SetConfiguration(Configuration* const configuration) noexcept
    {
        InstanceIdentifier::configuration_ = configuration;
    }

    json::Object Serialize() const noexcept;

    /**
     * @brief serialized format of this InstanceIdentifier instance
     */
    std::string serialized_string_;

    /**
     * @brief serialization format version.
     *
     * Whenever the state/content of this class changes in a way, which has effect on serialization, this version
     * has to be incremented! We potentially transfer instances of this class in a serialized form between processes
     * and need to know in the receiver process, if this serialized instance can be understood.
     */
    constexpr static std::uint32_t serializationVersion{1U};

    /**
     * \brief Global configuration object which is parsed from a json file and loaded by the runtime
     *
     * Whenever an InstanceIdentifier is created from another serialized InstanceIdentifier, the ServiceTypeDeployment /
     * ServiceInstanceDeployment held by the serialized InstanceIdentifier needs to be added to the maps within the
     * global configuration object. The newly created InstanceIdentifier will then store pointers to these structs.
     */
    static Configuration* configuration_;

    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // Design decision: Hide the constructor of the instance identifier.
    // This way more implementation details can be hidden from the user.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend InstanceIdentifier make_InstanceIdentifier(const ServiceInstanceDeployment& instance_deployment,
                                                      const ServiceTypeDeployment& type_deployment) noexcept;

    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // Design decision. This class provides a view to the private members of this class.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class InstanceIdentifierView;

    // Suppress "AUTOSAR C++14 A11-3-1", The rule declares: "Friend declarations shall not be used".
    // Design dessision: The "*Attorney" class is a helper, which sets the internal state of this class accessing
    // private members and used for testing purposes only.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class InstanceIdentifierAttorney;

    // Suppress "AUTOSAR C++14 A11-3-1", The rule states: "Friend declarations shall not be used".
    // Design decision: Instance identifier is used by the runtime which requires access to its internals.
    // coverity[autosar_cpp14_a11_3_1_violation]
    friend class Runtime;
};

/**
 * \brief A make_ function is introduced to hide the Constructor of InstanceIdentifier.
 * The InstanceIdentifier will be exposed to the API user and by not having a public constructor
 * we can avoid that by chance the user will construct this class. Introducing a custom make method
 * that is _not_ mentioned in the standard, will avoid this!
 *
 * \param service The service this instance is revering to
 * \param version The version of the service this instances revers to
 * \param deployment The deployment specific information for this instance
 * \return A constructed InstanceIdentifier
 */
inline InstanceIdentifier make_InstanceIdentifier(const ServiceInstanceDeployment& instance_deployment,
                                                  const ServiceTypeDeployment& type_deployment) noexcept
{
    return InstanceIdentifier{instance_deployment, type_deployment};
}

/**
 * \brief The score::mw::com::InstanceIdentifiers API is described by the ara::com standard.
 * But we also need to use it for internal purposes, why we need to access some state information
 * that is not exposed by the public API described in the adaptive AUTOSAR Standard.
 * In order to not leak implementation details, we come up with a `View` onto the InstanceIdentifier.
 * Since our view is anyhow _only_ located in the `impl` namespace, there is zero probability that
 * any well minded user would depend on it.
 */
class InstanceIdentifierView final
{
  public:
    explicit InstanceIdentifierView(const InstanceIdentifier&);

    json::Object Serialize() const noexcept
    {
        return identifier_.Serialize();
    };

    score::cpp::optional<ServiceInstanceId> GetServiceInstanceId() const noexcept;
    const ServiceInstanceDeployment& GetServiceInstanceDeployment() const noexcept;
    const ServiceTypeDeployment& GetServiceTypeDeployment() const;
    bool isCompatibleWith(const InstanceIdentifier&) const;
    bool isCompatibleWith(const InstanceIdentifierView&) const;
    constexpr static std::uint32_t GetSerializationVersion()
    {
        return InstanceIdentifier::serializationVersion;
    };

  private:
    const InstanceIdentifier& identifier_;
};

}  // namespace score::mw::com::impl

template <>
class std::hash<score::mw::com::impl::InstanceIdentifier>
{
  public:
    std::size_t operator()(const score::mw::com::impl::InstanceIdentifier&) const noexcept;
};

#endif  // SCORE_MW_COM_IMPL_INSTANCEIDENTIFIER_H
