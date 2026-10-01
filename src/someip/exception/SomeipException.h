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

#ifndef LG_SOMEIP_EXCEPTION_SOMEIP_EXCEPTION_H
#define LG_SOMEIP_EXCEPTION_SOMEIP_EXCEPTION_H

#include <iomanip>
#include <exception>
#include <string>
#include <sstream>
#include <cstdint>
#include <message/MessageConstant.h>

namespace lgsomeip {

class SOMEIPException : public std::exception {
public:
    SOMEIPException(const std::string file, int line) : line_(line) {
        auto position = file.find_last_of('/');
        file_ = file.substr(position + 1, file.length());
        message_stream_ << "[Exception/" << file_ << ":" << std::to_string(line_) << "] ";
    }
    virtual const char* what() const noexcept override {
        return message_.c_str();
    }

protected:
    std::ostringstream message_stream_;
    std::string file_;
    std::string message_;
    int line_ = 0;
};

} // namespace lgsomeip

#endif //  LG_SOMEIP_EXCEPTION_SOMEIP_EXCEPTION_H
