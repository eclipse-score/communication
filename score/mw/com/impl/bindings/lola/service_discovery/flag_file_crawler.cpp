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
#include "score/mw/com/impl/bindings/lola/service_discovery/flag_file_crawler.h"

#include "score/mw/com/impl/bindings/lola/service_discovery/flag_file.h"
#include "score/mw/com/impl/com_error.h"

#include "score/filesystem/filesystem.h"
#include "score/mw/log/logging.h"
#include "score/os/errno.h"
#include "score/result/error.h"
#include "score/result/result.h"

#include <score/assert.hpp>
#include <score/utility.hpp>

#include <charconv>
#include <chrono>
#include <cstdint>
#include <exception>
#include <optional>
#include <sstream>
#include <thread>
#include <utility>

namespace score::mw::com::impl::lola
{

namespace
{

QualityAwareContainer<KnownInstancesContainer> GetAlreadyExistingInstances(
    std::vector<EnrichedInstanceIdentifier>& quality_unaware_identifiers_to_check) noexcept
{
    QualityAwareContainer<KnownInstancesContainer> known_instances{};
    for (const auto& quality_unaware_identifier_to_check : quality_unaware_identifiers_to_check)
    {
        // Suppress "AUTOSAR C++14 M8-5-2" rule finding. This rule declares: "Braces shall be used to indicate and
        // match the structure in the non-zero initialization of arrays and structures.". False positive as we here
        // use initialization list.
        // coverity[autosar_cpp14_m8_5_2_violation]
        constexpr std::array<QualityType, 2> supported_quality_types{QualityType::kASIL_B, QualityType::kASIL_QM};
        for (const auto quality_type : supported_quality_types)
        {
            EnrichedInstanceIdentifier quality_aware_identifier_to_check{quality_unaware_identifier_to_check,
                                                                         quality_type};
            if (FlagFile::Exists(quality_aware_identifier_to_check))
            {
                // Suppress "AUTOSAR C++14 M6-4-3" rule finding. This rule declares: "A switch statement shall be
                // a well-formed switch statement".
                // We don't need a break statement at the end of default case as we use return.
                // coverity[autosar_cpp14_m6_4_3_violation]
                switch (quality_type)
                {
                    case QualityType::kASIL_B:
                    {
                        mw::log::LogDebug("lola")
                            << "LoLa SD: Added" << GetSearchPathForIdentifier(quality_aware_identifier_to_check)
                            << "(ASIL-B)";
                        score::cpp::ignore = known_instances.asil_b.Insert(quality_aware_identifier_to_check);
                        break;
                    }
                    case QualityType::kASIL_QM:
                    {
                        mw::log::LogDebug("lola")
                            << "LoLa SD: Added" << GetSearchPathForIdentifier(quality_aware_identifier_to_check)
                            << "(ASIL-QM)";
                        score::cpp::ignore = known_instances.asil_qm.Insert(quality_aware_identifier_to_check);
                        break;
                    }
                    // LCOV_EXCL_START (Defensive programming: We only enter this code branch if a flag file
                    // corresponding to the instance identifier could be found. The flag file path contains the quality
                    // type and therefore will never be created if the quality type is invalid or unknown.)
                    // Suppress "AUTOSAR C++14 M6-4-5" rule finding. This rule declares: "An unconditional throw or
                    // break statement shall terminate every non-empty switch-clause". We don't need a break statement
                    // at the end of default case as we use return. An error return is needed here to keep it robust for
                    // the future.
                    // coverity[autosar_cpp14_m6_4_5_violation]
                    case QualityType::kInvalid:
                    default:
                    {
                        mw::log::LogFatal("lola")
                            << "LoLa SD: Could not map instance to quality type - Invalid QualityType: "
                            << static_cast<int>(quality_type) << ". Terminating.";
                        std::terminate();
                        // LCOV_EXCL_STOP
                    }
                }
            }
        }
    }
    return known_instances;
}

}  // namespace

FlagFileCrawler::FlagFileCrawler(os::InotifyInstance& inotify_instance, filesystem::Filesystem filesystem)
    : inotify_instance_{inotify_instance}, filesystem_{std::move(filesystem)}
{
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::terminate() is implicitly called from 'result.value()' in case the
// 'result' doesn't have value but as we check before with 'has_value()' so no way for throwing std::bad_optional_access
// which leds to std::terminate().
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto FlagFileCrawler::Crawl(const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept
    -> score::Result<QualityAwareContainer<KnownInstancesContainer>>
{
    constexpr auto kAddWatch = false;
    const auto result = CrawlAndWatchImpl(enriched_instance_identifier, kAddWatch);
    if (!(result.has_value()))
    {
        return Unexpected{result.error()};
    }
    return std::get<1>(result.value());
}

auto FlagFileCrawler::CrawlAndWatch(const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept
    -> score::Result<std::tuple<std::unordered_map<os::InotifyWatchDescriptor, EnrichedInstanceIdentifier>,
                                QualityAwareContainer<KnownInstancesContainer>>>
{
    constexpr auto kAddWatch = true;
    return CrawlAndWatchImpl(enriched_instance_identifier, kAddWatch);
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::terminate() is implicitly called from 'instance_watch_descriptor.value()'
// in case the 'instance_watch_descriptor' doesn't have value but as we check before with 'has_value()' so no way for
// throwing std::bad_optional_access which leds to std::terminate().
// This suppression should be removed after fixing[Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto FlagFileCrawler::CrawlAndWatchImpl(const EnrichedInstanceIdentifier& enriched_instance_identifier,
                                        const bool add_watch) noexcept
    -> score::Result<std::tuple<std::unordered_map<os::InotifyWatchDescriptor, EnrichedInstanceIdentifier>,
                                QualityAwareContainer<KnownInstancesContainer>>>
{
    EnrichedInstanceIdentifier quality_unaware_enriched_instance_identifier{enriched_instance_identifier,
                                                                            QualityType::kInvalid};

    std::unordered_map<os::InotifyWatchDescriptor, EnrichedInstanceIdentifier> watch_descriptors{};

    if (add_watch)
    {
        // If we are in a find-any search, then this will add watch only to the service directory. Otherwise, it will
        // add a watch to the specific instance directory.
        const auto watch_descriptor = AddWatchToInotifyInstance(quality_unaware_enriched_instance_identifier);
        if (!(watch_descriptor.has_value()))
        {
            score::mw::log::LogError("lola") << "Could not add watch to instance identifier";
            return MakeUnexpected(ComErrc::kBindingFailure, "Could not add watch to main search directory");
        }
        score::cpp::ignore =
            watch_descriptors.emplace(watch_descriptor.value(), quality_unaware_enriched_instance_identifier);
    }

    std::vector<EnrichedInstanceIdentifier> quality_unaware_identifiers_to_check{};
    if (enriched_instance_identifier.GetBindingSpecificInstanceId<LolaServiceInstanceId>().has_value())
    {
        // We are not in a find-any search, a watch was already added to the specific instance directory above.
        quality_unaware_identifiers_to_check.push_back(quality_unaware_enriched_instance_identifier);
    }
    else
    {
        // We are in a find-any search, so a watch must be added for all instance directories relating to the service.
        const auto found_instance_directories =
            GatherExistingInstanceDirectories(quality_unaware_enriched_instance_identifier);

        if (!found_instance_directories.has_value())
        {
            return MakeUnexpected(ComErrc::kBindingFailure, "Could not crawl filesystem");
        }

        for (const auto& found_quality_unaware_identifier : *found_instance_directories)
        {
            quality_unaware_identifiers_to_check.push_back(found_quality_unaware_identifier);
            if (add_watch)
            {
                const auto instance_watch_descriptor = AddWatchToInotifyInstance(found_quality_unaware_identifier);
                if (!(instance_watch_descriptor.has_value()))
                {
                    mw::log::LogError("lola") << "Could not add watch for instance"
                                              << GetSearchPathForIdentifier(found_quality_unaware_identifier);
                    return MakeUnexpected(ComErrc::kBindingFailure, "Could not add watch to search subdirectory");
                }
                score::cpp::ignore =
                    watch_descriptors.emplace(instance_watch_descriptor.value(), found_quality_unaware_identifier);
            }
        }
    }

    // Get a container of all the existing instances (for which there is already a flag file in the instance directory)
    const auto known_instances = GetAlreadyExistingInstances(quality_unaware_identifiers_to_check);
    return {{watch_descriptors, known_instances}};
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, all results which are accessed with '.value()' that could implicitly call
// 'std::terminate()' (in case it doesn't have value) has a check in advance using '.has_value()', so no way for
// throwing std::bad_optional_access which leds to std::terminate(). This suppression should be removed after fixing
// [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto FlagFileCrawler::CrawlAndWatchWithRetry(const EnrichedInstanceIdentifier& enriched_instance_identifier,
                                             const std::uint8_t max_number_of_retries) noexcept
    -> score::Result<std::tuple<std::unordered_map<os::InotifyWatchDescriptor, EnrichedInstanceIdentifier>,
                                QualityAwareContainer<KnownInstancesContainer>>>
{
    constexpr std::chrono::milliseconds wait_between_retries{50U};

    std::uint8_t current_retry_count{0U};
    std::optional<score::result::Error> crawl_and_watch_error{};
    while (current_retry_count < max_number_of_retries)
    {
        const auto crawl_and_watch_result = CrawlAndWatch(enriched_instance_identifier);
        if (crawl_and_watch_result.has_value())
        {
            return crawl_and_watch_result.value();
        }
        crawl_and_watch_error = crawl_and_watch_result.error();

        current_retry_count++;
        score::mw::log::LogWarn("lola") << "CrawlAndWatch failed with error" << crawl_and_watch_result.error()
                                        << ". Retry attempt (" << current_retry_count << "/" << max_number_of_retries
                                        << ").";
        std::this_thread::sleep_for(wait_between_retries);
    }

    // Defensive programming: This branch can only be reached if CrawlAndWatch returned an error after all retries
    // and the retry loop will always fill crawl_and_watch_error on CrawlAndWatch failure.
    SCORE_LANGUAGE_FUTURECPP_ASSERT_PRD(crawl_and_watch_error.has_value());
    score::mw::log::LogError("lola") << "CrawlAndWatch failed with error" << crawl_and_watch_error.value()
                                     << "after all retries.";
    return MakeUnexpected<std::tuple<std::unordered_map<os::InotifyWatchDescriptor, EnrichedInstanceIdentifier>,
                                     QualityAwareContainer<KnownInstancesContainer>>>(
        std::move(crawl_and_watch_error).value());
}

auto FlagFileCrawler::ConvertFromStringToInstanceId(std::string_view view) noexcept -> Result<LolaServiceInstanceId>
{
    LolaServiceInstanceId::InstanceId actual_instance_id{};
    const auto conversion_result = std::from_chars(view.cbegin(), view.cend(), actual_instance_id);
    if (conversion_result.ec != std::errc{})
    {
        return MakeUnexpected(ComErrc::kBindingFailure, "Could not parse instance id from string");
    }
    return LolaServiceInstanceId{actual_instance_id};
}

auto FlagFileCrawler::ParseQualityTypeFromString(const std::string_view filename) noexcept -> QualityType
{
    if (filename.find(GetQualityTypeString(QualityType::kASIL_B)) != std::string::npos)
    {
        return QualityType::kASIL_B;
    }
    if (filename.find(GetQualityTypeString(QualityType::kASIL_QM)) != std::string::npos)
    {
        return QualityType::kASIL_QM;
    }
    return QualityType::kInvalid;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::terminate() is implicitly called from 'instance_id.value()' in case the
// 'instance_id' doesn't have value but as we check before with 'has_value()' so no way for throwing
// std::bad_optional_access which leds to std::terminate().
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto FlagFileCrawler::GatherExistingInstanceDirectories(
    const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept
    -> score::Result<std::vector<EnrichedInstanceIdentifier>>
{
    SCORE_LANGUAGE_FUTURECPP_PRECONDITION_PRD_MESSAGE(
        !enriched_instance_identifier.GetBindingSpecificInstanceId<LolaServiceInstanceId>().has_value(),
        "Handle must not have instance id");
    filesystem::DirectoryIterator directory_iterator{GetSearchPathForIdentifier(enriched_instance_identifier)};

    std::vector<EnrichedInstanceIdentifier> enriched_instance_identifiers{};
    for (const auto& entry : directory_iterator)
    {
        const auto status = entry.Status();
        if (!(status.has_value()))
        {
            mw::log::LogError("lola") << "Could not get directory status for" << entry.GetPath() << "."
                                      << status.error();
            return MakeUnexpected(ComErrc::kBindingFailure, "Could not add watch to search subdirectory");
        }
        if (status->Type() != filesystem::FileType::kDirectory)
        {
            mw::log::LogError("lola") << "Found file" << entry.GetPath() << "- should be directory";
            continue;
        }

        const auto filename = entry.GetPath().Filename().Native();
        const auto instance_id_result = ConvertFromStringToInstanceId(entry.GetPath().Filename().Native());
        if (!(instance_id_result.has_value()))
        {
            mw::log::LogError("lola") << "Could not parse" << entry.GetPath() << "to instance id";
            continue;
        }
        EnrichedInstanceIdentifier found_enriched_instance_identifier{
            enriched_instance_identifier.GetInstanceIdentifier(),
            ServiceInstanceId{LolaServiceInstanceId{instance_id_result.value().GetId()}}};
        score::cpp::ignore = enriched_instance_identifiers.emplace_back(std::move(found_enriched_instance_identifier),
                                                                        ParseQualityTypeFromString(filename));
    }
    return enriched_instance_identifiers;
}

// Suppress "AUTOSAR C++14 A15-5-3" rule findings. This rule states: "The std::terminate() function shall not be called
// implicitly". This is a false positive, std::terminate() is implicitly called from 'directory_creation_result.value()'
// in case the 'directory_creation_result' doesn't have value but as we check before with 'has_value()' so no way for
// throwing std::bad_optional_access which leds to std::terminate().
// This suppression should be removed after fixing [Ticket-173043](broken_link_j/Ticket-173043)
// coverity[autosar_cpp14_a15_5_3_violation : FALSE]
auto FlagFileCrawler::AddWatchToInotifyInstance(const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept
    -> Result<os::InotifyWatchDescriptor>
{

    auto directory_creation_result = FlagFile::CreateSearchPath(enriched_instance_identifier, filesystem_);
    if (!(directory_creation_result.has_value()))
    {
        mw::log::LogError("lola") << "Could not create search path with" << directory_creation_result.error();
        return MakeUnexpected(ComErrc::kBindingFailure, "Could not create search path");
    }

    const auto search_path = std::move(directory_creation_result).value();
    const auto watch_descriptor = inotify_instance_.AddWatch(
        search_path.Native(), os::Inotify::EventMask::kInCreate | os::Inotify::EventMask::kInDelete);
    if (!(watch_descriptor.has_value()))
    {
        mw::log::LogError("lola") << "Could not add watch for" << search_path << ":"
                                  << watch_descriptor.error().ToString();
        if (watch_descriptor.error() == os::Error::Code::kOperationNotPermitted)
        {
            const Result<filesystem::FileStatus> status_result = filesystem_.standard->Status(search_path.Native());
            if (status_result.has_value())
            {
                const auto permissions_integer = ModeToInteger(status_result.value().Permissions());

                // The resulting integer representing permissions is a decimal number. The "regular" format e.g. 666 for
                // read/write for all is in octal format. So we convert to octal.

                // Suppress "AUTOSAR C++14 M8-4-4" rule finding. This rule states: "A function identifier shall either
                // be used to call the function or it shall be preceded by &.". This is a false-positive, the
                // suppression has to be removed after fixing broken_link_j/Ticket-186944
                // coverity[autosar_cpp14_m8_4_4_violation: FALSE]
                std::stringstream permissions_integer_as_octal_string{};
                // coverity[autosar_cpp14_m8_4_4_violation: FALSE]
                permissions_integer_as_octal_string << std::oct << permissions_integer;
                mw::log::LogError("lola")
                    << "Current file permissions are:" << permissions_integer_as_octal_string.str();
            }
        }
        return score::MakeUnexpected(ComErrc::kBindingFailure, "Could not add watch for service id");
    }

    return watch_descriptor.value();
}

}  // namespace score::mw::com::impl::lola
