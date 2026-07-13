/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

use proc_macro::TokenStream;
use quote::quote;
use syn::spanned::Spanned;
use syn::{parse_macro_input, Data, DeriveInput, Fields, Type};

/// The macro generates a validator struct with phantom type parameters that track
/// both the initial value update AND handler registration of each field at compile time.
/// The `offer()` method is only available when all fields have been initialized AND
/// all handlers have been registered, preventing runtime errors.
///
/// # Usage
///
/// Apply this macro alongside the `interface!` macro for Field-based interfaces:
///
/// ```ignore
/// #[derive(TypeStateFieldValidator)]
/// struct VehicleFieldProducerInternal<R: Runtime> {
///     left_tire: R::FieldPublisher<Tire>,
///     exhaust: R::FieldPublisher<Exhaust>,
/// }
/// ```
///
/// The macro generates:
/// - `Uninit`/`Init` marker types for update state
/// - `HandlerNotSet`/`HandlerSet` marker types for handler registration state
/// - `ValidatorN<'a, R, S0, S1, ..., H0, H1, ...>` struct with dual type-state tracking
/// - `update_field_name()` methods that transition update state from Uninit -> Init
/// - `register_set_handler_field_name()` methods that transition handler state from HandlerNotSet -> HandlerSet
/// - `offer()` method only available when all fields are Init AND all handlers are HandlerSet
pub fn derive_typestate_field_validator_impl(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    let name = &input.ident;

    // Extract runtime generic parameter
    let (runtime_param_name, runtime_param_with_bounds) =
        if let Some(param) = input.generics.params.first() {
            match param {
                syn::GenericParam::Type(type_param) => {
                    let name = &type_param.ident;
                    (quote! { #name }, quote! { #param })
                }
                _ => (quote! { R }, quote! { R: com_api::Runtime + ?Sized }),
            }
        } else {
            (quote! { R }, quote! { R: com_api::Runtime + ?Sized })
        };

    let fields = match &input.data {
        Data::Struct(data) => match &data.fields {
            Fields::Named(fields) => &fields.named,
            _ => {
                return syn::Error::new_spanned(
                    name,
                    "TypeStateFieldValidator only supports structs with named fields",
                )
                .to_compile_error()
                .into();
            }
        },
        _ => {
            return syn::Error::new_spanned(name, "TypeStateFieldValidator only supports structs")
                .to_compile_error()
                .into();
        }
    };

    // Extract field information - use all fields except instance_info
    let field_info: Vec<_> = fields
        .iter()
        .filter_map(|f| {
            let ident = f.ident.as_ref()?;

            // Skip instance_info field
            // Note: type name is using here as we have same name in interface_macros
            // If that change then this also need to be updated.
            if ident == "instance_info" {
                return None;
            }

            Some((
                ident, // struct field name
                ident, // public field name (same as struct field)
                &f.ty, // field type
            ))
        })
        .collect();

    if field_info.is_empty() {
        return syn::Error::new_spanned(
            name,
            "No fields found for validation (excluding instance_info)",
        )
        .to_compile_error()
        .into();
    }

    let struct_field_names: Vec<_> = field_info.iter().map(|(sf, _, _)| sf).collect();
    let public_field_names: Vec<_> = field_info.iter().map(|(_, pf, _)| pf).collect();
    let field_types: Vec<_> = field_info.iter().map(|(_, _, ty)| ty).collect();

    // Extract inner types from R::FieldPublisher<T> -> T
    let inner_types: Vec<_> = field_types
        .iter()
        .map(|ty| {
            // Try to extract T from R::FieldPublisher<T>
            if let Type::Path(type_path) = ty {
                // Look for the last segment which should be FieldPublisher<T>
                if let Some(segment) = type_path.path.segments.last() {
                    //Note: Same here we are using trait name directly
                    // But if that change then this also need to be updated.
                    if segment.ident == "FieldPublisher" {
                        // Extract the type argument
                        if let syn::PathArguments::AngleBracketed(args) = &segment.arguments {
                            if let Some(syn::GenericArgument::Type(inner_ty)) = args.args.first() {
                                return inner_ty;
                            }
                        }
                    }
                }
            }
            // Fallback: use the full type
            *ty
        })
        .collect();

    let validator_name = syn::Ident::new(&format!("{}Validator", name), name.span());

    // Generate type parameters for each field's UPDATE state (S0, S1, S2, ...)
    let field_update_state_params: Vec<_> = public_field_names
        .iter()
        .enumerate()
        .map(|(i, _)| syn::Ident::new(&format!("S{}", i), proc_macro::Span::call_site().into()))
        .collect();

    // Generate type parameters for each field's HANDLER state (H0, H1, H2, ...)
    let field_handler_state_params: Vec<_> = public_field_names
        .iter()
        .enumerate()
        .map(|(i, _)| syn::Ident::new(&format!("H{}", i), proc_macro::Span::call_site().into()))
        .collect();

    // Generate update methods - each one changes its field's UPDATE state from current to Init
    // while preserving HANDLER state
    let update_methods = public_field_names
        .iter()
        .zip(struct_field_names.iter())
        .zip(inner_types.iter())
        .enumerate()
        .map(|(i, ((pub_name, struct_name), inner_ty))| {
            let update_fn = syn::Ident::new(&format!("update_{}", pub_name), pub_name.span());

            // Build the "after" UPDATE state parameter list where this field is Init
            let after_update_states: Vec<_> = field_update_state_params
                .iter()
                .enumerate()
                .map(|(j, param)| {
                    if i == j {
                        quote! { ::com_api::Init }
                    } else {
                        quote! { #param }
                    }
                })
                .collect();

            quote! {
                impl<#runtime_param_with_bounds, #(#field_update_state_params),*, #(#field_handler_state_params),*>
                    #validator_name<#runtime_param_name, #(#field_update_state_params),*, #(#field_handler_state_params),*>
                {
                    pub fn #update_fn(
                        mut self,
                        value: &#inner_ty
                    ) -> com_api::Result<#validator_name<#runtime_param_name, #(#after_update_states),*, #(#field_handler_state_params),*>>
                    {
                        self.producer.#struct_name.update(value)?;
                        Ok(#validator_name {
                            producer: self.producer,
                            _phantom: core::marker::PhantomData,
                        })
                    }
                }
            }
        });

    // Generate register_set_handler methods - each one changes its field's HANDLER state
    // from HandlerNotSet to HandlerSet while preserving UPDATE state
    let register_handler_methods = public_field_names
        .iter()
        .zip(struct_field_names.iter())
        .zip(inner_types.iter())
        .enumerate()
        .map(|(i, ((pub_name, struct_name), inner_ty))| {
            let register_fn = syn::Ident::new(
                &format!("register_set_handler_{}", pub_name),
                pub_name.span(),
            );

            // Build the "after" HANDLER state parameter list where this field is HandlerSet
            let after_handler_states: Vec<_> = field_handler_state_params
                .iter()
                .enumerate()
                .map(|(j, param)| {
                    if i == j {
                        quote! { ::com_api::HandlerSet }
                    } else {
                        quote! { #param }
                    }
                })
                .collect();

            quote! {
                impl<#runtime_param_with_bounds, #(#field_update_state_params),*, #(#field_handler_state_params),*>
                    #validator_name<#runtime_param_name, #(#field_update_state_params),*, #(#field_handler_state_params),*>
                where
                    <#runtime_param_name as com_api::Runtime>::FieldPublisher<#inner_ty>: Send,
                {
                    pub fn #register_fn<F>(mut self, handler: F) -> com_api::Result<#validator_name<#runtime_param_name, #(#field_update_state_params),*, #(#after_handler_states),*>>
                    where
                        F: Fn(&#inner_ty) + Send + 'static,
                    {
                        self.producer.#struct_name.register_set_handler(handler)?;
                        Ok(#validator_name {
                            producer: self.producer,
                            _phantom: core::marker::PhantomData,
                        })
                    }
                }
            }
        });

    // Generate list of all Init states for the offer() impl
    let all_init_states = vec![quote! { ::com_api::Init }; field_update_state_params.len()];

    // Generate list of all HandlerSet states for the offer() impl
    let all_handler_set_states =
        vec![quote! { ::com_api::HandlerSet }; field_handler_state_params.len()];

    // Generate list of all Uninit states for the validator() method
    let all_uninit_states = vec![quote! { ::com_api::Uninit }; field_update_state_params.len()];

    // Generate list of all HandlerNotSet states for the validator() method
    let all_handler_not_set_states =
        vec![quote! { ::com_api::HandlerNotSet }; field_handler_state_params.len()];

    let expanded = quote! {
        // Validator struct with dual type-state tracking:
        // - First set of params (S0, S1, ...) track field UPDATE state (Uninit/Init)
        // - Second set of params (H0, H1, ...) track HANDLER registration state (HandlerNotSet/HandlerSet)
        pub struct #validator_name<#runtime_param_with_bounds, #(#field_update_state_params),*, #(#field_handler_state_params),*> {
            producer: #name<#runtime_param_name>,
            _phantom: core::marker::PhantomData<(#(#field_update_state_params,)* #(#field_handler_state_params,)*)>,
        }

        // Update methods that change UPDATE state types (Uninit -> Init)
        #(#update_methods)*

        // Register set handler methods that change HANDLER state types (HandlerNotSet -> HandlerSet)
        #(#register_handler_methods)*

        // offer() is only available when ALL fields are Init AND all handlers are HandlerSet
        impl<#runtime_param_with_bounds> #validator_name<#runtime_param_name, #(#all_init_states),*, #(#all_handler_set_states),*> {
            pub fn offer(self) -> com_api::Result<<#name<#runtime_param_name> as com_api::Producer<#runtime_param_name>>::OfferedProducer> {
                // Extract producer and call its offer() method
                use com_api::Producer;
                self.producer.offer()
            }
        }

        // validator() method consumes producer and returns validator with all fields Uninit and all handlers HandlerNotSet
        impl<#runtime_param_with_bounds> #name<#runtime_param_name> {
            pub fn validator(self) -> #validator_name<#runtime_param_name, #(#all_uninit_states),*, #(#all_handler_not_set_states),*> {
                #validator_name {
                    producer: self,
                    _phantom: core::marker::PhantomData,
                }
            }
        }
    };

    TokenStream::from(expanded)
}
