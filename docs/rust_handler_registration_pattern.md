# Handler Registration Pattern in Rust

This document explains how to implement a flexible handler registration mechanism in Rust, similar to the C++ `RegisterHandler` template method that accepts any callable.

## C++ Pattern Overview

The C++ code uses:
```cpp
template <typename Callable>
Result<void> RegisterHandler(Callable&& callback);
```

This accepts any callable type (lambda, function pointer, functor) with perfect forwarding.

## Rust Equivalent Approaches

### Approach 1: Generic with Fn Trait Bounds (Recommended)

This is the most flexible and idiomatic Rust approach:

```rust
use com_api::Result;

pub struct SkeletonMethod<ReturnType, ArgTypes> {
    // Internal storage for type-erased handler
    handler: Option<Box<dyn FnMut(&mut ReturnType, &ArgTypes) + Send + 'static>>,
}

impl<ReturnType, ArgTypes> SkeletonMethod<ReturnType, ArgTypes> {
    /// Register a handler that will be called when the proxy invokes this method.
    ///
    /// The handler can be any callable (closure, function pointer, or callable struct)
    /// that matches the signature.
    ///
    /// # Type Parameters
    /// * `F` - Any callable type that implements the required signature
    ///
    /// # Parameters
    /// * `callback` - The handler function/closure to register
    ///
    /// # Examples
    /// ```
    /// // Using a closure
    /// method.register_handler(|ret, args| {
    ///     *ret = process(args);
    /// })?;
    ///
    /// // Using a function pointer
    /// fn my_handler(ret: &mut i32, args: &String) {
    ///     *ret = args.len() as i32;
    /// }
    /// method.register_handler(my_handler)?;
    /// ```
    pub fn register_handler<F>(&mut self, callback: F) -> Result<()>
    where
        F: FnMut(&mut ReturnType, &ArgTypes) + Send + 'static,
    {
        // Type-erase the callback by boxing it
        self.handler = Some(Box::new(callback));
        Ok(())
    }
}
```

**Key Features:**
- `F` is a generic type parameter (like C++ template)
- `FnMut` allows the callback to mutate captured variables
- `Send + 'static` ensures thread safety and proper lifetime
- Type erasure via `Box<dyn FnMut(...)>` for storage

### Approach 2: Multiple Argument Types (Tuple-based)

For methods with multiple arguments like `ReturnType(Arg1, Arg2, Arg3)`:

```rust
pub struct SkeletonMethod<ReturnType, ArgTuple> {
    handler: Option<Box<dyn FnMut(&mut ReturnType, ArgTuple) + Send + 'static>>,
}

impl<ReturnType, ArgTuple> SkeletonMethod<ReturnType, ArgTuple> {
    pub fn register_handler<F>(&mut self, callback: F) -> Result<()>
    where
        F: FnMut(&mut ReturnType, ArgTuple) + Send + 'static,
    {
        self.handler = Some(Box::new(callback));
        Ok(())
    }
}

// Usage example:
// let mut method: SkeletonMethod<i32, (String, f64, bool)> = ...;
// method.register_handler(|ret, (arg1, arg2, arg3)| {
//     *ret = process(arg1, arg2, arg3);
// })?;
```

### Approach 3: Separate Handler Trait (Advanced Pattern)

For more control and testability:

```rust
/// Trait for method handlers with specific signature
pub trait MethodHandler<ReturnType, ArgTypes>: Send {
    fn handle(&mut self, return_value: &mut ReturnType, args: &ArgTypes);
}

// Blanket implementation for all closures
impl<F, ReturnType, ArgTypes> MethodHandler<ReturnType, ArgTypes> for F
where
    F: FnMut(&mut ReturnType, &ArgTypes) + Send,
{
    fn handle(&mut self, return_value: &mut ReturnType, args: &ArgTypes) {
        self(return_value, args)
    }
}

pub struct SkeletonMethod<ReturnType, ArgTypes> {
    handler: Option<Box<dyn MethodHandler<ReturnType, ArgTypes>>>,
}

impl<ReturnType, ArgTypes> SkeletonMethod<ReturnType, ArgTypes> {
    pub fn register_handler<H>(&mut self, handler: H) -> Result<()>
    where
        H: MethodHandler<ReturnType, ArgTypes> + 'static,
    {
        self.handler = Some(Box::new(handler));
        Ok(())
    }
}
```

**Benefits:**
- Can mock handlers by implementing the trait
- More explicit about requirements
- Better for complex scenarios

### Approach 4: Macro-based (For Complex Signatures)

For generating methods with different arities:

```rust
macro_rules! impl_skeleton_method {
    ($($arg:ident),*) => {
        impl<ReturnType, $($arg),*> SkeletonMethod<ReturnType, ($($arg,)*)> {
            pub fn register_handler<F>(&mut self, mut callback: F) -> Result<()>
            where
                F: FnMut(&mut ReturnType, $(&$arg),*) + Send + 'static,
            {
                let type_erased = Box::new(move |ret: &mut ReturnType, args: &($($arg,)*)| {
                    #[allow(non_snake_case)]
                    let ($($arg,)*) = args;
                    callback(ret, $($arg),*);
                });
                
                self.handler = Some(type_erased);
                Ok(())
            }
        }
    };
}

// Generate implementations for 0-5 arguments
impl_skeleton_method!();
impl_skeleton_method!(A);
impl_skeleton_method!(A, B);
impl_skeleton_method!(A, B, C);
impl_skeleton_method!(A, B, C, D);
impl_skeleton_method!(A, B, C, D, E);
```

## Comparison with C++ Implementation

| Feature | C++ | Rust |
|---------|-----|------|
| Generic callable | `template<typename Callable>` | `fn register_handler<F>` |
| Perfect forwarding | `Callable&&` | Ownership transferred by default |
| Compile-time checks | `static_assert` | Trait bounds checked at compile time |
| Type erasure | Lambda + std::function | `Box<dyn Fn...>` |
| Lvalue/rvalue handling | Explicit branching | Automatic via move semantics |
| Thread safety | Manual | `Send` trait ensures it |

## Real-World Example

Here's a complete working example:

```rust
use std::sync::{Arc, Mutex};

pub struct SkeletonMethod<Ret, Args> {
    handler: Arc<Mutex<Option<Box<dyn FnMut(&mut Ret, &Args) + Send>>>>,
}

impl<Ret, Args> SkeletonMethod<Ret, Args> {
    pub fn new() -> Self {
        Self {
            handler: Arc::new(Mutex::new(None)),
        }
    }

    pub fn register_handler<F>(&mut self, callback: F) -> Result<()>
    where
        F: FnMut(&mut Ret, &Args) + Send + 'static,
    {
        let mut handler = self.handler.lock().unwrap();
        *handler = Some(Box::new(callback));
        Ok(())
    }

    pub fn invoke(&self, args: &Args) -> Result<Ret>
    where
        Ret: Default,
    {
        let mut handler = self.handler.lock().unwrap();
        if let Some(ref mut h) = *handler {
            let mut result = Ret::default();
            h(&mut result, args);
            Ok(result)
        } else {
            Err(Error::ServiceError(ServiceFailedReason::NoHandlerRegistered))
        }
    }
}

// Usage:
fn example() {
    let mut method: SkeletonMethod<i32, String> = SkeletonMethod::new();
    
    // Register with closure capturing environment
    let multiplier = 10;
    method.register_handler(move |ret, args| {
        *ret = args.len() as i32 * multiplier;
    }).unwrap();
    
    // Register with function pointer
    fn my_handler(ret: &mut i32, args: &String) {
        *ret = args.len() as i32;
    }
    method.register_handler(my_handler).unwrap();
}
```

## Best Practices

1. **Use `FnMut` for most cases** - Allows mutation of captured variables
2. **Add `Send` bound** - Ensures thread safety for concurrent access
3. **Use `'static` lifetime** - Handler outlives registration call
4. **Consider `FnOnce` for single-use handlers** - More flexible but can only be called once
5. **Type erase with `Box<dyn ...>`** - Store handlers of different types
6. **Use `Arc<Mutex<...>>` for shared handlers** - When multiple threads need access

## Summary

Rust provides multiple ways to accept "any callable" through:
- **Generic type parameters** with trait bounds (most common)
- **Trait objects** for type erasure
- **Macro generation** for complex patterns

The generic approach with `Fn`/`FnMut`/`FnOnce` traits is the Rust equivalent of C++ templates with `Callable&&`, providing compile-time type checking while maintaining flexibility.
