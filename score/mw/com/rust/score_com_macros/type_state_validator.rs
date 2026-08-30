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
use std::collections::HashSet;
use syn::{parse_macro_input, Data, DeriveInput, Fields, Type};

/// Parse a struct-level attribute of the form `#[attr_name(ident1, ident2, ...)]`
/// and return the set of identifier strings it contains.
/// Returns an empty set if the attribute is absent or its argument list is empty.
/// This is for FieldPublisher WithSetter / WithGetter capability tags, which are emitted by the interface_producer_mixed! macro.
fn parse_name_list_attr(attrs: &[syn::Attribute], attr_name: &str) -> HashSet<String> {
    let mut names = HashSet::new();
    for attr in attrs {
        if attr.path().is_ident(attr_name) {
            if let Ok(list) = attr.parse_args_with(
                syn::punctuated::Punctuated::<syn::Ident, syn::Token![,]>::parse_terminated,
            ) {
                for ident in list {
                    names.insert(ident.to_string());
                }
            }
        }
    }
    names
}

/// Unified type-state validator for producers containing `FieldPublisher` and/or
/// `MethodHandler` members.
///
/// Detects member type by the last segment of each field's type path:
/// - `FieldPublisher<T>` - generates `update_{name}()` (Uninit-Init) per member.
///   Additionally generates `register_set_handler_{name}()` (HandlerNotSet-HandlerSet)
///   for fields listed in `#[field_setter_list(...)]`, and
///   `register_get_handler_{name}()` (HandlerNotSet-HandlerSet) for fields listed in
///   `#[field_getter_list(...)]`.
/// - `MethodHandler<Args, Return>` - generates `register_{name}_handler()`
///   (HandlerNotSet - HandlerSet) per member.
/// - `instance_info` field is always skipped.
///
/// # Struct-level helper attributes (declared by the `TypeStateValidator` derive)
///
/// - `#[field_setter_list(name1, name2, ...)]` - comma-separated field names that have
///   the `WithSetter` capability tag. Only these fields get a `register_set_handler_*`
///   type-state step and an `Hi` generic parameter.
/// - `#[field_getter_list(name1, name2, ...)]` - comma-separated field names that have
///   the `WithGetter` capability tag. Only these fields get a `register_get_handler_*`
///   type-state step and a `Gi` generic parameter.
///
/// Both attributes are emitted by `interface_producer_mixed!` based on the capability
/// tags declared in the `interface!` macro invocation.
///
/// # Generated validator struct
///
/// `{Name}Validator<R, S0..Sn, H0..Hm, G0..Gk, M0..Mp>` where:
/// - `Si` tracks update state of field member `i` (`Uninit` / `Init`) - ALL fields
/// - `Hj` tracks set-handler state of setter field `j` (`HandlerNotSet` / `HandlerSet`) - WithSetter fields only
/// - `Gk` tracks get-handler state of getter field `k` (`HandlerNotSet` / `HandlerSet`) - WithGetter fields only
/// - `Mp` tracks handler state of method member `p` (`HandlerNotSet` / `HandlerSet`)
///
/// `offer()` is only generated for the impl where ALL `Si = Init`, ALL `Hj = HandlerSet`,
/// ALL `Gk = HandlerSet`, ALL `Mp = HandlerSet`. It calls `_offer_internal()` on the
/// wrapped producer.
///
/// Entry point on the producer: `init()` - returns the validator with every state
/// parameter set to its initial value (`Uninit` / `HandlerNotSet`).
///
/// Note: This macro identifies member types by the member types so if member type is changed to a different type or renamed,
/// then macro need to be updated to recognize the new type name or path segment.
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
                _ => (quote! { R }, quote! { R: score_com::Runtime + ?Sized }),
            }
        } else {
            (quote! { R }, quote! { R: score_com::Runtime + ?Sized })
        };

    // Read #[field_setter_list(name1, name2, ...)] and #[field_getter_list(name1, name2, ...)]
    // emitted by interface_producer_mixed! to know which fields have WithSetter / WithGetter.
    let setter_names = parse_name_list_attr(&input.attrs, "field_setter_list");
    let getter_names = parse_name_list_attr(&input.attrs, "field_getter_list");

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
        // TODO: If require support for enum or tuple struct then add support here.
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
        has_setter: bool,
        has_getter: bool,
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
        // Skip the `instance_info` field, which is not part of the type-state validation.
        if ident == "instance_info" {
            continue;
        }

        // Note: pattern matching ("FieldPublisher", "MethodHandler") must match the trait/type
        // names used in the Runtime associated types. If those names change, update here too.
        if let Type::Path(type_path) = &f.ty {
            if let Some(segment) = type_path.path.segments.last() {
                match segment.ident.to_string().as_str() {
                    "FieldPublisher" => {
                        if let syn::PathArguments::AngleBracketed(args) = &segment.arguments {
                            if let Some(syn::GenericArgument::Type(inner)) = args.args.first() {
                                let name_str = ident.to_string();
                                field_members.push(FieldMember {
                                    has_setter: setter_names.contains(&name_str),
                                    has_getter: getter_names.contains(&name_str),
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
    // If no FieldPublisher or MethodHandler members were found, emit a compile error.
    // because macro is only added to producer struct which has at least one FieldPublisher or MethodHandler member.
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
    //   S{i} - update state for field member i         (Uninit / Init)        - ALL fields
    //   H{j} - set-handler state for setter field j    (HandlerNotSet / HandlerSet) - WithSetter only
    //   G{k} - get-handler state for getter field k    (HandlerNotSet / HandlerSet) - WithGetter only
    //   M{p} - handler state for method member p       (HandlerNotSet / HandlerSet)
    // Combined order in the validator struct: [S0..Sn, H0..Hm, G0..Gk, M0..Mp]

    let field_update_params: Vec<syn::Ident> = (0..field_members.len())
        .map(|i| syn::Ident::new(&format!("S{}", i), proc_macro::Span::call_site().into()))
        .collect();

    // Collect setter/getter subsets once to avoid repeated filtering.
    let setter_field_indices: Vec<usize> = field_members
        .iter()
        .enumerate()
        .filter(|(_, m)| m.has_setter)
        .map(|(i, _)| i)
        .collect();
    let getter_field_indices: Vec<usize> = field_members
        .iter()
        .enumerate()
        .filter(|(_, m)| m.has_getter)
        .map(|(i, _)| i)
        .collect();

    let field_setter_params: Vec<syn::Ident> = (0..setter_field_indices.len())
        .map(|j| syn::Ident::new(&format!("H{}", j), proc_macro::Span::call_site().into()))
        .collect();
    let field_getter_params: Vec<syn::Ident> = (0..getter_field_indices.len())
        .map(|k| syn::Ident::new(&format!("G{}", k), proc_macro::Span::call_site().into()))
        .collect();
    let method_handler_params: Vec<syn::Ident> = (0..method_members.len())
        .map(|p| syn::Ident::new(&format!("M{}", p), proc_macro::Span::call_site().into()))
        .collect();

    // Flat list used in struct definition and impl generics:
    // [S0..Sn, H0..Hm, G0..Gk, M0..Mp]
    let all_params: Vec<&syn::Ident> = field_update_params
        .iter()
        .chain(field_setter_params.iter())
        .chain(field_getter_params.iter())
        .chain(method_handler_params.iter())
        .collect();

    let n_fields = field_members.len();
    let n_setters = setter_field_indices.len();
    let n_getters = getter_field_indices.len();

    // Initial states for init() entry point.
    let init_states: Vec<_> = (0..n_fields)
        .map(|_| quote! { ::score_com::Uninit })
        .chain((0..n_setters).map(|_| quote! { ::score_com::HandlerNotSet }))
        .chain((0..n_getters).map(|_| quote! { ::score_com::HandlerNotSet }))
        .chain((0..method_members.len()).map(|_| quote! { ::score_com::HandlerNotSet }))
        .collect();

    // All-satisfied states required by offer().
    let done_states: Vec<_> = (0..n_fields)
        .map(|_| quote! { ::score_com::Init })
        .chain((0..n_setters).map(|_| quote! { ::score_com::HandlerSet }))
        .chain((0..n_getters).map(|_| quote! { ::score_com::HandlerSet }))
        .chain((0..method_members.len()).map(|_| quote! { ::score_com::HandlerSet }))
        .collect();

    // update_{name}() impls for each field member.
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
                        quote! { ::score_com::Init }
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
                        value: #inner_ty,
                    ) -> score_com::Result<#validator_name<#runtime_param_name, #(#after),*>> {
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

    // register_set_handler_{name}() impls - only for WithSetter fields.
    // H{j} is at index n_fields + j in all_params.
    // Transitions Hj: HandlerNotSet - HandlerSet while all other state params stay generic.
    let register_set_handler_methods: Vec<_> = setter_field_indices
        .iter()
        .enumerate()
        .map(|(j, &field_idx)| {
            let member = &field_members[field_idx];
            let register_fn = syn::Ident::new(
                &format!("register_set_handler_{}", member.ident),
                member.ident.span(),
            );
            let inner_ty = &member.inner_ty;
            let field_ident = &member.ident;
            let hj_index = n_fields + j;

            let after: Vec<_> = all_params
                .iter()
                .enumerate()
                .map(|(k, p)| {
                    if k == hj_index {
                        quote! { ::score_com::HandlerSet }
                    } else {
                        quote! { #p }
                    }
                })
                .collect();

            quote! {
                impl<#runtime_param_with_bounds, #(#all_params),*>
                    #validator_name<#runtime_param_name, #(#all_params),*>
                where
                    <#runtime_param_name as score_com::Runtime>::FieldPublisher<#inner_ty>: Send,
                {
                    pub fn #register_fn<F>(
                        self,
                        handler: F,
                    ) -> #validator_name<#runtime_param_name, #(#after),*>
                    where
                        F: Fn(#inner_ty) -> #inner_ty + Send + 'static,
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

    // register_get_handler_{name}() impls - only for WithGetter fields.
    // G{k} is at index n_fields + n_setters + k in all_params.
    // Transitions Gk: HandlerNotSet - HandlerSet while all other state params stay generic.
    let register_get_handler_methods: Vec<_> = getter_field_indices
        .iter()
        .enumerate()
        .map(|(k, &field_idx)| {
            let member = &field_members[field_idx];
            let register_fn = syn::Ident::new(
                &format!("register_get_handler_{}", member.ident),
                member.ident.span(),
            );
            let inner_ty = &member.inner_ty;
            let field_ident = &member.ident;
            let gk_index = n_fields + n_setters + k;

            let after: Vec<_> = all_params
                .iter()
                .enumerate()
                .map(|(k2, p)| {
                    if k2 == gk_index {
                        quote! { ::score_com::HandlerSet }
                    } else {
                        quote! { #p }
                    }
                })
                .collect();

            quote! {
                impl<#runtime_param_with_bounds, #(#all_params),*>
                    #validator_name<#runtime_param_name, #(#all_params),*>
                where
                    <#runtime_param_name as score_com::Runtime>::FieldPublisher<#inner_ty>: Send,
                {
                    pub fn #register_fn<F>(
                        self,
                        handler: F,
                    ) -> #validator_name<#runtime_param_name, #(#after),*>
                    where
                        F: Fn() -> #inner_ty + Send + 'static,
                    {
                        self.producer.#field_ident.register_get_handler(handler);
                        #validator_name {
                            producer: self.producer,
                            _phantom: core::marker::PhantomData,
                        }
                    }
                }
            }
        })
        .collect();

    // register_{name}_handler() impls for each method member.
    // M{p} is at index n_fields + n_setters + n_getters + p in all_params.
    // Transitions Mp: HandlerNotSet - HandlerSet while all other state params stay generic.
    let register_handler_methods: Vec<_> = method_members
        .iter()
        .enumerate()
        .map(|(p, member)| {
            let register_fn = syn::Ident::new(
                &format!("register_{}_handler", member.ident),
                member.ident.span(),
            );
            let args_ty = &member.args_ty;
            let return_ty = &member.return_ty;
            let method_ident = &member.ident;
            let mp_index = n_fields + n_setters + n_getters + p;

            let after: Vec<_> = all_params
                .iter()
                .enumerate()
                .map(|(k, p_param)| {
                    if k == mp_index {
                        quote! { ::score_com::HandlerSet }
                    } else {
                        quote! { #p_param }
                    }
                })
                .collect();

            quote! {
                impl<#runtime_param_with_bounds, #(#all_params),*>
                    #validator_name<#runtime_param_name, #(#all_params),*>
                {
                    pub fn #register_fn<F>(
                        self,
                        handler: F,
                    ) -> #validator_name<#runtime_param_name, #(#after),*>
                    where
                        F: score_com::MethodHandlerCall<#args_ty, #return_ty>,
                    {
                        <_ as score_com::MethodHandler<#args_ty, #return_ty, #runtime_param_name>>::register_handler(
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
        // Layout: <R, S0..Sn (field updates), H0..Hm (set handlers), G0..Gk (get handlers), M0..Mp (method handlers)>
        pub struct #validator_name<#runtime_param_with_bounds, #(#all_params),*> {
            producer: #name<#runtime_param_name>,
            _phantom: core::marker::PhantomData<(#(#all_params,)*)>,
        }

        // update_{name}() - transitions Si: Uninit - Init (all fields)
        #(#update_methods)*

        // register_set_handler_{name}() - transitions Hj: HandlerNotSet - HandlerSet (WithSetter fields only)
        #(#register_set_handler_methods)*

        // register_get_handler_{name}() - transitions Gk: HandlerNotSet - HandlerSet (WithGetter fields only)
        #(#register_get_handler_methods)*

        // register_{name}_handler() - transitions Mp: HandlerNotSet - HandlerSet (methods)
        #(#register_handler_methods)*

        // offer() is only available when ALL Si = Init, ALL Hj = HandlerSet,
        // ALL Gk = HandlerSet, ALL Mp = HandlerSet.
        impl<#runtime_param_with_bounds>
            #validator_name<#runtime_param_name, #(#done_states),*>
        {
            pub fn offer(
                self,
            ) -> score_com::Result<<#name<#runtime_param_name> as score_com::Producer<#runtime_param_name>>::OfferedProducer> {
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
