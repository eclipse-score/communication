/********************************************************************************
 * Copyright (C) 2017-2026 LG Electronics Inc.
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 ********************************************************************************/

#include "LocalAddress.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <cstring>
#include <random>
#include <cstdlib>
#include <sys/file.h>
#include <sys/stat.h>
#include <ctime>
#include <stdexcept>

#include <utils/log/logger.h>

namespace lgsomeip {
namespace osabstraction {

LocalAddress::LocalAddress() {
    socket_address_.sun_family = AF_UNIX;
}

LocalAddress::~LocalAddress() {
    if (lock_fd_ >= 0) {
        ::flock(lock_fd_, LOCK_UN);
        ::close(lock_fd_);
        lock_fd_ = -1;
    }
}

struct sockaddr* LocalAddress::get_address() {
    return reinterpret_cast<struct sockaddr*>(&socket_address_);
}

int LocalAddress::get_type() const {
    return AF_UNIX;
}

int LocalAddress::get_address_size() const {
    return sizeof(socket_address_);
}

void LocalAddress::set_ip_address(std::string address) {
    // TODO(lg-someip): Report unsupported IP-address operations as exceptions.
    LGSOMEIP_LOG_ERROR << "LocalAddress::set_ip_address / Failed!";
}

std::string LocalAddress::get_ip_address() const {
    // TODO(lg-someip): Report unsupported IP-address operations as exceptions.
    LGSOMEIP_LOG_ERROR << "LocalAddress::get_ip_address / Failed!";
    return std::string();
}

std::string LocalAddress::to_string() {
    return address_;
}

void LocalAddress::create_path(std::string path) {
    std::string current_path = path;
    std::string temporary_directory;
    std::size_t separator_position = 0;

    while (true) {
        separator_position = current_path.find("/");
        if (separator_position == std::string::npos) {
            temporary_directory += current_path + "/";
        } else {
            temporary_directory += current_path.substr(0, separator_position) + "/";
        }

        if (access(temporary_directory.c_str(), 0) != 0) {
            if (mkdir(temporary_directory.c_str(), S_IRWXU) == 0) {
                // set permission of domain socket directory
                chmod(temporary_directory.c_str(), S_IRWXU);
            }
        }

        if (separator_position == std::string::npos) {
            break;
        }

        current_path = current_path.substr(separator_position + 1, current_path.length());
    }
}

void LocalAddress::set_file_path(std::string path) {
    if (path.empty() || path.size() >= sizeof(socket_address_.sun_path) || path.rfind("/") == std::string::npos) {
        throw std::invalid_argument("invalid Unix socket path");
    }

    address_ = path;
    std::string lock_path = address_ + ".lock";

    std::memset(socket_address_.sun_path, 0, sizeof(socket_address_.sun_path));
    std::memcpy(socket_address_.sun_path, address_.c_str(), address_.size());

    path.resize(path.rfind("/"));
    if (access(path.c_str(), F_OK)) {
        create_path(path);
    }
    if (chmod(path.c_str(), S_IRWXU) != 0) {
        LGSOMEIP_LOG_ERROR << "LocalAddress::set_file_path / Failed to secure socket directory: " << strerror(errno);
    }

    int open_flags = O_RDWR | O_CREAT;
#if defined(O_CLOEXEC)
    open_flags |= O_CLOEXEC;
#endif
#if defined(O_NOFOLLOW)
    open_flags |= O_NOFOLLOW;
#endif
    lock_fd_ = ::open(lock_path.c_str(), open_flags, S_IRUSR | S_IWUSR);

    if (lock_fd_ == -1) {
        // TODO(lg-someip): Report lock-file open failures as exceptions.
        if (errno == EACCES) {
            LGSOMEIP_LOG_DEBUG << "LocalAddress::set_file_path / This file is owned by the other process: "
                               << lock_path.c_str();
        } else {
            LGSOMEIP_LOG_ERROR << "LocalAddress::set_file_path / Open lock file error, file: " << lock_path.c_str()
                               << ", errno: " << strerror(errno);
        }

        return;
    }

    ::fchmod(lock_fd_, S_IRUSR | S_IWUSR);

    int ret = ::flock(lock_fd_, LOCK_EX | LOCK_NB);

    // remove socket file
    if (access(socket_address_.sun_path, F_OK) == 0 && ret == 0) {
        unlink(socket_address_.sun_path);
    }
}

std::string LocalAddress::get_file_path() const {
    return address_;
}

bool operator==(const LocalAddress& lhs, const LocalAddress& rhs) {
    return lhs.reliable_ == rhs.reliable_ && lhs.address_ == rhs.address_;
}

} // namespace osabstraction
} // namespace lgsomeip
