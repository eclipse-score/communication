// Copyright (C) 2014-2017 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <vsomeip/runtime.hpp>

#include <stdexcept>

#include "runtime_impl.hpp"

namespace vsomeip {

std::string runtime::get_property(const std::string& _name) {
    throw std::runtime_error("runtime::get_property / not implemented");
    return "";
}

void runtime::set_property(const std::string& _name, const std::string& _value) {
    throw std::runtime_error("runtime::set_property / not implemented");
}

std::shared_ptr<runtime> runtime::get() {
    return runtime_impl::get();
}

} // namespace vsomeip
