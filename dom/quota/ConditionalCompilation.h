/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_CONDITIONALCOMPILATION_H_
#define DOM_QUOTA_CONDITIONALCOMPILATION_H_

#include "mozilla/dom/quota/RemoveParen.h"

/**
 * Macros for conditional compilation based on build configuration.
 *
 * These macros are primarily used to inline debug or configuration specific
 * declarations or expressions in a single line without needing explicit #ifdef
 * blocks. This improves readability and avoids code clutter.
 *
 * Current macros include:
 * - DEBUGONLY(expr)
 * - DIAGNOSTICONLY(expr)
 *
 * This header may also include future macros such as:
 * - NIGHTLYONLY(expr)
 * - IF_NIGHTLY(expr)
 *
 * All macros in this file are designed for compile time control over code
 * inclusion and should not introduce runtime behavior.
 */

#ifdef DEBUG
#  define DEBUGONLY(expr) MOZ_REMOVE_PAREN(expr)
#else
#  define DEBUGONLY(expr)
#endif

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
#  define DIAGNOSTICONLY(expr) MOZ_REMOVE_PAREN(expr)
#else
#  define DIAGNOSTICONLY(expr)
#endif

#endif  // DOM_QUOTA_CONDITIONALCOMPILATION_H_
