/********************************************************************************
 * Copyright (c) 2025 Contributors to the Eclipse Foundation
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
use quote::{quote, ToTokens};
use syn::{parse_macro_input, parse_quote, Data, DeriveInput, Fields, Generics, Meta, Type};

/// Derive macro for the `CommData` trait. This macro automatically implements the `CommData` trait
/// for structs and C-like enums, ensuring that the type is marked with `#[repr(C)]`.
/// It internally also derives the `Reloc` trait for the type.
/// It also supports an optional attribute to specify a custom ID string:
/// ```
/// #[comm_data(id = "CustomID_String")]
/// ```
/// If no custom ID is provided, the struct or enum name is used as the ID.
/// Example usage:
/// ```
/// pub trait CommData {
///    const ID: &'static str;
/// }
/// use com_api_concept_macros::CommData;
/// #[derive(CommData)]
/// #[repr(C)]
/// pub struct MyData {
///    value: u32,
/// }
/// ```
/// with custom ID:
/// ```
/// pub trait CommData {
///   const ID: &'static str;
/// }
/// use com_api_concept_macros::CommData;
/// #[derive(CommData)]
/// #[repr(C)]
/// #[comm_data(id = "CustomID_MyData")]
/// pub struct MyData {
///  value: u32,
/// }
/// ```
#[proc_macro_derive(CommData, attributes(comm_data))]
pub fn derive_comm_data(input: TokenStream) -> TokenStream {
    let input_args = parse_macro_input!(input as DeriveInput);
    let ident_name = &input_args.ident;

    // Ensure #[repr(C)] on the struct itself
    if !has_repr_c(&input_args.attrs) {
        return syn::Error::new_spanned(
            &ident_name,
            "The #[derive(CommData)] macro requires #[repr(C)] on the type",
        )
        .to_compile_error()
        .into();
    }

    // Validate that the type is supported (struct, tuple struct, unit struct, or C-like enum)
    if let Err(()) = collect_field_types(&input_args.data) {
        return syn::Error::new_spanned(
            &ident_name,
            "The #[derive(CommData)] macro is supported only for enums(C-like), structs and tuple structs",
        )
        .to_compile_error()
        .into();
    }
    //Add where clause if there are generics
    let generics = input_args.generics.clone();
    let (impl_generics, type_generics, where_clause) = generics.split_for_impl();

    // Generate ID string from struct name
    let id_str =
        extract_id_from_attribute(&input_args.attrs).unwrap_or_else(|| ident_name.to_string());

    let comm_data_impl = quote! {
        impl #impl_generics CommData for #ident_name #type_generics #where_clause {
            const ID: &'static str = #id_str;
        }
    };
    // Get the input back as TokenStream for derive_reloc
    let input_for_reloc = input_args.to_token_stream().into();

    // Call derive_reloc to generate Reloc implementation
    let reloc_impl_tokens = derive_reloc(input_for_reloc);

    // Merge the TokenStreams
    let mut result = TokenStream::from(quote! { #comm_data_impl });
    result.extend(reloc_impl_tokens);
    result
}

/// Extract custom ID from #[comm_data(id = "...")] attribute
fn extract_id_from_attribute(attrs: &[syn::Attribute]) -> Option<String> {
    for attr in attrs {
        if attr.path().is_ident("comm_data") {
            if let Meta::List(list) = &attr.meta {
                if let Ok(Meta::NameValue(nv)) = list.parse_args::<Meta>() {
                    if nv.path.is_ident("id") {
                        if let syn::Expr::Lit(expr_lit) = &nv.value {
                            if let syn::Lit::Str(lit_str) = &expr_lit.lit {
                                return Some(lit_str.value());
                            }
                        }
                    }
                }
            }
        }
    }
    None
}
///
/// Derive macro for the `Reloc` trait. This macro automatically implements the `Reloc` trait
/// for structs, ensuring that all field types also implement `Reloc`. This also requires that the
/// struct is marked with `#[repr(C)]`.
///
#[proc_macro_derive(Reloc)]
pub fn derive_reloc(input: TokenStream) -> TokenStream {
    let input_args = parse_macro_input!(input as DeriveInput);
    let ident_name = &input_args.ident;

    // Ensure #[repr(C)] on the struct itself
    if !has_repr_c(&input_args.attrs) {
        return syn::Error::new_spanned(
            &ident_name,
            "The #[derive(Reloc)] macro requires #[repr(C)] on the type",
        )
        .to_compile_error()
        .into();
    }

    let mut generics = create_bounds_with_reloc(input_args.generics.clone());

    // Collect field types. Since we cannot check if given field type has implemented given trait we will use compiler
    // to check that generating trait bounds on fields like below:
    //
    // struct Example {
    //     field1: SomeType,
    //     field2: AnotherType,
    // }
    // unsafe impl Reloc for Example where SomeType: Reloc, AnotherType: Reloc {}
    //
    let field_types = match collect_field_types(&input_args.data){
        Ok(types) => types,
        Err(()) => return syn::Error::new_spanned(
            &ident_name,
            "The #[derive(Reloc)] macro is supported only for enums(C like), structs and tuple structs",
        )
        .to_compile_error()
        .into(),
    };

    {
        let where_clause_ref = generics.make_where_clause();
        for ty in field_types {
            where_clause_ref.predicates.push(parse_quote!(#ty: Reloc));
        }
    }

    // Create pieces for final implementation
    let (impl_generics, type_generics, where_clause) = generics.split_for_impl();

    let expanded = quote! {
        unsafe impl #impl_generics Reloc for #ident_name #type_generics #where_clause {}
    };

    expanded.into()
}

/// Add `T: Reloc` bounds to all generic parameters
fn create_bounds_with_reloc(mut generics: Generics) -> Generics {
    for param in generics.type_params_mut() {
        param.bounds.push(parse_quote!(Reloc));
    }
    generics
}

/// Check for #[repr(C)] existence
fn has_repr_c(attrs: &[syn::Attribute]) -> bool {
    for attr in attrs {
        if attr.path().is_ident("repr") {
            if let Meta::List(list) = &attr.meta {
                let tokens = list.tokens.to_string();
                if tokens.split(',').any(|t| t.trim() == "C") {
                    return true;
                }
            }
        }
    }
    false
}

/// Collect field types, skipping generic parameters and primitives
fn collect_field_types<'a>(data: &'a Data) -> Result<Vec<&'a Type>, ()> {
    let mut out = Vec::new();

    match data {
        Data::Struct(data_struct) => {
            out = match &data_struct.fields {
                Fields::Named(fields) => fields.named.iter().map(|f| &f.ty).collect(),
                Fields::Unnamed(fields) => fields.unnamed.iter().map(|f| &f.ty).collect(),
                Fields::Unit => Vec::new(),
            };
        }
        Data::Enum(data_enum) => {
            if !data_enum
                .variants
                .iter()
                .all(|variant| matches!(variant.fields, Fields::Unit))
            {
                return Err(());
            }
        }
        Data::Union(_) => return Err(()),
    };

    Ok(out)
}

// Use doctest to test failed compilations and successful ones

/// ```
/// pub unsafe trait Reloc {}
/// use com_api_concept_macros::Reloc;
///
/// #[derive(Reloc)]
/// #[repr(C)]
/// pub struct RelocType<T> { t : T}
///
/// #[derive(Reloc)]
/// #[repr(C)]
/// pub struct RelocTypeTwoGen<T, D> { t : T, d: D}
/// ```
#[cfg(doctest)]
fn reloc_works_on_generic_types() {}

/// ```
/// pub unsafe trait Reloc {}
/// use com_api_concept_macros::Reloc;
///
/// #[derive(Reloc)]
/// #[repr(C)]
/// enum EnumPlanType {
///     A,
///     B,
///     C
/// }
/// ```
#[cfg(doctest)]
fn reloc_works_on_c_like_enums() {}

/// ```
/// pub unsafe trait Reloc {}
/// use com_api_concept_macros::Reloc;
///
/// #[derive(Reloc)]
/// #[repr(C)]
/// struct StructTag{}
/// ```
#[cfg(doctest)]
fn reloc_works_on_unit() {}

/// ```
/// pub unsafe trait Reloc {}
/// unsafe impl Reloc for u32{}
/// unsafe impl Reloc for u64{}
///
/// use com_api_concept_macros::Reloc;
///
/// #[derive(Reloc)]
/// #[repr(C)]
/// pub struct RelocType<T> { t : T}
///
/// #[derive(Reloc)]
/// #[repr(C)]
/// pub struct StructType {
///     a: u32,
///     b: u64,
///     f: RelocType<u64>
/// }
/// ```
#[cfg(doctest)]
fn reloc_works_on_structs() {}

/// ```
/// pub unsafe trait Reloc {}
/// unsafe impl Reloc for u32{}
/// unsafe impl Reloc for u64{}
///
/// use com_api_concept_macros::Reloc;
///
/// #[derive(Reloc)]
/// #[repr(C)]
/// pub struct RelocType<T> { t : T}
///
/// #[derive(Reloc)]
/// #[repr(C)]
/// pub struct TupleStructType( u32, u64, RelocType<u64> );
/// ```
#[cfg(doctest)]
fn reloc_works_on_tuples() {}

// Failure cases

/// ```compile_fail
/// pub unsafe trait Reloc {}
/// use com_api_concept_macros::Reloc;
///
/// #[derive(Reloc)]
/// #[repr(C)]
/// enum EnumPlanType {
///     A(u32),
///     B,
///     C
/// }
/// ```
#[cfg(doctest)]
fn reloc_fail_on_non_c_like_enums() {}

/// ```compile_fail
/// pub unsafe trait Reloc {}
/// unsafe impl Reloc for u32{}
///
/// use com_api_concept_macros::Reloc;
///
/// #[derive(Reloc)]
/// #[repr(C)]
/// pub struct RelocType<T> { t : T}
///
/// #[derive(Reloc)]
/// #[repr(C)]
/// pub struct StructType {
///     a: u32,
///     b: u64,
///     f: RelocType<u64>
/// }
/// ```
#[cfg(doctest)]
fn reloc_fails_if_member_is_not_reloc_on_struct() {}

/// ```compile_fail
/// pub unsafe trait Reloc {}
/// use com_api_concept_macros::Reloc;
///
/// #[derive(Reloc)]
/// #[repr(C)]
/// pub struct RelocType<T> { t : T}
///
/// #[derive(Reloc)]
/// #[repr(C)]
/// pub struct ContainsNonRelocType(RelocType<u64>);
/// ```
#[cfg(doctest)]
fn reloc_fails_on_generic_types_that_do_not_impl_reloc() {}

/// ```compile_fail
/// pub trait CommData {
///    const ID: &'static str;
/// }
/// use com_api_concept_macros::CommData;
/// #[derive(CommData)]
/// pub struct MyData {
///     value: u32,
/// }
/// ```
/// This will fail because of missing #[repr(C)]
#[cfg(doctest)]
fn comm_data_fails_without_repr_c() {}

/// ```compile_fail
/// pub trait CommData {
///   const ID: &'static str;
/// }
/// use com_api_concept_macros::CommData;
/// #[derive(CommData)]
/// #[repr(C)]
/// pub enum MyEnum {
///   A,
///   B,
///  C(u32),
/// }
/// This will fail because of non C-like enum
/// ```
#[cfg(doctest)]
fn comm_data_fails_on_non_c_like_enum() {}

/// ```
/// pub trait CommData {
///  const ID: &'static str;
/// }
/// use com_api_concept_macros::CommData;
/// #[derive(CommData)]
/// #[repr(C)]
/// pub enum MyEnum {
///  A,
///  B,
///  C,
/// }
/// This will succeed because of C-like enum
/// ```
#[cfg(doctest)]
fn comm_data_works_on_c_like_enum() {}

/// ```
/// pub trait CommData {
/// const ID: &'static str;
/// }
/// use com_api_concept_macros::CommData;
/// #[derive(CommData)]
/// #[repr(C)]
/// pub struct MyStruct {
///    value: u32,
/// }
///
/// This will succeed because of struct with repr(C)
/// ```
#[cfg(doctest)]
fn comm_data_works_on_struct() {}

/// ```
/// pub trait CommData {
/// const ID: &'static str;
/// }
/// use com_api_concept_macros::CommData;
/// #[derive(CommData)]
/// #[repr(C)]
/// #[comm_data(id = "CustomID_MyStruct")]
/// pub struct MyStruct {
///   value: u32,
/// }
/// This will succeed because of struct with repr(C) and custom ID
/// ```
#[cfg(doctest)]
fn comm_data_works_on_struct_with_custom_id() {}
