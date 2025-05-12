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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_FLAG_FILE_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_FLAG_FILE_H

#include "score/mw/com/impl/enriched_instance_identifier.h"

#include "score/filesystem/filesystem.h"
#include "score/result/result.h"

#include <chrono>
#include <string_view>

namespace score::mw::com::impl::lola
{

auto GetQualityTypeString(QualityType quality_type) noexcept -> std::string_view;

/// \brief Gets the search path (without creating it in the filesystem) from an InstanceIdentifier which will either be
/// the path to the service ID directory or the instance ID directory depending on whether the InstanceIdentifier
/// contains an Instance ID.
///
/// The service discovery path is: `<sd>/mw_com_lola/<service_id>/<instance_id>.
auto GetSearchPathForIdentifier(const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept
    -> filesystem::Path;

class FlagFile
{
  public:
    using Disambiguator = std::chrono::steady_clock::time_point::rep;

    ~FlagFile();

    /// \brief Creates a flag file for the provided InstanceIdentifier which is read/writable by the creating user and
    /// only readable by everyone else.
    ///
    /// Will also create the service / instance ID directories in which the flag file is located if they don't already
    /// exist.
    static auto Make(EnrichedInstanceIdentifier enriched_instance_identifier,
                     Disambiguator offer_disambiguator,
                     filesystem::Filesystem filesystem) noexcept -> score::Result<FlagFile>;

    FlagFile(const FlagFile&) = delete;
    FlagFile(FlagFile&&) noexcept;
    FlagFile& operator=(const FlagFile&) = delete;
    FlagFile& operator=(FlagFile&&) = delete;

    /// \brief Checks if a flag file exists for an Instanceidentifier in the instance directory.
    ///
    /// The service discovery path is: `<sd>/mw_com_lola/<service_id>/<instance_id>. Since flag files are always created
    /// in the instance directory, this function will always return false if the InstanceIdentifier does not contain an
    /// Instance ID.
    static auto Exists(const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept -> bool;

    /// \brief Creates each directory in the search path (found using GetSearchPathForIdentifier()) for an
    /// InstanceIdentifier in the filesystem.
    static auto CreateSearchPath(
        const EnrichedInstanceIdentifier& enriched_instance_identifier,
        const filesystem::Filesystem& filesystem = filesystem::FilesystemFactory{}.CreateInstance()) noexcept
        -> score::Result<filesystem::Path>;

  private:
    EnrichedInstanceIdentifier enriched_instance_identifier_;
    Disambiguator offer_disambiguator_;
    bool is_offered_;

    filesystem::Filesystem filesystem_;

    FlagFile(EnrichedInstanceIdentifier enriched_instance_identifier,
             const Disambiguator offer_disambiguator,
             filesystem::Filesystem filesystem);
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_FLAG_FILE_H
