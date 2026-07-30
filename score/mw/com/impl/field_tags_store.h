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
#ifndef SCORE_MW_COM_IMPL_FIELD_TAGS_STORE_H
#define SCORE_MW_COM_IMPL_FIELD_TAGS_STORE_H

#include "score/mw/com/impl/field_tags.h"

namespace score::mw::com::impl
{

/// \brief Helper class for storing field tags represented as variadic template parameters as a set of booleans.
///
/// This allows us to pass around the tags without having to template all functions / classes with the tags.
class FieldTagsStore
{
  public:
    template <typename... Tags>
    static FieldTagsStore Create()
    {
        return {contains_type<WithNotifier, Tags...>::value,
                contains_type<WithSetter, Tags...>::value,
                contains_type<WithGetter, Tags...>::value};
    }

    [[nodiscard]] bool HasNotifier() const
    {
        return has_notifier_;
    }

    [[nodiscard]] bool HasSetter() const
    {
        return has_setter_;
    }

    [[nodiscard]] bool HasGetter() const
    {
        return has_getter_;
    }

  private:
    FieldTagsStore(bool has_notifier, bool has_setter, bool has_getter)
        : has_notifier_{has_notifier}, has_setter_{has_setter}, has_getter_{has_getter}
    {
    }

    bool has_notifier_;
    bool has_setter_;
    bool has_getter_;
};

bool operator==(const FieldTagsStore& lhs, const FieldTagsStore& rhs);

}  // namespace score::mw::com::impl

#endif  // SCORE_MW_COM_IMPL_FIELD_TAGS_STORE_H
