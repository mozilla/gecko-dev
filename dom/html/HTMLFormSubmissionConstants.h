/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLFormSubmissionConstants_h
#define mozilla_dom_HTMLFormSubmissionConstants_h

#define NS_FORM_METHOD_GET 0
#define NS_FORM_METHOD_POST 1
#define NS_FORM_METHOD_DIALOG 2
#define NS_FORM_ENCTYPE_URLENCODED 0
#define NS_FORM_ENCTYPE_MULTIPART 1
#define NS_FORM_ENCTYPE_TEXTPLAIN 2

static constexpr nsAttrValue::EnumTableEntry kFormMethodTable[] = {
    {"get", NS_FORM_METHOD_GET},
    {"post", NS_FORM_METHOD_POST},
    {"dialog", NS_FORM_METHOD_DIALOG}};

// Default method is 'get'.
static constexpr const nsAttrValue::EnumTableEntry* kFormDefaultMethod =
    &kFormMethodTable[0];

static constexpr nsAttrValue::EnumTableEntry kFormEnctypeTable[] = {
    {"multipart/form-data", NS_FORM_ENCTYPE_MULTIPART},
    {"application/x-www-form-urlencoded", NS_FORM_ENCTYPE_URLENCODED},
    {"text/plain", NS_FORM_ENCTYPE_TEXTPLAIN},
};

// Default method is 'application/x-www-form-urlencoded'.
static constexpr const nsAttrValue::EnumTableEntry* kFormDefaultEnctype =
    &kFormEnctypeTable[1];

#endif  // mozilla_dom_HTMLFormSubmissionConstants_h
