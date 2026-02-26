# Generic Skeleton

A `GenericSkeleton` is a provider-side (skeleton) object that can be configured at runtime to offer any service. Unlike standard, code-generated skeletons, it is not tied to a specific service interface at compile-time.

It operates on raw data , making it ideal for applications that need to handle data without understanding its structure, such as:
*   **Gateways/Bridges**: Forwarding data between different communication protocols.

## How to Use a Generic Skeleton

Using a `GenericSkeleton` requires two steps: defining the service in the deployment configuration and creating the skeleton instance with its runtime configuration.

### 1. Prerequisite: Deployment Configuration

The service instance must be defined in the `mw_com_config.json` file, just like any other service.

### 2. Creating and Using the Skeleton

A `GenericSkeleton` is created by providing it with metadata about the service elements (e.g., events, fields) it will offer. This metadata, including memory size and alignment for each element, is passed via the `GenericSkeletonCreateParams` struct.

The following example demonstrates how to create a generic skeleton that provides a single event.

**Example: Creating a Generic Skeleton with an Event**

```cpp
#include "score/mw/com/impl/generic_skeleton.h"
#include "score/mw/com/impl/data_type_meta_info.h"
#include <vector>

// The service instance specifier, as defined in mw_com_config.json
// const score::mw::com::InstanceSpecifier instance_specifier = ...;

// 1. Define the metadata for the event.
// The name must match the service definition.
const auto event_name = "map_api_lanes_stamped";
// The middleware needs the size and alignment for memory allocation.
const score::mw::com::impl::DataTypeMetaInfo event_meta_info{
    sizeof(MapApiLanesStamped), 
    alignof(MapApiLanesStamped)
};

// 2. Collect all event definitions.
const std::vector<score::mw::com::impl::EventInfo> events = {
    {event_name, event_meta_info}
};

// 3. Populate the creation parameters.
// Similar spans can be provided for fields and methods if they were supported.
score::mw::com::impl::GenericSkeletonCreateParams create_params;
create_params.events = events;

// 4. Create the Generic Skeleton instance.
auto create_result = score::mw::com::impl::GenericSkeleton::Create(
    instance_specifier, 
    create_params
);

if (!create_result.has_value()) {
    // Handle creation error
    return;
}
auto& skeleton = create_result.value();

// 5. Offer the service.
skeleton.OfferService();

// 6. Retrieve the event by name to send data.
auto event_it = skeleton.GetEvents().find(event_name);
if (event_it != skeleton.GetEvents().cend()) {
    // Get a non-const reference to the event to call non-const methods.
    auto& generic_event = const_cast<score::mw::com::impl::GenericSkeletonEvent&>(event_it->second);

    // Allocate a sample from the middleware. The size is known from the metadata
    auto allocate_result = generic_event.Allocate();
    if (allocate_result.has_value()) {
        auto sample = std::move(allocate_result.value());
        
        // Get the raw pointer from the smart pointer for populating the data.
        auto* typed_sample = static_cast<MapApiLanesStamped*>(sample.Get());
        
        // ... populate the typed_sample ...

        // Send the sample.
        generic_event.Send(std::move(sample));
    }
}
```