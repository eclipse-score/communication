#ifndef SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_PROXY_BINDING_FACTORY_H
#define SCORE_MW_COM_IMPL_BINDINGS_SOMEIP_PROXY_BINDING_FACTORY_H

#include "score/mw/com/impl/bindings/someip/proxy.h"
#include <memory>

namespace score::mw::com::impl::someip
{
struct ProxyBindingFactory final
{
    static std::unique_ptr<Proxy> Create() { return std::make_unique<Proxy>(); }
};
}  // namespace score::mw::com::impl::someip

#endif
