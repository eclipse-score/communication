# Shared Memory
In order to use shared memory more easily and also in combination with different dynamic containers,
an abstraction layer is introduced.

## Use Cases / Customer Functions
There are no direct Customer Functions associated with this part of `ara::core`.
This is caused by the fact that the shared memory abstraction represents an implementation detail, 
which is necessary to fulfill the [Basic Architectural thoughts](../../../../mw/com/design/README.md) of `ara::com`. 

In fact, the usage of shared memory or its allocators shall be fully transparent for a user of the `ara`-API.

## Shared Memory based allocation
The following section gives a textual reasoning and explanation of the class diagram that can be seen [here](memory_allocation.uxf).

![Memory Allocation](https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/swh/safe-posix-platform/score/memory/design/shared_memory/memory_allocation.uxf?ref=c8a52e508408b2f3905b833f32563264ccf4069c)

Further also some [guidance](#guidance-for-data-types-in-shared-memory) is given, which data types can be stored
in shared memory.

### Offset Pointer
See [OffsetPtr Design](./OffsetPtrDesign.md).

### Polymorphic OffsetPtr Allocator
A user of the `ara::com`-API is able to acquire a so called `AllocateePtr` [SWS_CM_00308]. This way he shifts the
responsibility of allocating memory for a specific data type towards the middleware. Depending on the used network binding,
it can be necessary to allocate the memory either directly in shared memory (to enable truly zero-copy mechanisms) or
on the heap (to serialize the data and send over network sockets). It shall be highlighted that the `AllocateePtr` in
both cases will point to the same data type, since this is a runtime decision (based on the results of the service discovery).
 
In order to support this behaviour, polymorphic allocation needs to be introduced. The
`std::pmr::polymorphic_allocator` (or its respective implementations of AMP) cannot be reused, because it will
allocate raw pointers. As explained in [Offset Pointer](#offset-pointer) this is not suitable for the shared memory use case
and thus a respective allocator needs to be introduced. It shall be pointed out that a classical usage of polymorphic
allocation is not working out. The classical way would pass a memory resource into the allocator as a raw pointer. Even if
we would swap the raw pointer with an `OffsetPtr` it would mean that the memory resource would need to be stored inside
the shared memory. This again is not applicable for some reasons:
* some memory resource classes may contain state/data, which only has a meaning in a specific process (e.g. a top-level
shared memory resource might hold a file descriptor (fd)) to control the (POSIX) shared memory object, but this fd is
different per process.
* our memory resources are designed in a certain inheritance hierarchy (abstract super class being
[ManagedMemoryResource](#managed-memory-resources)),
because we want to have runtime polymorphism to decide, which kind of memory resource implementation is providing storage.
Storing instances of polymorphic classes into shared memory is tricky, because they contain v-tables! But v-tables contain
raw pointers, which are again process specific!

These issues can only be solved by introducing a custom implemented indirection in the form of `MemoryResourceProxy`,
which is explained in more detail in the following chapter. Our polymorphic allocator class is `PolymorphicOffsetPtrAllocator`
and it gets such a `MemoryResourceProxy` as its "memory resource" on which it operates.

### Managed Memory Resources

The first part is the `ManagedMemoryResource`. Inspired by [`std::memory_resource`](https://en.cppreference.com/w/cpp/memory/memory_resource)
`ManagedMemoryResource` represents the interface which is used by any container to allocate memory. Its respective
implementations then either allocate the memory on shared memory (`SharedMemoryResource`) or heap (`HeapMemoryResource`).
It shall be noted that the important difference between an `std::memory_resource` and the newly mentioned
`ManagedMemoryResource` is, that it offers the possibility to get an `MemoryResourceProxy` for a specific resource
instance.
This `MemoryResourceProxy`, which each `ManagedMemoryResource` subclass provides, is the needed indirection mechanism,
we talked about in the previous chapter!
The idea behind the `MemoryResourceProxy` is, that it builds up a non-virtual class that can be stored
in shared memory and identifies a specific `ManagedMemoryResource` using a process-specific global instance of `MemoryResourceRegistry`.
One can think of it as a custom shared memory safe implementation of a v-table. In order for this to work, on construction
of a memory resource, it needs to register itself at the `MemoryResourceRegistry`. Then, when returning the `MemoryResourceProxy`
it needs to be constructed with the same identifier. This workflow is further illustrated in [Memory Allocation Workflow](memory_allocation_workflow.uxf).
On a second process, that did not create the shared memory, the workflow would look the same, with the only difference,
that the `MemoryResourceProxy` is not created, but rather reinterpreted from the shared memory region.

![Memory Allocation](https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/swh/safe-posix-platform/score/memory/design/shared_memory/memory_allocation_workflow.uxf?ref=8bc136c4746944bee94af634a9e2b4919c0803ab)

The key idea of this `MemoryResourceRegistry` concept is, that the keys used for registering a `ManagedMemoryResource` into
the registry (and being the essential part of the proxy) is globally (across all processes) unique and deterministic.
But this is _easily_ achievable. E.g.:
* for a `SharedMemoryResource` the key will be generated from its file system path
* for a `HeapMemoryResource` the key will be fixed to 0.
* for a [SubResource](#subresource) the key will be generated from a combination of the key of its parent resource and a
running number

While the `HeapMemoryResource` can be a trivial implementation using the standard global allocator, `SharedMemoryResource`
has more specifics that need to be explained. The most important one being, that it cannot be publicly constructed nor copied.
This is necessary to avoid that the same shared memory is mapped multiple times in one process. A factory
(`SharedMemoryFactory`) will take care of a safe creation and ownership transfer. Another specific is, that
`SharedMemoryResource` will interact with some operating system abstraction library (`osabstraction`) in order to
manage the shared memory physically (opening, truncating, closing).

#### SubResource

Besides the direct subclasses `SharedMemoryResource` and `HeapMemoryResource` of `ManagedMemoryResource` we also do foresee
another subclass `SubResource`, which allows multi-level/hierarchic stacking of memory resources.
Sample use case: We want to subdivide the memory of a shared memory object (represented by a `SharedMemoryResource`).
For instance in case of events of a specific data type DT1, where we know its max size requirements ex ante and eventually
also the max number of clients/subscribers the memory allocation strategy could be massively optimized! In such a scenario
we would create a specific/suitable `SubResource` within a `SharedMemoryResource` and for events of type DT1, we would attach an
`PolymorphicOffsetPtrAllocator` instance, which gets an `MemoryResourceProxy`, which references this `SubResource`.

**_Note_**: Usage of such `SubResource`s is an optimization feature, which we might not yet use in our first POC.

**Writer**

The writer, which wants to update the known regions, 1st needs to find a version among the N versions of known regions,
which is currently **not** used/accessed by any reader. For this each known regions version has an atomic `std::uint32_t`
as ref-counter, which reflects how many readers are currently doing a bounds-check lookup on this known regions version.
Therefore, the writer checks each version, starting with the oldest, and if the ref-count is 0, then tries to atomically 
change it to some specific marker value `INVALID_REF_COUNT_VAL_START` via atomic `compare_exchange` operation.
If the writer succeeds with the change, he has now unique ownership of this version, copies the map/known regions from
the current most recent version to this acquired version, does the bounds-update there, atomically sets the ref-count to
0 and finally with an atomic store operation declares this acquired version as the new current most recent version.

`INVALID_REF_COUNT_VAL_START` is chosen as being `std::numeric_limits<RegionVersionRefCountType>::max() / 2U`, i.e. it
is halfway in the range of the ref-counter type. This means, that any ref-count value between
0..`INVALID_REF_COUNT_VAL_START` are valid ref-counter values being used by readers (see below) to signal their current
usage. So we allow up to approx. 2*10⁷ threads concurrently accessing a known regions version.

**Reader**

A reader, which wants to access for bounds-checking the most recent known regions version does the following steps:
1. atomically load/read the indicator, which version is the most recent.
2. atomically increment the usage/reference counter of the known regions version read in step 1. and check the result
   (previous ref counter) of the atomic increment.

There are three outcomes in 2., i.e. the rec count **before** the atomic increment:
1. "Good" case: *old_ref_count < `INVALID_REF_COUNT_VAL_START` - 1.*

    This is the "good" case where the reader successfully acquired this version of known regions for read. Once a 
    version has been created by a writer, its ref_count will be 0. The ref_count will then be incremented every time a 
    reader is currently accessing that version, and then decremented again when it's finished. Therefore, if the old
    old_ref_count is less than "`INVALID_REF_COUNT_VAL_START` - 1", it is in this safe-for-reading state. 

    *Result*: Reader acquires the version. Other readers can also acquire it but a writer cannot write to it.
    
2. "Retry" case:  *`INVALID_REF_COUNT_VAL_START` <= old_ref_count < `INVALID_REF_COUNT_VAL_END`*
  
    A reader gets the latest version index which currently has no other readers. At this point, this thread blocks or 
    runs slowly. A writer updates another version, changing the latest version index to that newly modified version, so
    the reader thread has loaded a version index which no longer corresponds to the true latest version. The writer or a
    series of writers do this enough times that the next time that a writer acquires a version, it acquires the same 
    version index that the reader is accessing. But since the reader has not yet incremented the ref count of that 
    version, the writer acquires it. The writer will update the ref count to `INVALID_REF_COUNT_VAL_START` and begin 
    modifying the version. The reader thread finally unblocks and increments the ref count, but will see that the 
    old_ref_count is now `INVALID_REF_COUNT_VAL_START`. Therefore, it knows that a writer has acquired this version and 
    it should check for the new latest version index and try to acquire that version.

    *Result*: In this case, the reader will retry a specified number of times until it can acquire a version for 
    reading. If it cannot acquire a version for reading after these retries, it returns an empty value and the caller 
    can handle this case.

3. "Failure" cases:

    * (A) *old_ref_count == `INVALID_REF_COUNT_VAL_START` - 1.*

        If the old ref-counter was equal to `INVALID_REF_COUNT_VAL_START` - 1 before incrementing, the new ref_count 
        after incrementing would now be `INVALID_REF_COUNT_VAL_START`. This is the value used by the writer to indicate
        that it is currently writing to this version, which will prevent other readers from accessing this version. It 
        is also the initial value for an unused version (i.e. that have no readers), so a writer will assume that it is 
        free to write to this version which could lead to the version being updated *while* the reader is still reading
        it.

        This case will occur if we have almost 2x10⁷ readers concurrently accessing the same version. 
        Alternatively, if the decrement-logic of the ref_count (when a reader is finished with the version) is broken.
        
    * (B) *old_ref_count == `INVALID_REF_COUNT_VAL_END`*

        If the case described in the "Retry" case occurs, then the increment operation by the reader will cause the 
        ref_count to be (`INVALID_REF_COUNT_VAL_START` + 1). If this occurs, enough times, then eventually the ref_count
        will reach `INVALID_REF_COUNT_VAL_END`. If another reader tries to increment the ref_count of this version, then
        it will overflow to 0, despite the fact that the writer is still updating the region.

        This case will occur if this rety case is encountered by almost 2x10⁷ on the same version.

    *Result*: In both these cases, we terminate.

**_Note_**: The reason, that we use a vast range of "invalid refcounts" from `INVALID_REF_COUNT_VAL_START` to
`INVALID_REF_COUNT_VAL_END` instead of just using one marker/sentinel value `INVALID_REF_COUNT_VAL`, is that we want
to use solely `atomic_increment` in our algo instead of a pair of
`atomic_load`/`atomic_compare_exchange(loaded_val, loaded_val + 1)`! With just one marker/sentinel, we would have to
use semantically/from algo perspective such a pair of operations, which has the following downside/problem we saw in
load-tests!: If we have a concurrency/contention in reader threads between the `atomic_load` and the upcoming
`atomic_compare_exchange`, the `atomic_compare_exchange` fails, forcing us/the reader into a retry! Under simulated
heavy load, the number of needed re-tries for a reader to finally succeed in updating the ref-counter was **huge**!

### Usage

Since `ara::core` needs to implement different container types like `Vector` or `String` it shall be possible to reuse
standard library container. This is possible by overloading the standard library container with a custom allocator,
that follows the requirements specified in [`std::allocator_traits`](https://en.cppreference.com/w/cpp/memory/allocator_traits).
This custom allocator is called `PolymorphicOffsetPtrAllocator` and depends on the previously defined memory resource proxy
`MemoryResourceProxy`, which will then resolve the correct memory resource to use. In order to support multi-level 
allocations (e.g. vector in vector) the custom allocator needs to be wrapped in`std::scoped_allocator_adaptor`.

All in all an example usage and implementation of `ara::core::Vector<T>` could look like this:
```c++
template<typename T>
using ara::core::Vector<T> = std::vector<T, std::scoped_allocator_adaptor<ara::core::detail::PolymorphicOffsetPtrAllocator<T> >
auto memory_resource = score::memory::shared::SharedMemoryFactory.getInstance.open("/my_shm_name");
ara::core::Vector<std::uint8_t> myVector(memory_resource.getProxy());

myVector.push_back(42u); // Will land on shared memory

ara::core::Vector<std::uint8_t> onHeap();
onHeap.push_back(42u); // Will land on heap
```

### Guidance for data types in shared memory
Due to the fact that shared memory is interpreted by two processes, there are some limitations on data types
that can be stored meaningful in the shared memory. One point already mentioned in [Offset Pointer](#offset-pointer) is
that no raw pointer can be stored in shared memory, since the pointer will be invalid in other processes.

In order to store any data type in shared memory without serialization, it needs to be ensured that alignment and overall
memory layout will not differ between the processes that access it. Strictly speaking both processes need to be build
with the same compiler / linker and also need to use the same options for them. This includes that they use the same
standard library implementation.

Further it is not possible to store objects with virtual functions within the shared memory. This can be explained again
by Offset Pointer problem. The pointers within the v-table will just be invalid in the other process.

When storing templated types in shared memory, it needs to be ensured that their respective symbol names are mangled in
the same manner in both process and no conflicts arise.

Last but not least, when instantiating data types in shared memory, placement new shall be used. Copy-Construction shall
be avoided and move construction does not seem to be possible. This leaves us with a problem on the processes that read
the data but not create them. They have to use a `reinterpret_cast` to get the respective data types from the raw memory
they opened. This causes undefined behaviour since the C++-Standard states that such casts are only defined if the
object started life already in this process. The notion of shared memory is not considered to the C++-Standard. In
practice the cast will work, but for ASIL-B software this behaviour needs to be assured by the compiler vendor. 

At the end the interaction with shared memory can look like listed:
```c++
// 1. Process: 
void* ptr = shm_open(...);
int* value = new(ptr) int; // using placement new to store data type
*value = 5;

// 2. Process
void* ptr = shm_open(...);
int* value = reinterpret_cast<int*>(ptr);
std::cout << value << std::endl // will print 5
*value = 42; // undefined behaviour, because reinterpret_cast is only valid if the object would life there already
```
### SharedMemoryResource allocation
`SharedMemoryResource` objects can be requested in two different ways:
1. As named `SharedMemoryResource` objects with an entry in the path namespace (`/dev/shmem`)
2. As anonymous `SharedMemoryResource` objects that are accessed using a `shm_handle_t`

Named `SharedMemoryResource` can be created by calling either of the following `Create APIs`
- `SharedMemoryFactory::Create(...)`
- `SharedMemoryFactory::CreateOrOpen(...)`
- `SharedMemoryFactory::CreateAnonymous(...)`

Shared memory is allocated in either typed memory or OS-system memory based on the `prefer_typed_memory` parameter of the above APIs; when set to true, the shared memory will be allocated in a typed memory region. The [score::memory::shared::TypedMemory](../../shared/typedshm/typedshm_wrapper/typed_memory.h) class acts as a wrapper that uses the [`typed_memory_daemon`](../../../../intc/typedmemd/README.md)  client interface[`score::tmd::TypedSharedMemory`](../../../../intc/typedmemd/code/clientlib/typedsharedmemory.h) APIs to allocate shared memory in typed memory.
If allocation in typed memory fails, the allocation of shared memory will fall back to the OS-system memory ([DMA Accessible Memory Fallback](broken_link_c/issue/31034619)).

### User permissions of Shared Memory
The `permissions` parameter in the `Create APIs`, allows the user to specify the access rights for the `SharedMemoryResource`.

For the Named Shared Memory it can be set to one of the following:
- `WorldReadable`: allows read/write access for the user and read-only access for others.
- `WorldWritable`: allows read/write access for both the user and all other users.
- `UserPermissionsMap`: allows read/write access for the user and for named shared memory, sets specific permissions for additional users listed in the `UserPermissionsMap` using Access Control Lists ([ACLs](https://www.qnx.com/developers/docs/7.1/#com.qnx.doc.security.system/topic/manual/access_control.html)).

For the Anonymous Shared Memory check the details below [Anonymous Shared Memory](#anonymous-shared-memory)

### Named Shared Memory
Named shared memory allocated in typed memory will inherit the effective `UID/GID` of the `typed_memory_daemon`. Whereas, named shared memory allocated in OS-system memory will have the effective `UID/GID` of the user. 

For named shared memory allocated in OS-system memory with `world-writable` mode, permissions will be enforced using `fchmod` to ensure `world-writable` access.

The underlying `shm_open()` call uses the `O_EXCL` and `O_CREAT` flags, ensuring that duplicate names result in an error. The file permissions are determined by the `permissions` parameter[User permissions of Shared Memory](#user-permissions-of-shared-memory) passed to the `Create APIs`, and are applied via the `mode` argument in the `shm_open()` call.

From a safety standpoint, `ASIL-B` applications should avoid creating shared memory objects with `world-readable` or `world-writable` permissions in order to reduce security risks. For more details check [Access Control concepts](broken_link_a/ui/api/v1/download/contentBrowsing/ipnext-platform-documentation/master/html/features/dac/README.html)

![Named memory allocation](https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/swh/safe-posix-platform/score/memory/design/shared_memory/named_memory_allocation.uxf?ref=897553326c7c5317cd79d4ffb6323b32849e2758)

### Anonymous Shared Memory
Anonymous shared memory created with `SharedMemoryFactory::CreateAnonymous(...)` does not have a representation in the file system. In fact, there is no way for a random process to identify an anonymous shared memory object. Thus, there are no corresponding implementations of `SharedMemoryFactory::Open(...)` and `SharedMemoryFactory::CreateOrOpen(...)`. To share an anonymous shared memory object with another process it must be actively shared at runtime by:
- Creating a handle using `shm_create_handle(...)`
- Sharing this handle with the other process
- Opening the handle in the other process using `shm_open_handle(...)`

It is important to note that anonymous shared memory is currently only implemented for QNX environment. In the current design/implementation, there is no API available to share the `shm_handle_t` with other processes. Instead, the `SharedMemoryResource` holds the file descriptor of the anonymous shared memory object.

Anonymous shared memory allocated in typed memory are created with read/write access for the user. Whereas, for anonymous shared memory allocated in OS-system memory,
underlying `shm_open()` call is made with `mode` argument based on the`permissions` parameter[User permissions of Shared Memory](#user-permissions-of-shared-memory) passed to the `SharedMemoryFactory::CreateAnonymous(...)`.

Anonymous Shared Memory objects are created without `SHM_CREATE_HANDLE_OPT_NOFD`, such that the user is able to create further `shm_handle_t` for other processes.
These objects are then sealed by calling `shm_ctl()` with `SHMCTL_SEAL` flag to prevents the object's layout (e.g., its size and backing memory) and attributes from being modified. So that no process (including the object's creator) can modify the layout or change any attributes.

![Anonymous memory allocation](https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/swh/safe-posix-platform/score/memory/design/shared_memory/anonymous_memory_allocation.uxf?ref=8bc136c4746944bee94af634a9e2b4919c0803ab)

## Memory Management Algorithm
The allocated shared memory needs to be managed in some way. Meaning, freed memory needs to be reused before
new memory is allocated and the shared memory segment is enlarged.

For the proof of concept we only need a monotonic allocator. Meaning, it will only increase and not free any memory.

## Lifetime of SharedMemoryResource and MemoryResourceRegistry

### Background - Static construction / destruction sequence
All static objects will be destroyed at program end (after all other non-static objects have been destroyed). The order in which static objects are destroyed will be the inverse of the order in which they're created. E.g. if static object A is created and the static object B is created, then B will be destroyed before A.

### Lifetime of SharedMemoryResource in application code
The MemoryResourceRegistry is a singleton which is created when MemoryResourceRegistry::getInstance() is called for the first time. This will be called during the construction of a SharedMemoryResource, so it is guaranteed to be created if a SharedMemoryResource is created. The destructor of SharedMemoryResource also calls MemoryResourceRegistry::getInstance(). This means that the MemoryResourceRegistry should be destroyed only after the last SharedMemoryResource has been destroyed.

In application code, if the lifetime of a SharedMemoryResource is linked to the lifetime of a static object (e.g. it's destroyed in the destructor of the static object), then the SharedMemoryResource will be destroyed at some point during the static destruction sequence at the earliest. In this case, the user must ensure that MemoryResourceRegistry::getInstance() is called before the static object owning the shared_ptr is created. This can be solved via a singleton approach e.g. 

```
// Assuming that UserSharedMemoryResourceOwner is created as a static variable
class UserSharedMemoryResourceOwner
{
  public:
    UserSharedMemoryResourceOwner() 
    {
        // The MemoryResourceRegistry will be created before the UserSharedMemoryResourceOwner.
        // Therefore, it will be destroyed after the UserSharedMemoryResourceOwner
        MemoryResourceRegistry::getInstance();

        // Create the SharedMemoryResource and assign to memory_resource_...
    }    

    ~UserSharedMemoryResourceOwner() {
        // If the ref count of the memory_resource_ is greater than 1, the SharedMemoryResource will not
        // be destroyed here!
        assert(memory_resource_.ref_count() == 1);

        // The MemoryResourceRegistry is still alive here
    }      

  private:
    // Created in the constructor and destroyed in the destructor of UserSharedMemoryResourceOwner.
    std::shared_ptr<SharedMemoryResource> memory_resource_;
} 
```

Obviously, since the SharedMemoryResource is contained within a shared_ptr, in the example above, the user must ensure that the ref count of the shared_ptr goes to 0 when the static object is destroyed so that the SharedMemoryResource itself is destroyed.

## Ownership of the SharedMemoryResource

Getting the `uid` of the creator of the underlying memory managed by a `SharedMemoryResource` is essential to ensure that only memory with the expected allowed providers can be opened by a user of the library as this feature is used by some ASIL B components (for example see this [requirement](broken_link_c/issue/33047276) or this [one](broken_link_c/issue/8742625)). 
This library only supports opening a shared memory area based on the path therefore this concept is only valid for the named memory use case, annonymous memory is not considered. 

There are 2 possible use-cases :

1. Memory is allocated in System RAM: 

The owner `uid` of the allocated memory will be the `euid` (effective user ID) of the allocating process. 
Therefore, when another process opens the memory it only needs to perform a simple check by using `fstat` to get the `uid` for the memory and compare that `uid` with the passed expected provider. 

2. Memory is allocated in [Typed Memory](../../../../intc/typedmemd/README.md): 

Due to safety considerations, typed memory cannot be directly allocated by a process using the POSIX primitives, so the allocation is delegated to an ASIL B application called [`typed_memory_daemon`](broken_link_g/swh/ddad_platform/blob/master/aas/intc/typedmemd/README.md). 
The `typed_memory_daemon` will allocate the memory with its `euid` as the owner `uid`. 
For safety and security reasons the `typed_memory_daemon` cannot transfer ownership of the memory using `chown` to the application's `euid` from which it got the delegation, so it will use Acess Control Lists ([ACLs](https://www.qnx.com/developers/docs/7.1/#com.qnx.doc.security.system/topic/manual/access_control.html)) to give read/write permissions to the requestor's `euid` as well as any other needed `uid`. \
In this case, to be able to identify the requestor process, the `typed_memory_daemon` will add on top of the read/write permissions also the execution permissions to the requestor's `euid` **only** as per its [ASIL B requirement](broken_link_c/issue/42033981). 
When another process opens the `SharedMemoryResource` it will then check if the `uid` of the memory is identical to the `typed_memory_daemon`'s `euid`. 
The `typed_memory_daemon`'s `euid` is currently a hardcoded constant in the code. 
If the check is succesfull it means that the memory is in typed memory and the corresponding internal flag (`is_shm_in_typed_memory_`) will also be updated. 
Then it will inspect the ACLs and compare the single `uid` with execution permissions with the passed expected provider. 
If there are none or multiple `uids` with execution permissions the application will be terminated. 

This solution of using the execution bit to mark the owner/creator of the memory was chosen as a workaround to enable the usage of typed memory by components relying on the allowed providers check (e.g. users of `mw::com`). 
Using the execution is a lightweight solution as it only needs one system call to get the necessary information on the ACLs. 
It also does not have any impact on security as [PathTrust](https://www.qnx.com/developers/docs/7.1/#com.qnx.doc.security.system/topic/manual/pathtrust.html) is enabled which makes it impossible to execute anything allocated in `/dev/shmem` and even prevents other side effects like mapped and executed by the default QNX loader.

The complete sequence to find the owner is:

![GetOwnerUid sequence](https://www.plantuml.com/plantuml/proxy?src=https://raw.githubusercontent.com/swh/safe-posix-platform/score/memory/design/shared_memory/get_owner_uid_seq.puml?ref=7f42212f92084db1c548d7361c2cf4336cafc6dd)
