//field.rs
// Need to discuss:
// 1.Get and Set methods for field it is enabled based on tag, do we want to keep same kind of mechanism
// or by default we will enable for user, My suggestion is we can keep it default enable as of now,
// and later we can add tag based mechanism if required because Interface side we need to check how we can do this
use crate::*;
use std::fmt::Debug;

#[allow(dead_code)]

//Temp for build test
struct MethodReturnTypePtr<T: CommData + Debug> {
    pub value: T,
    pub status: Result<()>,
}

// we are using the event trait as a base trait for field, because we do offer same subscription APIs
// Only addation to that is get and set methods for field, so we can keep it as a base trait for field as well.
// FieldMethods trait is added to provide get and set methods for field.
pub trait FieldSubscriber<T: CommData + Debug, R: Runtime + ?Sized>:
    FieldMethods<T, R> + com_api_concept::Subscriber<T, R>
{
}

// Again we are using the event trait as a base trait for field, because we do offer same subscription APIs
// Like try_receive(GetNewSamples) and receive with async,
// Also we do need to provide get and set method because when subscription is created it takes the subscriber as a value.
// It has additional methods to get the number of new samples available and get the number of free sample count available for subscription.
pub trait FieldSubscription<T: CommData + Debug, R: Runtime + ?Sized>:
    com_api_concept::Subscription<T, R> + FieldMethods<T, R>
{
    /// Get the number of new samples available in the field subscription.
    /// This does not take max_samples parameter like try_receive, but returns the number of samples available to be received.
    fn get_num_new_samples_available(&self) -> Result<usize>;

    /// Get the number of sample count that can be received from the field subscription.
    fn get_free_sample_count(&self) -> Result<usize>;
}

/// FieldMethods trait provides get and set methods for field.
// MethodReturnTypePtr is like samplePtr for event.
pub trait FieldMethods<T: CommData + Debug, R: Runtime + ?Sized> {
    ///Get the current value of the field.
    ///
    /// #returns
    /// Return the result of `MethodReturnTypePtr<T>` which contains the current value of the field.
    fn get(&self) -> Result<MethodReturnTypePtr<T>>;

    ///Set the value of the field.
    ///
    /// # Parameters
    /// * `value` - The value to set for the field.
    ///
    /// # Returns
    /// Return the result of `MethodReturnTypePtr<T>` which contains the status of the set operation.
    /// with the current value of the field.
    fn set(&self, value: T) -> Result<MethodReturnTypePtr<T>>;
}

// We can not use publisher trait from event because that contains the Send Method which is not correct semantic for field.
// TODO: SampleMaybeUninit is return by allocated and that trait has write method but which return SampleMut
// which has send method, we can add Update method there but in that case that will be exposed to Event and still Send is exposed to Event.
// Tried to fix this issue making SampleMut trait as marker trait in concept and then implementing Event/Field specific SampleMut trait which has send/update method.
pub trait FieldPublisher<T: CommData + Debug, R: Runtime + ?Sized> {
    type CommittedSample<'a>: FieldSampleMut<T>
    where
        Self: 'a;

    type SampleMaybeUninit<'a>: SampleMaybeUninit<T, SampleMut = Self::CommittedSample<'a>> + 'a
    where
        Self: 'a;

    /// Create a new publisher for the specified event source.
    fn new(identifier: &str, instance_info: R::ProviderInfo) -> Result<Self>
    where
        Self: Sized;

    /// Allocate a buffer slot for the event publication.
    fn allocate(&self) -> Result<Self::SampleMaybeUninit<'_>>;

    /// Publish the event data to the field source.
    fn update(&self, value: T) -> Result<()>;

    /// Set a handler for the field publication from proxy set request.
    fn set_handler<'a>(&self) -> impl Future<Output = Result<T>> + 'a;
}

pub trait FieldSampleMut<T>: com_api_concept::SampleMut<T>
where
    T: CommData + Debug,
{
    fn update(self) -> Result<()>;
}
