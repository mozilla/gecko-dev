/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is a shim designed to keep Gecko compiling while we wait for MinGW
// to add all of the various UIA attributes we need. We should only include this
// file if compiling with MinGW. We should remove this file when MinGW adds the
// necessary definitions. See bug 1929755.

#ifndef mozilla_a11y_supplementalMinGWDefinitions_h__
#define mozilla_a11y_supplementalMinGWDefinitions_h__

// UIA Annotation Type Identifiers
const long AnnotationType_Unknown = 60000;
const long AnnotationType_SpellingError = 60001;
const long AnnotationType_GrammarError = 60002;
const long AnnotationType_Comment = 60003;
const long AnnotationType_FormulaError = 60004;
const long AnnotationType_TrackChanges = 60005;
const long AnnotationType_Header = 60006;
const long AnnotationType_Footer = 60007;
const long AnnotationType_Highlighted = 60008;
const long AnnotationType_Endnote = 60009;
const long AnnotationType_Footnote = 60010;
const long AnnotationType_InsertionChange = 60011;
const long AnnotationType_DeletionChange = 60012;
const long AnnotationType_MoveChange = 60013;
const long AnnotationType_FormatChange = 60014;
const long AnnotationType_UnsyncedChange = 60015;
const long AnnotationType_EditingLockedChange = 60016;
const long AnnotationType_ExternalChange = 60017;
const long AnnotationType_ConflictingChange = 60018;
const long AnnotationType_Author = 60019;
const long AnnotationType_AdvancedProofingIssue = 60020;
const long AnnotationType_DataValidationError = 60021;
const long AnnotationType_CircularReferenceError = 60022;
const long AnnotationType_Mathematics = 60023;
const long AnnotationType_Sensitive = 60024;

// UIA Style Identifiers
const long StyleId_Custom = 70000;
const long StyleId_Heading1 = 70001;
const long StyleId_Heading2 = 70002;
const long StyleId_Heading3 = 70003;
const long StyleId_Heading4 = 70004;
const long StyleId_Heading5 = 70005;
const long StyleId_Heading6 = 70006;
const long StyleId_Heading7 = 70007;
const long StyleId_Heading8 = 70008;
const long StyleId_Heading9 = 70009;
const long StyleId_Title = 70010;
const long StyleId_Subtitle = 70011;
const long StyleId_Normal = 70012;
const long StyleId_Emphasis = 70013;
const long StyleId_Quote = 70014;
const long StyleId_BulletedList = 70015;
const long StyleId_NumberedList = 70016;

#endif  // mozilla_a11y_supplementalMinGWDefinitions_h__
