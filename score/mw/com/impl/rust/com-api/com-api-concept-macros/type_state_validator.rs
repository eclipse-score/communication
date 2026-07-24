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
use syn::{parse_macro_input, Data, DeriveInput, Fields, Type};

/// Unified type-state validator for producers containing `FieldPublisher` and/or
/// `MethodHandler` members.
///
/// Detects member type by the last segment of each field's type path:
/// - `FieldPublisher<T>` - generates `update_{name}()` (Uninit - Init) and
///   `register_set_handler_{name}()` (HandlerNotSet - HandlerSet) per member.
/// - `MethodHandler<Args, Return>` - generates `register_{name}_handler()`
///   (HandlerNotSet - HandlerSet) per member.
/// - `instance_info` field is always skipped.
///
/// # Generated validator struct
///
/// `{Name}Validator<R, S0..Sn, H0..Hn, M0..Mm>` where:
/// - `Si` tracks update state of field member `i` (`Uninit` / `Init`)
/// - `Hi` tracks set-handler state of field member `i` (`HandlerNotSet` / `HandlerSet`)
/// - `Mj` tracks handler state of method member `j` (`HandlerNotSet` / `HandlerSet`)
///
/// `offer()` is only generated for the impl where ALL `Si = Init`, ALL `Hi = HandlerSet`,
/// ALL `Mj = HandlerSet`. It calls `_offer_internal()` on the wrapped producer.
///
/// Entry point on the producer: `init()` - returns the validator with every state
/// parameter set to its initial value (`Uninit` / `HandlerNotSet`).
pub fn derive_typestate_validator_impl(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);
    let name = &input.ident;

    // Extract runtime generic parameter from the first generic param of the struct.
    let (runtime_param_name, runtime_param_with_bounds) =
        if let Some(param) = input.generics.params.first() {
            match param {
                syn::GenericParam::Type(type_param) => {
                    let n = &type_param.ident;
                    (quote! { #n }, quote! { #param })
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
                    "TypeStateValidator only supports structs with named fields",
                )
                .to_compile_error()
                .into();
            }
        },
        _ => {
            return syn::Error::new_spanned(name, "TypeStateValidator only supports structs")
                .to_compile_error()
                .into();
        }
    };

    // Classify each field by the last segment of its type path.
    // Note: these string names ("FieldPublisher", "MethodHandler") must match the trait/type
    // names used in the Runtime associated types. If those names change, update here too.
    struct FieldMember {
        ident: syn::Ident,
        inner_ty: Type, // T extracted from FieldPublisher<T>
    }
    struct MethodMember {
        ident: syn::Ident,
        args_ty: Type,   // Args extracted from MethodHandler<Args, Return>
        return_ty: Type, // Return extracted from MethodHandler<Args, Return>
    }

    let mut field_members: Vec<FieldMember> = Vec::new();
    let mut method_members: Vec<MethodMember> = Vec::new();

    for f in fields.iter() {
        let ident = match f.ident.as_ref() {
            Some(i) => i.clone(),
            None => continue,
        };
        // Skip the bookkeeping field — it carries no type-state.
        if ident == "instance_info" {
            continue;
        }

        if let Type::Path(type_path) = &f.ty {
            if let Some(segment) = type_path.path.segments.last() {
                match segment.ident.to_string().as_str() {
                    "FieldPublisher" => {
                        if let syn::PathArguments::AngleBracketed(args) = &segment.arguments {
                            if let Some(syn::GenericArgument::Type(inner)) = args.args.first() {
                                field_members.push(FieldMember {
                                    ident,
                                    inner_ty: inner.clone(),
                                });
                            }
                        }
                    }
                    "MethodHandler" => {
                        if let syn::PathArguments::AngleBracketed(args) = &segment.arguments {
                            if args.args.len() >= 2 {
                                if let (
                                    Some(syn::GenericArgument::Type(args_ty)),
                                    Some(syn::GenericArgument::Type(return_ty)),
                                ) = (args.args.get(0), args.args.get(1))
                                {
                                    method_members.push(MethodMember {
                                        ident,
                                        args_ty: args_ty.clone(),
                                        return_ty: return_ty.clone(),
                                    });
                                }
                            }
                        }
                    }
                    _ => {} // Other fields (e.g. PhantomData) are ignored.
                }
            }
        }
    }

    if field_members.is_empty() && method_members.is_empty() {
        return syn::Error::new_spanned(
            name,
            "TypeStateValidator: no FieldPublisher or MethodHandler fields found \
             (excluding instance_info)",
        )
        .to_compile_error()
        .into();
    }

    let validator_name = syn::Ident::new(&format!("{}Validator", name), name.span());

    // State param naming:
    //   S{i} — update state for field member i    (Uninit / Init)
    //   H{i} — set-handler state for field member i (HandlerNotSet / HandlerSet)
    //   M{j} — handler state for method member j  (HandlerNotSet / HandlerSet)
    // Combined order in the validator struct: [S0..Sn, H0..Hn, M0..Mm]
    let field_update_params: Vec<syn::Ident> = (0..field_members.len())
        .map(|i| syn::Ident::new(&format!("S{}", i), proc_macro::Span::call_site().into()))
        .collect();
    let field_handler_params: Vec<syn::Ident> = (0..field_members.len())
        .map(|i| syn::Ident::new(&format!("H{}", i), proc_macro::Span::call_site().into()))
        .collect();
    let method_handler_params: Vec<syn::Ident> = (0..method_members.len())
        .map(|j| syn::Ident::new(&format!("M{}", j), proc_macro::Span::call_site().into()))
        .collect();

    // Flat list used in struct definition and impl generics: [S0..Sn, H0..Hn, M0..Mm]
    let all_params: Vec<&syn::Ident> = field_update_params
        .iter()
        .chain(field_handler_params.iter())
        .chain(method_handler_params.iter())
        .collect();

    // Initial states for init() entry point.
    let init_states: Vec<_> = (0..field_members.len())
        .map(|_| quote! { ::com_api::Uninit })
        .chain((0..field_members.len()).map(|_| quote! { ::com_api::HandlerNotSet }))
        .chain((0..method_members.len()).map(|_| quote! { ::com_api::HandlerNotSet }))
        .collect();

    // All-satisfied states required by offer().
    let satisfied_states: Vec<_> = (0..field_members.len())
        .map(|_| quote! { ::com_api::Init })
        .chain((0..field_members.len()).map(|_| quote! { ::com_api::HandlerSet }))
        .chain((0..method_members.len()).map(|_| quote! { ::com_api::HandlerSet }))
        .collect();

    // update_{name}() impls for each field member
    // Transitions Si: Uninit - Init while all other state params stay generic.
    let update_methods: Vec<_> = field_members
        .iter()
        .enumerate()
        .map(|(i, member)| {
            let update_fn =
                syn::Ident::new(&format!("update_{}", member.ident), member.ident.span());
            let inner_ty = &member.inner_ty;
            let field_ident = &member.ident;

            // After-state list: Si becomes Init, every other param stays generic.
            let after: Vec<_> = all_params
                .iter()
                .enumerate()
                .map(|(k, p)| {
                    if k == i {
                        quote! { ::com_api::Init }
                    } else {
                        quote! { #p }
                    }
                })
                .collect();

            quote! {
                impl<#runtime_param_with_bounds, #(#all_params),*>
                    #validator_name<#runtime_param_name, #(#all_params),*>
                {
                    pub fn #update_fn(
                        mut self,
                        value: &#inner_ty,
                    ) -> com_api::Result<#validator_name<#runtime_param_name, #(#after),*>> {
                        self.producer.#field_ident.update(value)?;
                        Ok(#validator_name {
                            producer: self.producer,
                            _phantom: core::marker::PhantomData,
                        })
                    }
                }
            }
        })
        .collect();

    // register_set_handler_{name}() impls for each field member
    // Hi is at index field_members.len() + i in all_params.
    // Transitions Hi: HandlerNotSet - HandlerSet while all other state params stay generic.
    let register_set_handler_methods: Vec<_> = field_members
        .iter()
        .enumerate()
        .map(|(i, member)| {
            let register_fn = syn::Ident::new(
                &format!("register_set_handler_{}", member.ident),
                member.ident.span(),
            );
            let inner_ty = &member.inner_ty;
            let field_ident = &member.ident;
            let hi_index = field_members.len() + i;

            let after: Vec<_> = all_params
                .iter()
                .enumerate()
                .map(|(k, p)| {
                    if k == hi_index {
                        quote! { ::com_api::HandlerSet }
                    } else {
                        quote! { #p }
                    }
                })
                .collect();

            quote! {
                impl<#runtime_param_with_bounds, #(#all_params),*>
                    #validator_name<#runtime_param_name, #(#all_params),*>
                where
                    <#runtime_param_name as com_api::Runtime>::FieldPublisher<#inner_ty>: Send,
                {
                    pub fn #register_fn<F>(
                        mut self,
                        handler: F,
                    ) -> #validator_name<#runtime_param_name, #(#after),*>
                    where
                        F: Fn(&#inner_ty) + Send + 'static,
                    {
                        self.producer.#field_ident.register_set_handler(handler);
                        #validator_name {
                            producer: self.producer,
                            _phantom: core::marker::PhantomData,
                        }
                    }
                }
            }
        })
        .collect();

    // register_{name}_handler() impls for each method member
    // Mj is at index 2 * field_members.len() + j in all_params.
    // Transitions Mj: HandlerNotSet - HandlerSet while all other state params stay generic.
    let register_handler_methods: Vec<_> = method_members
        .iter()
        .enumerate()
        .map(|(j, member)| {
            let register_fn = syn::Ident::new(
                &format!("register_{}_handler", member.ident),
                member.ident.span(),
            );
            let args_ty = &member.args_ty;
            let return_ty = &member.return_ty;
            let method_ident = &member.ident;
            let mj_index = 2 * field_members.len() + j;

            let after: Vec<_> = all_params
                .iter()
                .enumerate()
                .map(|(k, p)| {
                    if k == mj_index {
                        quote! { ::com_api::HandlerSet }
                    } else {
                        quote! { #p }
                    }
                })
                .collect();

            quote! {
                impl<#runtime_param_with_bounds, #(#all_params),*>
                    #validator_name<#runtime_param_name, #(#all_params),*>
                {
                    pub fn #register_fn<F>(
                        mut self,
                        handler: F,
                    ) -> #validator_name<#runtime_param_name, #(#after),*>
                    where
                        F: com_api::MethodHandlerCall<#args_ty, #return_ty>,
                    {
                        <_ as com_api::MethodHandler<#args_ty, #return_ty, #runtime_param_name>>::register_handler(
                            &self.producer.#method_ident,
                            handler,
                        );
                        #validator_name {
                            producer: self.producer,
                            _phantom: core::marker::PhantomData,
                        }
                    }
                }
            }
        })
        .collect();

    let expanded = quote! {
        // Validator struct type params track state of every Field and Method member.
        // Layout: <R, S0..Sn (field updates), H0..Hn (field handlers), M0..Mm (method handlers)>
        pub struct #validator_name<#runtime_param_with_bounds, #(#all_params),*> {
            producer: #name<#runtime_param_name>,
            _phantom: core::marker::PhantomData<(#(#all_params,)*)>,
        }

        // update_{name}() - transitions Si: Uninit - Init
        #(#update_methods)*

        // register_set_handler_{name}() - transitions Hi: HandlerNotSet - HandlerSet
        #(#register_set_handler_methods)*

        // register_{name}_handler() - transitions Mj: HandlerNotSet - HandlerSet
        #(#register_handler_methods)*

        // offer() is only available when ALL Si = Init, ALL Hi = HandlerSet, ALL Mj = HandlerSet.
        impl<#runtime_param_with_bounds>
            #validator_name<#runtime_param_name, #(#satisfied_states),*>
        {
            pub fn offer(
                self,
            ) -> com_api::Result<<#name<#runtime_param_name> as com_api::Producer<#runtime_param_name>>::OfferedProducer> {
                self.producer._offer_internal()
            }
        }

        // init() - entry point on the original producer, begins the type-state chain.
        impl<#runtime_param_with_bounds> #name<#runtime_param_name> {
            pub fn init(
                self,
            ) -> #validator_name<#runtime_param_name, #(#init_states),*> {
                #validator_name {
                    producer: self,
                    _phantom: core::marker::PhantomData,
                }
            }
        }
    };

    TokenStream::from(expanded)
}
