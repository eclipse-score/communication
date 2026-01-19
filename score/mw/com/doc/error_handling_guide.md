# Error Handling Guide

This document provides comprehensive information about error codes, messages, and recovery strategies for all mw::com public APIs.

## Error Classification

- **Recoverable**: Error can be resolved through retry, re-subscription, or alternative approaches
- **Potentially Recoverable**: May be recoverable depending on context and timing
- **Non-Recoverable**: Indicates configuration, programming, or infrastructure issues that cannot be resolved at runtime

---

## Public API Error Analysis

### Runtime APIs

<details>
<summary><b><code>score::mw::com::runtime::ResolveInstanceIDs()</code></b></summary>

| Error | Error Description | Error Handling |
|-------|------------------|----------------|
| `kInstanceIDCouldNotBeResolved` | Runtime could not resolve a valid InstanceIdentifier from the provided InstanceSpecifier. | **Non-Recoverable**: Configuration error in service manifest. Service not defined in JSON configuration file. <br> **Fix**: Update manifest configuration - this is a deployment issue, not runtime issue. |

</details>

---

### Instance Identifier APIs

<details>
<summary><b><code>score::mw::com::InstanceIdentifier::Create()</code></b></summary>

| Error | Error Description | Error Handling |
|-------|------------------|----------------|
| `kInvalidConfiguration` | InstanceIdentifier configuration pointer hasn't been set. | **Non-Recoverable**: Runtime not initialized before calling InstanceIdentifier::Create(). <br> **What happened**: Application called InstanceIdentifier::Create() before initializing the COM runtime. <br> **Fix**: <br> • Initialize runtime explicitly by calling `score::mw::com::runtime::Initialize()` before using InstanceIdentifier::Create() <br> • OR: Initialize runtime implicitly by creating a Proxy or Skeleton first (which triggers runtime initialization) <br> • Ensure runtime initialization happens during application startup, before any COM API usage |
| `kInvalidInstanceIdentifierString` | InstanceIdentifier serialized string is invalid. | **Non-Recoverable**: Corrupted serialized data - system must terminate. <br> **Fix**: Validate JSON format before calling Create(), retry with corrected serialized string. |

</details>

---

### Instance Specifier APIs

<details>
<summary><b><code>score::mw::com::InstanceSpecifier::Create()</code></b></summary>

| Error | Error Description | Error Handling |
|-------|------------------|----------------|
| `kInvalidMetaModelShortname` | Meta model short name does not adhere to naming requirements. | **Non-Recoverable**: Programming error in shortname format. <br> **Fix**: Correct shortname path to follow AUTOSAR conventions (e.g., "/ServiceName/InstanceName"). |

</details>

---

### Proxy APIs

<a id="bmwmwcomimplproxywrapperclasscreate"></a>
<details>
<summary><b><code>score::mw::com::impl::ProxyWrapperClass::Create()</code></b></summary>

| Error | Error Description | Error Handling |
|-------|------------------|----------------|
| `kBindingFailure` (timing issue) | Service was found but disappeared before proxy could connect to it. Temporary lock conflict prevented binding. | **Sometimes Recoverable** (~10% of cases): <br> **What happened**: Service went away between discovery and connection attempt. <br> **Fix**: <br> • Wait for FindService callback to trigger again <br> • Retry Create() when service reappears <br> • This is normal during service restarts |
| `kBindingFailure` (partial restart failure) | Transaction log rollback failed during partial restart logic execution. | **Not Recoverable**: Partial restart mechanism failure. <br> **What happened**: Failed to roll back transaction logs during proxy initialization after service restart. <br> **Fix**: <br> • Check logs for specific error message: "Could not create Proxy: Rolling back transaction log failed" <br> • Verify shared memory integrity and access permissions <br> • Check that skeleton process (provider) is running correctly <br> • Restart both proxy and skeleton applications <br> • If problem persists, check system logs for shared memory corruption |
| `kBindingFailure` (configuration problem) | Service ID missing, wrong deployment settings, or system ran out of resources. | **Not Recoverable** (~90% of cases): <br> **What happened**: Configuration error or system problem. Cannot tell exact cause from error code alone. <br> **Fix**: <br> • Check logs for specific error message <br> • If log says "Event provided in ServiceTypeDeployment could not be found in shared memory": Fix deployment JSON file <br> • If log says "lola service instance id does not exist": Fix service configuration <br> • If resource errors: Check system has enough memory and resources <br> • Correct configuration files <br> • Restart application <br> **Note**: Code cannot detect if error is recoverable - assume not recoverable unless service comes back via FindService. |

</details>

---

<a id="bmwmwcomimplproxywrapperclassfindservice"></a>
<details>
<summary><b><code>score::mw::com::impl::ProxyWrapperClass::FindService()</code></b></summary>

| Error | Error Description | Error Handling |
|-------|------------------|----------------|
| `kBindingFailure` | Binding layer failed during service discovery. | **Not Recoverable**: Infrastructure problem at binding layer. <br> **What happened**: <br> • Service discovery client failed, OR <br> • Communication infrastructure error, OR <br> • System resource exhaustion <br> **Fix**: <br> • Check system logs for binding errors <br> • Verify service discovery is running <br> • Restart application <br> **Note**: When no services are found, FindService() returns success with empty container, not this error. |

</details>

---

<a id="bmwmwcomimplproxywrapperclassstartfindservice"></a>
<details>
<summary><b><code>score::mw::com::impl::ProxyWrapperClass::StartFindService()</code></b></summary>

| Error | Error Description | Error Handling |
|-------|------------------|----------------|
| `kFindServiceHandlerFailure` | Failed to register service discovery handler. System cannot monitor filesystem for service changes. | **Not Recoverable**: Permanent failure. Calling again will also fail. <br> **What happened**: <br> • Application lacks filesystem access permissions, OR <br> • System ran out of resources (file descriptors, memory, inotify watches) <br> **Fix**: <br> • Check logs to identify specific cause <br> • If permission error: Fix application access rights to service discovery paths (restart alone won't help) <br> • If resource error: <br> &nbsp;&nbsp;- Check limits: `ulimit -a` and `/proc/sys/fs/inotify/max_user_watches` <br> &nbsp;&nbsp;- Increase system limits or restart to free resources |

</details>

---

<a id="bmwmwcomimplproxywrapperclassstopfindservice"></a>
<details>
<summary><b><code>score::mw::com::impl::ProxyWrapperClass::StopFindService()</code></b></summary>

| Error | Error Description | Error Handling |
|-------|------------------|----------------|
| `kInvalidHandle` | The provided FindServiceHandle is not valid or does not exist. | **Not Recoverable**: Programming error. <br> **What happened**: <br> • Handle was never created (StartFindService failed), OR <br> • Handle already used in previous StopFindService call, OR <br> • Handle is corrupted or uninitialized <br> **Fix**: <br> • Verify StartFindService succeeded and handle was stored correctly <br> • Ensure StopFindService is only called once per handle <br> • Check handle is not used after being stopped <br> • Review application logic for handle lifetime management |

</details>

---

<a id="bmwmwcomimplproxyeventbasesubscribe"></a>
<details>
<summary><b><code>score::mw::com::impl::ProxyEventBase::Subscribe()</code></b></summary>

| Error | Error Description | Error Handling |
|-------|------------------|----------------|
| `kBindingFailure` | Cannot subscribe to event. Binding resources unavailable or skeleton service not ready. | **Potentially Recoverable**: Service or resources not ready. <br> **What happened**: <br> • Skeleton service hasn't offered the event yet, OR <br> • Shared memory not configured correctly, OR <br> • Temporary resource unavailability <br> **Fix**: <br> • Retry with appropriate delay <br> • Verify skeleton service is running and offering events <br> • Check shared memory configuration matches deployment <br> • If retries fail consistently, check deployment configuration |
| `kMaxSampleCountNotRealizable` | Requested buffer size exceeds what the system can provide. | **Recoverable**: Buffer size too large. <br> **What happened**: Application requested more sample slots than available. <br> **Fix**: <br> • Reduce `max_sample_count` parameter in Subscribe() call <br> • Check skeleton event buffer configuration <br> • Alternatively: Increase event buffer size in deployment configuration if application needs larger buffer |

</details>

---

<a id="bmwmwcomimplproxyeventbasegetnumnewsamplesavailable"></a>
<details>
<summary><b><code>score::mw::com::impl::ProxyEventBase::GetNumNewSamplesAvailable()</code></b></summary>

| Error | Error Description | Error Handling |
|-------|------------------|----------------|
| `kNotSubscribed` | Cannot check sample count because event is not subscribed. | **Recoverable**: Programming error - Subscribe() not called. <br> **What happened**: Trying to check available samples without subscribing first. <br> **Fix**: <br> • Call `proxy_event.Subscribe(buffer_size)` before calling GetNumNewSamplesAvailable() <br> • Use `GetSubscriptionState()` to verify subscription status <br> • Fix application logic to ensure Subscribe() is called during initialization |
| `kBindingFailure` | Cannot get sample count due to binding layer failure. | **Mostly Not Recoverable** (~90% of cases): <br> **What happened**: Generic error from binding layer - could be temporary or permanent. <br> **Fix**: <br> • Retry with brief delay (may be transient communication issue) <br> • If multiple retries fail, assume permanent failure <br> • Restart process and check system logs for: <br> &nbsp;&nbsp;- Shared memory errors or corruption <br> &nbsp;&nbsp;- IPC mechanism failures <br> &nbsp;&nbsp;- Memory exhaustion <br> |

</details>

---

<a id="bmwmwcomimplproxyeventsetreceivehandler"></a>
<details>
<summary><b><code>score::mw::com::impl::ProxyEvent::SetReceiveHandler()</code></b></summary>

| Error | Error Description | Error Handling |
|-------|------------------|----------------|
| `kSetHandlerNotSet` | Failed to register receive handler in binding layer. | **Not Recoverable**: Binding layer rejected handler registration. <br> **What happened**: <br> • Binding infrastructure problem, OR <br> • Resource exhaustion, OR <br> • Internal binding error <br> **Fix**: <br> • Check system logs for binding layer errors <br> • Verify system has sufficient resources <br> • Restart process <br> • If problem persists, check deployment configuration |

</details>

---

<a id="bmwmwcomimplproxyeventunsetreceivehandler"></a>
<details>
<summary><b><code>score::mw::com::impl::ProxyEvent::UnsetReceiveHandler()</code></b></summary>

| Error | Error Description | Error Handling |
|-------|------------------|----------------|
| `kUnsetFailure` | Failed to unregister receive handler from binding layer. | **Not Recoverable**: Binding layer rejected handler removal. <br> **What happened**: <br> • Binding infrastructure problem, OR <br> • Handler was already removed, OR <br> • Internal binding error <br> **Fix**: <br> • Check system logs for binding layer errors <br> • Application can continue but handler may remain registered <br> • Restart process to clean up state <br> **Note**: If no handler was registered, this call succeeds silently (no error) |

</details>

---

<a id="bmwmwcomimplproxyeventgetnewsamples"></a>
<details>
<summary><b><code>score::mw::com::impl::ProxyEvent::GetNewSamples()</code></b></summary>

| Error | Error Description | Error Handling |
|-------|------------------|----------------|
| `kMaxSamplesReached` | Application already holds maximum number of `SamplePtr` objects. All sample slots currently in use. | **Recoverable**: Too many samples held simultaneously. <br> **What happened**: Application holds `max_sample_count` `SamplePtr` objects without releasing them. All buffer slots occupied. <br> **Fix**: <br> • Release held `SamplePtr` objects by letting them go out of scope <br> • Process samples quickly without holding references too long <br> • If application needs more concurrent samples: Increase `max_sample_count` in Subscribe() call. |
| `kNotSubscribed` | Cannot get samples because event is not subscribed. | **Recoverable**: Programming error - Subscribe() not called. <br> **What happened**: Trying to retrieve samples without subscribing first. <br> **Fix**: <br> • Call `proxy_event.Subscribe(buffer_size)` before GetNewSamples() <br> • Use `GetSubscriptionState()` to verify subscription status <br> • Fix application logic to ensure Subscribe() during initialization |
| `kBindingFailure` | Cannot get samples due to binding layer failure. | **Mostly Not Recoverable** (~90% of cases): <br> **What happened**: Generic binding error - could be temporary or permanent. <br> **Fix**: <br> • Retry with brief delay (may be transient) <br> • If multiple retries fail, assume permanent failure <br> • Restart process and check system logs for: <br> &nbsp;&nbsp;- Shared memory corruption or access errors <br> &nbsp;&nbsp;- IPC mechanism failures <br> &nbsp;&nbsp;- Memory exhaustion or resource limits <br> &nbsp;&nbsp;- Binding infrastructure problems |

</details>

---

<a id="bmwmwcomimplproxyfieldbasesubscribe"></a>
<details>
<summary><b><code>score::mw::com::impl::ProxyFieldBase::Subscribe()</code></b></summary>

see [`ProxyEventBase::Subscribe()`](#scoremwcomimplproxyeventbasesubscribe)

</details>

---

<a id="bmwmwcomimplproxyfieldbasegetnumnewsamplesavailable"></a>
<details>
<summary><b><code>score::mw::com::impl::ProxyFieldBase::GetNumNewSamplesAvailable()</code></b></summary>

see [`ProxyEventBase::GetNumNewSamplesAvailable()`](#scoremwcomimplproxyeventbasegetnumnewsamplesavailable)

</details>

---

<a id="bmwmwcomimplproxyfieldsetreceivehandler"></a>
<details>
<summary><b><code>score::mw::com::impl::ProxyField::SetReceiveHandler()</code></b></summary>

see [`ProxyEvent::SetReceiveHandler()`](#scoremwcomimplproxyeventsetreceivehandler)

</details>

---

<a id="bmwmwcomimplproxyfieldunsetreceivehandler"></a>
<details>
<summary><b><code>score::mw::com::impl::ProxyField::UnsetReceiveHandler()</code></b></summary>

see [`ProxyEvent::UnsetReceiveHandler()`](#scoremwcomimplproxyeventunsetreceivehandler)

</details>

---

<a id="bmwmwcomimplproxyfieldgetnewsamples"></a>
<details>
<summary><b><code>score::mw::com::impl::ProxyField::GetNewSamples()</code></b></summary>

see [`ProxyEvent::GetNewSamples()`](#scoremwcomimplproxyeventgetnewsamples)

</details>

---

### Skeleton APIs

<details>
<summary><b><code>score::mw::com::impl::SkeletonWrapperClass::Create()</code></b></summary>

| Error | Error Description | Error Handling |
|-------|------------------|----------------|
| `kBindingFailure` | Cannot create skeleton binding or service element bindings. | **Not Recoverable**: Binding infrastructure error. <br> **What happened**: <br> • Skeleton binding creation failed, OR <br> • Service element bindings could not be created, OR <br> • Shared memory initialization failed, OR <br> • Resource exhaustion <br> **Fix**: <br> • Check logs for specific error message <br> • Verify deployment configuration is correct <br> • Check shared memory is properly configured <br> • Ensure system has sufficient resources (memory, file descriptors) <br> • Correct configuration files and restart application |

</details>

---

<details>
<summary><b><code>score::mw::com::impl::SkeletonWrapperClass::OfferService()</code></b></summary>

| Error | Error Description | Error Handling |
|-------|------------------|----------------|
| `kBindingFailure` | Cannot offer service to network. Binding layer failed to prepare service offering. | **Not Recoverable**: Infrastructure or configuration error. <br> **What happened**: <br> • Event/field binding preparation failed, OR <br> • Service registration failed, OR <br> • Shared memory setup failed <br> **Fix**: <br> • Check logs for specific error <br> • Verify deployment configuration <br> • Restart application |
| `kFieldValueIsNotValid` | Cannot offer service because field has no value set. | **Recoverable**: Field not initialized. <br> **What happened**: Field value not set before offering service. <br> **Fix**: <br> • Set valid initial value for all fields before calling OfferService() <br> • Use field setter methods to initialize field values |

</details>

---

<a id="bmwmwcomimplskeletoneventsend"></a>
<details>
<summary><b><code>score::mw::com::impl::SkeletonEvent::Send()</code></b></summary>

| Error | Error Description | Error Handling |
|-------|------------------|----------------|
| `kNotOffered` | Cannot send event because service not offered yet or has been stopped. | **Recoverable**: Service not offered. <br> **What happened**: Trying to send event before calling OfferService() or after StopOfferService(). <br> **Fix**: <br> • Call OfferService() before sending events <br> • Don't send events after StopOfferService() <br> • Check service offering state before sending |
| `kBindingFailure` | Cannot send event due to binding layer failure. | **Not Recoverable** (~90% of cases): <br> **What happened**: <br> • Shared memory full or corrupted, OR <br> • No subscribers connected, OR <br> • IPC mechanism failure, OR <br> • Resource exhaustion <br> **Fix**: <br> • Check logs for specific error <br> • Verify subscribers are properly connected <br> • Check shared memory configuration <br> • Restart application if problem persists |

</details>

---

<a id="bmwmwcomimplskeletoneventallocate"></a>
<details>
<summary><b><code>score::mw::com::impl::SkeletonEvent::Allocate()</code></b></summary>

| Error | Error Description | Error Handling |
|-------|------------------|----------------|
| `kNotOffered` | Cannot allocate sample because service not offered yet or has been stopped. | **Recoverable**: Service not offered. <br> **What happened**: Trying to allocate sample before calling OfferService() or after StopOfferService(). <br> **Fix**: <br> • Call OfferService() before allocating samples <br> • Don't allocate after StopOfferService() <br> • Check service offering state before allocating |
| `kBindingFailure` | Cannot allocate sample due to binding layer failure. | **Not Recoverable**: <br> **What happened**: <br> • Shared memory exhausted, OR <br> • Resource limits reached, OR <br> • Binding infrastructure problem <br> **Fix**: <br> • Check logs for specific error <br> • Verify shared memory has sufficient space <br> • Check system resource limits <br> • Restart application |

</details>

---

<details>
<summary><b><code>score::mw::com::impl::SkeletonField::Update()</code></b></summary>

see [`SkeletonEvent::Send()`](#scoremwcomimplskeletoneventsend)

</details>

---

<details>
<summary><b><code>score::mw::com::impl::SkeletonField::Allocate()</code></b></summary>

see [`SkeletonEvent::Allocate()`](#scoremwcomimplskeletoneventallocate)

</details>

---

### Generic Proxy APIs

<details>
<summary><b><code>score::mw::com::impl::GenericProxy::Create()</code></b></summary>

see [`ProxyWrapperClass::Create()`](#scoremwcomimplproxywrapperclasscreate)

</details>

---

<details>
<summary><b><code>score::mw::com::impl::GenericProxyEvent::GetNewSamples()</code></b></summary>

see [`ProxyEvent::GetNewSamples()`](#scoremwcomimplproxyeventgetnewsamples)

</details>


---

> [!Note]
> `GenericProxy` inherits from `ProxyBase`. `GenericProxyEvent` inherits from `ProxyEventBase`. For error handling of inherited methods, see:
> • `ProxyWrapperClass` APIs: [FindService()](#scoremwcomimplproxywrapperclassfindservice), [StartFindService()](#scoremwcomimplproxywrapperclassstartfindservice), [StopFindService()](#scoremwcomimplproxywrapperclassstopfindservice)
> • `ProxyEventBase` APIs: [Subscribe()](#scoremwcomimplproxyeventbasesubscribe), [GetNumNewSamplesAvailable()](#scoremwcomimplproxyeventbasegetnumnewsamplesavailable)

---
