# User Facing API Example

> [!NOTE]
> This document is still a work in progress, thus not all `mw::com` APIs are documented here.
> However all provided information is up to date.

This document contains examples of each mw::com user facing API.

## `Error_Com` API Examples:

**Complete API Reference**: See [`score/mw/com/impl/com_error.h`](../impl/com_error.h) for all available ComErrc error codes.

### Example 1: Using `ComErrc` for InstanceIdentifier Creation Error Handling

`ComErrc` values are used to form the Error part of a `score::Result`.
For example, a function that creates an InstanceIdentifier could have this form:

<details>
<summary> Examples:</summary>

```cpp
score::Result<InstanceIdentifier> create(std::string_view serialized_format) {
    // other error handling
    if (does_not_contain_valid_InstanceIdentifier(serialized_format)) { // <- some helper function that performs a check
        // other error logging
        return MakeUnexpected(ComErrc::kInvalidInstanceIdentifierString);
    }
    // happy path of the code
    // return well formed InstanceIdentifier
}
```

**Key Points**:
- `ComErrc` values form the Error part of `score::Result<T>`
- `ComErrc::kInvalidInstanceIdentifierString`: Used for validation failures
- `MakeUnexpected(ComErrc)`: Converts error codes to Result error state
- Enables structured error handling without exceptions
</details>

---

### Example 2: Using `ComErrorDomain.MessageFor()` for Error Messages

`ComErrorDomain.MessageFor()` converts error codes to human-readable messages for logging and debugging.
It provides standardized error descriptions for all `ComErrc` error codes.

<details>
<summary> Examples:</summary>

``` cpp
auto error_message = ComErrorDomain.MessageFor(static_cast<score::result::ErrorCode>(score::mw::com::ComErrc::kServiceNotAvailable));
// error_message will contain: "Service is not available."
```
**Key Points**:
- `MessageFor()` converts error codes to human-readable messages
- Error codes must be cast to `score::result::ErrorCode` type
- Each error code has a predefined, standardized message
- Useful for logging and debugging
</details>

---

## `Runtime` API Examples:

### Example 1: Using `ResolveInstanceIDs()` for Instance Identifier Resolution

`ResolveInstanceIDs()` maps instance specifiers to instance identifier containers for service discovery.
It requires a valid `InstanceSpecifier` and validates shortname path format.

<details>
<summary> Examples:</summary>

```cpp
#include "score/mw/com/runtime.h"
#include "score/mw/com/impl/instance_specifier.h"

// Create a valid InstanceSpecifier from shortname path
auto instance_spec_result = score::mw::com::impl::InstanceSpecifier::Create("first/second/final");

if (instance_spec_result.has_value()) {
    auto instance_specifier = instance_spec_result.value();

    // Resolve instance IDs from the InstanceSpecifier
    auto result = score::mw::com::runtime::ResolveInstanceIDs(instance_specifier);

    if (result.has_value()) {
        auto instance_container = result.value();
        // Use the resolved instance identifiers
    } else {
        // Handle error - instance could not be resolved
        auto error = result.error();
    }
} else {
    // Handle error - invalid shortname path
    auto error = instance_spec_result.error();
}
```

**Key Points**:
- Requires `InstanceSpecifier` data type, not string literals
- `InstanceSpecifier::Create()` validates shortname path format (e.g., "first/second/final")
- `IsShortNameValid()` checks: alphanumeric/underscore/slash chars, no consecutive slashes, no trailing slash
- Returns `Result<InstanceIdentifierContainer>` - check with `has_value()`
- Requires runtime initialization before use
- Essential for service discovery scenarios
</details>

---

### Example 2: Using `InitializeRuntime()` for Runtime Setup

`InitializeRuntime()` sets up the mw::com system before any other mw::com functions can be used.
This function must be called once at the start of your application and supports multiple initialization methods.

#### Command Line Arguments Overload

This overload initializes the runtime using command line arguments for configuration.
It provides the simplest setup method when configuration comes from program startup parameters.

<details>
<summary> Examples:</summary>


```cpp
#include "score/mw/com/runtime.h"

int main(int argc, char* argv[]) {
    // Initialize mw::com runtime with command line arguments
    score::mw::com::runtime::InitializeRuntime(argc, argv);

    // Now you can use other mw::com functions
    // Runtime is initialized and ready to use

    return 0;
}
```

**Key Points**:
- Must be called once before using any mw::com functions
- Accepts `argc, argv` from main function for configuration
- Enables runtime initialization from command line arguments
- Simple one-line setup for basic usage
</details>

#### RuntimeConfiguration Overload

`InitializeRuntime()` can also be initialized with a `RuntimeConfiguration` object for direct configuration control.
This approach is ideal when you know the exact configuration file path at compile time.

<details>
<summary> Examples:</summary>

```cpp
#include "score/mw/com/runtime.h"
#include "score/mw/com/runtime_configuration.h"

int main() {
    // Create configuration with explicit path
    score::mw::com::runtime::RuntimeConfiguration config{"/path/to/mw_com_config.json"};

    // Initialize mw::com runtime with the configuration
    score::mw::com::runtime::InitializeRuntime(config);

    // Runtime is now ready to use
    return 0;
}
```

#### Key Points
- Creates `RuntimeConfiguration` with explicit config file path
- Direct initialization without command line arguments
- Ideal when configuration path is known at compile time
- Two-step process: create config object, then initialize runtime
</details>

---

### Example 3: Using `RuntimeConfiguration` for Configuration Management

`RuntimeConfiguration` provides flexible ways to manage mw::com configuration paths and supports multiple initialization methods.
It offers constructors for default paths, explicit paths, and command line argument parsing.

<details>
<summary> Examples:</summary>

```cpp
#include "score/mw/com/runtime_configuration.h"

// Default constructor - uses "./etc/mw_com_config.json"
score::mw::com::runtime::RuntimeConfiguration default_config;

// Explicit path constructor
score::mw::com::runtime::RuntimeConfiguration explicit_config{"/custom/path/config.json"};

// Command line arguments constructor
score::mw::com::runtime::RuntimeConfiguration cmd_config{argc, argv};

// Get the resolved configuration path
const auto& config_path = default_config.GetConfigurationPath();
```

#### Key Points
- Default constructor uses `"./etc/mw_com_config.json"` as fallback path
- Explicit path constructor allows direct configuration file specification
- Command line constructor accepts `argc, argv` for configuration parsing
- `GetConfigurationPath()` returns the resolved configuration file path

</details>

---

## `mw::com Data Type` API Examples:

### Example 1: Using `InstanceIdentifier` for Service Instance Management

`InstanceIdentifier` represents a specific instance of a service in the mw::com framework.
It provides serialization and comparison capabilities for service instance identification.

<details>
<summary> Examples:</summary>

```cpp
#include "score/mw/com/impl/instance_identifier.h"
#include <iostream>
#include <set>

// Example: Converting InstanceIdentifier to string
void ConvertToString(const score::mw::com::impl::InstanceIdentifier& identifier) {
	// Get string representation for serialization/logging
	std::string_view serialized_form = identifier.ToString();
	std::cout << "Serialized identifier: " << serialized_form << std::endl;
}

	// Example: Creating InstanceIdentifier from serialized string
void CreateInstanceFromString(const std::string& serialized_format) {

	// Create InstanceIdentifier from serialized format, this string will have previously been generated by another process through identifier.ToString();
	auto instance_result = score::mw::com::impl::InstanceIdentifier::Create(std::move(serialized_format));

	if (instance_result.has_value()) {
	    auto identifier = instance_result.value();
	    std::cout << "InstanceIdentifier created successfully" << std::endl;
	} else {
	    std::cerr << "Failed to create InstanceIdentifier" << std::endl;
	}
}

	// Example: Using InstanceIdentifier in containers
void UseInContainers() {
	// Ordered containers (deterministic iteration)
	std::set<score::mw::com::impl::InstanceIdentifier> sorted_instances;

	// Container operations work due to operator== and operator< implementations
}
```

**Key Points**:
- `Create()`: Factory method for creating InstanceIdentifier from serialized string
- `ToString()`: Returns string representation for serialization and logging
- Exception-safe: Uses `score::Result` instead of exceptions for error handling
- Immutable: Once created, InstanceIdentifier cannot be modified
- Essential for service discovery and instance management in BMW mw::com framework

</details>

---

### Example 2: Using `InstanceSpecifier` for Application Port Identification

Inside the `mw_com_configuration.json` [file](../impl/configuration/README.md#instanceSpecifier) each service-instance object within the `serviceInstances` array of the configuration has a unique, identifying property `instanceSpecifier`. The `InstanceSpecifier` class is the in-code representation of this unique identifying property and must be constructed from the same string. Which has a URI-like structure.

**Shortname Path Validation Rules:**
- Must not be empty
- First character must be: letter (a-z, A-Z), underscore (_), or forward slash (/)
- Subsequent characters must be: alphanumeric (a-z, A-Z, 0-9), underscore (_), or forward slash (/)
- Must not end with a forward slash (/)
- Must not contain consecutive forward slashes (//)

**Valid Examples:** `"NavigationService/MainInstance"`, `"ServiceA/Instance1"`, `"_service/data"`
**Invalid Examples:** `"Service/"` (ends with slash), `"Service//Instance"` (consecutive slashes), `"123Service"` (starts with number)

<details>
<summary> Examples:</summary>

```cpp
#include "score/mw/com/impl/instance_specifier.h"
#include <iostream>
#include <set>

// Example: Creating InstanceSpecifier from shortname path
void CreateSpecifierFromShortname() {
    std::string shortname_path = "NavigationService/MainInstance/DataProvider";

    // Create InstanceSpecifier from shortname path
    auto specifier_result = score::mw::com::impl::InstanceSpecifier::Create(std::move(shortname_path));

    if (specifier_result.has_value()) {
        auto specifier = specifier_result.value();
        std::cout << "InstanceSpecifier created successfully" << std::endl;
    } else {
        std::cerr << "Failed to create InstanceSpecifier: invalid shortname path" << std::endl;
    }
}

// Example: Converting InstanceSpecifier to string
void ConvertSpecifierToString(const score::mw::com::impl::InstanceSpecifier& specifier) {
    // Get string representation for logging/debugging
    std::string_view shortname_path = specifier.ToString();
    std::cout << "Shortname path: " << shortname_path << std::endl;
}

// Example: Using InstanceSpecifier in containers
void UseSpecifierInContainers() {
    // Ordered containers (deterministic iteration)
    std::set<score::mw::com::impl::InstanceSpecifier> sorted_specifiers;

    // Create and insert specifiers
    auto spec1 = score::mw::com::impl::InstanceSpecifier::Create("ServiceA/Instance1");
    auto spec2 = score::mw::com::impl::InstanceSpecifier::Create("ServiceB/Instance2");

    if (spec1.has_value() && spec2.has_value()) {
        sorted_specifiers.insert(spec1.value());
        sorted_specifiers.insert(spec2.value());
    }

    // Container operations work due to comparison operators
}
```

**Production Example Reference**: `score/analysis/tracing/test/LoadTester/lola_binding/proxy/lola_flat_deserializing_proxy.cpp:119`


**Key Points**:
- `Create()`: Static factory method for creating InstanceSpecifier from shortname path
- `ToString()`: Returns string_view representation for logging and debugging
- Exception-safe: Uses `score::Result` instead of exceptions for error handling
- Validation: Automatically validates shortname path format during creation
- Immutable: Once created, InstanceSpecifier cannot be modified
- Essential for application port identification and service discovery mapping

</details>

---

### Example 3: Using `FindServiceHandle` for Service Discovery Management

`FindServiceHandle` represents a handle for managing service discovery operations.
It's returned by `StartFindService()` and used to identify and control different service searches.

<details>
<summary> Examples:</summary>

```cpp
#include "score/mw/com/impl/find_service_handle.h"
#include <iostream>
#include <set>

// Example: Basic FindServiceHandle usage in service discovery
template<typename ProxyType, typename HandlerType>
void ManageServiceSearch(ProxyType& proxy, HandlerType& handler) {
    // FindServiceHandle is returned by StartFindService()
    score::mw::com::impl::FindServiceHandle search_handle = proxy.StartFindService(handler);

    // Store handle to manage the search later
    std::cout << "Service search started with handle: " << search_handle << std::endl;

    // Later, use handle to stop the search when done
    proxy.StopFindService(search_handle);
    std::cout << "Service search stopped" << std::endl;
}

// Example: Using FindServiceHandle in callback to stop search automatically
template<typename ProxyType>
void AutoStopServiceSearch(ProxyType& proxy) {
    // Create a handler that can stop the search from within the callback
    auto handler = [&proxy](auto available_services, score::mw::com::impl::FindServiceHandle search_handle) {
        std::cout << "Service availability changed!" << std::endl;
        std::cout << "Found " << available_services.size() << " services" << std::endl;

        // Stop the search automatically when first service is found
        if (!available_services.empty()) {
            std::cout << "First service found! Stopping search using handle from callback" << std::endl;
            proxy.StopFindService(search_handle);  // Use handle from callback parameter
        }
    };

    // Start the search - the callback can use the handle to stop itself
    score::mw::com::impl::FindServiceHandle search_handle = proxy.StartFindService(handler);
    std::cout << "Started auto-stopping service search" << std::endl;
}

// Example: Storing multiple handles in container
void TrackMultipleSearches() {
    std::set<score::mw::com::impl::FindServiceHandle> active_searches;

    // Add search handles to track ongoing operations
    // active_searches.insert(handle1);
    // active_searches.insert(handle2);

    std::cout << "Tracking " << active_searches.size() << " active searches" << std::endl;
}
```

**Production Example Reference**: `score/analysis/tracing/test/AccessTester/score/static_reflection_with_serialization/proxy_observer.h:40`

**Key Points**:
- Returned by `StartFindService()` to identify service discovery operations
- Used with `StopFindService()` to cancel searches
- Supports container storage for managing multiple searches
- Essential for service discovery lifecycle management

</details>

---

### Example 4: Using `FindServiceHandler` for Service Discovery Callbacks

`FindServiceHandler` is a callback function that gets called when service availability changes.
It receives a `ServiceHandleContainer` with available services and a `FindServiceHandle` for managing the search.

<details>
<summary> Examples:</summary>

```cpp
#include "score/mw/com/types.h"
#include <iostream>

// Example: Basic FindServiceHandler usage for service discovery
template<typename ServiceProxy>
void SetupServiceDiscovery(ServiceProxy& proxy) {
    // Create a handler that processes service availability changes
    score::mw::com::FindServiceHandler<ServiceProxy> handler =
        [](score::mw::com::ServiceHandleContainer<ServiceProxy> available_services,
           score::mw::com::FindServiceHandle search_handle) {

            std::cout << "Service availability changed!" << std::endl;
            std::cout << "Found " << available_services.size() << " services" << std::endl;

            // Process each available service
            for (const auto& service : available_services) {
                std::cout << "Processing available service instance" << std::endl;
                // Use the service handle for communication
            }

            // Optionally stop search when first service is found
            if (!available_services.empty()) {
                std::cout << "Service found! Using first available service" << std::endl;
            }
        };

    // Use the handler with StartFindService
    auto search_handle = proxy.StartFindService(handler);
    std::cout << "Started service discovery with handler" << std::endl;
}
```

**Key Points**:
- Template type alias for callback function with two parameters
- Used as parameter for `StartFindService()` method
- Receives `ServiceHandleContainer` with available service instances
- Receives `FindServiceHandle` to control the search operation
- Called automatically when service availability changes
- Essential for reactive service discovery

</details>

---

### Example 5: Using `MethodCallProcessingMode` for Service Implementation Control

> [!WARNING]
> Do not use!

>This Enum is non-functional and was implemented for compatibility reasons with `ara::com` standard. Since we do not maintain this compatibility anymore, we have no use for this API and will deprecate and remove it in the future.
---

### Example 6: Using `SubscriptionState` for Event Subscription Management

`SubscriptionState` represents the current state of a proxy event subscription.
It's returned by `GetSubscriptionStatus()` to check if the proxy is receiving event data.

<details>
<summary> Examples:</summary>

```cpp
#include "score/mw/com/types.h"
#include <iostream>

// Example: Basic subscription state checking for proxy events
template<typename ProxyEvent>
void CheckEventSubscriptionStatus(ProxyEvent& event) {
    // Get current subscription state
    score::mw::com::SubscriptionState state = event.GetSubscriptionStatus();

    // Handle different subscription states
    switch (state) {
        case score::mw::com::SubscriptionState::kSubscribed:
            std::cout << "Event is subscribed - receiving data" << std::endl;
            break;

        case score::mw::com::SubscriptionState::kNotSubscribed:
            std::cout << "Event is not subscribed - no data received" << std::endl;
            event.Subscribe();  // Start subscription
            break;

        case score::mw::com::SubscriptionState::kSubscriptionPending:
            std::cout << "Subscription is pending - waiting for confirmation" << std::endl;
            break;
    }
}
```
**Production Example Reference**: `ecu/ipnext/domains/ref_applications/80_lola/client/src/ref_app_lola_client.cpp:202`

**Key Points**:
- Three states: `kSubscribed` (receiving data), `kNotSubscribed` (no data), `kSubscriptionPending` (waiting)
- Retrieved using `GetSubscriptionStatus()` method on proxy events
- Used to check subscription status before expecting event data
- Essential for managing event data flow on proxy side

</details>

---

### Example 7: Using `SamplePtr` for Receiving Event Data

`SamplePtr` is a smart pointer that carries received data on the proxy side.
It provides safe access to event data with automatic memory management.

<details>
<summary> Examples:</summary>

```cpp
#include "score/mw/com/types.h"
#include <iostream>

// Example: Basic SamplePtr usage for receiving event data
template<typename DataType>
void ProcessReceivedData(score::mw::com::SamplePtr<DataType> sample) {
    // Check if sample contains valid data
    if (sample) {
        std::cout << "Valid data received" << std::endl;

        // Access the data using get() method
        const DataType* data_ptr = sample.get();

        // Or access directly using pointer operators
        const DataType& data_ref = *sample;

        std::cout << "Data processed successfully" << std::endl;
    } else {
        std::cout << "No data available" << std::endl;
    }
}
```

**Key Points**:
- Smart pointer for received event data on proxy side
- Check validity using `if (sample)` before accessing data
- Access data using `get()` method or pointer operators (`*`, `->`)
- Automatically manages memory for received data

</details>

---

### Example 8: Using `SampleAllocateePtr` for Sending Event Data

`SampleAllocateePtr` is a smart pointer for allocated memory that will hold data to be sent.
It provides safe data preparation on the skeleton side before sending to subscribers.

<details>
<summary> Examples:</summary>

```cpp
#include "score/mw/com/types.h"
#include <iostream>

// Example: Basic SampleAllocateePtr usage for preparing data to send
template<typename DataType>
void PrepareDataForSending(score::mw::com::SampleAllocateePtr<DataType> sample) {
    // Check if allocation was successful
    if (sample) {
        std::cout << "Memory allocated successfully" << std::endl;

        // Access the allocated memory using get() method
        DataType* data_ptr = sample.Get();

        // Or access directly using pointer operators
        DataType& data_ref = *sample;

        // Fill the data (example)
        // data_ref.field1 = value1;

        std::cout << "Data prepared and ready to send" << std::endl;
    } else {
        std::cout << "Memory allocation failed" << std::endl;
    }
}
```

**Key Points**:
- Smart pointer for allocated memory on skeleton side
- Check validity using `if (sample)` before accessing memory
- Access data using `Get()` method or pointer operators (`*`, `->`)
- Data automatically sent when pointer goes out of scope

</details>

---

### Example 9: Using `EventReceiveHandler` for Event Notifications

`EventReceiveHandler` is a simple callback function for handling event notifications on the proxy side.
It's called when new event data becomes available from the service.

<details>
<summary> Examples:</summary>

```cpp
#include "score/mw/com/types.h"
#include <iostream>

// Example: Basic EventReceiveHandler usage for event notifications
template<typename ProxyEvent>
void SetupEventNotifications(ProxyEvent& event) {
    // Create a simple event receive handler (no parameters)
    score::mw::com::EventReceiveHandler handler = []() {
        std::cout << "Event notification received!" << std::endl;
        std::cout << "New data is available" << std::endl;
    };

    // Set the handler for the proxy event
    event.SetReceiveHandler(handler);
    std::cout << "Event handler configured successfully" << std::endl;
}
```

**Key Points**:
- Simple callback function with no parameters
- Called automatically when new event data becomes available
- Used with proxy event `SetReceiveHandler()` method
- Enables reactive event processing on proxy side

</details>

---

### Example 10: Using `AsProxy` for Interface Type Interpretation

`AsProxy` interprets an interface template as a proxy type for service consumption.
It provides a simple way to create proxy instances from service interface definitions.

<details>
<summary> Examples:</summary>

```cpp
#include "score/mw/com/types.h"
#include <iostream>

// Example: Basic AsProxy usage for creating proxy from interface
template<template<class> class ServiceInterface>
void CreateProxyFromInterface(HandleType service_handle) {
    // Interpret interface as proxy type
    using ProxyType = score::mw::com::AsProxy<ServiceInterface>;

    // Create proxy instance using the static Create method
    auto proxy_result = ProxyType::Create(service_handle);

    if (proxy_result.has_value()) {
        auto proxy = proxy_result.value();
        std::cout << "Proxy created successfully from interface" << std::endl;

        // Use the proxy for service communication
        std::cout << "Proxy ready for service consumption" << std::endl;
    } else {
        std::cout << "Failed to create proxy from interface" << std::endl;
    }
}
```
**Production Example Reference**: `ecu/ipnext/domains/ref_applications/80_lola/interface/buffer_lola.h`

**Key Points**:
- Template type alias for creating proxy types from interfaces
- Use `Create()` static method with service handle to create proxy instance
- Returns `score::Result` - check with `has_value()` before using
- Enables type-safe proxy creation for service consumption

</details>

---

### Example 11: Using `AsSkeleton` for Interface Type Interpretation

`AsSkeleton` interprets an interface template as a skeleton type for service implementation.
It provides a simple way to create skeleton instances from service interface definitions.

<details>
<summary> Examples:</summary>

```cpp
#include "score/mw/com/types.h"
#include <iostream>

// Example: Basic AsSkeleton usage for creating skeleton from interface
template<template<class> class ServiceInterface>
void CreateSkeletonFromInterface(const score::mw::com::InstanceSpecifier& specifier) {
    // Interpret interface as skeleton type
    using SkeletonType = score::mw::com::AsSkeleton<ServiceInterface>;

    // Create skeleton instance using the static Create method
    auto skeleton_result = SkeletonType::Create(specifier);

    if (skeleton_result.has_value()) {
        auto skeleton = skeleton_result.value();
        std::cout << "Skeleton created successfully from interface" << std::endl;

        // Offer the service to make it available
        auto offer_result = skeleton.OfferService();
        if (offer_result.has_value()) {
            std::cout << "Service offered successfully" << std::endl;
        }
    } else {
        std::cout << "Failed to create skeleton from interface" << std::endl;
    }
}
```
**Production Example Reference**: `ecu/ipnext/domains/ref_applications/80_lola/interface/buffer_lola.h`

**Key Points**:
- Template type alias for creating skeleton types from interfaces
- Use `Create()` static method with instance specifier to create skeleton instance
- Returns `score::Result` - check with `has_value()` before using
- Call `OfferService()` to make the service available to clients

</details>

---

### Example 12: Using `GenericProxy` for Type-Erased Data Access

`GenericProxy` is a type-erased proxy that can access service events without knowing the specific data types at compile time.
It's used for runtime service discovery and generic event handling when exact types are unknown.

<details>
<summary> Examples:</summary>

```cpp
#include "score/mw/com/types.h"
#include "score/mw/com/impl/handle_type.h"
#include <iostream>

// Example: Basic GenericProxy usage for type-erased service access
void AccessServiceGeneric(score::mw::com::impl::HandleType service_handle) {
    // Create GenericProxy from service handle
    auto proxy_result = score::mw::com::GenericProxy::Create(service_handle);

    if (proxy_result.has_value()) {
        auto generic_proxy = proxy_result.value();
        std::cout << "GenericProxy created successfully" << std::endl;

        // Access the event map for type-erased event handling
        auto& events = generic_proxy.GetEvents();
        std::cout << "Event map accessed - can process events without type info" << std::endl;

        std::cout << "GenericProxy ready for runtime service introspection" << std::endl;
    } else {
        std::cout << "Failed to create GenericProxy" << std::endl;
    }
}
```
**Production Example Reference**: `score/analysis/tracing/test/TestApps/AccessTester/score/static_reflection_with_serialization/sample_sender_receiver.cpp:277`

**Key Points**:
- `Create()`: Static factory method for creating GenericProxy from HandleType
- `GetEvents()`: Returns event map for accessing service events without type information
- Type-erased access to service events at runtime
- Useful for debugging, monitoring, and generic service introspection

</details>

---
