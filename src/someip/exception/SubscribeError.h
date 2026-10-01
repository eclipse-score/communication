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

#ifndef LG_SOMEIP_EXCEPTION_SUBSCRIBE_ERROR_H
#define LG_SOMEIP_EXCEPTION_SUBSCRIBE_ERROR_H

#include <exception/SomeipException.h>

namespace lgsomeip {

class SubscribeErrorException : public SOMEIPException {
public:
    SubscribeErrorException(std::string message, const std::string file, int line) : SOMEIPException(file, line) {
        message_stream_ << "Subscribe Error : " << message;
        message_ = message_stream_.str();
    }
};

} // namespace lgsomeip

#endif //  LG_SOMEIP_EXCEPTION_SUBSCRIBE_ERROR_H
