# MISRA C++:2023 Guideline Enforcement Plan (GEP)

This is the guideline enforcement plan for S-CORE communication for MISRA C++:2023.
The main tool choosen to enforce MISRA C++:2023 is CodeQL. Whenever available, a rule is checked with CodeQL.

Additionally, we use compiler warnings (from gcc and clang) and clang-tidy for the following reasons:
(i) compiler warnings allow us to check or partially check some rules in a very early stage of the development workflow,
(ii) even though there are no plans to qualify clang-tidy, it adds a level of redundancy that could compensate some CodeQL bugs.
However, because we do not want to introduce more false positives than needed, we only list clang-tidy checks in the enforcement plan if we know that cover cases that are not covered by the listed compiler warnings.

Note: According to MISRA Compliance:2020 document, every guideline within "The Guidelines" shall appear in the GEP. However, some of the guidelines here, are later disapplied in the guideline re-categorization plan (GRP).

| Rule | gcc | clang | clang-tidy | CodeQL |
| ----- | -------- | ---- | ---- | ---- |
| RULE-0-0-1 | | `-Wunreachable-code` <br/> `-Wunreachable-code-return` | | `UnreachableStatement.ql` |
| RULE-0-0-2 | `-Wtype-limits` | `-Wtautological-unsigned-zero-compare` <br/> `-Wtautological-type-limit-compare` | `misc-redundant-expression` | `InvariantCondition.ql` |
| RULE-0-1-1 | | `-Wunused-variable` | `clang-analyzer-deadcode.DeadStores` | `UnnecessaryWriteToLocalObject.ql` |
| RULE-0-1-2 | | | `bugprone-unused-return-value` | `UnusedReturnValueMisraCpp.ql` |
| RULE-0-2-1 | `-Wunused-variable` | `-Wunused-variable` | `clang-diagnostic-unused-const-variable` <br/> `clang-diagnostic-unused-template` <br/> `clang-diagnostic-unused-variable` | `UnusedLimitedVisibilityVariable.ql` |
| RULE-0-2-2 | | `-Wunused-exception-parameter` <br/> `-Wunused-lambda-capture` <br/> `-Wunused-parameter` | | `UnusedParameterMisraCpp.ql` |
| RULE-0-2-3 | `-Wunused-local-typedefs` | | `clang-diagnostic-unused-local-typedef` | `UnusedTypeWithLimitedVisibility.ql` |
| RULE-0-2-4 | `-Wunused-function` | `-Wunused-function` <br/> `-Wunused-member-function` <br/> `-Wunused-template` | | `UnusedLimitedVisibilityFunction.ql` |
| DIR-0-3-1 | | | | `PossibleMisuseOfInfiniteFloatingPointValue.ql`<br/> `PossibleMisuseOfNaNFloatingPointValue.ql` |
| DIR-0-3-2 | See note [1] | See note [1] | | |
| RULE-4-1-1 | | | | `CompilerLanguageExtensionsUsed.ql` |
| RULE-4-1-2 | | `-Wdeprecated` | | `RedeclarationOfStaticConstexprDataMember.ql`<br/> `ImplicitDeclarationOfCopyConstructor.ql`<br/> `ImplicitDeclarationOfCopyConstructorAudit.ql`<br/> `NoexceptSpecifierThrow.ql`<br/> `UseOfDeprecatedCHeaders.ql`<br/> `UseOfDeprecatedStrStreamClass.ql`<br/> `UseOfUncaughtException.ql`<br/> `UseOfDeprecatedFunctionBinderTypedefMember.ql`<br/> `UseOfDeprecatedUnaryOrBinaryNegate.ql`<br/> `UseOfDeprecatedAllocatorVoid.ql`<br/> `UseOfDeprecatedStdAllocatorMember.ql`<br/> `UseOfDeprecatedRawStorageIterator.ql`<br/> `UseOfDeprecatedTemporaryBuffers.ql`<br/> `UseOfDeprecatedIsLiteralTypeTraits.ql`<br/> `UseOfDeprecatedStdIteratorBaseClass.ql`<br/> `UseOfDeprecatedSharedPtrUnique.ql` |
| RULE-4-1-3 | See note [2] | See note [2] | | `PossibleDataRaceBetweenThreads.ql`<br/> `ArrayDeletedThroughPointerOfIncorrectType.ql`<br/> `SignedIntegerOverflow.ql`<br/> `DivisionByZeroUndefinedBehavior.ql`<br/> `DeallocationTypeMismatch.ql`<br/> `StringLiteralPossiblyModifiedAudit.ql`<br/> `OutOfRangeEnumCastCriticalUnspecifiedBehavior.ql`<br/> `NullPointerToMemberAccessUndefinedBehavior.ql`<br/> `UninitializedStaticPointerToMemberUndefinedBehavior.ql`<br/> `NonExistentMemberAccessUndefinedBehavior.ql` |
| RULE-4-6-1 | See note [3] | `-Wsequence-point` <br/> `-Wunsequenced` <br/> See note [3] | | `MemoryUsageNotSequenced.ql` |
| RULE-5-0-1 | `-Wtrigraphs` | `-Wtrigraphs` | | `TrigraphLikeSequencesShouldNotBeUsed.ql` |
| RULE-5-7-1 | `-Wcomment` | `-Wcomment` | | `CharacterSequenceUsedWithinACStyleComment.ql` |
| DIR-5-7-2 | | | | `SectionsOfCodeShouldNotBeCommentedOut.ql` |
| RULE-5-7-3 | `-Wcomment` | `-Wcomment` | | `LineSplicingUsedInComments.ql` |
| RULE-5-10-1 | | `-Wuser-defined-literals` | | `PoorlyFormedIdentifier.ql` |
| RULE-5-13-1 | | `-Wunknown-escape-sequence` | | `BackslashCharacterMisuse.ql` |
| RULE-5-13-2 | | | | `NonTerminatedEscapeSequences.ql` |
| RULE-5-13-3 | | | | `OctalConstantsUsed.ql` |
| RULE-5-13-4 | | | | `UnsignedIntegerLiteralsNotAppropriatelySuffixed.ql` |
| RULE-5-13-5 | | | | `LowercaseLStartsInLiteralSuffix.ql` |
| RULE-5-13-6 | | | | `LongLongLiteralWithSingleLSuffix.ql` |
| RULE-5-13-7 | | | | See notes [4] |
| RULE-6-0-1 | `-Wparentheses` | `-Wredundant-parens` <br/> `-Wvexing-parse` | | `BlockScopeFunctionAmbiguous.ql` |
| RULE-6-0-2 | | | | `ExternalLinkageArrayWithoutExplicitSizeMisra.ql` |
| RULE-6-0-3 | | | | `GlobalNamespaceDeclarations.ql` |
| RULE-6-0-4 | | | | `NonGlobalFunctionMain.ql` |
| RULE-6-2-1 | | | | `OneDefinitionRuleViolated.ql` |
| RULE-6-2-2 | | | | `IncompatibleObjectDeclarationsCpp.ql`<br/> `IncompatibleFunctionDeclarationsCpp.ql` |
| RULE-6-2-3 | | | | `DuplicateInlineFunctionDefinitions.ql`<br/> `TemplateSpecializationWrongLocation.ql`<br/> `DuplicateTypeDefinitions.ql` |
| RULE-6-2-4 | | | `misc-definitions-in-headers` | `ViolationsOfOneDefinitionRuleMisra.ql` |
| RULE-6-4-1 | `-Wshadow` | `-Wshadow-all` | | `VariableDeclaredInInnerScopeHidesOuterScope.ql` |
| RULE-6-4-2 | | | | `InheritedNonOverridableMemberFunction.ql`<br/> `InheritedOverridableMemberFunction.ql`<br/> `DefinitionShallBeConsideredForUnqualifiedLookup.ql` |
| RULE-6-4-3 | | | | `NameShallBeReferredUsingAQualifiedIdOrThis.ql`<br/> `NameShallBeReferredUsingAQualifiedIdOrThisAudit.ql` |
| RULE-6-5-1 | | | `misc-use-internal-linkage` | `ExternalLinkageNotDeclaredInHeaderFileMisra.ql` |
| RULE-6-5-2 | | | | `InternalLinkageSpecifiedAppropriately.ql` |
| RULE-6-7-1 | | | | `LocalVariableStaticStorageDuration.ql` |
| RULE-6-7-2 | | | `misc-use-internal-linkage` | `GlobalVariableUsed.ql` |
| RULE-6-8-1 | See note [5] | See note [5] | `bugprone-return-const-ref-from-parameter` <br/> `clang-analyzer-cplusplus.NewDelete` | `ObjectAccessedBeforeLifetimeMisra.ql`<br/> `ObjectAccessedAfterLifetimeMisra.ql` |
| RULE-6-8-2 | `-Wreturn-local-addr` | `-Wreturn-stack-address` | | `ReturnReferenceOrPointerToAutomaticLocalVariable.ql` |
| RULE-6-8-3 | | | | `AutomaticStorageAssignedToObjectGreaterLifetime.ql` |
| RULE-6-8-4 | | | | `MemberFunctionsRefqualified.ql` |
| RULE-6-9-1 | | | | `TypeAliasesDeclaration.ql` |
| RULE-6-9-2 | | | `google-runtime-int` | `AvoidStandardIntegerTypeNames.ql` |
| RULE-7-0-1 | `-Wswitch-bool` | `-Wswitch-bool` | | `NoConversionFromBool.ql` |
| RULE-7-0-2 | | | `modernize-use-bool-literals` <br/> `readability-implicit-bool-conversion` | `NoImplicitBoolConversion.ql` |
| RULE-7-0-3 | | `-Wconstant-conversion` <br/> `-Wimplicit-int-conversion` | `bugprone-narrowing-conversions` <br/> `readability-implicit-bool-conversion` | `NoCharacterNumericalValue.ql` |
| RULE-7-0-4 | | `-Wshift-sign-overflow` | `hicpp-signed-bitwise` | `InappropriateBitwiseOrShiftOperands.ql` |
| RULE-7-0-5 | `-Wsign-compare` <br/> `-Wconversion` | `-Wsign-compare` | | `NoSignednessChangeFromPromotion.ql` |
| RULE-7-0-6 | `-Wconversion` <br/> `-Wfloat-conversion` | `-Wc++11-narrowing` <br/> `-Wimplicit-int-conversion` <br/> `-Wsign-conversion` | `bugprone-narrowing-conversions` | `NumericAssignmentTypeMismatch.ql` |
| RULE-7-11-1 | | `-Wzero-as-null-pointer-constant` | | `NullptrNotTheOnlyFormOfTheNullPointerConstant.ql` |
| RULE-7-11-2 | | | `cppcoreguidelines-pro-bounds-array-to-pointer-decay` | `ArrayPassedAsFunctionArgumentDecayToAPointer.ql` |
| RULE-7-11-3 | `-Waddress` | `-Wtautological-pointer-compare` | | `FunctionPointerConversionContext.ql` |
| RULE-8-0-1 | `-Wparentheses` | `-Wparentheses` | | `MissingPrecedenceClarifyingParenthesis.ql`<br/> `MissingSizeofOperatorParenthesis.ql` |
| RULE-8-1-1 | | | | `NonTransientLambdaImplicitlyCapturesThis.ql` |
| RULE-8-1-2 | | | | `ImplicitCapturesDisallowedInNonTransientLambda.ql` |
| RULE-8-2-1 | | `-Wreinterpret-base-class` | | `VirtualBaseClassCastToDerived.ql` |
| RULE-8-2-2 | | `-Wold-style-cast` | | `NoCStyleOrFunctionalCasts.ql` |
| RULE-8-2-3 | | `-Wcast-qual` | | `CastRemovesConstOrVolatileFromPointerOrReference.ql` |
| RULE-8-2-4 | `-Wcast-function-type` | | | `CastsBetweenAPointerToFunctionAndAnyOtherType.ql` |
| RULE-8-2-5 | | | `cppcoreguidelines-pro-type-reinterpret-cast` | `ReinterpretCastShallNotBeUsed.ql` |
| RULE-8-2-6 | | | `performance-no-int-to-ptr` | `IntToPointerCastProhibited.ql` |
| RULE-8-2-7 | | `-Wpointer-to-int-cast` | | `NoPointerToIntegralCast.ql` |
| RULE-8-2-8 | | `-Wpointer-to-int-cast` | | `PointerToIntegralCast.ql` |
| RULE-8-2-9 | | | | `PolymorphicClassTypeExpressionInTypeid.ql` |
| RULE-8-2-10 | | `-Winfinite-recursion` | `misc-no-recursion` | `FunctionsCallThemselvesEitherDirectlyOrIndirectly.ql` |
| RULE-8-2-11 | | | | `InappropriateArgumentTypePassedViaEllipsis.ql` |
| RULE-8-3-1 | | | | `BuiltInUnaryOperatorAppliedToUnsignedExpression.ql` |
| RULE-8-3-2 | | | | `BuiltInUnaryPlusOperatorShouldNotBeUsed.ql` |
| RULE-8-7-1 | | `-Warray-bounds` <br/> `-Warray-bounds-pointer-arithmetic` | `cppcoreguidelines-pro-bounds-pointer-arithmetic` | `PointerArithmeticFormsAnInvalidPointer.ql`<br/> `PointerArgumentToCstringFunctionIsInvalid.ql` |
| RULE-8-7-2 | | | | `PointerDifferenceTakenBetweenDifferentArrays.ql` |
| RULE-8-9-1 | | | | `PointerComparedBetweenDifferentArrays.ql` |
| RULE-8-14-1 | | | | `ShortCircuitedPersistentSideEffect.ql` |
| RULE-8-18-1 | | | | `ObjectAssignedToAnOverlappingObjectMisraCpp.ql`<br/> `ObjectCopiedToAnOverlappingObjectMisraCpp.ql` |
| RULE-8-18-2 | `-Wparentheses` | `-Wparentheses` | | `ResultOfAnAssignmentOperatorShouldNotBeUsed.ql` |
| RULE-8-19-1 | | `-Wcomma` | | `CommaOperatorShouldNotBeUsed.ql` |
| RULE-8-20-1 | | `-Wconstant-conversion` | | `UnsignedOperationWithConstantOperandsWraps.ql` |
| RULE-9-2-1 | | `-Wunused-value` | `bugprone-unused-raii` | `NoStandaloneTypeCastExpression.ql` |
| RULE-9-3-1 | `-Wdangling-else` <br/> `-Wmisleading-indentation` | `-Wdangling-else` <br/> `-Wmisleading-indentation` <br/> `-Wempty-body` | `readability-braces-around-statements` | `SwitchBodyCompoundCondition.ql`<br/> `LoopBodyCompoundCondition.ql` |
| RULE-9-4-1 | | | | `IfElseIfEndCondition.ql` |
| RULE-9-4-2 | | `-Wimplicit-fallthrough` <br/> `-Wswitch` <br/> `-Wswitch-default` <br/> `-Wswitch-enum` | `hicpp-multiway-paths-covered` | `AppropriateStructureOfSwitchStatement.ql` |
| RULE-9-5-1 | | | `bugprone-too-small-loop-variable` | `LegacyForStatementsShouldBeSimple.ql` |
| RULE-9-5-2 | | `-Wdangling-gsl` | | `ForRangeInitializerAtMostOneFunctionCall.ql` |
| RULE-9-6-1 | | | `cppcoreguidelines-avoid-goto` | `GotoStatementShouldNotBeUsed.ql` |
| RULE-9-6-2 | | | | `GotoReferenceALabelInSurroundingBlock.ql` |
| RULE-9-6-3 | | | | `GotoShallJumpToLabelDeclaredLaterInTheFunction.ql` |
| RULE-9-6-4 | | `-Winvalid-noreturn` | | `FunctionDeclaredWithTheNoreturnAttributeReturn.ql` |
| RULE-9-6-5 | `-Wreturn-type` | `-Wreturn-type` | | `NonVoidFunctionShallReturnAValueOnAllPaths.ql` |
| RULE-10-0-1 | | | | `UseSingleLocalDeclarators.ql`<br/> `UseSingleGlobalOrMemberDeclarators.ql` |
| RULE-10-1-1 | | | `readability-non-const-parameter` | `PointerOrRefParamNotConst.ql` |
| RULE-10-1-2 | `-Wignored-qualifiers` | `-Wignored-qualifiers` | | `VolatileQualifierNotUsedAppropriately.ql` |
| RULE-10-2-1 | | | | `EnumerationNotDefinedWithAnExplicitUnderlyingType.ql` |
| RULE-10-2-2 | | `-Wshadow` | | `UnscopedEnumerationsShouldNotBeDeclared.ql` |
| RULE-10-2-3 | `-Wenum-compare` | `-Wenum-compare` | | `UnscopedEnumWithoutFixedUnderlyingTypeUsed.ql` |
| RULE-10-3-1 | | | `google-build-namespaces` | `UnnamedNamespacesInHeaderFiles.ql` |
| RULE-10-4-1 | | | `hicpp-no-assembler` | `AsmDeclarationShallNotBeUsed.ql` |
| RULE-11-3-1 | | | `modernize-avoid-c-arrays` | `VariableDeclaredArrayType.ql` |
| RULE-11-3-2 | | | | `DeclarationOfAnObjectIndirectionsLevel.ql` |
| RULE-11-6-1 | | | `cppcoreguidelines-init-variables` | `UninitializedVariable.ql` |
| RULE-11-6-2 | `-Wuninitialized` | `-Wuninitialized` | `cppcoreguidelines-pro-type-member-init` | `ValueOfAnObjectMustNotBeReadBeforeItHasBeenSet.ql` |
| RULE-11-6-3 | | `-Wduplicate-enum` | | `NonUniqueEnumerationConstant.ql` |
| RULE-12-2-1 | | | | `BitFieldsShouldNotBeDeclaredMisraCpp.ql` |
| RULE-12-2-2 | | | | `BitFieldShallHaveAnAppropriateType.ql` |
| RULE-12-2-3 | | | | `SignedIntegerNamedBitFieldHaveALengthOfOneBit.ql` |
| RULE-12-3-1 | | | | `UnionKeywordUsed.ql` |
| RULE-13-1-1 | | | `fuchsia-virtual-inheritance` | `VirtualInheritanceNotAllowed.ql` |
| RULE-13-1-2 | | | | `VirtualAndNonVirtualClassInTheHierarchy.ql` |
| RULE-13-3-1 | | `-Winconsistent-missing-destructor-override` <br/> `-Winconsistent-missing-override` | `modernize-use-override` | `MemberSpecifiersNotUsedAppropriately.ql` |
| RULE-13-3-2 | | | | `OverridingShallSpecifyDifferentDefaultArguments.ql` |
| RULE-13-3-3 | | | | `DeclarationsOfAFunctionSameParameterName.ql` |
| RULE-13-3-4 | | | | `PotentiallyVirtualPointerOnlyComparesToNullptr.ql` |
| RULE-14-1-1 | | | | `PrivateAndPublicDataMembersMixed.ql` |
| RULE-15-0-1 | | `-Wextra-semi-stmt` | `cppcoreguidelines-slicing` <br/> `cppcoreguidelines-special-member-functions` <br/> `cppcoreguidelines-virtual-class-destructor` <br/> `modernize-use-equals-default` | `ImproperlyProvidedSpecialMemberFunctions.ql`<br/> `ImproperlyProvidedSpecialMemberFunctionsAudit.ql` |
| RULE-15-0-2 | | | `misc-unconventional-assign-operator` | `InvalidSignatureForSpecialMemberFunction.ql` |
| RULE-15-1-1 | | `-Wcall-to-pure-virtual-from-ctor-dtor` | | `ObjectsDynamicTypeUsedFromConstructorOrDestructor.ql` |
| RULE-15-1-2 | | | | `InitializeAllVirtualBaseClasses.ql` |
| RULE-15-1-3 | | | `google-explicit-constructor` | `NonExplicitConversionMember.ql` |
| RULE-15-1-4 | | | `cppcoreguidelines-pro-type-member-init` | `NonStaticMemberNotInitBeforeUse.ql` |
| RULE-15-1-5 | | | | `InitializerListConstructorIsTheOnlyConstructor.ql` |
| DIR-15-8-1 | | | `bugprone-unhandled-self-assignment` | `CopyAndMoveAssignmentsShallHandleSelfAssignment.ql` |
| RULE-16-5-1 | | | `fuchsia-overloaded-operator` | `LogicalAndAndLogicalOrOperatorsOverloaded.ql` |
| RULE-16-5-2 | | | `google-runtime-operator` | `AddressOfOperatorOverloaded.ql` |
| RULE-16-6-1 | | | | `InvalidOperatorOverloadedAsMemberFunction.ql` |
| RULE-17-8-1 | | | | `FunctionTemplatesExplicitlySpecialized.ql` |
| RULE-18-1-1 | | | | `ExceptionObjectHavePointerType.ql` |
| RULE-18-1-2 | | | | `EmptyThrowOnlyWithinACatchHandler.ql` |
| RULE-18-3-1 | | | | `MissingCatchAllExceptionHandlerInMain.ql` |
| RULE-18-3-2 | | | `misc-throw-by-value-catch-by-reference` | `ClassExceptionCaughtByValue.ql` |
| RULE-18-3-3 | | `-Wexceptions` | | `HandlersReferToNonStaticMembersFromTheirClass.ql` |
| RULE-18-4-1 | | | | `ExceptionUnfriendlyFunctionMustBeNoexcept.ql` |
| RULE-18-5-1 | `-Wterminate` | `-Wexceptions` | | `NoexceptFunctionShouldNotPropagateToTheCaller.ql` |
| RULE-18-5-2 | | | | `AvoidProgramTerminatingFunctions.ql` |
| RULE-19-0-1 | | | | See notes [4] |
| RULE-19-0-2 | | | `cppcoreguidelines-macro-usage` | `FunctionLikeMacrosDefined.ql` |
| RULE-19-0-3 | | | | `IncludeDirectivesPrecededByPreprocessorDirectives.ql` |
| RULE-19-0-4 | | | | `UndefOfMacroNotDefinedInFile.ql` |
| RULE-19-1-1 | `-Wexpansion-to-defined` | `-Wexpansion-to-defined` | | `InvalidTokenInDefinedOperator.ql`<br/> `DefinedOperatorExpandedInIfDirective.ql` |
| RULE-19-1-2 | | | | See notes [4] |
| RULE-19-1-3 | | `-Wundef` | | `IdentifiersUsedInTheControllingExpressionOf.ql` |
| RULE-19-2-1 | | | | `NoValidIfdefGuardInHeader.ql`<br/> `IncludeOutsideGuard.ql` |
| RULE-19-2-2 | | `-Wendif-labels` <br/> `-Wextra-tokens` | | `InvalidIncludeDirective.ql` |
| RULE-19-2-3 | | | | `CharsThatShouldNotOccurInHeaderFileName.ql` |
| RULE-19-3-1 | | | | `AndPreprocessorOperatorsShouldNotBeUsed.ql` |
| RULE-19-3-2 | | | | `MacroParameterFollowingHash.ql` |
| RULE-19-3-3 | | | | `AMixedUseMacroArgumentSubjectToExpansion.ql` |
| RULE-19-3-4 | | | `readability-math-missing-parentheses` | `UnparenthesizedMacroArgument.ql` |
| RULE-19-3-5 | | `-Wembedded-directive` | | `TokensThatLookLikeDirectivesInAMacroArgument.ql` |
| RULE-19-6-1 | | `-Wpragma-once-outside-header` | | `DisallowedUseOfPragma.ql` |
| RULE-21-2-1 | | | | `AtofAtoiAtolAndAtollUsed.ql` |
| RULE-21-2-2 | | | | `UnsafeStringHandlingFunctions.ql` |
| RULE-21-2-3 | | | | `BannedSystemFunction.ql` |
| RULE-21-2-4 | | | | `MacroOffsetofShallNotBeUsed.ql` |
| RULE-21-6-1 | | | `cppcoreguidelines-no-malloc` | `DynamicMemoryShouldNotBeUsed.ql` |
| RULE-21-6-2 | | | `cppcoreguidelines-no-malloc` | `DynamicMemoryManagedManually.ql` |
| RULE-21-6-3 | | | `fuchsia-overloaded-operator` | `AdvancedMemoryManagementUsed.ql` |
| RULE-21-6-4 | | | | `GlobalSizedOperatorDeleteShallBeDefined.ql`<br/> `GlobalUnsizedOperatorDeleteShallBeDefined.ql` |
| RULE-21-6-5 | `-Wdelete-incomplete` | `-Wdelete-incomplete` | | `PointerToAnIncompleteClassTypeDeleted.ql` |
| RULE-21-10-1 | | | `cppcoreguidelines-pro-type-vararg` | `NoVariadicFunctionMacros.ql` |
| RULE-21-10-2 | | | | `NoCsetjmpHeader.ql` |
| RULE-21-10-3 | | | | `CsignalFacilitiesUsed.ql`<br/> `CsignalTypesShallNotBeUsed.ql` |
| RULE-22-3-1 | | | | `AssertMacroUsedWithAConstantExpression.ql` |
| RULE-22-4-1 | | | | `InvalidAssignmentToErrno.ql` |
| RULE-23-11-1 | | | `modernize-make-shared` | `UseSmartPtrFactoryFunctions.ql` |
| RULE-24-5-1 | | | | `CharacterHandlingFunctionRestrictions.ql` |
| RULE-24-5-2 | | | `bugprone-suspicious-memory-comparison` | `NoMemoryFunctionsFromCString.ql` |
| RULE-25-5-1 | | | | `LocaleGlobalFunctionNotAllowed.ql` |
| RULE-25-5-2 | | | | `PointersReturnedByLocaleFunctionsMustBeUsedAsConst.ql` |
| RULE-25-5-3 | | | | `CallToSetlocaleInvalidatesOldPointersMisra.ql`<br/> `CallToSetlocaleInvalidatesOldPointersWarnMisra.ql` |
| RULE-26-3-1 | | | | `VectorShouldNotBeSpecializedWithBool.ql` |
| RULE-28-3-1 | | | | `PredicateWithPersistentSideEffects.ql`<br/> `NonConstPredicateFunctionObject.ql` |
| RULE-28-6-1 | | | | `StdMoveWithNonConstLvalue.ql` |
| RULE-28-6-2 | | | | `ForwardingReferencesAndForwardNotUsedTogether.ql` |
| RULE-28-6-3 | | | `bugprone-use-after-move` | `ObjectUsedWhileInPotentiallyMovedFromState.ql` |
| RULE-28-6-4 | | | `bugprone-unused-return-value` | `PotentiallyErroneousContainerUsage.ql` |
| RULE-30-0-1 | | | `modernize-deprecated-headers` | `CstdioFunctionsShallNotBeUsed.ql`<br/> `CstdioMacrosShallNotBeUsed.ql`<br/> `CstdioTypesShallNotBeUsed.ql` |
| RULE-30-0-2 | | | | `ReadsAndWritesOnStreamNotSeparatedByPositioning.ql` |

[1] This rule cannot be enfoced with clang, gcc, clang-tidy, or CodeQL. However, both [libc++](https://libcxx.llvm.org/Hardening.html) and [libstd++](https://gcc.gnu.org/onlinedocs/libstdc++/manual/debug_mode.html), provide a version of the C++ standard library in hardned mode. Such hardened mode allow to detect the violations of some of the preconditions of the C++ standard library. Our plan to follow DIR-0-3-2 is to follow a multi tool approach. Make use of the hardened mode in both standard libraries as well as the use of UB sanitizers. Additionally, for test making use of `SCORE_LANGUAGE_FUTURECPP_PRECONDITION_*` that run only in debug mode, the plan is to run our unit and integration tests making sure that such preconditions get evaluated.

[2] Apart from using CodeQL, our plan is to also use undefined behavior sanitizers to mitigate the risk of some findings not being found by CodeQL. For the unspecified behavior, it is also mitigated by the use of multiple compilers.

[3] Apart from using CodeQL, our plan is to also use memory sanitizers to mitigate the risk of some findings not being found by CodeQL.

[4] The plan is to rely on the compiler for the enforcement of this rule. The CodeQL user manual mentions "The rules `5.13.7`, `19.0.1` and `19.1.2` are not planned to be implemented by CodeQL as they are compiler checked in all supported compilers.".

[5] Apart from using CodeQL, our plan is to also use address sanitizers to mitigate the risk of some findings not being found by CodeQL.
