#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_EVENT_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_EVENT_BINDING_FACTORY_H

#include "score/mw/com/impl/bindings/someip/proxy_event.h"
#include "score/mw/com/impl/bindings/someip/skeleton_event.h"
#include <memory>

namespace score::mw::com::impl::someip
{
template <typename SampleType>
struct EventBindingFactory final
{
    static std::unique_ptr<ProxyEvent<SampleType>> CreateProxyEvent()
    {
        return std::make_unique<ProxyEvent<SampleType>>();
    }
    static std::unique_ptr<SkeletonEvent<SampleType>> CreateSkeletonEvent()
    {
        return std::make_unique<SkeletonEvent<SampleType>>();
    }
};
}  // namespace score::mw::com::impl::someip

#endif
