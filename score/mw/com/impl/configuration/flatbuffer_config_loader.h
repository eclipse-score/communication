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

#ifndef SCORE_MW_COM_IMPL_CONFIGURATION_FLATBUFFER_CONFIG_LOADER_H
#define SCORE_MW_COM_IMPL_CONFIGURATION_FLATBUFFER_CONFIG_LOADER_H

#include "score/mw/com/impl/configuration/ara_com_config_generated.h"
#include "score/mw/com/impl/configuration/configuration.h"
#include "score/mw/com/impl/configuration/global_configuration.h"
#include "score/mw/com/impl/configuration/tracing_configuration.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include <cstddef>
namespace score::mw::com::impl::configuration
{

/// FlatBuffer configuration loader that reads and converts FlatBuffer binary files
/// to Configuration objects. Manages the lifetime of the FlatBuffer data.
class FlatBufferConfigLoader
{
  public:
    /// Load and convert a FlatBuffer binary configuration file to a Configuration object
    /// @param path Path to the FlatBuffer binary file (.bin)
    /// @return Configuration object constructed from the FlatBuffer data
    /// @note Terminates process on file read errors or invalid FlatBuffer data
    static Configuration CreateConfiguration(std::string_view path) noexcept;

  private:
    /// Private constructor - use CreateConfiguration() factory method
    explicit FlatBufferConfigLoader(std::string_view path);

    /// Destructor - closes file descriptor and unmaps memory
    ~FlatBufferConfigLoader();

    // Non-copyable
    FlatBufferConfigLoader(const FlatBufferConfigLoader&) = delete;
    FlatBufferConfigLoader& operator=(const FlatBufferConfigLoader&) = delete;

    /// Load and verify the FlatBuffer binary file
    void LoadBuffer(std::string_view path);

    /// Create service type deployments from FlatBuffer data
    Configuration::ServiceTypeDeployments CreateServiceTypes() const noexcept;

    /// Create service instance deployments from FlatBuffer data
    Configuration::ServiceInstanceDeployments CreateServiceInstances() const noexcept;

    /// Create global configuration from FlatBuffer data
    GlobalConfiguration CreateGlobalConfiguration() const noexcept;

    /// Create tracing configuration from FlatBuffer data
    TracingConfiguration CreateTracingConfiguration() const noexcept;

    /// Deleter for mmap'd memory
    struct MmapDeleter
    {
        std::size_t size;
        void operator()(void* ptr) const noexcept;
    };

    // File descriptor - closed in destructor
    int fd_ = -1;

    // RAII mmap'd memory - automatically unmapped
    std::unique_ptr<void, MmapDeleter> mapped_ptr_;

    // Non-owning view into mmap'd region
    const ComConfiguration* com_config_ = nullptr;
};

}  // namespace score::mw::com::impl::configuration

#endif  // SCORE_MW_COM_IMPL_CONFIGURATION_FLATBUFFER_CONFIG_LOADER_H
