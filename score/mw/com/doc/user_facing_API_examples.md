# User Facing API Example

This document contains examples of each mw::com user facing API.

## API Reference Table

| API |
|-----|
| **Error Handling** |
| [`ComErrc`](#example-1-using-comerrc-for-instanceidentifier-creation-error-handling) |
| [`ComErrorDomain.MessageFor()`](#example-2-using-comerrordommessagefor-for-error-messages) |
| **Runtime & Configuration** |
| [`ResolveInstanceIDs()`](#example-1-using-resolveinstanceids-for-instance-identifier-resolution) |
| [`InitializeRuntime(argc, argv)`](#command-line-arguments-overload) |
| [`InitializeRuntime(RuntimeConfiguration)`](#runtimeconfiguration-overload) |
| [`RuntimeConfiguration(argc, argv)`](#example-3-using-runtimeconfiguration-for-configuration-management) |
| [`RuntimeConfiguration(Path)`](#example-3-using-runtimeconfiguration-for-configuration-management) |
| [`RuntimeConfiguration::GetConfigurationPath()`](#example-3-using-runtimeconfiguration-for-configuration-management) |
| **Data Types** |
| [`InstanceIdentifier::Create()`](#example-1-using-instanceidentifier-for-service-instance-management) |
| [`InstanceIdentifier::ToString()`](#example-1-using-instanceidentifier-for-service-instance-management) |
| [`InstanceSpecifier::Create()`](#example-2-using-instancespecifier-for-application-port-identification) |
| [`InstanceSpecifier::ToString()`](#example-2-using-instancespecifier-for-application-port-identification) |
| [`FindServiceHandle`](#example-3-using-findservicehandle-for-service-discovery-management) |
| [`FindServiceHandler`](#example-4-using-findservicehandler-for-service-discovery-callbacks) |
| [`SubscriptionState`](#example-6-using-subscriptionstate-for-event-subscription-management) |
| [`SamplePtr`](#example-7-using-sampleptr-for-receiving-event-data) |
| [`SampleAllocateePtr`](#example-8-using-sampleallocateeptr-for-sending-event-data) |
| [`EventReceiveHandler`](#example-9-using-eventreceivehandler-for-event-notifications) |
| [`AsProxy`](#example-10-using-asproxy-for-interface-type-interpretation) |
| [`AsSkeleton`](#example-11-using-asskeleton-for-interface-type-interpretation) |
| [`GenericProxy`](#example-12-using-genericproxy-for-type-erased-data-access) |
| **Proxy** |
| [`ProxyWrapper::Create()`](#example-1-create---creating-proxy-from-service-handle) |
| [`ProxyWrapper::FindService()`](#example-2-findservice---synchronous-service-discovery-polling-mode) |
| [`ProxyWrapper::StartFindService()`](#example-3-startfindservice---asynchronous-service-discovery) |
| [`ProxyWrapper::StopFindService()`](#example-4-stopfindservice---cancel-service-discovery) |
| [`ProxyWrapper::GetHandle()`](#example-5-gethandle---get-service-instance-handle) |
| **Proxy Event** |
| [`ProxyEvent::Subscribe()`](#example-6-subscribe---subscribe-to-service-event) |
| [`ProxyEvent::GetNewSamples()`](#example-7-getnewsamples---retrieve-event-samples) |
| [`ProxyEvent::SetReceiveHandler()`](#example-8-setreceivehandler---register-event-notification-handler) |
| [`ProxyEvent::GetSubscriptionState()`](#example-9-getsubscriptionstate---check-subscription-status) |
| [`ProxyEvent::GetFreeSampleCount()`](#example-10-getfreesamplecount---check-available-sample-buffer-space) |
| [`ProxyEvent::GetNumNewSamplesAvailable()`](#example-11-getnumnewsamplesavailable---check-pending-sample-count) |
| [`ProxyEvent::UnsetReceiveHandler()`](#example-12-unsetreceivehandler---remove-event-handler) |
| [`ProxyEvent::IsBindingValid()`](#example-13-isbindingvalid---check-event-binding-health) |
| [`ProxyEvent::Unsubscribe()`](#example-14-unsubscribe---cancel-event-subscription) |
| **Proxy Field** |
| [`ProxyField::Subscribe()`](#example-15-subscribe---subscribe-to-field-changes) |
| [`ProxyField::GetSubscriptionState()`](#example-16-getsubscriptionstate---check-field-subscription-status) |
| [`ProxyField::GetFreeSampleCount()`](#example-17-getfreesamplecount---check-available-field-buffer-space) |
| [`ProxyField::GetNumNewSamplesAvailable()`](#example-18-getnumnewsamplesavailable---check-pending-field-changes) |
| [`ProxyField::GetNewSamples()`](#example-19-getnewsamples---retrieve-field-change-samples) |
| [`ProxyField::SetReceiveHandler()`](#example-20-setreceivehandler---register-field-change-handler) |
| [`ProxyField::UnsetReceiveHandler()`](#example-21-unsetreceivehandler---remove-field-change-handler) |
| [`ProxyField::Unsubscribe()`](#example-22-unsubscribe---cancel-field-change-subscription) |
| **Skeleton** |
| [`SkeletonWrapper::Create()`](#example-1-create---creating-skeleton-instance) |
| [`SkeletonWrapper::OfferService()`](#example-2-offerservice---make-service-available-to-clients) |
| [`SkeletonWrapper::StopOfferService()`](#example-3-stopofferservice---stop-service-availability) |
| **Skeleton Event** |
| [`SkeletonEvent::Send()`](#example-4-send---send-event-data-to-subscribers) |
| [`SkeletonEvent::Allocate()`](#example-5-allocate---allocate-memory-for-event-data) |
| **Skeleton Field** |
| [`SkeletonField::Update()`](#example-6-update---update-field-value-and-notify-subscribers) |
| [`SkeletonField::Allocate()`](#example-7-allocate---allocate-memory-for-zero-copy-field-update) |

---

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
This function can be called once at the start of your application and supports multiple initialization methods. However, the call can also be omitted. The runtime will then be lazy initialized at the time when it is first needed. And the program will expect a configuration file called mw_com_config.json inside the etc subfolder of the working directory of the running process.

> [!Warning]
> Relying on Lazy initialization might cause unpredictably long runtime for the first LoLa API call that relies on the runtime.

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

## `Proxy` APIs - Client-Side Service Communication

The Proxy side APIs enable client applications to discover, connect to, and interact with services in the mw::com framework.
Proxies provide access to remote service events and fields.

### Proxy

A proxy class can be constructed from an Interface Template through the `AsProxy` template (see [`AsProxy`](#example-10-using-asproxy-for-interface-type-interpretation)).

#### Example 1: `Create()` - Creating Proxy from Service Handle

`Create()` instantiates a proxy using a service handle obtained from `FindService()` or `StartFindService()`.

<details>
<summary> Examples: </summary>

```cpp
#include "generated_service_proxy.h"
#include <thread>
#include <chrono>

// Step 1: Find service using polling
auto spec = score::mw::com::InstanceSpecifier::Create("my/service/path");
if (spec.has_value()) {
    std::optional<MyServiceProxy::HandleType> service_handle;

    // Poll until service is found
    while (!service_handle.has_value()) {
        auto handles = MyServiceProxy::FindService(spec.value());

        if (handles.has_value() && !handles.value().empty()) {
            service_handle = handles.value()[0];
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Step 2: Create proxy using the handle
    auto proxy_result = MyServiceProxy::Create(service_handle.value());
    if (proxy_result.has_value()) {
        auto proxy = std::move(proxy_result.value());
        // Use proxy to interact with service
    }
}
```

**Production Example Reference**: `score/mw/com/performance_benchmarks/macro_benchmark/lola_benchmarking_client.cpp:334`

**Key Points**:
- Requires `HandleType` obtained from `FindService()` or `StartFindService()`
- Returns `Result<ProxyWrapper>` - check with `has_value()` before use
- Proxy lifetime controls connection to service - destroyed proxy releases resources
- Handle contains binding information (InstanceIdentifier, serviceId, binding type)
- Handle caches discovery results

</details>

---

#### Example 2: `FindService()` - Synchronous Service Discovery (Polling Mode)

`FindService()` looks up currently available service instances and returns immediately.
It does NOT wait for services to appear.

<details>
<summary> Examples: </summary>

```cpp
#include "score/mw/com/impl/proxy_base.h"
#include <thread>
#include <chrono>

// Poll for service using InstanceSpecifier
auto spec = score::mw::com::InstanceSpecifier::Create("my/service/path");
if (spec.has_value()) {
    std::optional<MyServiceProxy::HandleType> service_handle;

    // Polling loop - repeatedly check until service is found
    while (!service_handle.has_value()) {
        auto handles = MyServiceProxy::FindService(spec.value());

        if (handles.has_value() && !handles.value().empty()) {
            service_handle = handles.value()[0];
            break;
        }

        // Wait before next poll attempt
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Create proxy once service is found
    auto proxy = MyServiceProxy::Create(service_handle.value());
}
```

**Production Example Reference**: `score/mw/com/performance_benchmarks/macro_benchmark/lola_benchmarking_client.cpp:91`

**Key Points**:
- Returns immediately with current state - does NOT block waiting for services
- Returns `ServiceHandleContainer` with available instances or empty container
- Can be used in polling loop to check for service availability

</details>

---

#### Example 3: `StartFindService()` - Asynchronous Service Discovery

`StartFindService()` initiates asynchronous service discovery with callback notifications for service availability changes.

<details>
<summary> Examples: </summary>

```cpp
#include "score/mw/com/impl/proxy_base.h"

// Asynchronous service discovery with callback using InstanceSpecifier
auto spec = score::mw::com::InstanceSpecifier::Create("my/service/path");
if (spec.has_value()) {
    auto callback = [](ServiceHandleContainer<MyServiceProxy::HandleType> handles,
                       FindServiceHandle find_handle) {
        for (const auto& handle : handles) {
            // Create proxy using HandleType from callback
            auto proxy_result = MyServiceProxy::Create(handle);
            if (proxy_result.has_value()) {
                // Process discovered service
            }
        }
    };

    auto find_handle = MyServiceProxy::StartFindService(callback, spec.value());
    // Callback invoked when services are found
}
```

**Key Points**:
- Non-blocking operation - returns immediately
- Callback receives `ServiceHandleContainer<HandleType>` with discovered service handles
- Each handle passed to `Create()` to instantiate proxy
- Returns `FindServiceHandle` for later cancellation
- Use `StopFindService()` to cancel discovery

</details>

---

#### Example 4: `StopFindService()` - Cancel Service Discovery

`StopFindService()` cancels an active asynchronous service discovery operation using the handle from `StartFindService()`.

<details>
<summary> Examples: </summary>

```cpp
// Stop active service discovery
auto find_handle = MyServiceProxy::StartFindService(callback, spec);
if (find_handle.has_value()) {
    // Later, cancel discovery
    auto stop_result = MyServiceProxy::StopFindService(find_handle.value());
}
```

**Key Points**:
- Requires valid `FindServiceHandle` from `StartFindService()`
- Stops callback notifications
- Safe to call even if discovery already completed
- Returns `ResultBlank` - check for errors

</details>

---

#### Example 5: `GetHandle()` - Get Service Instance Handle

`GetHandle()` returns the handle that was used to instantiate this proxy.

<details>
<summary> Examples: </summary>

```cpp
#include <thread>
#include <chrono>

// First, find and create proxy using handle
auto spec = score::mw::com::InstanceSpecifier::Create("my/service/path");
if (spec.has_value()) {
    std::optional<MyServiceProxy::HandleType> service_handle;

    // Poll until service is found
    while (!service_handle.has_value()) {
        auto handles = MyServiceProxy::FindService(spec.value());

        if (handles.has_value() && !handles.value().empty()) {
            service_handle = handles.value()[0];
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Create proxy using HandleType from FindService
    auto proxy_result = MyServiceProxy::Create(service_handle.value());

    if (proxy_result.has_value()) {
        auto proxy = std::move(proxy_result.value());

        // Get the service handle that was used to create this proxy
        const auto& handle = proxy.GetHandle();
        auto instance_id = handle.GetInstanceIdentifier();
        // Use instance_id for logging or tracking
    }
}
```

**Key Points**:
- Returns reference to `HandleType` (contains InstanceIdentifier)
- Handle identifies the specific service instance
- Can extract InstanceIdentifier from handle

</details>

---

#### Example 6: `Subscribe()` - Subscribe to Service Event

`Subscribe()` establishes event subscription with maximum sample buffer size.
Must be called before `GetNewSamples()` can retrieve event data.

<details>
<summary> Examples: </summary>

```cpp
#include "generated_service_proxy.h"
#include <thread>
#include <chrono>

// First, find and create proxy using handle
auto spec = score::mw::com::InstanceSpecifier::Create("my/service/path");
if (spec.has_value()) {
    std::optional<MyServiceProxy::HandleType> service_handle;

    // Poll until service is found
    while (!service_handle.has_value()) {
        auto handles = MyServiceProxy::FindService(spec.value());

        if (handles.has_value() && !handles.value().empty()) {
            service_handle = handles.value()[0];
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Create proxy using the handle
    auto proxy_result = MyServiceProxy::Create(service_handle.value());

    if (proxy_result.has_value()) {
        auto proxy = std::move(proxy_result.value());

        // Subscribe to event with buffer for 10 samples
        auto result = proxy.my_event.Subscribe(10);

        if (result.has_value()) {
            // Successfully subscribed - can now receive events
        }
    }
}
```

**Key Points**:
- Takes only `max_sample_count` parameter
- Returns `ResultBlank` - check with `has_value()` before use
- Must subscribe before calling `GetNewSamples()`

</details>

---

#### Example 7: `GetNewSamples()` - Retrieve Event Samples

`GetNewSamples()` retrieves pending event samples through a callback for each available sample.
Requires active subscription via `Subscribe()` before use.

<details>
<summary> Examples: </summary>

```cpp
// Subscribe to event first
proxy->my_event.Subscribe(10);

// Retrieve new samples with callback
auto result = proxy->my_event.GetNewSamples(
    [](score::mw::com::SamplePtr<const MyDataType> sample) {
        if (sample) {
            // Process sample data
            ProcessData(*sample);
        }
    },
    5  // Maximum 5 samples per call
);

if (result.has_value()) {
    // result.value() contains number of samples retrieved
}
```

**Production Example Reference**: `ecu/ipnext/domains/ref_applications/80_lola/client/src/ref_app_lola_client.cpp`

**Key Points**:
- Must call `Subscribe()` before `GetNewSamples()`
- Callback invoked once per sample
- `SamplePtr` is smart pointer managing sample lifetime
- Second parameter limits samples retrieved per call
- Returns `Result<size_t>` with number of samples processed

</details>

---

#### Example 8: `SetReceiveHandler()` - Register Event Notification Handler

`SetReceiveHandler()` registers a callback that's invoked when new event samples arrive.

<details>
<summary> Examples: </summary>

```cpp
// Register handler for automatic event notification
proxy->my_event.SetReceiveHandler([]() {
    // Called when new samples available
    // Retrieve samples using GetNewSamples()
});
```

**Key Points**:
- Handler called asynchronously when samples arrive
- Must still call `GetNewSamples()` to retrieve data
- Handler executes in mw::com internal thread
- Keep handler lightweight - offload work to application threads

</details>

---

#### Example 9: `GetSubscriptionState()` - Check Subscription Status

`GetSubscriptionState()` returns current subscription state for the event.
Primarily used for logging, monitoring subscription progress, or debugging subscription issues.

<details>
<summary> Examples: </summary>

```cpp
#include "score/mw/com/types.h"
#include <chrono>
#include <thread>

// Example: Logging subscription states for monitoring/debugging
proxy.my_event.Subscribe(10);

// Log current subscription state
auto state = proxy.my_event.GetSubscriptionState();
switch (state) {
    case score::mw::com::SubscriptionState::kSubscribed:
        LOG_INFO("Event is subscribed - receiving data");
        break;
    case score::mw::com::SubscriptionState::kNotSubscribed:
        LOG_WARN("Event is not subscribed");
        break;
    case score::mw::com::SubscriptionState::kSubscriptionPending:
        LOG_INFO("Subscription is pending - waiting for confirmation");
        break;
}
```

**Production Example Reference**: `score/mw/com/test/field_initial_value/client.cpp:57`

**Key Points**:
- `kSubscribed`: Event is active and receiving samples
- `kNotSubscribed`: Must call `Subscribe()` first
- `kSubscriptionPending`: Subscription in progress
- Primarily used for logging/monitoring

</details>

---

#### Example 10: `GetFreeSampleCount()` - Check Available Sample Buffer Space

`GetFreeSampleCount()` returns the number of samples that can still be received by the application.
If this returns 0, you must drop at least one `SamplePtr` before calling `GetNewSamples()` again.

<details>
<summary> Examples: </summary>

```cpp
// Check available buffer space before retrieving samples
auto free_count = proxy->my_event.GetFreeSampleCount();
if (free_count > 0) {
    auto samples = proxy->my_event.GetNewSamples(callback, free_count);
}
```

**Key Points**:
- Returns number of samples that can still be buffered
- Value is 0 when maximum buffer capacity is reached
- Drop `SamplePtr` references to free buffer space
- Unspecified if event is not subscribed

</details>

---

#### Example 11: `GetNumNewSamplesAvailable()` - Check Pending Sample Count

`GetNumNewSamplesAvailable()` returns the number of new samples currently available for retrieval.

> [!Note]
> This API is just as expensive to call as `GetNewSamples` and has no added value for most users. It should be avoided.

---

#### Example 12: `UnsetReceiveHandler()` - Remove Event Handler

`UnsetReceiveHandler()` removes a previously registered receive handler set via `SetReceiveHandler()`.
Stops automatic callback invocations when new samples arrive.

<details>
<summary> Examples: </summary>

```cpp
// Remove registered receive handler
auto result = proxy->my_event.UnsetReceiveHandler();
if (result.has_value()) {
    auto samples = proxy->my_event.GetNewSamples(callback, 10);
}
```

**Key Points**:
- Removes handler registered with `SetReceiveHandler()`
- Safe to call even if no handler registered
- Returns `ResultBlank` - check for errors

</details>

---

#### Example 13: `IsBindingValid()` - Check Event Binding Health

`IsBindingValid()` checks if the event has a valid connection to the underlying communication binding.

<details>
<summary> Examples: </summary>

```cpp
// Check if event binding is valid before use
if (proxy->my_event.IsBindingValid()) {
    // Binding is valid - safe to use event operations
    auto result = proxy->my_event.Subscribe(10);
} else {
    // Binding is invalid - likely construction or move error
    LOG_ERROR("Event binding is invalid - cannot use event operations");
}
```

**Key Points**:
- Returns `true` if internal binding pointer is valid (not nullptr)
- Returns `false` indicates critical internal error
- Typically caused by failed move operation or incorrect construction
- Low-level diagnostic API - rarely needed in normal application code
- Check before using event operations if construction is uncertain

</details>

---

#### Example 14: `Unsubscribe()` - Cancel Event Subscription

`Unsubscribe()` cancels active event subscription and stops receiving new samples.

<details>
<summary> Examples: </summary>

```cpp
// Unsubscribe from event
proxy->my_event.Unsubscribe();
// Event no longer receives samples
```

**Key Points**:
- Stops receiving new event samples
- Clears buffered samples
- Removes any registered receive handler
- Safe to call even if not subscribed
- After unsubscribe, event behaves as newly constructed
- Synchronizes with running receive handlers before returning

</details>

---

#### Example 15: `Subscribe()` - Subscribe to Field Changes

see [#example-6-subscribe---subscribe-to-service-event](#example-6-subscribe---subscribe-to-service-event)

---

#### Example 16: `GetSubscriptionState()` - Check Field Subscription Status

see [#example-9-getsubscriptionstate---check-subscription-status](#example-9-getsubscriptionstate---check-subscription-status)

---

#### Example 17: `GetFreeSampleCount()` - Check Available Field Buffer Space

see [#example-10-getfreesamplecount---check-available-sample-buffer-space](#example-10-getfreesamplecount---check-available-sample-buffer-space)

---

#### Example 18: `GetNumNewSamplesAvailable()` - Check Pending Field Changes

see [#example-11-getnumnewsamplesavailable---check-pending-sample-count](#example-11-getnumnewsamplesavailable---check-pending-sample-count)

---

#### Example 19: `GetNewSamples()` - Retrieve Field Change Samples

see [#example-7-getnewsamples---retrieve-event-samples](#example-7-getnewsamples---retrieve-event-samples)

---

#### Example 20: `SetReceiveHandler()` - Register Field Change Handler

see [#example-8-setreceivehandler---register-event-notification-handler](#example-8-setreceivehandler---register-event-notification-handler)

---

#### Example 21: `UnsetReceiveHandler()` - Remove Field Change Handler

see [#example-12-unsetreceivehandler---remove-event-handler](#example-12-unsetreceivehandler---remove-event-handler)

---

#### Example 22: `Unsubscribe()` - Cancel Field Change Subscription

see [#example-14-unsubscribe---cancel-event-subscription](#example-14-unsubscribe---cancel-event-subscription)

> [!NOTE]
> **ProxyField `Get()` and `Set()` methods** are currently under development and will be added in a future update.

---

> [!NOTE]
> **ProxyMethod API documentation** for remote procedure call (RPC) functionality is currently under development and will be added in a future update.

---

### Production Example of Usage of Proxy API: [score/mw/com/performance_benchmarks/macro_benchmark/lola_benchmarking_client.cpp](../performance_benchmarks/macro_benchmark/lola_benchmarking_client.cpp)

---

## `Skeleton` APIs - Server-Side Service Implementation

The Skeleton side APIs enable service provider applications to implement and offer services in the mw::com framework.
Skeletons publish events and manage fields calls from clients.

### Skeleton

A skeleton class can be constructed from an Interface Template through the `AsSkeleton` template (see [`AsSkeleton`](#example-11-using-asskeleton-for-interface-type-interpretation)).

#### Example 1: `Create()` - Creating Skeleton Instance

`Create()` instantiates a skeleton for implementing a service instance.
Returns a `Result` containing the skeleton instance or an error if creation fails.

<details>
<summary> Examples: </summary>

```cpp
#include "generated_service_skeleton.h"  // Generated from service interface

// Create skeleton with explicit processing mode
auto spec = score::mw::com::InstanceSpecifier::Create("my/service/path");
if (spec.has_value()) {
    auto skeleton_result = MyServiceSkeleton::Create(spec.value());
    if (skeleton_result.has_value()) {
        auto skeleton = std::move(skeleton_result.value());
        // Skeleton created - can now offer service

    }
}
```

**Key Points**:
- Requires valid `InstanceSpecifier` from configuration
- Returns `Result<SkeletonWrapper>` - check with `has_value()`
- Skeleton lifetime determines service availability

</details>

---

#### Example 2: `OfferService()` - Make Service Available to Clients

`OfferService()` makes the service instance available for discovery and connection by proxy clients.
Must be called after skeleton creation to enable client communication.

<details>
<summary> Examples: </summary>

```cpp
#include "generated_service_skeleton.h"

// Create skeleton instance
auto spec = score::mw::com::InstanceSpecifier::Create("my/service/path");
auto skeleton_result = MyServiceSkeleton::Create(spec.value());

if (skeleton_result.has_value()) {
    auto skeleton = std::move(skeleton_result.value());

    // Offer the service - makes it discoverable by proxies
    auto offer_result = skeleton.OfferService();

    if (offer_result.has_value()) {
        // Service is now offered successfully
        // Proxies can discover and connect to this service

        // Skeleton can now send events and update fields
        // skeleton.my_event.Send(data);
        // skeleton.my_field.Update(value);
    } else {
        // Handle error - service could not be offered
        auto error = offer_result.error();
    }
}
```

**Key Points**:
- Call after skeleton creation to enable service discovery
- Returns `ResultBlank` - check with `has_value()` for success
- Service remains offered until `StopOfferService()` or skeleton destruction
- After offering, you can send events and update fields
- Proxies cannot discover the service after `OfferService()` succeeds

</details>

---

#### Example 3: `StopOfferService()` - Stop Service Availability

`StopOfferService()` stops offering the service, making it unavailable for client discovery.

<details>
<summary> Examples: </summary>

```cpp
#include "generated_service_skeleton.h"

void ManageServiceLifecycle() {
    auto spec = score::mw::com::InstanceSpecifier::Create("my/service/path");
    auto skeleton_result = MyServiceSkeleton::Create(spec.value());

    if (skeleton_result.has_value()) {
        auto skeleton = std::move(skeleton_result.value());

        // Offer service
        skeleton.OfferService();

        // Service is active - process events, update fields
        // ... application logic ...

        // Later, when shutting down or suspending service
        skeleton.StopOfferService();

        // Service is no longer discoverable by new clients
        // Existing connections may be terminated
    }
}
```

**Key Points**:
- Void return type - always succeeds
- Makes service unavailable for new client discovery
- Automatically called when skeleton is destroyed
- Skeleton can be re-offered by calling `OfferService()` again

</details>

---

#### Example 4: `Send()` - Send Event Data to Subscribers

`Send()` transmits event data to all subscribed proxy clients.
Supports two overloads: one for direct data sending (copy-based) and another for zero-copy using pre-allocated memory.

<details>
<summary> Examples: </summary>

```cpp
#include "generated_service_skeleton.h"

// Create and offer skeleton
auto spec = score::mw::com::InstanceSpecifier::Create("my/service/path");
auto skeleton_result = MyServiceSkeleton::Create(spec.value());

if (skeleton_result.has_value()) {
    auto skeleton = std::move(skeleton_result.value());
    skeleton.OfferService();

    // Method 1: Copy-based sending - simple approach
    MyDataType event_data;
    event_data.field1 = 42;
    event_data.field2 = "Hello";

    auto send_result = skeleton.my_event.Send(event_data);
    if (send_result.has_value()) {
        // Event sent successfully - data copied internally
    }

    // Method 2: Zero-copy sending - for performance-critical scenarios
    auto allocate_result = skeleton.my_event.Allocate();
    if (allocate_result.has_value()) {
        auto sample = std::move(allocate_result.value());

        // Fill the allocated memory
        sample->field1 = 100;
        sample->field2 = "Zero-Copy";

        // Send using zero-copy (memory ownership transferred)
        auto send_result = skeleton.my_event.Send(std::move(sample));
        if (send_result.has_value()) {
            // Event sent successfully without copying data
            // sample is now invalid (moved)
        }
    }
}
```

**Key Points**:
- Copy-based accepts `const EventType&` and zero-copy accepts `SampleAllocateePtr<EventType>`
- Zero-copy requires `Allocate()` first and uses less memory bandwidth
- Must call `OfferService()` before sending
- Returns `ResultBlank` - check with `has_value()`

</details>

---

#### Example 5: `Allocate()` - Allocate Memory for Event Data

`Allocate()` pre-allocates memory for event data before sending.
Use this method for zero-copy transmission in shared memory scenarios.

<details>
<summary> Examples: </summary>

```cpp
#include "generated_service_skeleton.h"

void SendEventWithZeroCopy() {
    auto skeleton = MyServiceSkeleton::Create(spec.value()).value();
    skeleton.OfferService();

    // Allocate memory for the event data
    auto allocate_result = skeleton.my_event.Allocate();

    if (allocate_result.has_value()) {
        auto sample = std::move(allocate_result.value());

        // sample is a SampleAllocateePtr<MyDataType>
        // Fill the data using pointer operators
        sample->field1 = 100;
        sample->field2 = "Zero-Copy Data";

        // Send the pre-allocated data
        skeleton.my_event.Send(std::move(sample));
    } else {
        // Handle allocation failure
        auto error = allocate_result.error();
    }
}
```

**Key Points**:
- Returns `Result<SampleAllocateePtr<EventType>>` - check with `has_value()`
- Allocates memory from shared memory pools for zero-copy transmission
- Must be called after `OfferService()` - shared memory not yet set up before offering
- Memory ownership transferred to `Send()` when transmitted

</details>

---

#### Example 6: `Update()` - Update Field Value and Notify Subscribers

`Update()` sets a new field value and notifies all subscribed proxy clients of the change.
Supports two overloads: one for direct value update (copy-based) and another for zero-copy using pre-allocated memory.

<details>
<summary> Examples: </summary>

```cpp
#include "generated_service_skeleton.h"

// Create skeleton
auto spec = score::mw::com::InstanceSpecifier::Create("my/service/path");
auto skeleton_result = MyServiceSkeleton::Create(spec.value());

if (skeleton_result.has_value()) {
    auto skeleton = std::move(skeleton_result.value());

    // Set initial field value BEFORE offering service
    MyFieldType initial_value;
    initial_value.temperature = 20.0;
    initial_value.pressure = 100.0;

    auto set_result = skeleton.my_field.Update(initial_value);
    if (!set_result.has_value()) {
        // Handle error - initial value must be set
        return;
    }

    // Offer service
    skeleton.OfferService();

    // Method 1: Copy-based update - simple approach
    MyFieldType new_value;
    new_value.temperature = 25.5;
    new_value.pressure = 101.3;

    auto update_result = skeleton.my_field.Update(new_value);
    if (update_result.has_value()) {
        // Field updated successfully - data copied internally
    }

    // Method 2: Zero-copy update - for performance-critical scenarios
    auto allocate_result = skeleton.my_field.Allocate();
    if (allocate_result.has_value()) {
        auto sample = std::move(allocate_result.value());

        // Fill the allocated memory
        sample->temperature = 30.0;
        sample->pressure = 105.0;

        // Update field using zero-copy (memory ownership transferred)
        auto update_result = skeleton.my_field.Update(std::move(sample));
        if (update_result.has_value()) {
            // Field updated successfully without copying data
            // sample is now invalid (moved)
        }
    }
}
```

**Key Points**:
- Initial field value must be set before `OfferService()` is called
- Copy-based accepts `const FieldType&` and zero-copy accepts `SampleAllocateePtr<FieldType>`
- Zero-copy requires `Allocate()` first and uses less memory bandwidth
- Returns `ResultBlank` - check with `has_value()`

</details>

---

#### Example 7: `Allocate()` - Allocate Memory for Zero-Copy Field Update

see [#example-5-allocate---allocate-memory-for-event-data](#example-5-allocate---allocate-memory-for-event-data)

---

> [!NOTE]
> **SkeletonMethod API documentation** for implementing remote procedure call (RPC) handlers is currently under development and will be added in a future update.

---

### Production Example of Usage of Skeleton API: [score/mw/com/performance_benchmarks/macro_benchmark/lola_benchmarking_service.cpp](../performance_benchmarks/macro_benchmark/lola_benchmarking_service.cpp)

---
