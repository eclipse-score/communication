#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_SKELETON_BINDING_FACTORY_H

#include "score/mw/com/impl/bindings/someip/skeleton.h"
#include <memory>

namespace score::mw::com::impl::someip
{
struct SkeletonBindingFactory final
{
    static std::unique_ptr<Skeleton> Create() { return std::make_unique<Skeleton>(); }
};
}  // namespace score::mw::com::impl::someip

#endif
