/**
 * MIT License
 *
 * Copyright (c) 2022 GitHub
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Modified in 2026 by Contributors to the Eclipse Foundation.
 * SPDX-License-Identifier: MIT
 *
 * @id cpp/misra/type-aliases-declaration
 * @name RULE-6-9-1: The same type aliases shall be used in all declarations of the same entity
 * @description Using different type aliases on redeclarations can make code hard to understand and
 *              maintain.
 * @kind problem
 * @precision very-high
 * @problem.severity warning
 * @tags external/misra/id/rule-6-9-1
 *       maintainability
 *       readability
 *       scope/single-translation-unit
 *       external/misra/enforcement/decidable
 *       external/misra/obligation/required
 */

import cpp
import codingstandards.cpp.misra

// Some alias-template instances have no declaration location in the extracted
// database. Link their actual type-name use, retaining the alias name, rather
// than emitting a root-only URI. Located alias declarations remain unchanged.
pragma[inline]
bindingset[t, use]
Element typeAliasLink(TypedefType t, DeclarationEntry use) {
  if t.getLocation().getFile().getAbsolutePath() != ""
  then result = t
  else result = use
}

from DeclarationEntry decl1, DeclarationEntry decl2, TypedefType t
where
  not isExcluded(decl1, Declarations5Package::typeAliasesDeclarationQuery()) and
  not isExcluded(decl2, Declarations5Package::typeAliasesDeclarationQuery()) and
  not decl1 = decl2 and
  decl1.getDeclaration() = decl2.getDeclaration() and
  t.getATypeNameUse() = decl1 and
  not t.getATypeNameUse() = decl2 and
  //exception cases - we dont want to disallow struct typedef name use
  not t.getBaseType() instanceof Struct and
  not t.getBaseType() instanceof Enum
select decl1,
  "Declaration entry has a different type alias than $@ where the type alias used is '$@'.", decl2,
  decl2.getName(), typeAliasLink(t, decl1), t.getName()
