use std::mem::ManuallyDrop;

// @todo check whether we can get this info "somehow" from C++ code
type CustomDeleterAlignment = libc::max_align_t;
const CUSTOM_DELETER_SIZE: usize = 32;

#[repr(C)]
union CustomDeleterInner {
    _callback: [u8; CUSTOM_DELETER_SIZE],
    _align: [CustomDeleterAlignment; 0],
}

#[repr(C)]
struct WrapperBase {
    _dummy: [u8; 0],
}

#[repr(C)]
struct CustomDeleter {
    _inner: CustomDeleterInner,
    _wrapper: *mut WrapperBase,
}

#[repr(C)]
struct UniquePtr<T, D = ()> {
    _deleter: D,
    _ptr: *mut T,
}

type MockBinding<T> = UniquePtr<T, CustomDeleter>;

#[repr(C)]
union MockVariant<T> {
    _variant: ManuallyDrop<MockBinding<T>>,
}

#[repr(C)]
struct CxxOptional<T> {
    _data: T,
    _engaged: bool,
}

#[repr(C)]
struct EventDataControl {
    _dummy: [u8; 0],
}

type SlotIndexType = u16;
type TransactionLogIndex = u8;

#[repr(C)]
struct SlotDecrementer {
    _event_data_control: *mut EventDataControl,
    _event_slot_index: SlotIndexType,
    _transaction_log_idx: TransactionLogIndex,
}

#[repr(C)]
struct LolaBinding<T> {
    _managed_object: *const T,
    _slot_decrementer: CxxOptional<SlotDecrementer>,
}

#[repr(C)]
union LolaVariant<T> {
    _variant: ManuallyDrop<LolaBinding<T>>,
    _other: ManuallyDrop<MockVariant<T>>,
}

#[repr(C)]
struct BlankBinding {
    _data: [u8; 0],
}

#[repr(C)]
union BlankVariant<T> {
    _variant: ManuallyDrop<BlankBinding>,
    _other: ManuallyDrop<LolaVariant<T>>,
}

#[repr(C)]
struct BindingVariant<T> {
    _data: BlankVariant<T>,
    _index: u8,
}

#[repr(C)]
struct SampleReferenceTracker {
    _dummy: [u8; 0],
}

#[repr(C)]
struct SampleReferenceGuard {
    _tracker: *mut SampleReferenceTracker,
}

#[repr(C)]
pub struct SamplePtr<T> {
    _binding_sample_ptr: BindingVariant<T>,
    _sample_reference_guard: SampleReferenceGuard,
}
