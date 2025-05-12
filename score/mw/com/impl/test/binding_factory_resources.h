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
#ifndef SCORE_MW_COM_IMPL_TEST_BINDING_FACTORY_RESOURCES_H
#define SCORE_MW_COM_IMPL_TEST_BINDING_FACTORY_RESOURCES_H

#include "score/mw/com/impl/bindings/mock_binding/generic_proxy_event.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy.h"
#include "score/mw/com/impl/bindings/mock_binding/proxy_event.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton.h"
#include "score/mw/com/impl/bindings/mock_binding/skeleton_event.h"
#include "score/mw/com/impl/plumbing/proxy_binding_factory.h"
#include "score/mw/com/impl/plumbing/proxy_binding_factory_mock.h"
#include "score/mw/com/impl/plumbing/proxy_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/proxy_event_binding_factory_mock.h"
#include "score/mw/com/impl/plumbing/proxy_field_binding_factory.h"
#include "score/mw/com/impl/plumbing/proxy_field_binding_factory_mock.h"
#include "score/mw/com/impl/plumbing/skeleton_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_binding_factory_mock.h"
#include "score/mw/com/impl/plumbing/skeleton_event_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_event_binding_factory_mock.h"
#include "score/mw/com/impl/plumbing/skeleton_field_binding_factory.h"
#include "score/mw/com/impl/plumbing/skeleton_field_binding_factory_mock.h"

#include <memory>
#include <unordered_map>

#include <gmock/gmock.h>

namespace score::mw::com::impl
{

class GenericProxyEventBindingFactoryMockGuard
{
  public:
    GenericProxyEventBindingFactoryMockGuard() noexcept
    {
        GenericProxyEventBindingFactory::InjectMockBinding(&factory_mock_);
    }
    ~GenericProxyEventBindingFactoryMockGuard() noexcept
    {
        GenericProxyEventBindingFactory::InjectMockBinding(nullptr);
    }

    ::testing::StrictMock<GenericProxyEventBindingFactoryMock> factory_mock_{};
};

class ProxyBindingFactoryMockGuard
{
  public:
    ProxyBindingFactoryMockGuard() noexcept
    {
        ProxyBindingFactory::InjectMockBinding(&factory_mock_);
    }
    ~ProxyBindingFactoryMockGuard() noexcept
    {
        ProxyBindingFactory::InjectMockBinding(nullptr);
    }

    ::testing::StrictMock<ProxyBindingFactoryMock> factory_mock_;
};

template <typename SampleType>
class ProxyEventBindingFactoryMockGuard
{
  public:
    ProxyEventBindingFactoryMockGuard() noexcept
    {
        ProxyEventBindingFactory<SampleType>::InjectMockBinding(&factory_mock_);
    }
    ~ProxyEventBindingFactoryMockGuard() noexcept
    {
        ProxyEventBindingFactory<SampleType>::InjectMockBinding(nullptr);
    }

    ::testing::StrictMock<ProxyEventBindingFactoryMock<SampleType>> factory_mock_;
};

template <typename SampleType>
class ProxyFieldBindingFactoryMockGuard
{
  public:
    ProxyFieldBindingFactoryMockGuard() noexcept
    {
        ProxyFieldBindingFactory<SampleType>::InjectMockBinding(&factory_mock_);
    }
    ~ProxyFieldBindingFactoryMockGuard() noexcept
    {
        ProxyFieldBindingFactory<SampleType>::InjectMockBinding(nullptr);
    }

    ::testing::StrictMock<ProxyFieldBindingFactoryMock<SampleType>> factory_mock_;
};

class SkeletonBindingFactoryMockGuard
{
  public:
    SkeletonBindingFactoryMockGuard() noexcept
    {
        SkeletonBindingFactory::InjectMockBinding(&factory_mock_);
    }
    ~SkeletonBindingFactoryMockGuard() noexcept
    {
        SkeletonBindingFactory::InjectMockBinding(nullptr);
    }

    ::testing::StrictMock<SkeletonBindingFactoryMock> factory_mock_;
};

template <typename SampleType>
class SkeletonEventBindingFactoryMockGuard
{
  public:
    SkeletonEventBindingFactoryMockGuard() noexcept
    {
        SkeletonEventBindingFactory<SampleType>::InjectMockBinding(&factory_mock_);
    }
    ~SkeletonEventBindingFactoryMockGuard() noexcept
    {
        SkeletonEventBindingFactory<SampleType>::InjectMockBinding(nullptr);
    }

    ::testing::StrictMock<SkeletonEventBindingFactoryMock<SampleType>> factory_mock_;
};

template <typename SampleType>
class SkeletonFieldBindingFactoryMockGuard
{
  public:
    SkeletonFieldBindingFactoryMockGuard() noexcept
    {
        SkeletonFieldBindingFactory<SampleType>::InjectMockBinding(&factory_mock_);
    }
    ~SkeletonFieldBindingFactoryMockGuard() noexcept
    {
        SkeletonFieldBindingFactory<SampleType>::InjectMockBinding(nullptr);
    }

    ::testing::StrictMock<SkeletonFieldBindingFactoryMock<SampleType>> factory_mock_;
};

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_TEST_BINDING_FACTORY_RESOURCES_H
