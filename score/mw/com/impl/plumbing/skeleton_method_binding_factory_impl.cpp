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
#include "score/mw/com/impl/plumbing/skeleton_method_binding_factory_impl.h"
#include "score/mw/com/impl/bindings/lola/methods/type_erased_call_queue.h"
#include "score/mw/com/impl/bindings/lola/skeleton.h"
#include "score/mw/com/impl/bindings/lola/skeleton_method.h"
#include "score/mw/com/impl/instance_identifier.h"

namespace score::mw::com::impl
{
auto SkeletonMethodBindingFactoryImpl::Create(const InstanceIdentifier& instance_identifier,
                                              SkeletonBinding* parent_binding,
                                              const std::string_view method_name,
                                              const MethodSizeInfo& size_info) -> std::unique_ptr<SkeletonMethodBinding>
{

    const InstanceIdentifierView instance_identifier_view{instance_identifier};

    using LambdaReturnType = std::unique_ptr<SkeletonMethodBinding>;
    auto lola_deployment_handler = [&instance_identifier_view, parent_binding, &method_name, &size_info](
                                       const LolaServiceTypeDeployment& lola_type_deployment) -> LambdaReturnType {
        auto* const lola_parent = dynamic_cast<lola::Skeleton*>(parent_binding);
        if (lola_parent == nullptr)
        {
            constexpr std::string_view error_msg =
                "Skeleton Method could not be created because parent skeleton binding is a not a lola binding.";

            score::mw::log::LogError("lola") << error_msg;

            return nullptr;
        }

        const auto instance_id_maybe = instance_identifier_view.GetServiceInstanceId();
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(instance_id_maybe.has_value(),
                                                    "Skeletons must always be configured with a valid InstanceId");
        const auto* const lola_service_instance_id =
            std::get_if<LolaServiceInstanceId>(&(instance_id_maybe.value().binding_info_));
        SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD_MESSAGE(lola_service_instance_id != nullptr,
                                                    "ServiceInstanceId does not contain lola binding.");

        constexpr auto element_type{ServiceElementType::METHOD};

        const auto lola_method_id = GetServiceElementId<element_type>(lola_type_deployment, std::string{method_name});
        const lola::ElementFqId element_fq_id{
            lola_type_deployment.service_id_, lola_method_id, lola_service_instance_id->GetId(), element_type};

        // MethodSizeInfo is binding-agnostic and carries impl::DataTypeMetaInfo. The lola call
        // queue primitives speak score::memory::DataTypeSizeInfo, so translate here.
        const auto to_size_info =
            [](const std::optional<DataTypeMetaInfo>& meta) -> std::optional<memory::DataTypeSizeInfo> {
            if (!meta.has_value())
            {
                return std::nullopt;
            }
            return memory::DataTypeSizeInfo{meta->size, meta->alignment};
        };
        const lola::TypeErasedCallQueue::TypeErasedElementInfo type_erased_element_info{
            to_size_info(size_info.in_args_type_info), to_size_info(size_info.return_type_info), size_info.queue_size};

        return std::make_unique<lola::SkeletonMethod>(*lola_parent, element_fq_id, type_erased_element_info);
    };

    auto deployment_info_visitor = score::cpp::overload(
        lola_deployment_handler,
        [](const score::cpp::blank&) noexcept -> LambdaReturnType {
            // coverage false positive. Covered by the test:
            // SkeletonMethodFactoryFixture.CannotConstructEventFromSomeIpBinding
            // inside score/mw/com/impl/plumbing/skeleton_method_binding_factory_test.cpp
            // LCOV_EXCL_BR_LINE (false positive. See justification above).
            return nullptr;
        },
        [](const SomeIpServiceInstanceDeployment&) noexcept -> LambdaReturnType {
            // coverage false positive. Covered by the test:
            // SkeletonMethodFactoryFixture.CannotConstructEventFromBlankBinding
            // inside score/mw/com/impl/plumbing/skeleton_method_binding_factory_test.cpp
            // LCOV_EXCL_BR_LINE(false positive. See justification above).
            return nullptr;
        });

    const auto& type_deployment = instance_identifier_view.GetServiceTypeDeployment();
    return std::visit(deployment_info_visitor, type_deployment.binding_info_);
}
}  // namespace score::mw::com::impl
