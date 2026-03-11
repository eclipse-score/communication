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
#include "score/mw/com/impl/plumbing/skeleton_binding_factory_impl.h"

#include "score/mw/com/impl/bindings/lola/partial_restart_path_builder.h"
#include "score/mw/com/impl/bindings/lola/shm_path_builder.h"
#include "score/mw/com/impl/bindings/lola/skeleton.h"

#include "score/filesystem/filesystem.h"

#include <score/assert.hpp>
#include <score/blank.hpp>
#include <score/overload.hpp>

#include <memory>
#include <utility>
#include <variant>

namespace score::mw::com::impl
{

namespace
{

const LolaServiceTypeDeployment& GetLolaServiceTypeDeploymentFromInstanceIdentifier(
    const InstanceIdentifier& identifier) noexcept
{
    const auto& service_type_depl_info = InstanceIdentifierView{identifier}.GetServiceTypeDeployment();
    const auto* lola_service_type_deployment =
        std::get_if<LolaServiceTypeDeployment>(&service_type_depl_info.binding_info_);
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(lola_service_type_deployment != nullptr,
                           "GetLolaServiceTypeDeploymentFromInstanceIdentifier: failed to get "
                           "LolaServiceTypeDeployment from binding_info_ variant.");
    return *lola_service_type_deployment;
}

}  // namespace

// Suppress "AUTOSAR C++14 A15-5-3" rule finding. This rule states: "The std::terminate() function shall
// not be called implicitly.". std::visit Throws std::bad_variant_access if
// as-variant(vars_i).valueless_by_exception() is true for any variant vars_i in vars. The variant may only become
// valueless if an exception is thrown during different stages. Since we don't throw exceptions, it's not possible
// that the variant can return true from valueless_by_exception and therefore not possible that std::visit throws
// an exception.
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto SkeletonBindingFactoryImpl::Create(const InstanceIdentifier& identifier) noexcept
    -> std::unique_ptr<SkeletonBinding>
{
    const InstanceIdentifierView identifier_view{identifier};

    auto visitor = score::cpp::overload(
        [&identifier](const LolaServiceInstanceDeployment&) -> std::unique_ptr<SkeletonBinding> {
            score::filesystem::Filesystem filesystem = filesystem::FilesystemFactory{}.CreateInstance();
            auto shm_path_builder = std::make_unique<lola::ShmPathBuilder>(
                GetLolaServiceTypeDeploymentFromInstanceIdentifier(identifier).service_id_);
            auto partial_restart_path_builder = std::make_unique<lola::PartialRestartPathBuilder>(
                GetLolaServiceTypeDeploymentFromInstanceIdentifier(identifier).service_id_);
            return lola::Skeleton::Create(
                identifier, filesystem, std::move(shm_path_builder), std::move(partial_restart_path_builder));
        },
        [](const SomeIpServiceInstanceDeployment&) noexcept -> std::unique_ptr<SkeletonBinding> {
            return nullptr; /* not yet implemented */
        },
        // Suppress "AUTOSAR C++14 A7-1-7" rule finding. This rule states: "Each
        // expression statement and identifier declaration shall be placed on a
        // separate line.". Following line statement is fine, this happens due to
        // clang formatting.
        // coverity[autosar_cpp14_a7_1_7_violation]
        [](const score::cpp::blank&) noexcept -> std::unique_ptr<SkeletonBinding> {
            return nullptr;
        });
    const auto& service_instance_deployment = identifier_view.GetServiceInstanceDeployment();
    return std::visit(visitor, service_instance_deployment.bindingInfo_);
}

}  // namespace score::mw::com::impl
