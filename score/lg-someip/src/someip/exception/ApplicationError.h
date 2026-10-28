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

#ifndef LG_SOMEIP_EXCEPTION_APPLICATION_ERROR_H
#define LG_SOMEIP_EXCEPTION_APPLICATION_ERROR_H

#include <cstdint>

#include <exception/SomeipException.h>

namespace lgsomeip {

class ApplicationErrorException : public SOMEIPException {
public:
    ApplicationErrorException(std::uint8_t code, const std::string file, int line)
        : SOMEIPException(file, line), error_code_(code) {
        message_stream_ << "Application Error : Error Code(0x" << std::setfill('0') << std::setw(2) << std::hex
                        << std::to_string((int)error_code_) << ")";
        message_ = message_stream_.str();
    }

    std::uint8_t get_error_code() const {
        return error_code_;
    }

private:
    std::uint8_t error_code_{SOMEIP_RETURN_CODE::E_OK};
};

} // namespace lgsomeip

#endif //  LG_SOMEIP_EXCEPTION_APPLICATION_ERROR_H
