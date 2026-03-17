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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_FLAG_FILE_CRAWLER_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_FLAG_FILE_CRAWLER_H

#include "score/mw/com/impl/bindings/lola/service_discovery/known_instances_container.h"
#include "score/mw/com/impl/bindings/lola/service_discovery/quality_aware_container.h"

#include "score/filesystem/filesystem.h"
#include "score/mw/com/impl/enriched_instance_identifier.h"
#include "score/os/utils/inotify/inotify_instance.h"
#include "score/os/utils/inotify/inotify_watch_descriptor.h"
#include "score/result/result.h"

#include <cstdint>
#include <string_view>
#include <tuple>
#include <unordered_map>

namespace score::mw::com::impl::lola
{

class FlagFileCrawler
{
  public:
    explicit FlagFileCrawler(os::InotifyInstance& inotify_instance,
                             filesystem::Filesystem filesystem = filesystem::FilesystemFactory{}.CreateInstance());

    auto Crawl(const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept
        -> score::Result<QualityAwareContainer<KnownInstancesContainer>>;

    auto CrawlAndWatch(const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept
        -> score::Result<std::tuple<std::unordered_map<os::InotifyWatchDescriptor, EnrichedInstanceIdentifier>,
                                    QualityAwareContainer<KnownInstancesContainer>>>;

    auto CrawlAndWatchWithRetry(const EnrichedInstanceIdentifier& enriched_instance_identifier,
                                const std::uint8_t max_number_of_retries) noexcept
        -> score::Result<std::tuple<std::unordered_map<os::InotifyWatchDescriptor, EnrichedInstanceIdentifier>,
                                    QualityAwareContainer<KnownInstancesContainer>>>;

    static auto ConvertFromStringToInstanceId(std::string_view view) noexcept -> Result<LolaServiceInstanceId>;
    static auto ParseQualityTypeFromString(const std::string_view filename) noexcept -> QualityType;

  private:
    auto CrawlAndWatchImpl(const EnrichedInstanceIdentifier& enriched_instance_identifier,
                           const bool add_watch) noexcept
        -> score::Result<std::tuple<std::unordered_map<os::InotifyWatchDescriptor, EnrichedInstanceIdentifier>,
                                    QualityAwareContainer<KnownInstancesContainer>>>;

    static auto GatherExistingInstanceDirectories(
        const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept
        -> score::Result<std::vector<EnrichedInstanceIdentifier>>;

    auto AddWatchToInotifyInstance(const EnrichedInstanceIdentifier& enriched_instance_identifier) noexcept
        -> Result<os::InotifyWatchDescriptor>;

    os::InotifyInstance& inotify_instance_;
    filesystem::Filesystem filesystem_;
};

}  // namespace score::mw::com::impl::lola

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_FLAG_FILE_CRAWLER_H
