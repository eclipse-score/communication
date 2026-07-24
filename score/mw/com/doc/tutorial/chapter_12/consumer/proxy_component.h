/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
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

#ifndef SCORE_MW_COM_TUTORIAL_CONSUMER_PROXY_COMPONENT_H
#define SCORE_MW_COM_TUTORIAL_CONSUMER_PROXY_COMPONENT_H

#include "score/mw/com/doc/tutorial/chapter_12/hello_world_service.h"
#include "score/mw/com/types.h"

#include <optional>

namespace score::mw::com::tutorial
{
class ProxyComponent
{
  public:
    ProxyComponent(HelloWorldProxy&& hello_world_proxy)
        : hello_world_proxy_(std::move(hello_world_proxy)), receive_counter_(0)
    {
    }

    bool Subscribe();
    void GetNewSamples();
    size_t GetReceiveCounter() const;

  private:
    std::optional<HelloWorldProxy> hello_world_proxy_;
    size_t receive_counter_;
};

}  // namespace score::mw::com::tutorial

#endif  // SCORE_MW_COM_TUTORIAL_CONSUMER_PROXY_COMPONENT_H
