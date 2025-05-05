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
#include "score/mw/com/impl/bindings/lola/service_discovery/flag_file.h"

#include "score/os/unistd.h"

#include <score/utility.hpp>

#include <algorithm>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

namespace score::mw::com::impl::lola
{
namespace
{
// Suppress "AUTOSAR C++14 A16-0-1" rule findings. This rule stated: "The pre-processor shall only be used for
// unconditional and conditional file inclusion and include guards, and using the following directives: (1) #ifndef,
// #ifdef, (3) #if, (4) #if defined, (5) #elif, (6) #else, (7) #define, (8) #endif, (9) #include.".
// Checks whether we run over QNX or linux to choose the appropriate path.
// coverity[autosar_cpp14_a16_0_1_violation]
#ifdef __QNXNTO__
// Suppress "AUTOSAR C++14 A3-3-2" rule finding. This rule states: "Static and thread-local objects shall be
// constant-initialized.".
// score::filesystem::Path is not literal class as it has std::vector data member.
// coverity[autosar_cpp14_a3_3_2_violation]
const filesystem::Path TMP_PATH{"/tmp_discovery/mw_com_lola/service_discovery"};
// coverity[autosar_cpp14_a16_0_1_violation]
#else
// coverity[autosar_cpp14_a3_3_2_violation]
const filesystem::Path TMP_PATH{"/tmp/mw_com_lola/service_discovery"};
// coverity[autosar_cpp14_a16_0_1_violation]
#endif

constexpr os::Stat::Mode ALL_PERMISSIONS{os::Stat::Mode::kReadWriteExecUser | os::Stat::Mode::kReadWriteExecGroup |
                                         os::Stat::Mode::kReadWriteExecOthers};

auto GetFlagFilePath(const EnrichedInstanceIdentifier& enriched_instance_identifier,
                     const FlagFile::Disambiguator disambiguator,
                     const os::Unistd& unistd = os::internal::UnistdImpl{}) noexcept -> filesystem::Path
{
    const auto pid = unistd.getpid();

    const auto disambiguator_string = std::to_string(disambiguator);
    const auto quality_string = GetQualityTypeString(enriched_instance_identifier.GetQualityType());
    const auto file_name = std::to_string(pid) + "_" + std::string{quality_string} + "_" + disambiguator_string;

    return GetSearchPathForIdentifier(enriched_instance_identifier) / file_name;
}

auto GetMatchingFlagFilePaths(const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept
    -> std::vector<filesystem::Path>
{
    const auto search_path = GetSearchPathForIdentifier(enriched_instance_identifier);
    filesystem::DirectoryIterator directory_iterator{search_path};

    const auto quality_type = std::string{GetQualityTypeString(enriched_instance_identifier.GetQualityType())};
    std::vector<filesystem::Path> flag_file_paths{};
    score::cpp::ignore =
        std::for_each(begin(directory_iterator),
                      end(directory_iterator),
                      // Suppress "AUTOSAR C++14 A0-1-3" rule finding. This rule states: "Every function defined in an
                      // anonymous namespace, or static function with internal linkage, or private member function shall
                      // be used.". Rationale: False-positive, this lambda function is used by std::for_each.
                      // coverity[autosar_cpp14_a0_1_3_violation : FALSE]
                      [&flag_file_paths, quality_type](const filesystem::DirectoryEntry& entry) noexcept {
                          const auto file_status = entry.Status();
                          const auto substring_iterator = entry.GetPath().Native().find(quality_type);
                          const bool is_regular_file =
                              (file_status.has_value() && (file_status->Type() == filesystem::FileType::kRegular));
                          const bool path_contains_quality_type = substring_iterator != std::string::npos;
                          if (is_regular_file && path_contains_quality_type)
                          {
                              score::cpp::ignore = flag_file_paths.emplace_back(entry.GetPath());
                          }
                      });

    return flag_file_paths;
}

auto RemoveMatchingFlagFiles(const EnrichedInstanceIdentifier& enriched_instance_identifier,
                             const FlagFile::Disambiguator offer_disambiguator,
                             filesystem::Filesystem& filesystem) noexcept -> score::ResultBlank
{
    const auto matching_file_paths = GetMatchingFlagFilePaths(enriched_instance_identifier);

    if (!matching_file_paths.empty())
    {
        mw::log::LogInfo("lola") << "Found conflicting flag files during creation of flag file:"
                                 << GetFlagFilePath(enriched_instance_identifier, offer_disambiguator);
    }

    ResultBlank result{};
    for (const auto& matching_file_path : matching_file_paths)
    {
        const auto remove_result = filesystem.standard->Remove(matching_file_path);
        if (!(remove_result.has_value()))
        {
            mw::log::LogError("lola") << "Outside tampering! Failed to clear flag file" << matching_file_path.Native()
                                      << ":" << remove_result.error();
            result = MakeUnexpected(ComErrc::kBindingFailure, "Could not clear directory for flag file");
        }
    }

    return result;
}

}  // namespace

auto GetQualityTypeString(QualityType quality_type) noexcept -> std::string_view
{
    const char* result{nullptr};

    switch (quality_type)
    {
        case QualityType::kASIL_B:
        {
            result = "asil-b";
            break;
        }
        case QualityType::kASIL_QM:
        {
            result = "asil-qm";
            break;
        }
        case QualityType::kInvalid:
        default:
        {
            result = "invalid";
            break;
        }
    }
    return result;
}

auto GetSearchPathForIdentifier(const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept
    -> filesystem::Path
{
    const auto service_id =
        enriched_instance_identifier.GetBindingSpecificServiceId<LolaServiceTypeDeployment>().value();
    auto search_path = TMP_PATH / std::to_string(static_cast<std::uint32_t>(service_id));

    const auto lola_instance_id_result =
        enriched_instance_identifier.GetBindingSpecificInstanceId<LolaServiceInstanceId>();
    if (lola_instance_id_result.has_value())
    {
        search_path /= std::to_string(static_cast<std::uint32_t>(*lola_instance_id_result));
    }

    return search_path;
}

FlagFile::~FlagFile()
{
    if (is_offered_)
    {
        const auto flag_file_path = GetFlagFilePath(enriched_instance_identifier_, offer_disambiguator_);

        const auto flag_file_result = filesystem_.standard->Remove(flag_file_path);
        if (!(flag_file_result.has_value()))
        {
            mw::log::LogFatal("lola") << "Outside tampering! Bailing! Failed to remove flag file"
                                      << flag_file_path.Native() << ":" << flag_file_result.error();
            std::terminate();
        }
    }
}

auto FlagFile::Make(EnrichedInstanceIdentifier enriched_instance_identifier,
                    Disambiguator offer_disambiguator,
                    filesystem::Filesystem filesystem) noexcept -> score::Result<FlagFile>
{
    const auto clearing_result = RemoveMatchingFlagFiles(enriched_instance_identifier, offer_disambiguator, filesystem);
    if (!clearing_result.has_value())
    {
        return MakeUnexpected(ComErrc::kBindingFailure, "Could not clear directory for flag file");
    }

    const auto flag_file_path = GetFlagFilePath(enriched_instance_identifier, offer_disambiguator);

    const auto path_result = CreateSearchPath(enriched_instance_identifier, filesystem);
    if (!path_result.has_value())
    {
        mw::log::LogError("lola") << "Failed to create path to flag file" << flag_file_path.ParentPath().Native() << ":"
                                  << path_result.error();
        return MakeUnexpected(ComErrc::kBindingFailure, "Could not create directories for flag file");
    }

    const auto flag_file_result = filesystem.streams->Open(flag_file_path, std::ios_base::out);
    if (!(flag_file_result.has_value()))
    {
        mw::log::LogError("lola") << "Failed to create flag file" << flag_file_path.Native() << ":"
                                  << flag_file_result.error();
        return score::MakeUnexpected(ComErrc::kBindingFailure, "Could not create flag file");
    }

    constexpr auto permissions = filesystem::Perms::kWriteUser | filesystem::Perms::kReadUser |
                                 filesystem::Perms::kReadGroup | filesystem::Perms::kReadOthers;
    const auto permission_result =
        filesystem.standard->Permissions(flag_file_path, permissions, filesystem::PermOptions::kReplace);
    if (!permission_result.has_value())
    {
        mw::log::LogError("lola") << "Failed to set permissions on flag file" << flag_file_path.Native() << ":"
                                  << permission_result.error();
        return score::MakeUnexpected(ComErrc::kBindingFailure, "Could not set permissions on flag file");
    }
    mw::log::LogDebug("lola") << "created flag file for service:" << flag_file_path.Native();
    return FlagFile{std::move(enriched_instance_identifier), offer_disambiguator, std::move(filesystem)};
}

FlagFile::FlagFile(FlagFile&& other) noexcept
    : enriched_instance_identifier_{std::move(other.enriched_instance_identifier_)},
      offer_disambiguator_{other.offer_disambiguator_},
      is_offered_{other.is_offered_},
      filesystem_{std::move(other.filesystem_)}
{
    other.is_offered_ = false;
}

auto FlagFile::Exists(const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept -> bool
{
    return !GetMatchingFlagFilePaths(enriched_instance_identifier).empty();
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::terminate() is implicitly called from 'status.value()' in case the status
// doesn't have value but as we check before with 'has_value()' so no way for throwing std::bad_optional_access which
// leds to std::terminate().
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto FlagFile::CreateSearchPath(const EnrichedInstanceIdentifier& enriched_instance_identifier,
                                const filesystem::Filesystem& filesystem) noexcept -> Result<filesystem::Path>
{
    auto path = GetSearchPathForIdentifier(enriched_instance_identifier);

    score::ResultBlank result{};
    constexpr auto retry_count = 3;
    constexpr std::chrono::milliseconds backoff_time{10};
    for (auto i = 0; i < retry_count; ++i)
    {
        result = filesystem.utils->CreateDirectories(path, ALL_PERMISSIONS);
        if (result.has_value())
        {
            mw::log::LogInfo("lola") << "Successfully created offer path" << path;
            break;
        }
        const Result<filesystem::FileStatus> status = filesystem.standard->Status(path);
        if (status.has_value())
        {
            if ((status.value().Type() == filesystem::FileType::kDirectory) &&
                (status.value().Permissions() == ALL_PERMISSIONS))
            {
                result = ResultBlank{};
                break;
            }
        }

        mw::log::LogInfo("lola") << "Failed to create offer path" << path << " - Path maybe in concurrent creation (Try"
                                 << i << "of" << retry_count << ")";
        std::this_thread::sleep_for(backoff_time);
    }

    if (!result.has_value())
    {
        mw::log::LogError("lola") << "Failed to create offer path" << path;
        return MakeUnexpected(ComErrc::kBindingFailure, "Could not create search path");
    }

    return path;
}

FlagFile::FlagFile(EnrichedInstanceIdentifier enriched_instance_identifier,
                   const Disambiguator offer_disambiguator,
                   filesystem::Filesystem filesystem)
    : enriched_instance_identifier_{std::move(enriched_instance_identifier)},
      offer_disambiguator_{offer_disambiguator},
      is_offered_{true},
      filesystem_{std::move(filesystem)}
{
}

}  // namespace score::mw::com::impl::lola
