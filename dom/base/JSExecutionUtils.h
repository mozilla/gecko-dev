/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_BASE_JSEXECUTIONUTILS_H_
#define DOM_BASE_JSEXECUTIONUTILS_H_

#include "js/TypeDecls.h"               // JSScript
#include "js/experimental/JSStencil.h"  // JS::Stencil
#include "js/CompileOptions.h"          // JS::CompileOptions
#include "mozilla/ErrorResult.h"        // ErrorResult
#include "nsStringFwd.h"                // nsAString

namespace mozilla::dom {

nsresult EvaluationExceptionToNSResult(ErrorResult& aRv);

// Compile a script contained in a string.
void Compile(JSContext* aCx, JS::CompileOptions& aCompileOptions,
             const nsAString& aScript, RefPtr<JS::Stencil>& aStencil,
             ErrorResult& aRv);

}  // namespace mozilla::dom

#endif /* DOM_BASE_JSEXECUTIONUTILS_H_ */
