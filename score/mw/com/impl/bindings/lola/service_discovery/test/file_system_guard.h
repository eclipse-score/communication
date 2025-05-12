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
#ifndef SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_FILE_SYSTEM_GUARD_H
#define SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_FILE_SYSTEM_GUARD_H

#include "score/filesystem/filesystem_struct.h"
#include "score/filesystem/path.h"

namespace score::mw::com::impl::lola::test
{

/// \brief RAII wrapper around a Filesytem which removes the provided path on destruction
class FileSystemGuard
{
  public:
    explicit FileSystemGuard(filesystem::Filesystem& filesystem, const filesystem::Path& path_to_remove) noexcept
        : filesystem_{filesystem}, path_to_remove_{path_to_remove}
    {
    }
    ~FileSystemGuard() noexcept
    {
        filesystem_.standard->RemoveAll(path_to_remove_);
    }

    FileSystemGuard(const FileSystemGuard&) = delete;
    FileSystemGuard& operator=(const FileSystemGuard&) = delete;
    FileSystemGuard(FileSystemGuard&&) = delete;
    FileSystemGuard& operator=(FileSystemGuard&&) = delete;

  private:
    filesystem::Filesystem& filesystem_;
    filesystem::Path path_to_remove_;
};

}  // namespace score::mw::com::impl::lola::test

#endif  // SCORE_MW_COM_IMPL_BINDINGS_LOLA_SERVICE_DISCOVERY_FILE_SYSTEM_GUARD_H
