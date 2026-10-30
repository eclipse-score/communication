# public_api.puml — exact gap matrix

Rule for this report: **the puml's own conventions and exclusion bullets are
ignored.** Nothing is "correctly not modelled". Every public C++ element is
listed with a plain yes/no, so the gaps can be judged, not justified.

Legend: `Y` = modelled in public_api.puml · `N` = not modelled ·
`~` = modelled but does not match the code · `--` = no C++ counterpart exists.

The six C++ special members are listed at the end of every class table in the
fixed order *default ctor, destructor, copy ctor, copy assign, move ctor, move
assign*, written in italics to distinguish them from verbatim declarations. How
the class defines each one is stated with a fixed six-token vocabulary:

| Token | Meaning |
|---|---|
| `= delete` | explicitly deleted |
| `= default` | explicitly defaulted |
| impl | user-provided body |
| implicit | not declared, compiler generates it |
| impl-del | not declared, compiler generates it **as deleted** |
| suppressed | not declared **and not generated** |

A special member with its **own implementation** gets its own row, so that the
model decision for that body can be traced individually. The others are grouped
on one row per shared token.

## Decision column

The third column records, per row, **whether the element shall be modelled in
`public_api.puml`**. It is the input to the FMEA: the analysis walks the model
method by method, so what is written here determines what gets analysed.

| Decision | Meaning |
|---|---|
| `—` | nothing to decide — already modelled (`Y`), or no C++ counterpart (`--`) |
| `model` | shall be added to `public_api.puml` |
| `fix` | modelled already, but the model does not match the code — correct it |
| `open` | **decision required** |
| `out: <code>` | shall deliberately not be modelled, for the stated reason |

Reason codes:

| Code | Meaning                                                                                                                                                                |
|---|------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `out: not-callable` | `= delete`, `suppressed` or `impl-del` — no call site can exist, so no failure mode                                                                                    |
| `out: internal` | declared only inside an internal namespace (`score::mw::com::impl`, or a nested `impl::test` / `impl::lola` / `runtime::detail`) and not re-exported through `types.h` |
| `out: not-user-facing-api` | declared in the public section of a class, but no user facing public API                                                                                               |
| `out: compile-time` | type alias or trait struct — no runtime behaviour                                                                                                                      |
| `out: deprecated` | marked deprecated in the code                                                                                                                                          |
| `out: duplicate` | same callable behaviour as an operation already modelled                                                                                                               |

Members declared `private` or `protected` are **not listed at all** — they are
not public API, so they are not a gap in a matrix of the public API. A class
table may therefore show fewer than six special members: `ProxyBase` and
`SkeletonBase` declare their move constructor and move assignment `protected`.

Current tally: **3 `model`**, 3 `fix` (all on `EventMapView`, as are the 3 `model`),
0 `open`, and 98 `out` (26 internal, 25 not-user-facing-api, 14 compile-time,
14 not-callable, 6 generated, 5 deprecated, 3 logging, 3 duplicate, 2 bare
`out`). Every other `model` / `fix` row is implemented in `public_api.puml`,
where a member that cannot be called by the user is shown with `-` visibility.

The proposals below were derived mechanically from these rules; **every row is
meant to be overridden where the safety judgement differs.** Rows carrying
`open` are compiler-generated members (`= default` / `implicit`): they are
callable, so they can fail, but whether each one needs its own modelled
operation is a safety decision that is not pre-empted here.

Sources: `types.h` + full transitive closure, plus `runtime.h`,
`runtime_configuration.h`, `com_error_domain.h`, `test_types.h`.

---

## 0. Element-level: all 26 `types.h` exports

All 26 names exported by `types.h` have a corresponding element in the diagram.
**There is no element-level gap in the `types.h` closure.** Every gap below is at
member level, or concerns elements outside `types.h`.

| # | `types.h` export | puml element |
|---|---|---|
| 1 | `InstanceIdentifier` | `InstanceIdentifier` |
| 2 | `InstanceSpecifier` | `InstanceSpecifier` |
| 3 | `InstanceIdentifierContainer` | `InstanceIdentifierContainer` |
| 4 | `FindServiceHandle` | `FindServiceHandle` |
| 5 | `HandleType` | `HandleType` |
| 6 | `ServiceHandleContainer<T>` | `ServiceHandleContainer` |
| 7 | `FindServiceHandler<T>` | `FindServiceHandler` |
| 8 | `SubscriptionState` | `SubscriptionState` |
| 9 | `SamplePtr<SampleType>` | `SamplePtr` |
| 10 | `SampleAllocateePtr<SampleType>` | `SampleAllocateePtr` |
| 11 | `MethodReturnTypePtr<ReturnType>` | `MethodReturnTypePtr` |
| 12 | `MethodInArgTypePtr<ReturnType>` | `MethodInArgTypePtr` |
| 13 | `EventReceiveHandler` | `EventReceiveHandler` |
| 14-16 | `WithGetter` / `WithSetter` / `WithNotifier` | same |
| 17 | `AsProxy<T>` | `Proxy` |
| 18 | `AsSkeleton<T>` | `Skeleton` |
| 19 | `GenericProxy` | `GenericProxy` |
| 20 | `GenericProxyEvent` | `GenericProxyEvent` |
| 21 | `GenericSkeleton` | `GenericSkeleton` |
| 22 | `DataTypeMetaInfo` | `DataTypeMetaInfo` |
| 23 | `EventInfo` | `EventInfo` |
| 24 | `GenericSkeletonServiceElementInfo` | `GenericSkeletonServiceElementInfo` |
| 25 | `GenericSkeletonEvent` | `GenericSkeletonEvent` |
| 26 | `ReceiveHandlerRegistrationChangedCallback` | same |

---

## 1. Service instance identification

### `InstanceIdentifier`
| C++ public member | puml | decision       |
|---|---|----------------|
| `static Result<InstanceIdentifier> Create(std::string&&)` | Y | —              |
| `std::string_view ToString() const noexcept` | Y | —              |
| `friend operator==` | Y `IsEqual` | —              |
| `friend operator<` | Y `LessThan` | —              |
| `std::hash<>::operator()` | Y `Hash` | —              |
| *default ctor* — `= delete` | Y `-ConstructDefault` | — |
| *destructor / copy ctor / copy assign / move ctor / move assign* — `= default` | N | out: generated |

### `InstanceSpecifier`
| C++ public member | puml | decision        |
|---|---|-----------------|
| `static Result<InstanceSpecifier> Create(std::string&&) noexcept` | Y | —               |
| `std::string_view ToString() const noexcept` | Y | —               |
| `operator==(const IS&, const IS&)` | Y `IsEqual` | —               |
| `operator==(const IS&, const string_view&)` | Y `IsEqual` | —               |
| `operator==(const string_view&, const IS&)` | Y `IsEqual` | —               |
| `operator!=(const IS&, const IS&)` | Y `IsNotEqual` | —               |
| `operator!=(const IS&, const string_view&)` | Y `IsNotEqual` | —               |
| `operator!=(const string_view&, const IS&)` | Y `IsNotEqual` | —               |
| `operator<(const IS&, const IS&)` | Y `LessThan` | —               |
| `std::hash<>::operator()` | Y `Hash` | —               |
| `static Create(T&&)` (deprecated template) | N | out: deprecated |
| `operator<<(LogStream&, const IS&)` | N | out: logging         |
| *default ctor* — suppressed | Y `-ConstructDefault` | — |
| *destructor / copy ctor / copy assign / move ctor / move assign* — implicit | N | out: generated  |

### `HandleType`
| C++ public member | puml | decision                 |
|---|---|--------------------------|
| `const InstanceIdentifier& GetInstanceIdentifier() const&` | N | out: not-user-facing-api |
| `const ServiceInstanceDeployment& GetServiceInstanceDeployment() const` | N | out: not-user-facing-api |
| `const ServiceTypeDeployment& GetServiceTypeDeployment() const` | N | out: not-user-facing-api |
| `ServiceInstanceId GetInstanceId() const` | N | out: not-user-facing-api |
| `friend operator==` / `operator<` | Y `IsEqual` / `LessThan` | —                        |
| `std::hash<>::operator()` | Y `Hash` | —                        |
| *default ctor* — `= delete` | Y `-ConstructDefault` | — |
| *destructor / copy ctor / copy assign / move ctor / move assign* — `= default` | N | out: generated           |
| free `make_HandleType(identifier, instance_id = {})` | N | out: internal            |

### `FindServiceHandle`
| C++ public member | puml | decision        |
|---|---|-----------------|
| `friend operator==` / `operator<` | Y `IsEqual` / `LessThan` | —               |
| `std::hash<>::operator()` | Y `Hash` | —               |
| *default ctor* — `= delete` | Y `-ConstructDefault` | — |
| *destructor / copy ctor / copy assign / move ctor / move assign* — `= default` | N | out: generated  |
| `friend operator<<(LogStream&, ...)` | N | out: logging |
| `friend operator<<(std::ostream&, ...)` | N | out: logging |
| free `make_FindServiceHandle(std::size_t uid)` | N | out: internal   |
| `FindServiceHandleView` (2 public members) | N | out: internal   |

### Aliases / enums
| C++ | puml | decision |
|---|---|---|
| `ServiceHandleContainer<T> = std::vector<T>` | Y | — |
| `FindServiceHandler<T> = callback<void(ServiceHandleContainer<T>, FindServiceHandle)>` | Y `Invoke` | — |
| `InstanceIdentifierContainer = std::vector<InstanceIdentifier>` | Y | — |
| `SubscriptionState` — `kSubscribed` / `kNotSubscribed` / `kSubscriptionPending` | Y all 3 | — |
| `EventReceiveHandler = callback<void(void)>` | Y `Invoke` | — |
| `ReceiveHandlerRegistrationChangedCallback = callback<void(bool)>` | Y `Invoke` | — |
| `SubscriptionStateChangeHandler = callback<bool(SubscriptionState)>` | Y — **but not exported by `types.h`** | — |

### Field tags — `impl/field_tags.h`
| C++ | puml | decision |
|---|---|---|
| `struct WithGetter` / `WithSetter` / `WithNotifier` (empty) | Y | — |
| `struct contains_type<TargetType, Tags...>` | N | out: compile-time |
| `struct is_tag_enabled<FieldType, ClassFieldType, RequiredTag, EnabledTags...>` | N | out: compile-time |

---

## 2. Sample / method pointers

### `SamplePtr<SampleType>`
| C++ public member                                                       | puml                              | decision                     |
|-------------------------------------------------------------------------|-----------------------------------|------------------------------|
| `pointer Get() const noexcept`                                          | Y                                 | —                            |
| `pointer get() const noexcept` (lowercase duplicate)                    | **N**                             | out: duplicate               |
| `operator*() const`                                                     | Y `Dereference`                   | —                            |
| `operator->() const`                                                    | Y `MemberAccess`                  | —                            |
| `operator bool() const` (explicit)                                      | Y `IsValid`                       | —                            |
| `operator<(const SamplePtr&) const`                                     | Y `LessThan`                      | —                            |
| `operator>(const SamplePtr&) const`                                     | Y `GreaterThan`                   | —                            |
| `void Swap(SamplePtr&)`                                                 | Y                                 | —                            |
| `void Swap(SamplePtr<OtherSampleType>&)`                                | Y `SwapOtherType`                 | —                            |
| `void Reset(SamplePtr other = nullptr)`                                 | Y                                 | —                            |
| `using pointer` / `using element_type`                                  | N                                 | out: compile-time            |
| `constexpr SamplePtr(std::nullptr_t) noexcept`                          | N                                 | out: not-user-facing-api     |
| `SamplePtr(BindingSamplePtrType&&, SampleReferenceGuard)`               | N                                 | out: not-user-facing-api     |
| `SamplePtr(SamplePtr<OtherSampleType>&&) noexcept` (converting)         | N                                 | out: not-user-facing-api     |
| `operator=(std::nullptr_t) noexcept`                                    | Y `AssignNull`                    | —                            |
| *default ctor* — impl (public) | Y `-ConstructDefault` | out: not-user-facing-api |
| *destructor* — `= default`                                              | Y `Destroy`                       | —                            |
| *copy ctor / copy assign* — `= delete`                                  | Y `-CopyConstruct` / `-CopyAssign` | —                         |
| *move ctor* — impl                                                      | Y `MoveConstruct`                 | —                            |
| *move assign* — `= default`                                             | Y `MoveAssign`                    | —                            |
| free `operator==` / `operator!=` vs `nullptr_t` (4 overloads)           | Y `IsEqual` / `IsNotEqual`        | —                            |
| free `swap(SamplePtr&, SamplePtr&)`                                     | N                                 | out: duplicate               |
| free `CreateSamplePtrFromSampleAllocateePtr(SampleAllocateePtr<void>&)` | N                                 | out: internal                |

### `SampleAllocateePtr<SampleType>`
| C++ public member | puml | decision                     |
|---|---|------------------------------|
| `pointer Get() const noexcept` | Y | —                            |
| `operator*() const` | Y `Dereference` | —                            |
| `operator->() const` | Y `MemberAccess` | —                            |
| `operator bool() const` (explicit) | Y `IsValid` | —                            |
| `void reset() noexcept` | Y `reset` | —                            |
| `void Swap(SampleAllocateePtr&)` | Y | —                            |
| `void Swap(SampleAllocateePtr<OtherSampleType>&)` | Y `SwapOtherSampleType` | —                            |
| `using pointer` / `using element_type` | N | out: compile-time            |
| `constexpr explicit SampleAllocateePtr(std::nullptr_t) noexcept` | N | out: not-user-facing-api     |
| converting move ctor from `SampleAllocateePtr<Other>&&` | Y `MoveConstructOtherSampleType` | —                            |
| `operator=(std::nullptr_t) noexcept` | Y `AssignNull` | —                            |
| *default ctor* — impl (public) | Y `-ConstructDefault` | out: not-user-facing-api |
| *destructor* — `= default` | Y `Destroy` | —                            |
| *copy ctor / copy assign* — `= delete` | Y `-CopyConstruct` / `-CopyAssign` | —                          |
| *move ctor* — impl | Y `MoveConstruct` | —                            |
| *move assign* — impl | Y `MoveAssign` | —                            |
| free `operator==` / `operator!=` (cross-type `<T1>` vs `<T2>`) | Y `IsEqual` / `IsNotEqual` | —                            |
| free `swap(...)`, `MakeSampleAllocateePtr(...)` ×2 | N | out: duplicate               |
| `SampleAllocateePtrView` (3 members), `SampleAllocateePtrMutableView` (2) | N | out: internal                |

### `MethodSignatureElementPtr<SignatureElement>` → `MethodReturnTypePtr` / `MethodInArgTypePtr`
| C++ public member | puml | decision                 |
|---|---|--------------------------|
| `SignatureElement* get() const noexcept` | Y `get` | —                        |
| `operator->() const` | Y `MemberAccess` | —                        |
| `operator*() const` | Y `Dereference` | —                        |
| `std::size_t GetQueuePosition() const noexcept` | N | out: not-user-facing-api |
| `MethodSignatureElementPtr(SignatureElement&, bool&, std::size_t)` | N | out: not-user-facing-api |
| converting ctor from `MethodSignatureElementPtr<Other>&&` | Y `MoveConstructOtherType` | —                        |
| *default ctor* — suppressed | Y `-ConstructDefault` | —                        |
| *destructor* — impl | Y `Destroy` | —                        |
| *copy ctor / copy assign / move assign* — `= delete` | Y `-CopyConstruct` / `-CopyAssign` / `-MoveAssign` | —                      |
| *move ctor* — impl | Y `MoveConstruct` | —                        |

Naming note: the code alias is `MethodInArgPtr`; `types.h` renames it to
`MethodInArgTypePtr`. Template parameter is `Type` in code, `SignatureElement`
in the puml.

---

## 3. Typed Proxy

### `ProxyBase` (flattened into puml `Proxy`)
| C++ public member | puml | decision                |
|---|---|-------------------------|
| `static FindService(InstanceSpecifier)` | Y `FindServiceBySpecifier` | —                       |
| `static FindService(InstanceIdentifier)` | Y `FindServiceByIdentifier` | —                       |
| `static StartFindService(FindServiceHandler<HandleType>, InstanceSpecifier)` | Y | —                       |
| `static StartFindService(FindServiceHandler<HandleType>, InstanceIdentifier)` | Y | —                       |
| `static StopFindService(const FindServiceHandle)` | Y | —                       |
| `const HandleType& GetHandle() const&` | Y | — |
| `using HandleType` | N | out: compile-time       |
| `ProxyBase(std::unique_ptr<ProxyBinding>, HandleType)` | N | out: not-user-facing-api |
| *default ctor* — suppressed | Y `-ConstructDefault` (on `Proxy`) | — |
| *destructor* — `= default` | Y `Destroy` | —                       |
| *copy ctor / copy assign* — `= delete` | Y `-CopyConstruct` / `-CopyAssign` (on `Proxy`) | — |
| `ProxyBaseView` (6 public members) | N | out: internal           |

### `ProxyWrapperClass<Interface>` = `AsProxy` (puml `Proxy`)
| C++ public member | puml | decision      |
|---|---|---------------|
| `static Result<ProxyWrapperClass> Create(const HandleType&)` | Y `Create` | —             |
| *destructor* — impl | Y `Destroy` | —             |
| *copy ctor / copy assign* — `= delete` | Y `-CopyConstruct` / `-CopyAssign` | —           |
| *move ctor* — impl | Y `MoveConstruct` | —             |
| *move assign* — impl | Y `MoveAssign` | —             |
| `ProxyWrapperClassTestView` | N | out: internal |

### `ProxyEventBase` (flattened into `ProxyEvent` + `ProxyFieldWithNotifier` + `GenericProxyEvent`)
| C++ public member | puml | decision                |
|---|---|-------------------------|
| `Result<void> Subscribe(std::size_t)` | Y | —                       |
| `SubscriptionState GetSubscriptionState() const noexcept` | Y | —                       |
| `void Unsubscribe()` | Y | —                       |
| `Result<void> SetSubscriptionStateChangeHandler(SubscriptionStateChangeHandler)` | Y | —                       |
| `Result<void> UnsetSubscriptionStateChangeHandler()` | Y | —                       |
| `std::size_t GetFreeSampleCount() const noexcept` | Y | —                       |
| `Result<std::size_t> GetNumNewSamplesAvailable() const` | Y | —                       |
| `Result<void> SetReceiveHandler(EventReceiveHandler)` | Y | —                       |
| `Result<void> UnsetReceiveHandler()` | Y | —                       |
| **`bool IsBindingValid() const noexcept`** | **N — missing on all 3 derived classes** | out: deprecated         |
| `ProxyEventBase(std::string_view, Result<std::unique_ptr<ProxyEventBinding>>)` | N | out: not-user-facing-api                   |
| *default ctor* — suppressed | N | out: not-callable       |
| *destructor* — impl | Y `Destroy` (on `ProxyEvent`, `GenericProxyEvent`) | — |
| *copy ctor / copy assign* — `= delete` | N | out: not-callable       |
| *move ctor* — impl | Y `MoveConstruct` (on `ProxyEvent`) | — |
| *move assign* — impl | Y `MoveAssign` (on `ProxyEvent`) | — |
| inherited `GetReferenceToMoveable()` | N | out: not-user-facing-api |
| `ProxyEventBaseView` | N | out: internal           |

All six service element base classes — `ProxyEventBase`, `ProxyFieldBase`,
`ProxyMethodBase`, `SkeletonEventBase`, `SkeletonFieldBase`,
`SkeletonMethodBase` — derive **publicly** from
`EnableReferenceToMoveableFromThis<T>`, so its one public member
`GetReferenceToMoveable()` is callable on every service element. It is listed
once per base class below.

### `ProxyEvent<SampleDataType>`
| C++ public member | puml | decision          |
|---|---|-------------------|
| `template<F> Result<std::size_t> GetNewSamples(F&&, std::size_t)` | Y | —                 |
| `using SampleType` | N | out: compile-time |
| 3 constructors | Y `Construct` = `ProxyEvent(ProxyBase& base, const std::string_view event_name)`; the other 2 N | —                 |
| *default ctor* — suppressed | N | out: not-callable |
| *destructor / move ctor / move assign* — `= default` | Y `Destroy` / `MoveConstruct` / `MoveAssign` | —                 |
| *copy ctor / copy assign* — `= delete` | Y `-CopyConstruct` / `-CopyAssign` | —               |
| `InjectMock(IProxyEvent<SampleType>&)` | N | out: internal     |
| `ProxyEventView<SampleType>` | N | out: internal     |

### `ProxyField<SampleDataType, Tags...>` → `ProxyFieldWithNotifier` / `WithGetter` / `WithSetter`
Tag gating verified: the base operations are `protected` on `ProxyFieldBase`
and re-exposed per tag via `enable_if`. The per-tag split is accurate. Its
inherited `GetReferenceToMoveable()` is public — **N**.

| C++ public member | tag | puml | decision          |
|---|---|---|-------------------|
| `Subscribe(std::size_t)` | WithNotifier | Y | —                 |
| `Unsubscribe()` | WithNotifier | Y | —                 |
| `GetSubscriptionState()` | WithNotifier | Y | —                 |
| `GetFreeSampleCount()` | WithNotifier | Y | —                 |
| `GetNumNewSamplesAvailable()` | WithNotifier | Y | —                 |
| `SetReceiveHandler(EventReceiveHandler)` | WithNotifier | Y | —                 |
| `UnsetReceiveHandler()` | WithNotifier | Y | —                 |
| `SetSubscriptionStateChangeHandler(...)` | WithNotifier | Y | —                 |
| `UnsetSubscriptionStateChangeHandler()` | WithNotifier | Y | —                 |
| `GetNewSamples(ReceiverType&&, std::size_t)` | WithNotifier | Y | —                 |
| `Result<MethodReturnTypePtr<T>> Get()` | WithGetter | Y | —                 |
| `Result<MethodReturnTypePtr<T>> Set(const SampleDataType&)` | WithSetter | Y | —                 |
| `InjectMock(IProxyEvent<FieldType>&)` | WithNotifier | N | out: internal     |
| `using FieldType` | — | N | out: compile-time |
| ctors ×2 | — | Y `Construct` = `ProxyFieldImpl(ProxyBase& proxy_base, const std::string_view field_name)`; the other N | —                 |
| *default ctor* — suppressed | — | N | out: not-callable |
| *destructor / move ctor / move assign* — `= default` | — | Y `Destroy` / `MoveConstruct` / `MoveAssign` | —                 |
| *copy ctor / copy assign* — `= delete` | — | Y `-CopyConstruct` / `-CopyAssign` | —               |
| `ProxyFieldAttorney<FieldType, Tags...>` | — | N | out: internal     |

The members above are declared on `ProxyFieldImpl`. `ProxyField` is a `final`
wrapper that derives from it, declares no special member of its own and only
inherits the other constructors, so the compiler derives its special members
from the base:

| `ProxyField` special member | tag | puml | decision |
|---|---|---|---|
| *default ctor / copy ctor / copy assign* — impl-del | — | N | out: not-callable |
| *destructor / move ctor / move assign* — implicit | — | N | —  |

### `ProxyMethod` — 4 partial specializations
| Specialization | C++ public member | puml | decision          |
|---|---|---|-------------------|
| `void(ArgTypes...)` | `Allocate()` | Y | —                 |
| | `operator()(const ArgTypes&...)` | Y `InvokeWithArgs` | —                 |
| | `operator()(MethodInArgPtr<ArgTypes>...)` | Y `InvokeWithAllocatedArgs` | —                 |
| `ReturnType(ArgTypes...)` | `Allocate()` | Y | —                 |
| | `operator()(const ArgTypes&...)` | Y `InvokeWithArgs` | —                 |
| | `operator()(MethodInArgPtr<ArgTypes>...)` | Y `InvokeWithAllocatedArgs` | —                 |
| `ReturnType()` | `operator()()` | Y `Invoke` | —                 |
| `void()` | `operator()()` | Y `Invoke` | —                 |
| all 4 | `Result<void> InitializeInArgsAndReturnValues(ProxyBinding&)` (public override) | N | out: internal     |
| all 4 | ctors (2–3 each) | Y `Construct` = `ProxyMethod(ProxyBase& proxy_base, std::string_view method_name)`; the others N | —                 |
| all 4 | *default ctor* — suppressed | N | out: not-callable |
| all 4 | *destructor / move ctor / move assign* — `= default` | Y `Destroy` / `MoveConstruct` / `MoveAssign` | —                 |
| all 4 | *copy ctor / copy assign* — `= delete` | Y `-CopyConstruct` / `-CopyAssign` | —               |
| primary | `static_assert` only — intentionally no API | -- | —                 |

`ProxyMethodBase`: ctor, pure virtual `InitializeInArgsAndReturnValues` and
inherited `GetReferenceToMoveable()` — all **N**. Its special members are identical to those of `ProxyMethod` above.
`ProxyMethodView` (2 members), `ProxyMethodBaseView` — **N**.
Namespace-scope helpers in `proxy_method.h` — `DetermineNextAvailableQueueSlot`
×2, `InitializeArg<T>`, `InitializeInArgs<ArgTypes...>` — **N**.

---

## 4. Typed Skeleton

### `SkeletonBase` (flattened into puml `Skeleton`)
| C++ public member | puml | decision |
|---|---|---|
| `[[nodiscard]] Result<void> OfferService()` | Y | — |
| `void StopOfferService()` | Y | — |
| `using SkeletonEvents` / `SkeletonFields` / `SkeletonMethods` | N | out: compile-time |
| `SkeletonBase(std::unique_ptr<SkeletonBinding>, InstanceIdentifier)` | N | out: not-user-facing-api |
| *default ctor* — suppressed | N | out: not-callable |
| *destructor* — `= default` | Y `Destroy` | — |
| *copy ctor / copy assign* — `= delete` | N | out: not-callable |
| `void InjectMock(ISkeletonBase&)` | N | out: internal |
| `SkeletonBaseView` (6 members) | N | out: internal |
| free `GetInstanceIdentifier(const InstanceSpecifier&)` | N | out: internal |

### `SkeletonWrapperClass<Interface>` = `AsSkeleton` (puml `Skeleton`)
| C++ public member | puml | decision      |
|---|---|---------------|
| `static Create(const InstanceSpecifier&)` | Y `CreateBySpecifier` | —             |
| `static Create(const InstanceIdentifier&)` | Y `CreateByIdentifier` | —             |
| *default ctor* — suppressed | Y `-ConstructDefault` | —             |
| *destructor* — impl | Y `Destroy` | —             |
| *copy ctor / copy assign* — `= delete` | Y `-CopyConstruct` / `-CopyAssign` | —           |
| *move ctor* — impl | Y `MoveConstruct` | —             |
| *move assign* — impl | Y `MoveAssign` | —             |
| `SkeletonWrapperClassTestView` | N | out: internal |

### `SkeletonEventBase`
| C++ public member | puml | decision                 |
|---|---|--------------------------|
| `Result<void> PrepareOffer() noexcept` | N | out: not-user-facing-api |
| `void PrepareStopOffer() noexcept` | N | out: not-user-facing-api |
| ctor | N | out                      |
| *default ctor* — suppressed | N | out: not-callable        |
| *destructor / move ctor / move assign* — `= default` | N | out                      |
| *copy ctor / copy assign* — `= delete` | N | out: not-callable        |
| inherited `GetReferenceToMoveable()` | N | out: not-user-facing-api |
| `SkeletonEventBaseView` (3 members) | N | out: internal            |

### `SkeletonEvent<SampleDataType>`
| C++ public member | puml | decision                 |
|---|---|--------------------------|
| `Result<void> Send(const EventType&)` | Y `Send` | —                        |
| `Result<void> Send(SampleAllocateePtr<EventType>)` | Y `SendAllocated` | —                        |
| `Result<SampleAllocateePtr<EventType>> Allocate() noexcept` | Y | —                        |
| inherited `Result<void> PrepareOffer() noexcept` | N | out: not-user-facing-api |
| inherited `void PrepareStopOffer() noexcept` | N | out: not-user-facing-api |
| `using EventType` | N | out: compile-time        |
| 3 constructors | Y `Construct` = `SkeletonEvent(SkeletonBase& skeleton_base, const std::string_view event_name)`; the other 2 N | —                        |
| *default ctor* — suppressed | N | out: not-callable        |
| *destructor / move ctor / move assign* — `= default` | Y `Destroy` / `MoveConstruct` / `MoveAssign` | —                        |
| *copy ctor / copy assign* — `= delete` | Y `-CopyConstruct` / `-CopyAssign` | —                      |
| `InjectMock(ISkeletonEvent<EventType>&)` | N | out: internal            |
| `SkeletonEventView<T>` | N | out: internal            |

### `SkeletonField<SampleDataType, Tags...>` → `SkeletonFieldWithNotifierOrGetter` / `WithSetter`
| C++ public member | tag gate in C++ | puml | decision          |
|---|---|---|-------------------|
| `Result<void> Update(const FieldType&)` | **none — always available** | Y on both `SkeletonFieldWithNotifierOrGetter` and `SkeletonFieldWithSetter` | —                 |
| `Result<void> Update(SampleAllocateePtr<FieldType>)` | **none** | Y on both `SkeletonFieldWithNotifierOrGetter` and `SkeletonFieldWithSetter` as `UpdateAllocated` | —                 |
| `Result<SampleAllocateePtr<FieldType>> Allocate()` | **none** | Y on both `SkeletonFieldWithNotifierOrGetter` and `SkeletonFieldWithSetter` | —                 |
| `RegisterSetHandler(CallableType&&)` | `WithSetter` | Y | —                 |
| `InjectMock(ISkeletonField<FieldType>&)` | none | N | out: internal     |
| `using FieldType` | — | N | out: compile-time |
| ctors ×2 | — | Y `Construct` = `SkeletonFieldImpl(SkeletonBase& parent, const std::string_view field_name)`; the other N | —                 |
| *default ctor* — suppressed | — | N | out: not-callable |
| *destructor / move ctor / move assign* — `= default` | — | Y `Destroy` / `MoveConstruct` / `MoveAssign` | —                 |
| *copy ctor / copy assign* — `= delete` | — | Y `-CopyConstruct` / `-CopyAssign` | —               |

**Mismatch:** the diagram implies `Update` / `UpdateAllocated` / `Allocate` are
gated on `WithNotifier` or `WithGetter`. In the code they are unconditional
public members of every `SkeletonField`, including a setter-only field. Either
the gating is missing in the code, or the element name overstates it.

`SkeletonFieldBase`: ctor, `PrepareOffer()`, `PrepareStopOffer()` and inherited
`GetReferenceToMoveable()` — all **N**.
Its special members are identical to those of `SkeletonFieldImpl` above.

The members above are declared on `SkeletonFieldImpl`. `SkeletonField` is a
`final` wrapper that derives from it, declares no special member of its own and
only inherits the other constructors, so the compiler derives its special
members from the base:

| `SkeletonField` special member | tag gate in C++ | puml | decision |
|---|---|---|---|
| *default ctor / copy ctor / copy assign* — impl-del | — | N | out: not-callable |
| *destructor / move ctor / move assign* — implicit | — | N | —  |

`SkeletonFieldBaseView` — **N**.

### `SkeletonMethod<ReturnType(ArgTypes...)>`
| C++ public member | puml | decision          |
|---|---|-------------------|
| `template<Callable> Result<void> RegisterHandler(Callable&&)` | Y — but the diagram shows **4** named overloads (`RegisterHandlerWithInArgsAndReturn`, `WithInArgs`, `WithReturnType`, `WithoutInArgsOrReturn`); the code has exactly **one** template | —                 |
| `using MethodType` | N | out: compile-time |
| 3 constructors | Y `Construct` = `SkeletonMethod(SkeletonBase& skeleton_base, const std::string_view method_name)`; the other 2 N | —                 |
| *default ctor* — suppressed | N | out: not-callable |
| *destructor / move ctor / move assign* — `= default` | Y `Destroy` / `MoveConstruct` / `MoveAssign` | —                 |
| *copy ctor / copy assign* — `= delete` | Y `-CopyConstruct` / `-CopyAssign` | —               |
| free `InvokeWithOptionalQuality<WithQuality, Callable, Ts...>` | N | out: internal     |
| primary template `SkeletonMethod<Signature>` (`static_assert`) | -- | —                 |

`SkeletonMethodBase`: ctor and inherited `GetReferenceToMoveable()` — both
**N**. Its special members are identical to those of `SkeletonMethod` above.
`SkeletonMethodBaseView` — **N**.

---

## 5. Generic Proxy

### `GenericProxy`
| C++ public member | puml | decision          |
|---|---|-------------------|
| `static Result<GenericProxy> Create(HandleType)` | Y | —                 |
| `EventMapView GetEvents() const noexcept` | Y | —                 |
| inherited `ProxyBase` statics (FindService ×2, StartFindService ×2, StopFindService) | Y | —                 |
| inherited `GetHandle()` | Y | —                 |
| `using EventMapView = ServiceElementMapView<GenericProxyEvent>` | N | out: compile-time |
| *default ctor* — suppressed | Y `-ConstructDefault` | —                 |
| *destructor* — impl | Y `Destroy` | —                 |
| *copy ctor / copy assign* — `= delete` | Y `-CopyConstruct` / `-CopyAssign` | —               |
| *move ctor* — impl | Y `MoveConstruct` | —                 |
| *move assign* — impl | Y `MoveAssign` | —                 |
| `test::GenericProxyAttorney` | N | out: internal     |

### `GenericProxyEvent`
| C++ public member | puml | decision                 |
|---|---|--------------------------|
| `template<F> Result<std::size_t> GetNewSamples(F&&, std::size_t)` | Y | —                        |
| `memory::DataTypeSizeInfo GetDataTypeSizeInfo() const` | Y | —                        |
| `bool HasSerializedFormat() const noexcept` | Y | —                        |
| inherited `ProxyEventBase` operations (9) | Y | —                        |
| inherited `IsBindingValid()` | **N** | out: deprecated          |
| `GenericProxyEvent(std::string_view, std::unique_ptr<GenericProxyEventBinding>)` | N | out: not-user-facing-api |
| `GenericProxyEvent(ProxyBase&, std::string_view)` | Y `Construct` | —                        |
| *default ctor* — suppressed | Y `-ConstructDefault` | —                        |
| *destructor* — `= default` | Y `Destroy` | —                        |
| *copy ctor / copy assign / move ctor / move assign* — `= delete` | Y `-CopyConstruct` / `-CopyAssign` / `-MoveConstruct` / `-MoveAssign` | —                     |

All four copy/move members are `= delete`: the class is fully immovable,
unlike typed `ProxyEvent`.

---

## 6. Generic Skeleton

### `GenericSkeleton`
| C++ public member | puml | decision          |
|---|---|-------------------|
| `static Create(const InstanceIdentifier&, const GenericSkeletonServiceElementInfo&)` | Y `CreateByIdentifier` | —                 |
| `static Create(const InstanceSpecifier&, const GenericSkeletonServiceElementInfo&)` | Y `CreateBySpecifier` | —                 |
| `EventMapView GetEvents() const noexcept` | Y | —                 |
| `Result<void> OfferService()` | Y | —                 |
| `void StopOfferService()` | Y | —                 |
| `using EventMapView = ServiceElementMapView<GenericSkeletonEvent>` | N | out: compile-time |
| *default ctor* — suppressed | Y `-ConstructDefault` | —                 |
| *destructor* — implicit | Y `Destroy` | —                 |
| *copy ctor / copy assign* — impl-del | Y `-CopyConstruct` / `-CopyAssign` | —              |
| *move ctor / move assign* — implicit | Y `MoveConstruct` / `MoveAssign` | —                 |

### `GenericSkeletonEvent`
| C++ public member | puml | decision                 |
|---|---|--------------------------|
| `Result<void> Send(SampleAllocateePtr<void>)` | Y | —                        |
| `Result<SampleAllocateePtr<void>> Allocate() noexcept` | Y | —                        |
| `Result<void> Notify()` | Y | —                        |
| `DataTypeMetaInfo GetSizeInfo() const noexcept` | Y | —                        |
| `Result<void> SetReceiveHandlerRegistrationChangedHandler(ReceiveHandlerRegistrationChangedCallback)` | Y | —                        |
| `Result<void> UnsetReceiveHandlerRegistrationChangedHandler()` | Y | —                        |
| inherited `Result<void> PrepareOffer() noexcept` | N | out: not-user-facing-api |
| inherited `void PrepareStopOffer() noexcept` | N | out: not-user-facing-api |
| `GenericSkeletonEvent(SkeletonBase&, std::string_view, std::unique_ptr<SkeletonEventBinding>)` | N | out: not-user-facing-api |
| *default ctor* — suppressed | Y `-ConstructDefault` | —                        |
| *destructor / move ctor / move assign* — implicit | Y `Destroy` / `MoveConstruct` / `MoveAssign` | —                        |
| *copy ctor / copy assign* — impl-del | Y `-CopyConstruct` / `-CopyAssign` | —                     |

### Data structs
| C++ | puml | decision |
|---|---|---|
| `DataTypeMetaInfo::size` / `::alignment` | Y both | — |
| `EventInfo::name` / `::data_type_meta_info` | Y both | — |
| `GenericSkeletonServiceElementInfo::events` | Y | — |
| free `MakeDataTypeSizeInfo(const DataTypeMetaInfo&) -> Result<memory::DataTypeSizeInfo>` | **N** | out: internal |

### `ServiceElementMapView<Value>` → puml `EventMapView`
| C++ public member | puml | decision          |
|---|---|-------------------|
| `size_type size() const noexcept` | Y `Size` | —                 |
| `bool empty() const noexcept` | Y `IsEmpty` | —                 |
| `iterator find(const key&) noexcept` | ~ `Find` — puml returns `GenericProxyEvent*`, code returns `iterator` | fix |
| `const_iterator find(const key&) const noexcept` | **N** second overload | model |
| `iterator begin() const noexcept` | ~ `Begin` — same return-type mismatch | fix |
| `iterator end() const noexcept` | ~ `End` — same | fix |
| `const_iterator cbegin() const noexcept` | **N** | model |
| `const_iterator cend() const noexcept` | **N** | model |
| `using key` / `value_type` / `iterator` / `const_iterator` / `size_type` | N | out: compile-time |
| *default ctor* — suppressed | Y `-ConstructDefault` | — |
| *destructor / copy ctor / copy assign / move ctor / move assign* — implicit | N | out: generated    |
| `ServiceElementMapViewFactory<ElementType>::Create` | N | out: internal     |

---

## 7. Runtime (outside the `types.h` closure)

`runtime.h` and `runtime_configuration.h` are **not** transitively reachable
from `types.h`.

### `score::mw::com::runtime` free functions
| C++ | puml | decision |
|---|---|---|
| `Result<InstanceIdentifierContainer> ResolveInstanceIDs(const InstanceSpecifier&)` | Y | — |
| `void InitializeRuntime(cpp::span<safecpp::zstring_view>)` | Y `InitializeRuntimeFromCommandLine` | — |
| `void InitializeRuntime(const RuntimeConfiguration&)` | Y `InitializeRuntimeFromConfiguration` | — |
| `Result<void> AddConfiguration(const RuntimeConfiguration&)` | Y | — |
| `Result<void> AddConfiguration(score::json::Any)` | Y `AddConfigurationFromJson` | — |
| `void InitializeRuntime(std::int32_t argc, const char* argv[])` (deprecated) | N | out: deprecated |
| `detail::RuntimeMockHolder::GetRuntimeMock()` | N | out: internal |

### `RuntimeConfiguration`
| C++ public member | puml | decision        |
|---|---|-----------------|
| `explicit RuntimeConfiguration(cpp::span<safecpp::zstring_view>)` | Y `CreateFromCommandLine` | —               |
| `explicit RuntimeConfiguration(filesystem::Path)` | Y `CreateFromPath` | —               |
| `const filesystem::Path& GetConfigurationPath() const&` | Y | —               |
| `RuntimeConfiguration()` (default ctor) | Y `ConstructDefault` | — |
| `RuntimeConfiguration(std::int32_t, const char*[])` (deprecated) | N | out: deprecated |
| *default ctor* — impl | Y `ConstructDefault` | — |
| *destructor / copy ctor / copy assign / move ctor / move assign* — implicit | N | out: generated    |

---

## 8. Public C++ elements with no puml element at all

| C++ element | header | in `types.h`? |
|---|---|---|
| `ComErrc` | `com_error_domain.h` | no |
| `ComErrorDomain` | `com_error_domain.h` | no |
| `ServiceElementType` enum + its `operator<<` | `impl/service_element_type.h` | no |
| `ProxyTrait` (`Base`, `Event`, `Field`, `Method`) | `impl/traits.h` | **yes, transitively** |
| `SkeletonTrait` (`Base`, `Event`, `Field`, `Method`) | `impl/traits.h` | **yes, transitively** |
| `ProxyWithSemiDynamicMethodFactory<T>::Create` | `impl/traits.h` | **yes, transitively** |
| `MakeDataTypeSizeInfo` | `impl/data_type_meta_info.h` | **yes, transitively** |
| `contains_type`, `is_tag_enabled` | `impl/field_tags.h` | **yes, transitively** |
| `make_InstanceIdentifier`, `make_HandleType`, `make_FindServiceHandle` | respective | **yes, transitively** |
| `CreateSamplePtrFromSampleAllocateePtr` | `plumbing/sample_ptr.h` | **yes, transitively** |
| `MakeSampleAllocateePtr` ×2 | `plumbing/sample_allocatee_ptr.h` | **yes, transitively** |
| `GetInstanceIdentifier(const InstanceSpecifier&)` | `impl/skeleton_base.h` | **yes, transitively** |
| 13 `*View` / `*Attorney` classes | various | **yes, transitively** |
| `runtime::InjectRuntimeMock` + mock types | `test_types.h` | no |
| `impl::Runtime` (whole class, 12 public members) | `impl/runtime.h` | no |

---

## 9. puml elements with no C++ counterpart

| puml element | reality |
|---|---|
| `SampleReceiver<SampleType>` `<<callback>>` | no type — any callable accepted |
| `SetHandler<FieldType>` `<<callback>>` | no type — any callable accepted |
| `MethodHandler<ReturnType, ArgTypes>` `<<callback>>` | no type — any callable accepted |
| `MethodInArgTypePtrs` | no type — `std::tuple<MethodInArgPtr<ArgTypes>...>` |
| `CommandLineArguments` | no type — `(argc, argv)` / `span<zstring_view>` |
| `runtime` `<<free_functions>>` | namespace, not a class |
| `Result<T>` | `score::Result` — external |
| `Path`, `Json`, `DataTypeSizeInfo` | `score::filesystem::Path`, `score::json::Any`, `score::memory::DataTypeSizeInfo` — external |
| `SkeletonMethod::RegisterHandlerWith*` (4 ops) | one `RegisterHandler` template in code |
| `GenericSkeleton::Destroy()` | no destructor declared in code |
| `SkeletonFieldWithNotifierOrGetter` gating of `Update`/`Allocate` | not gated in code |
| `ProxyFieldWithNotifier::GetSubscriptionState/GetFreeSampleCount/GetNumNewSamplesAvailable` non-`{query}` | matches code (non-const) — asymmetry vs `ProxyEvent` is real |

---

## 10. Gap summary

**Operations missing from the model that exist and are callable:**

1. `ProxyEventBase::IsBindingValid()` → `ProxyEvent`, `GenericProxyEvent`
2. `ServiceElementMapView::cbegin()`, `cend()`, const `find()` overload
3. `SamplePtr::get()` (lowercase duplicate of `Get()`)
4. `MethodSignatureElementPtr::GetQueuePosition()`
5. `MakeDataTypeSizeInfo()`
6. `RuntimeConfiguration()` default constructor
7. `SkeletonEventBase::PrepareOffer/PrepareStopOffer`, `SkeletonFieldBase::PrepareOffer/PrepareStopOffer`
8. `ProxyMethod*::InitializeInArgsAndReturnValues()` (public on all 4 specializations)
9. `SkeletonBase::InjectMock`, `ProxyEvent/ProxyField/SkeletonEvent/SkeletonField::InjectMock`
10. `ProxyWithSemiDynamicMethodFactory::Create`
11. `operator<<` on `InstanceSpecifier`, `FindServiceHandle` ×2, `ServiceElementType`
12. free `swap()` on both sample pointers; `operator==`/`!=` on both; the 5 `make_*` factories
13. deprecated: `InitializeRuntime(argc,argv)`, `RuntimeConfiguration(argc,argv)`, `InstanceSpecifier::Create(T&&)`

**Model/code mismatches:**

1. `EventMapView` return types — iterators, not `GenericProxyEvent*`
2. `SampleAllocateePtr::reset()` vs modelled `Reset()`; `MethodReturnTypePtr::get()` vs `Get()`
3. `SkeletonField` `Update`/`UpdateAllocated`/`Allocate` are not tag-gated
4. `SkeletonMethod` — 4 modelled overloads vs 1 real template
5. `GenericSkeleton::Destroy()` has no declared destructor

**Special-member policy, entirely absent from the model.** Across the 29
classes, none of the 174 special members is modelled apart from the eight
destructors surfaced as `Destroy()`, and they follow seven distinct copy/move
policies, not one:

- immovable and uncopyable — `GenericProxyEvent`;
- move-only with defaulted moves — `SamplePtr`, `ProxyEvent`, `SkeletonEvent`,
  `ProxyMethod`, `SkeletonMethod`, `ProxyFieldImpl`, `SkeletonFieldImpl`;
- move-only with hand-written moves — `SampleAllocateePtr`, `ProxyEventBase`,
  `GenericProxy`, `ProxyWrapperClass`, `SkeletonWrapperClass`;
- move-constructible but not move-assignable — `MethodSignatureElementPtr`;
- fully copyable value types — `InstanceIdentifier`, `HandleType`,
  `FindServiceHandle`, `InstanceSpecifier`;
- base classes whose moves are `protected` — `ProxyBase`, `SkeletonBase`,
  `SkeletonEventBase`, `SkeletonFieldBase`;
- classes that declare nothing and inherit a deleted copy — `ProxyField`,
  `SkeletonField`, `GenericSkeleton`, `GenericSkeletonEvent`.

The entries most worth a safety review are:

1. `MethodSignatureElementPtr` — move **assignment** is `= delete` while its
   move **constructor** is implemented; it is the only type in the API that is
   move-constructible but not move-assignable.
2. `GenericProxyEvent` — all four copy/move members are `= delete`, making it
   fully immovable, unlike its typed counterpart `ProxyEvent`.
3. `ProxyField`, `SkeletonField`, `GenericSkeleton`, `GenericSkeletonEvent` —
   copy construction and copy assignment are implicitly **defined as deleted**
   (`impl-del`) purely through the base class; nothing at the class itself says
   so.
4. Twenty classes have **no default constructor at all** (`suppressed`, a
   silent consequence of declaring another constructor); only
   `InstanceIdentifier`, `HandleType` and `FindServiceHandle` say so with
   `= delete`.
5. `GenericSkeleton` and `GenericSkeletonEvent` declare no destructor, yet
   `public_api.puml` models `Destroy()` on `GenericSkeleton`.

**Types reachable but not exported by `types.h`:** `SubscriptionStateChangeHandler`,
`ServiceInstanceId`, `ServiceInstanceDeployment`, `ServiceTypeDeployment`,
`ProxyEvent`, `ProxyField`, `ProxyMethod`, `SkeletonEvent`, `SkeletonField`,
`SkeletonMethod`, `ServiceElementMapView`.
