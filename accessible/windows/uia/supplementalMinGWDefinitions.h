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
#ifndef AnnotationType_Unknown
const long AnnotationType_Unknown = 60000;
#endif
#ifndef AnnotationType_SpellingError
const long AnnotationType_SpellingError = 60001;
#endif
#ifndef AnnotationType_GrammarError
const long AnnotationType_GrammarError = 60002;
#endif
#ifndef AnnotationType_Comment
const long AnnotationType_Comment = 60003;
#endif
#ifndef AnnotationType_FormulaError
const long AnnotationType_FormulaError = 60004;
#endif
#ifndef AnnotationType_TrackChanges
const long AnnotationType_TrackChanges = 60005;
#endif
#ifndef AnnotationType_Header
const long AnnotationType_Header = 60006;
#endif
#ifndef AnnotationType_Footer
const long AnnotationType_Footer = 60007;
#endif
#ifndef AnnotationType_Highlighted
const long AnnotationType_Highlighted = 60008;
#endif
#ifndef AnnotationType_Endnote
const long AnnotationType_Endnote = 60009;
#endif
#ifndef AnnotationType_Footnote
const long AnnotationType_Footnote = 60010;
#endif
#ifndef AnnotationType_InsertionChange
const long AnnotationType_InsertionChange = 60011;
#endif
#ifndef AnnotationType_DeletionChange
const long AnnotationType_DeletionChange = 60012;
#endif
#ifndef AnnotationType_MoveChange
const long AnnotationType_MoveChange = 60013;
#endif
#ifndef AnnotationType_FormatChange
const long AnnotationType_FormatChange = 60014;
#endif
#ifndef AnnotationType_UnsyncedChange
const long AnnotationType_UnsyncedChange = 60015;
#endif
#ifndef AnnotationType_EditingLockedChange
const long AnnotationType_EditingLockedChange = 60016;
#endif
#ifndef AnnotationType_ExternalChange
const long AnnotationType_ExternalChange = 60017;
#endif
#ifndef AnnotationType_ConflictingChange
const long AnnotationType_ConflictingChange = 60018;
#endif
#ifndef AnnotationType_Author
const long AnnotationType_Author = 60019;
#endif
#ifndef AnnotationType_AdvancedProofingIssue
const long AnnotationType_AdvancedProofingIssue = 60020;
#endif
#ifndef AnnotationType_DataValidationError
const long AnnotationType_DataValidationError = 60021;
#endif
#ifndef AnnotationType_CircularReferenceError
const long AnnotationType_CircularReferenceError = 60022;
#endif
#ifndef AnnotationType_Mathematics
const long AnnotationType_Mathematics = 60023;
#endif
#ifndef AnnotationType_Sensitive
const long AnnotationType_Sensitive = 60024;
#endif

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
