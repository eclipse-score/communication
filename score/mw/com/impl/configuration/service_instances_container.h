#ifndef COMMUNICATION_SERVICE_INSTANCES_CONTAINER_H
#define COMMUNICATION_SERVICE_INSTANCES_CONTAINER_H

#include "score/mw/com/impl/configuration/service_instance_deployment.h"
#include "score/mw/com/impl/instance_specifier.h"

#include "score/result/result.h"

#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>

#include "configuration_error.h"

namespace score::mw::com::impl
{

class ServiceInstancesContainer
{

    using ServiceInstanceDeployments = std::unordered_map<InstanceSpecifier, ServiceInstanceDeployment>;

  public:
    explicit ServiceInstancesContainer(ServiceInstanceDeployments service_instances)
        : service_instances_{std::move(service_instances)}
    {
    }

    ServiceInstancesContainer() = default;

    ServiceInstancesContainer(const ServiceInstancesContainer& other) = delete;
    ServiceInstancesContainer(ServiceInstancesContainer&& other) noexcept
        : service_instances_{std::move(other.service_instances_)}
    {
    }
    ServiceInstancesContainer& operator=(const ServiceInstancesContainer&) = delete;
    ServiceInstancesContainer& operator=(ServiceInstancesContainer&&) = delete;

    ServiceInstanceDeployment at(const InstanceSpecifier& specifier) const
    {
        std::lock_guard lock_instances{instances_mutex_};
        return service_instances_.at(specifier);
    }

    typename ServiceInstanceDeployments::const_iterator begin() const
    {
        std::lock_guard lock_instances{instances_mutex_};
        return service_instances_.begin();
    }

    typename ServiceInstanceDeployments::const_iterator end() const
    {
        std::lock_guard lock_instances{instances_mutex_};
        return service_instances_.end();
    }

    std::optional<std::reference_wrapper<const ServiceInstanceDeployment>> find(
        const InstanceSpecifier& specifier) const
    {
        std::lock_guard lock_instances{instances_mutex_};
        const auto it = service_instances_.find(specifier);
        if (it == service_instances_.end())
        {
            return std::nullopt;
        }
        return std::cref(it->second);
    }

    std::size_t size() const
    {
        std::lock_guard lock_instances{instances_mutex_};
        return service_instances_.size();
    }

    bool empty() const
    {
        std::lock_guard lock_instances{instances_mutex_};
        return service_instances_.empty();
    }

    Result<void> MergeServiceEntries(const ServiceInstanceDeployments& other_instances) noexcept
    {
        std::lock_guard lock_instances{instances_mutex_};
        for (const auto& service_instance : other_instances)
        {
            for (const auto& existing_service_instance : service_instances_)
            {
                if (existing_service_instance.first.ToString() == service_instance.first.ToString())
                {
                    return Unexpected(MakeError(configuration_errc::configuration_merge_duplicate_service_instance));
                }
            }
            std::ignore = service_instances_.emplace(service_instance.first, service_instance.second);
        }
        return {};
    }

    Result<void> MergeServiceEntries(const ServiceInstancesContainer& other) noexcept
    {
        std::lock_guard lock_other{other.instances_mutex_};
        return MergeServiceEntries(other.service_instances_);
    }

    template <typename... Args>
    std::pair<typename ServiceInstanceDeployments::iterator, bool> emplace(Args&&... args)
    {
        std::lock_guard lock_instances{instances_mutex_};
        return service_instances_.emplace(std::forward<Args>(args)...);
    }

  private:
    ServiceInstanceDeployments service_instances_;
    mutable std::mutex instances_mutex_;
};

}  // namespace score::mw::com::impl

#endif  // COMMUNICATION_SERVICE_INSTANCES_CONTAINER_H
