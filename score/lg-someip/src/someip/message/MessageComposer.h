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

#ifndef LG_SOMEIP_MESSAGE_MESSAGE_COMPOSER_H
#define LG_SOMEIP_MESSAGE_MESSAGE_COMPOSER_H

#include <message/MessageBuilder.h>

namespace lgsomeip {

class MessageComposer {
public:
    static std::shared_ptr<MessageSD> add_entry(std::shared_ptr<MessageSD> message, SDEntry* entry,
                                                SDOption* first_option = nullptr, SDOption* second_option = nullptr);
};

} // namespace lgsomeip

#endif // LG_SOMEIP_MESSAGE_MESSAGE_COMPOSER_H
