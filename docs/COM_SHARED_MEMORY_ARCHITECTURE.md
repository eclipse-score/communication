# COM Shared Memory Architecture - Complete Guide

**Date:** July 6, 2026
**Topics:** Field Set API, Shared Memory, Zero-Copy Communication, Events vs Fields

---

## Table of Contents
1. [Overview](#overview)
2. [Set API Flow - Basic to In-Depth](#set-api-flow---basic-to-in-depth)
3. [Queue Architecture in Shared Memory](#queue-architecture-in-shared-memory)
4. [Shared Memory Creation & Ownership](#shared-memory-creation--ownership)
5. [Field Architecture - Dual Memory Model](#field-architecture---dual-memory-model)
6. [Events vs Field Notifiers Memory Separation](#events-vs-field-notifiers-memory-separation)
7. [Concurrency and Thread-Safety Model](#concurrency-and-thread-safety-model)
8. [Complete Flow Examples](#complete-flow-examples)
9. [Key Takeaways](#key-takeaways)

---

## Overview

The SCORE middleware uses a **zero-copy architecture** based on shared memory for efficient inter-process communication (IPC). Data values never move through IPC channels - only **pointers** and **offsets** are exchanged.

### Core Principle
```
Proxy writes → Shared Memory ← Skeleton reads
      ↓                              ↓
  Only pointer passed via IPC    Only pointer used
```

---

## Set API Flow - Basic to In-Depth

### Basic Level: What is Set API?

The `Set` API in `proxy_field.h:309-313` allows a **Proxy (client)** to set a new value for a **Field** on the **Skeleton (server)** side.

```cpp
score::Result<MethodReturnTypePtr<T>> Set(const SampleDataType& new_field_value) noexcept
{
    return proxy_method_set_dispatch_->operator()(new_field_value);
}
```

**Key Concepts:**
- **Field**: Service element with getter, setter, and/or notifier capabilities
- **Proxy**: Client-side representation of a service
- **Skeleton**: Server-side implementation of a service
- `Set()` only available when field created with `WithSetter` tag

---

### Intermediate Level: Call Flow Overview

```
User Code
   ↓
ProxyFieldImpl::Set()
   ↓
ProxyMethod<FieldType(FieldType)>::operator()
   ↓
ProxyMethodBinding::DoCall()
   ↓
Message Passing Layer (IPC)
   ↓
SkeletonMethod Handler
   ↓
Actual Service Implementation
   ↓
Value Written to Memory
```

---

### Advanced Level: Detailed Component Interaction

#### 1. Initialization Phase (Proxy Construction)

At `proxy_field.h:267-281`, when a field with `WithSetter` is created:

```cpp
static std::unique_ptr<ProxyMethod<FieldType(FieldType)>> MakeSetMethodDispatchIfEnabled(...)
{
    if constexpr (kHasSetter)
    {
        return std::make_unique<ProxyMethod<FieldType(FieldType)>>(
            proxy_base,
            field_name,
            ProxyFieldBindingFactory<FieldType>::CreateSetMethodBinding(proxy_base, field_name),
            typename ProxyMethod<FieldType(FieldType)>::FieldOnlyConstructorEnabler{});
    }
}
```

Creates:
- **ProxyMethod** instance stored in `proxy_method_set_dispatch_`
- **ProxyMethodBinding** created by factory for actual transport

#### 2. Call Invocation Phase

**Step 1:** Entry point - `proxy_field.h:309-313`
```cpp
proxy_method_set_dispatch_->operator()(new_field_value)
```

**Step 2:** ProxyMethod operator() - `proxy_method_with_in_args_and_return.h:174-189`
1. Calls `Allocate()` to get storage for arguments and return value
2. Copies argument values into allocated storage
3. Calls zero-copy variant

**Step 3:** Zero-copy operator() - `proxy_method_with_in_args_and_return.h:191-211`
```cpp
auto queue_position = detail::GetCommonQueuePosition(args...);
auto allocated_return_type_storage = binding_->GetReturnValueBuffer(queue_position);
auto call_result = binding_->DoCall(queue_position);
```

Gets queue position, allocates return buffer, invokes `DoCall()`.

---

### In-Depth Level: Implementation Details

#### 3. Binding Layer - LoLa ProxyMethod

**Step 4:** ProxyMethod::DoCall() - `proxy_method.cpp:86-95`

```cpp
score::Result<void> ProxyMethod::DoCall(std::size_t queue_position)
{
    if (!is_subscribed_)
        return MakeUnexpected(ComErrc::kBindingFailure);

    auto& lola_message_passing = lola_runtime_.GetLolaMessaging();
    return lola_message_passing.CallMethod(
        asil_level_, proxy_method_instance_identifier_, queue_position, proxy_.GetSourcePid());
}
```

**Key Points:**
- Checks subscription status
- Calls LoLa message passing service
- Passes `queue_position` (identifies where in shared memory)

#### 4. Message Passing Layer

**Step 5:** MessagePassingServiceInstance::CallMethod() - `message_passing_service_instance.cpp:1580-1605`

```cpp
Result<void> MessagePassingServiceInstance::CallMethod(...)
{
    const auto are_skeleton_and_proxy_in_same_process = (target_node_id == self_pid_);
    if (are_skeleton_and_proxy_in_same_process)
        return CallServiceMethodLocally(...);
    else
        return CallServiceMethodRemotely(...);
}
```

**Two Paths:**

##### A. Local Call (Same Process)

**Step 6a:** CallServiceMethodLocally() - `message_passing_service_instance.cpp:719-759`

```cpp
score::Result<void> CallServiceMethodLocally(
    const ProxyMethodInstanceIdentifier& proxy_method_instance_identifier,
    const std::size_t queue_position,
    const uid_t proxy_uid)
{
    // Find registered handler under lock
    std::shared_lock<std::shared_mutex> read_lock{call_method_handlers_mutex_};
    auto method_call_handler_it = call_method_handlers_.find(proxy_method_instance_identifier);
    auto [method_call_handler_copy, allowed_proxy_uid] = method_call_handler_it->second;
    read_lock.unlock();

    // Invoke handler with queue_position
    auto invocation_result = std::invoke(method_call_handler_copy, queue_position);
    return {};
}
```

**Process:**
1. Looks up registered `MethodCallHandler`
2. Validates proxy UID (security)
3. Invokes handler with `queue_position`
4. Handler reads from shared memory at that position
5. Calls actual skeleton method
6. Writes return value to shared memory

##### B. Remote Call (Different Process)

**Step 6b:** CallServiceMethodRemotely() - `message_passing_service_instance.cpp:825-858`

```cpp
Result<void> CallServiceMethodRemotely(...)
{
    const MethodCallUnserializedPayload unserialized_payload{
        proxy_method_instance_identifier, queue_position};
    const auto message = SerializeToMessage(
        score::cpp::to_underlying(MessageWithReplyType::kCallMethod),
        unserialized_payload);
    auto sender = client_cache_.GetMessagePassingClient(target_node_id);

    const auto send_wait_reply_result = sender->SendWaitReply(message, reply_buffer);
    return {};
}
```

**Process:**
1. Serializes identifier + queue_position into message
2. Sends via IPC to skeleton process
3. Skeleton receives via `HandleCallMethodMsg()`
4. Skeleton calls `CallServiceMethodLocally()` on its side
5. Skeleton reads arguments from shared memory
6. Executes handler
7. Writes return value to shared memory
8. Sends reply back
9. Proxy reads return value

---

### Memory Management: Where Data is Written

#### Shared Memory Architecture

**Buffer Allocation:**

- **InArgs Buffer:** `proxy_method.cpp:51-67`
  ```cpp
  GetInArgsBuffer(queue_position)  // Returns span<byte> to shared memory
  ```

- **Return Value Buffer:** `proxy_method.cpp:69-84`
  ```cpp
  GetReturnValueBuffer(queue_position)  // Returns span<byte> for return value
  ```

**Memory Flow:**

1. **Proxy writes argument** → Shared memory (at queue_position offset for InArgs)
2. **Skeleton reads argument** ← Same shared memory (mapped to skeleton's address space)
3. **Skeleton writes return** → Same shared memory (at queue_position offset for Return)
4. **Proxy reads return** ← Same shared memory

**Type Erasure:**
- Binding layer uses `TypeErasedCallQueue` and `DataTypeSizeInfo`
- Only size and alignment information passed through binding
- `reinterpret_cast` used to cast back from `std::byte*` to `FieldType*`

---

### Complete Call Chain Summary

```
User: field.Set(value)
  ↓
ProxyFieldImpl::Set(value)                     [proxy_field.h:311]
  ↓
ProxyMethod::operator()(value)                 [proxy_method_with_in_args_and_return.h:174]
  ├→ Allocate() - get InArgs & Return buffers
  ├→ Copy value to InArgs buffer (shared memory)
  └→ ProxyMethod::operator()(MethodInArgPtr)   [proxy_method_with_in_args_and_return.h:191]
      ↓
      binding_->GetReturnValueBuffer(queue_pos)
      binding_->DoCall(queue_pos)
        ↓
        lola::ProxyMethod::DoCall()             [proxy_method.cpp:86]
          ↓
          MessagePassingService::CallMethod()   [message_passing_service.cpp:239]
            ↓
            MessagePassingServiceInstance::CallMethod()
              ↓
              ┌─────────────────────────────────────┐
              │ Same Process?                       │
              ├─────────────────────────────────────┤
              │ YES → CallServiceMethodLocally()    │
              │   - Direct handler invocation       │
              │   - Reads from shared memory        │
              │   - Calls skeleton handler          │
              │   - Writes return to shared memory  │
              │                                     │
              │ NO → CallServiceMethodRemotely()    │
              │   - Serialize message               │
              │   - IPC via message passing         │
              │   - Skeleton receives message       │
              │   - Skeleton calls handler locally  │
              │   - Skeleton writes to shared mem   │
              │   - Reply sent back via IPC         │
              └─────────────────────────────────────┘
                ↓
              SkeletonMethod handler executes
                ↓
              User's skeleton method implementation runs
                ↓
              Return value written to shared memory
                ↓
              Proxy reads return value from shared memory
                ↓
              Returns MethodReturnTypePtr<FieldType>
```

---

## Queue Architecture in Shared Memory

### Current State: Queue Size = 1

From `proxy_method_base.h:66`:

```cpp
/// Size of the call-queue is currently fixed to 1!
static constexpr containers::DynamicArray<int>::size_type kCallQueueSize = 1U;
```

**Current Reality:** Only **1 concurrent call** supported per method.

### Why "Queue" Architecture?

Designed to support **concurrent method calls** in the future:

```
Queue Position 0: [InArgs Buffer] [Return Buffer]
Queue Position 1: [InArgs Buffer] [Return Buffer]  ← Future
Queue Position 2: [InArgs Buffer] [Return Buffer]  ← Future
```

**Benefits:**
1. **Concurrent calls**: Multiple threads can call same method simultaneously
2. **Isolation**: Each call has its own buffer space (no data corruption)
3. **Configurable**: Queue size could come from configuration

**Current Limitation:** With queue_size=1, calling a method before previous call completes overwrites data.

---

## Shared Memory Creation & Ownership

### Two Types of Shared Memory

| Memory Type | **Method/Field SHM** | **Event/Field Notifier SHM** |
|-------------|---------------------|------------------------------|
| **Creator** | **PROXY** | **SKELETON** |
| **Opener** | SKELETON | PROXY |
| **Purpose** | Method calls (Get/Set) | Event/Field notifications |
| **Lifecycle** | Lives with Proxy | Lives with Skeleton |
| **Direction** | Bidirectional | Unidirectional (Skeleton → Proxy) |

---

### Method Shared Memory (Proxy Creates)

#### Step A: Proxy Creates

From `proxy.cpp:680-698`:

```cpp
// Proxy::SetupMethods() - called when proxy is created
method_shm_resource_ = memory::shared::SharedMemoryFactory::CreateOrOpen(
    [this, &enabled_method_data, &type_erased_element_infos](
        std::shared_ptr<score::memory::shared::ManagedMemoryResource> memory) {
        InitializeSharedMemoryForMethods(*memory, enabled_method_data, type_erased_element_infos);
    },
    required_shm_size,
    skeleton_shm_permissions);  // ← Gives skeleton write permission

// Notify skeleton via IPC
lola_message_passing.SubscribeServiceMethod(
    quality_type_, skeleton_instance_identifier, proxy_instance_identifier_, GetSourcePid());
```

**Proxy Actions:**
1. Calculates required size for all enabled methods
2. Creates shared memory with unique name
3. Allocates `TypeErasedCallQueue` for each method
4. Sets permissions for skeleton read/write access
5. Sends IPC: "Hey skeleton, I created SHM at `/dev/shm/XYZ`"

#### Step B: Skeleton Opens

From `skeleton.cpp:687-702`:

```cpp
Result<void> Skeleton::OnServiceMethodsSubscribed(
    const ProxyInstanceIdentifier& proxy_instance_identifier, ...)
{
    // Skeleton receives SubscribeServiceMethod IPC
    const auto method_channel_shm_name =
        shm_path_builder_->GetMethodChannelShmName(lola_instance_id_, proxy_instance_identifier);

    const bool is_read_write{true};  // ← Skeleton needs write access

    // Open existing shared memory
    auto opened_shm_region = memory::shared::SharedMemoryFactory::Open(
        method_channel_shm_name, is_read_write, allowed_providers);

    // Access MethodData structures
    auto& method_data = GetMethodData(*(resource_it->second));

    // Register handlers
    SubscribeMethods(method_data, proxy_instance_identifier, ...);
}
```

**Skeleton Actions:**
1. Receives IPC notification from proxy
2. Opens existing shared memory (doesn't create new)
3. Gets pointers to `TypeErasedCallQueue` structures
4. Registers method call handlers with buffer pointers
5. Both sides now access same physical memory

---

### Permission Model

```
/dev/shm/score_com_<service>_<instance>_<proxy_id>
Permissions: Owner (proxy): RW, Skeleton UID: RW
```

| Process | Read | Write | Why Write? |
|---------|------|-------|------------|
| **Proxy** | ✅ | ✅ | Writes InArgs, Reads Return values |
| **Skeleton** | ✅ | ✅ | Reads InArgs, Writes Return values |

---

### Complete Lifecycle Example

```
1. Proxy Created:
   ├─ Calculates: Need 256 bytes (InArgs=128, Return=128)
   ├─ Creates: /dev/shm/score_method_svc1_inst2_proxy7
   ├─ Allocates: TypeErasedCallQueue (queue_size=1)
   └─ Sends IPC: "SubscribeServiceMethod(proxy_id=7)"

2. Skeleton Receives:
   ├─ Opens: /dev/shm/score_method_svc1_inst2_proxy7
   ├─ Gets pointers: in_args_storage, return_storage
   ├─ Registers: "When proxy_id=7 calls, use these buffers"
   └─ Sends reply: "OK, subscribed"

3. User Calls field.Set(42):
   ├─ Proxy writes 42 → in_args_storage[0]
   ├─ Proxy sends IPC: "CallMethod(queue_position=0)"
   ├─ Skeleton reads: value = in_args_storage[0]  // 42
   ├─ Skeleton executes: user_handler(42)
   ├─ Skeleton writes result → return_storage[0]
   ├─ Sends IPC reply: "Done"
   └─ Proxy reads: return_value = return_storage[0]

4. Proxy Destroyed:
   ├─ Sends IPC: "UnsubscribeServiceMethod"
   ├─ Unlinks: /dev/shm/score_method_svc1_inst2_proxy7
   └─ Skeleton closes mapping and cleans up
```

---

## Field Architecture - Dual Memory Model

### Fields Use TWO Types of Shared Memory

A field with all three capabilities uses **BOTH** memory types:

```
Field with WithGetter + WithSetter + WithNotifier:
├─ Get/Set operations    → Uses METHOD shared memory (Proxy creates)
└─ Notifier operations   → Uses EVENT shared memory (Skeleton creates)
```

---

### Field Construction

From `proxy_field.h:342-362`:

```cpp
ProxyFieldImpl(ProxyBase& proxy_base, const std::string_view field_name)
    : ProxyFieldImpl{
        proxy_base,
        field_name,
        MakeEventDispatchIfEnabled(proxy_base, field_name),      // ← EVENT for notifier
        MakeSetMethodDispatchIfEnabled(proxy_base, field_name),   // ← METHOD for Set
        MakeGetMethodDispatchIfEnabled(proxy_base, field_name)}   // ← METHOD for Get
{
    if constexpr (kHasNotifier)
        ASSERT(proxy_event_dispatch_ != nullptr);      // Uses ProxyEvent!
    if constexpr (kHasSetter)
        ASSERT(proxy_method_set_dispatch_ != nullptr); // Uses ProxyMethod!
    if constexpr (kHasGetter)
        ASSERT(proxy_method_get_dispatch_ != nullptr); // Uses ProxyMethod!
}
```

**Component Storage:**
- `proxy_event_dispatch_` → **ProxyEvent** (uses event SHM)
- `proxy_method_set_dispatch_` → **ProxyMethod** for Set (uses method SHM)
- `proxy_method_get_dispatch_` → **ProxyMethod** for Get (uses method SHM)

---

### Event Shared Memory (For Notifier)

#### Who Creates?

**SKELETON** creates event shared memory during `OfferService()`.

From `skeleton_memory_manager.cpp:150-160`:

```cpp
Result<void> SkeletonMemoryManager::CreateSharedMemory(events, fields, ...)
{
    // Skeleton creates shared memory for events and field notifiers
    if (!OpenSharedMemoryForControl(QualityType::kASIL_QM))
        return error;

    if (!OpenSharedMemoryForData(...))  // ← Creates data storage
        return error;
}
```

#### Structure

```
EVENT Shared Memory (Skeleton creates):
┌──────────────────────────────────────────────────────┐
│ ServiceDataControl (Control SHM):                    │
│   ├─ EventControl for each event/field               │
│   └─ Transaction logs, subscription info             │
└──────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────┐
│ ServiceDataStorage (Data SHM):                       │
│   ├─ Event Slot 0: [FieldValue]                     │
│   ├─ Event Slot 1: [FieldValue]                     │
│   ├─ Event Slot 2: [FieldValue]                     │
│   ├─ ...                                             │
│   └─ EventMetaInfo (size, alignment, pointer)       │
└──────────────────────────────────────────────────────┘
```

---

### Field Notifier Flow

#### When Skeleton Updates Field:

```cpp
skeleton_field.Update(new_value);
  ↓
// Write to EVENT shared memory
EventDataStorage: Write new_value → Slot[next_slot]
                  Update transaction log
  ↓
// Notify all subscribed proxies
MessagePassingService::NotifyEvent(event_id)
```

#### Proxy Receives Notification:

```cpp
// Notification callback invoked
SlotIndices slot_indices = GetNewSamplesSlotIndices(max_count);
  ↓
// Reads from EVENT shared memory
for (slot_idx : slot_indices) {
    FieldType* value_ptr = event_slots_raw_array_[slot_idx];
    SamplePtr sample{value_ptr, ...};
    callback(sample);
}
```

---

### Complete Comparison Table

| Aspect | **Field Get/Set** | **Field Notifier** |
|--------|-------------------|-------------------|
| **Shared Memory** | Method SHM | Event SHM |
| **Creator** | **Proxy** | **Skeleton** |
| **When Created** | Proxy construction | Skeleton OfferService() |
| **Storage** | TypeErasedCallQueue | EventDataStorage (slots) |
| **Direction** | Bidirectional | Unidirectional (Skeleton → Proxy) |
| **Pattern** | Request-Response | Publish-Subscribe |
| **Write Access** | Both Proxy & Skeleton | **Only Skeleton** |
| **Read Access** | Both | **Only Proxy** |
| **IPC Message** | CallMethod(queue_pos) | NotifyEvent(event_id) |
| **Lifecycle** | Lives with Proxy | Lives with Skeleton |
| **Path** | `/dev/shm/score_method_...` | `/dev/shm/score_event_...` |

---

## Events vs Field Notifiers Memory Separation

### Critical Finding: COMPLETELY SEPARATE Memory

Events and field notifiers do **NOT** share slots. They have separate `EventDataStorage` allocations.

#### Evidence

**1. Regular Event Creation** - `proxy_event.h:162`:
```cpp
ProxyEvent<SampleType>::ProxyEvent(ProxyBase& base, const std::string_view event_name)
    : ProxyEvent{base, event_name,
                 ProxyEventBindingFactory<SampleType>::Create(base, event_name,
                     ServiceElementType::EVENT)}  // ← Uses EVENT type
```

**2. Field Notifier Creation** - `proxy_field_binding_factory_impl.h:70`:
```cpp
inline std::unique_ptr<ProxyEventBinding<SampleType>>
ProxyFieldBindingFactoryImpl<SampleType>::CreateEventBinding(...)
{
    return ProxyEventBindingFactory<SampleType>::Create(parent, field_name,
        ServiceElementType::FIELD);  // ← Uses FIELD type
}
```

**3. Storage Structure** - `service_data_storage.h:42-46`:
```cpp
class ServiceDataStorage
{
    // Map with ElementFqId as key - events and fields have different keys
    score::memory::shared::Map<ElementFqId, OffsetPtr<void>> events_;
    score::memory::shared::Map<ElementFqId, EventMetaInfo> events_metainfo_;
};
```

**4. ElementFqId** - `element_fq_id.h:70-82`:
```cpp
class ElementFqId
{
    std::uint16_t service_id_;
    std::uint16_t element_id_;      // ← Different for event vs field
    std::uint16_t instance_id_;
    ServiceElementType element_type_;  // ← EVENT vs FIELD
};
```

---

### How Memory is Organized

```
Skeleton's EVENT Shared Memory (One per service instance):
┌──────────────────────────────────────────────────────────────┐
│ ServiceDataStorage.events_ (Map):                            │
│                                                               │
│ Key: ElementFqId{service=1, element=10, inst=1, type=EVENT}  │
│   └─> EventDataStorage[20 slots] for MyEvent                 │
│                                                               │
│ Key: ElementFqId{service=1, element=20, inst=1, type=FIELD}  │
│   └─> EventDataStorage[15 slots] for MyField notifier        │
│                                                               │
│ Key: ElementFqId{service=1, element=11, inst=1, type=EVENT}  │
│   └─> EventDataStorage[10 slots] for AnotherEvent            │
└──────────────────────────────────────────────────────────────┘
```

**Why Separate:**
- **Different Element IDs**: From AUTOSAR model (event=10, field=20)
- **Different Element Types**: `EVENT` vs `FIELD` enum value
- **Result**: Different `ElementFqId` keys → Separate storage allocations

---

### Practical Example

```cpp
// Service definition
class MyServiceSkeleton {
    SkeletonEvent<int> speed_event;               // Regular event
    SkeletonField<int, WithNotifier> speed_field;  // Field with notifier
};
```

**In Shared Memory:**

```
events_ map:
├─ Key: {service=1, element=10, type=EVENT}
│  └─> EventDataStorage[20 slots] for speed_event
│      [slot0: 100] [slot1: 105] [slot2: 110]
│
└─ Key: {service=1, element=20, type=FIELD}
   └─> EventDataStorage[15 slots] for speed_field
       [slot0: 50] [slot1: 55] [slot2: 60]
```

**Proxy Side:**

```cpp
proxy.speed_event.Subscribe(10);
proxy.speed_event.GetNewSamples(...);  // Reads from EVENT storage

proxy.speed_field.Subscribe(10);
proxy.speed_field.GetNewSamples(...);  // Reads from FIELD storage

// COMPLETELY INDEPENDENT!
```

---

## Complete Flow Examples

### Example 1: Field with All Three Tags

```
Field<int> with WithGetter + WithSetter + WithNotifier
```

#### Operation: field.Set(42)

```
Uses: METHOD shared memory (Proxy created)
Flow:
  1. Proxy writes 42 → method_shm[InArgs at queue_pos=0]
  2. Proxy sends IPC: CallMethod(queue_pos=0)
  3. Skeleton reads from method_shm[InArgs] → gets 42
  4. Skeleton executes Set handler(42)
  5. Skeleton writes result → method_shm[Return at queue_pos=0]
  6. Skeleton sends IPC reply
  7. Proxy reads from method_shm[Return]
```

#### Operation: field.Get()

```
Uses: METHOD shared memory (Proxy created)
Flow:
  1. Proxy sends IPC: CallMethod(queue_pos=0)
  2. Skeleton reads current value
  3. Skeleton writes value → method_shm[Return at queue_pos=0]
  4. Skeleton sends IPC reply
  5. Proxy reads from method_shm[Return]
```

#### Operation: field.Subscribe() + GetNewSamples()

```
Uses: EVENT shared memory (Skeleton created)
Flow:
  1. Skeleton updates field value internally
  2. Skeleton writes new_value → event_shm[slot_3]
  3. Skeleton updates transaction log in event_shm
  4. Skeleton sends IPC: NotifyEvent(field_id)
  5. Proxy receives notification callback
  6. User calls GetNewSamples()
  7. Proxy reads slot indices from event_shm
  8. Proxy reads data directly from event_shm[slot_3]
  9. Returns SamplePtr (pointer to slot) - NO COPYING
```

---

### Example 2: Memory Flow Diagram

```
┌─────────────────────────────────────────────────────────────┐
│  PROXY SIDE (Consumer)          SHARED MEMORY               │
├─────────────────────────────────────────────────────────────┤
│  1. field.Set(value)                                        │
│     ↓                                                        │
│  2. Copy value → [Method SHM: InArgs Buffer]                │
│                  at queue_position                           │
│     │                                                        │
│     └→ Only queue_position passed via IPC                   │
│                                                              │
│  3. DoCall(queue_position)                                  │
│     ↓                                                        │
│     Send IPC message (method_id + queue_position only)      │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│  SKELETON SIDE (Provider)       SAME SHARED MEMORY          │
├─────────────────────────────────────────────────────────────┤
│  4. Receive IPC (method_id + queue_position)                │
│     ↓                                                        │
│  5. Handler(queue_position)                                 │
│     ↓                                                        │
│  6. Read value ← [Method SHM: InArgs Buffer]                │
│                  at queue_position                           │
│     ↓                                                        │
│  7. Execute user handler                                    │
│     ↓                                                        │
│  8. Write return → [Method SHM: Return Buffer]              │
│                    at queue_position                         │
│     ↓                                                        │
│  9. Send IPC reply                                          │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│  PROXY SIDE                                                  │
├─────────────────────────────────────────────────────────────┤
│  10. Receive IPC reply                                       │
│      ↓                                                       │
│  11. Read return ← [Method SHM: Return Buffer]              │
│                    at queue_position                         │
│      ↓                                                       │
│  12. Return MethodReturnTypePtr to user                     │
└─────────────────────────────────────────────────────────────┘
```

---

## Concurrency and Thread-Safety Model

### Overview

The SCORE COM middleware has specific concurrency rules that differ between **Proxy** (consumer) and **Skeleton** (provider) sides, and between **Methods** (Get/Set) and **Events/Notifiers**.

---

### Proxy Side Concurrency

#### ❌ **Methods (Get/Set) - NOT Thread-Safe**

**From `proxy_method_base.h:66`:**
```cpp
/// Size of the call-queue is currently fixed to 1!
static constexpr containers::DynamicArray<int>::size_type kCallQueueSize = 1U;
```

**Restriction:**
- ❌ **NO concurrent method calls** on same proxy instance
- Queue size = 1 means only **one call at a time**
- Calling `Set()` or `Get()` while previous call in progress overwrites data

**Example:**
```cpp
// ❌ UNSAFE - Two threads calling same proxy method
Thread 1: proxy.my_field.Set(42);
Thread 2: proxy.my_field.Set(100);  // Overwrites Thread 1's data in queue_position=0!
```

**Documentation from `proxy_field.h:254`:**
> "API calls on a Proxy/Proxy field are **thread safe/can't be called concurrently**."

**Workaround:**
- User must synchronize access with external mutex
- Or use separate proxy instances per thread

---

#### ❌ **Events/Fields (Notifier) - NOT Thread-Safe**

**From `proxy_event_base.h:143`:**
> "API calls on a Proxy/Proxy event are **thread safe/can't be called concurrently**."

**From `slot_collector.h:66-67`:**
> "This function is **not thread-safe**: It may be called from different threads, but the calls need to be synchronized."

**Restrictions:**
- ❌ **NO concurrent calls** to `Subscribe()`, `GetNewSamples()`, etc.
- User must provide external synchronization

**Example:**
```cpp
// ❌ UNSAFE - Two threads accessing same event
Thread 1: proxy.speed_event.Subscribe(10);
Thread 2: proxy.speed_event.GetNewSamples(...);  // Race condition!

// ✅ SAFE - User adds mutex
std::mutex proxy_event_mutex;
{
    std::lock_guard lock(proxy_event_mutex);
    proxy.speed_event.GetNewSamples(...);
}
```

---

### Skeleton Side Concurrency

#### ✅ **Methods - Multiple Proxies Can Call Concurrently**

**From `message_passing_service_instance.cpp:719-729`:**
```cpp
score::Result<void> CallServiceMethodLocally(...)
{
    // Copy handler under shared_lock (allows multiple readers)
    std::shared_lock<std::shared_mutex> read_lock{call_method_handlers_mutex_};
    auto [method_call_handler_copy, allowed_proxy_uid] =
        method_call_handler_it->second;
    read_lock.unlock();

    // Handler invoked OUTSIDE lock - allows concurrent calls!
    auto invocation_result = std::invoke(method_call_handler_copy, queue_position);
}
```

**Allowed:**
- ✅ **Multiple proxies** can call the **same skeleton method concurrently**
- Each proxy has its own method shared memory region
- Handler is copied under shared lock, then invoked outside lock

**Example:**
```cpp
// ✅ SAFE - Two proxies calling same skeleton method
Proxy1: proxy1.my_method(10);  // Uses method_shm_proxy1[queue_pos=0]
Proxy2: proxy2.my_method(20);  // Uses method_shm_proxy2[queue_pos=0]
// Both handlers can execute in parallel!
```

**Important:**
- ⚠️ **User's skeleton method handler** must be thread-safe if called from multiple proxies
- Skeleton must protect shared state with own mutexes

---

#### ⚠️ **Events/Fields (Send/Update) - Partially Thread-Safe**

**Lock-Free Slot Control (from `events_fields/README.md:139-141`):**
> "Since `EventDataControl` instances get **concurrently accessed** in a r/w manner from skeleton and proxy instances, the central member `state_slots` has been designed for **concurrent lock-free access** based on atomics."

**Concurrent Access Supported:**
```
✅ Skeleton allocates/sends event sample (lock-free slot search)
✅ Proxy searches events (lock-free timestamp comparison)
✅ Proxy dereferences refcount (atomic decrement)
```

**BUT Single Event Send is NOT Thread-Safe:**
- ❌ **Same skeleton event** should **NOT** be sent from multiple threads simultaneously
- ❌ User must synchronize `skeleton_event.Send()` calls

**Example:**
```cpp
// ✅ SAFE - Different events sent concurrently
Thread 1: skeleton.speed_event.Send(100);   // Uses event1 slots
Thread 2: skeleton.rpm_event.Send(5000);    // Uses event2 slots
// Lock-free slot management allows this!

// ❌ UNSAFE - Same event sent from multiple threads
Thread 1: skeleton.speed_event.Send(100);
Thread 2: skeleton.speed_event.Send(200);
// User must add synchronization!

// ✅ SAFE - User adds mutex
std::mutex speed_event_mutex;
{
    std::lock_guard lock(speed_event_mutex);
    skeleton.speed_event.Send(value);
}
```

---

### Summary Table

| Component | Proxy Side | Skeleton Side |
|-----------|-----------|---------------|
| **Method Calls (Get/Set)** | ❌ NOT thread-safe<br>Queue size = 1<br>No concurrent calls | ✅ Multiple proxies can call concurrently<br>⚠️ User handler must be thread-safe |
| **Events/Notifier** | ❌ NOT thread-safe<br>User must synchronize | ⚠️ Slot control is lock-free<br>❌ Single event Send() not thread-safe |
| **Field Get/Set** | ❌ NOT thread-safe<br>Same as methods | ✅ Multiple proxies OK<br>Same as methods |
| **Field Notifier** | ❌ NOT thread-safe<br>Same as events | ⚠️ Same as events |

---

### Key Concurrency Rules

#### Proxy Side (Consumer):

1. ❌ **NO concurrent calls** on same proxy method/event/field
2. ❌ Queue size = 1 prevents concurrent method calls
3. ✅ **Different proxies** in same process can operate independently
4. ⚠️ User must provide external synchronization (mutexes)

#### Skeleton Side (Provider):

5. ✅ **Multiple proxies** can call same method concurrently
6. ✅ **Lock-free event slot management** allows concurrent slot operations
7. ❌ **Single event Send()** must be synchronized by user
8. ⚠️ **User's method/event handlers** must be thread-safe

#### Per-Proxy Isolation:

9. ✅ Each proxy has **separate method shared memory** region
10. ✅ Skeleton maintains **one handler per proxy** for methods
11. ✅ This enables **concurrent handling** of multiple proxies

---

### Practical Examples

#### Example 1: Proxy Multi-Threading (UNSAFE without mutex)

```cpp
// Application with multiple threads using SAME proxy
class App {
    MyServiceProxy proxy_;
    std::mutex proxy_mutex_;  // Required!

    void thread1() {
        std::lock_guard lock(proxy_mutex_);
        proxy_.my_field.Set(42);
    }

    void thread2() {
        std::lock_guard lock(proxy_mutex_);
        proxy_.my_field.Get();
    }
};
```

#### Example 2: Skeleton Multi-Proxy (SAFE - Built-in)

```cpp
// Skeleton serving multiple proxies - automatically safe
class MyServiceSkeleton {
    SkeletonMethod<int(int)> my_method_;
    std::mutex shared_state_mutex_;  // For skeleton's internal state
    int shared_counter_ = 0;

    void RegisterHandler() {
        my_method_.RegisterHandler([this](int value) -> int {
            // This handler called concurrently from multiple proxies!
            std::lock_guard lock(shared_state_mutex_);
            return ++shared_counter_;  // Protect shared state
        });
    }
};

// Multiple proxies call concurrently - built-in support:
Proxy1 → CallMethod → Handler1(value=10) ──┐
Proxy2 → CallMethod → Handler2(value=20) ──┼→ Both execute in parallel
Proxy3 → CallMethod → Handler3(value=30) ──┘   (different SHM regions)
```

#### Example 3: Event Multi-Threading (Requires User Sync)

```cpp
class MyServiceSkeleton {
    SkeletonEvent<int> speed_event_;
    std::mutex event_mutex_;  // Required for same event!

    void thread1() {
        std::lock_guard lock(event_mutex_);
        speed_event_.Send(100);
    }

    void thread2() {
        std::lock_guard lock(event_mutex_);
        speed_event_.Send(200);
    }

    // BUT different events - no mutex needed:
    void updateSpeedAndRpm() {
        // These can be called concurrently:
        speed_event_.Send(100);  // Thread 1
        rpm_event_.Send(5000);   // Thread 2 - OK!
    }
};
```

---

### Future: Queue Size > 1

**From `proxy_method_base.h:66-67`:**
> "As soon as we are going to support larger call-queues, the call-queue-size shall be taken from configuration."

**When implemented:**
- ✅ Will enable **concurrent method calls** on same proxy
- ✅ Each call gets its own queue position
- ✅ No data corruption between concurrent calls

**Future behavior:**
```cpp
// Future: queue_size = 3
Thread 1: proxy.method(10);  // Uses queue_position=0
Thread 2: proxy.method(20);  // Uses queue_position=1
Thread 3: proxy.method(30);  // Uses queue_position=2
// All three can execute concurrently!
```

---

## Key Takeaways

### Zero-Copy Architecture

✅ **Data never moves** - Written once by sender, read once by receiver
✅ **Only pointers/offsets** exchanged via IPC (method_id + queue_position)
✅ **No serialization** - Raw bytes in shared memory
✅ **Performance** - Minimal IPC overhead, bulk data stays in memory

---

### Two Types of Shared Memory

✅ **Method SHM** (Proxy creates):
- For Get/Set operations
- Bidirectional (both read & write)
- Request-response pattern
- Per-proxy isolation

✅ **Event SHM** (Skeleton creates):
- For Event and Field notifier operations
- Unidirectional (Skeleton writes, Proxy reads)
- Publish-subscribe pattern
- One-to-many efficiency

---

### Field Architecture

✅ **Fields with all tags** use **BOTH** shared memory types
✅ **Get/Set** → Method SHM (proxy-created)
✅ **Notifier** → Event SHM (skeleton-created)
✅ **Completely independent** memory regions

---

### Events vs Field Notifiers

✅ **Separate storage** - Different `ElementFqId` keys in map
✅ **Independent slots** - Each has own `EventDataStorage`
✅ **Same SHM file** - But different map entries
✅ **No interference** - Event and field can have same data type

---

### Queue Architecture

✅ **Current size = 1** - Only one concurrent call supported
✅ **Future-proofed** - Designed for multiple concurrent calls
✅ **Per-call isolation** - Each queue position has InArgs + Return buffers

---

### Subscription Model

✅ **Proxy creates method SHM** then notifies skeleton
✅ **Skeleton opens proxy's method SHM** on SubscribeServiceMethod
✅ **Skeleton creates event SHM** during OfferService
✅ **Proxy opens skeleton's event SHM** on Subscribe

---

## References

### Key Source Files

- **proxy_field.h**: Field API, dual dispatch (event + methods)
- **proxy_method_with_in_args_and_return.h**: Method call implementation
- **proxy_method.cpp**: DoCall implementation, binding layer
- **message_passing_service_instance.cpp**: IPC routing logic
- **proxy.cpp**: Method SHM creation (SetupMethods)
- **skeleton.cpp**: Method subscription handling
- **skeleton_memory_manager.cpp**: Event SHM creation
- **type_erased_call_queue.h**: Queue buffer management
- **service_data_storage.h**: Event storage map structure
- **element_fq_id.h**: Element identification
