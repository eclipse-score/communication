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
/// handler registration of each method at compile time.
/// The `offer()` method is only available when all handlers have been registered,
/// preventing runtime errors.
///
/// Unlike TypeStateFieldValidator which tracks both field updates and handler registration,
/// this macro only tracks handler registration state (single dimension).
///
/// It generates handler registration methods with concatenated names like `register_handler_<method_name>`.
/// e.g. for method `update_tire_pressure`, the generated method will be `register_handler_update_tire_pressure`.
///
// TODO: This has common part with TypeStateFieldValidator, we can refactor to avoid code duplication.
// consider creating a common function to extract method information and generate the validator struct.
pub fn derive_typestate_method_validator_impl(input: TokenStream) -> TokenStream {
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

    // Currently supporting only struct but in future if required will support enum.
    let fields = match &input.data {
        Data::Struct(data) => match &data.fields {
            Fields::Named(fields) => &fields.named,
            _ => {
                return syn::Error::new_spanned(
                    name,
                    "TypeStateMethodValidator only supports structs with named fields",
                )
                .to_compile_error()
                .into();
            }
        },
        _ => {
            return syn::Error::new_spanned(name, "TypeStateMethodValidator only supports structs")
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
            if ident == "instance_info" {
                return None;
            }

            // Extract Args and Return from either:
            // 1. MethodHandler<R, Args, Return> (old 3-param pattern)
            // 2. R::MethodHandler<Args, Return> (new 2-param associated type pattern)
            let (args_ty, return_ty) = if let Type::Path(type_path) = &f.ty {
                if let Some(segment) = type_path.path.segments.last() {
                    if segment.ident == "MethodHandler" {
                        if let syn::PathArguments::AngleBracketed(args) = &segment.arguments {
                            // Check for both patterns
                            match args.args.len() {
                                // Old pattern: MethodHandler<R, Args, Return>
                                // Why three and two pattern
                                3 => {
                                    if let (
                                        Some(syn::GenericArgument::Type(_)),
                                        Some(syn::GenericArgument::Type(args_ty)),
                                        Some(syn::GenericArgument::Type(return_ty)),
                                    ) = (args.args.get(0), args.args.get(1), args.args.get(2))
                                    {
                                        (args_ty.clone(), return_ty.clone())
                                    } else {
                                        return None;
                                    }
                                }
                                // New pattern: R::MethodHandler<Args, Return>
                                2 => {
                                    if let (
                                        Some(syn::GenericArgument::Type(args_ty)),
                                        Some(syn::GenericArgument::Type(return_ty)),
                                    ) = (args.args.get(0), args.args.get(1))
                                    {
                                        (args_ty.clone(), return_ty.clone())
                                    } else {
                                        return None;
                                    }
                                }
                                _ => return None,
                            }
                        } else {
                            return None;
                        }
                    } else {
                        return None;
                    }
                } else {
                    return None;
                }
            } else {
                return None;
            };

            Some((
                ident,     // struct field name (method name)
                ident,     // public method name (same as struct field)
                args_ty,   // Args type parameter
                return_ty, // Return type parameter
            ))
        })
        .collect();

    if field_info.is_empty() {
        return syn::Error::new_spanned(
            name,
            "No methods found for validation (excluding instance_info)",
        )
        .to_compile_error()
        .into();
    }

    let struct_field_names: Vec<_> = field_info.iter().map(|(sf, _, _, _)| sf).collect();
    let public_method_names: Vec<_> = field_info.iter().map(|(_, pf, _, _)| pf).collect();
    let args_types: Vec<_> = field_info.iter().map(|(_, _, args, _)| args).collect();
    let return_types: Vec<_> = field_info.iter().map(|(_, _, _, ret)| ret).collect();

    // Generate the validator struct name - e.g., for VehicleMethodsProducer, the validator will be VehicleMethodsProducerValidator
    let validator_name = syn::Ident::new(&format!("{}Validator", name), name.span());

    // Generate type parameters for each method's HANDLER state (H0, H1, H2, ...)
    let handler_state_params: Vec<_> = public_method_names
        .iter()
        .enumerate()
        .map(|(i, _)| syn::Ident::new(&format!("H{}", i), proc_macro::Span::call_site().into()))
        .collect();

    // Generate register_handler methods - each one changes its method's HANDLER state
    // from HandlerNotSet to HandlerSet
    let register_handler_methods = public_method_names
        .iter()
        .zip(struct_field_names.iter())
        .zip(args_types.iter())
        .zip(return_types.iter())
        .enumerate()
        .map(|(i, (((pub_name, struct_name), args_ty), return_ty))| {
            let register_fn = syn::Ident::new(
                &format!("register_{}_handler", pub_name),
                pub_name.span(),
            );

            // Build the "after" HANDLER state parameter list where this method's handler is HandlerSet
            let after_handler_states: Vec<_> = handler_state_params
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
                impl<#runtime_param_with_bounds, #(#handler_state_params),*>
                    #validator_name<#runtime_param_name, #(#handler_state_params),*>
                {
                    pub fn #register_fn<F>(
                        mut self,
                        handler: F
                    ) -> com_api::Result<#validator_name<#runtime_param_name, #(#after_handler_states),*>>
                    where
                        F: com_api::MethodHandlerCall<#args_ty, #return_ty>,
                    {
                        <_ as com_api::MethodHandler<#args_ty, #return_ty, #runtime_param_name>>::register_handler(&self.producer.#struct_name, handler)?;
                        Ok(#validator_name {
                            producer: self.producer,
                            _phantom: core::marker::PhantomData,
                        })
                    }
                }
            }
        });

    // Generate list of all HandlerSet states for the offer() impl
    let all_handler_set_states = vec![quote! { ::com_api::HandlerSet }; handler_state_params.len()];

    // Generate list of all HandlerNotSet states for the init_handlers() method
    let all_handler_not_set_states =
        vec![quote! { ::com_api::HandlerNotSet }; handler_state_params.len()];

    let expanded = quote! {
        // Validator struct with type-state tracking for handler registration
        // Type params (H0, H1, ...) track HANDLER registration state (HandlerNotSet/HandlerSet)
        pub struct #validator_name<#runtime_param_with_bounds, #(#handler_state_params),*> {
            producer: #name<#runtime_param_name>,
            _phantom: core::marker::PhantomData<(#(#handler_state_params,)*)>,
        }

        // Register handler methods that change HANDLER state types (HandlerNotSet -> HandlerSet)
        #(#register_handler_methods)*

        // offer() is only available when ALL handlers are HandlerSet
        impl<#runtime_param_with_bounds> #validator_name<#runtime_param_name, #(#all_handler_set_states),*> {
            pub fn offer(self) -> com_api::Result<<#name<#runtime_param_name> as com_api::Producer<#runtime_param_name>>::OfferedProducer> {
                // Extract producer and call its offer() method
                use com_api::Producer;
                self.producer.offer()
            }
        }

        // init_handlers() method consumes producer and returns validator with all handlers HandlerNotSet
        impl<#runtime_param_with_bounds> #name<#runtime_param_name> {
            pub fn init_handlers(self) -> #validator_name<#runtime_param_name, #(#all_handler_not_set_states),*> {
                #validator_name {
                    producer: self,
                    _phantom: core::marker::PhantomData,
                }
            }
        }
    };

    TokenStream::from(expanded)
}
