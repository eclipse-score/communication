#include "score/mw/com/impl/bindings/someip/runtime.h"

#include "score/mw/com/impl/com_error.h"

namespace score::mw::com::impl::someip
{
namespace
{
class Discovery final : public IServiceDiscoveryClient
{
  public:
    Result<void> OfferService(const InstanceIdentifier) override { return MakeUnexpected(ComErrc::kBindingFailure); }
    Result<void> StopOfferService(const InstanceIdentifier, const IServiceDiscovery::QualityTypeSelector) override
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    Result<void> StartFindService(const FindServiceHandle, FindServiceHandler<HandleType>,
                                  const EnrichedInstanceIdentifier) override
    {
        return MakeUnexpected(ComErrc::kBindingFailure);
    }
    Result<void> StopFindService(const FindServiceHandle) override { return MakeUnexpected(ComErrc::kBindingFailure); }
    Result<ServiceHandleContainer<HandleType>> FindService(const EnrichedInstanceIdentifier) override
    {
        return MakeUnexpected<ServiceHandleContainer<HandleType>>(ComErrc::kBindingFailure);
    }
};
}  // namespace

IServiceDiscoveryClient& Runtime::GetServiceDiscoveryClient() & noexcept
{
    static Discovery discovery;
    return discovery;
}
}  // namespace score::mw::com::impl::someip
