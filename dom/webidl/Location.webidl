/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://html.spec.whatwg.org/multipage/history.html#the-location-interface
 *
 * © Copyright 2004-2011 Apple Computer, Inc., Mozilla Foundation, and
 * Opera Software ASA. You are granted a license to use, reproduce
 * and create derivative works of this document.
 */

[LegacyUnforgeable,
 Exposed=Window,
 InstrumentedProps=(ancestorOrigins)]
interface Location {
  [Throws, CrossOriginWritable, NeedsSubjectPrincipal]
  stringifier attribute UTF8String href;
  [Throws, NeedsSubjectPrincipal]
  readonly attribute UTF8String origin;
  [Throws, NeedsSubjectPrincipal]
           attribute UTF8String protocol;
  [Throws, NeedsSubjectPrincipal]
           attribute UTF8String host;
  [Throws, NeedsSubjectPrincipal]
           attribute UTF8String hostname;
  [Throws, NeedsSubjectPrincipal]
           attribute UTF8String port;
  [Throws, NeedsSubjectPrincipal]
           attribute UTF8String pathname;
  [Throws, NeedsSubjectPrincipal]
           attribute UTF8String search;
  [Throws, NeedsSubjectPrincipal]
           attribute UTF8String hash;

  [Throws, NeedsSubjectPrincipal]
  undefined assign(UTF8String url);

  [Throws, CrossOriginCallable, NeedsSubjectPrincipal]
  undefined replace(UTF8String url);

  // XXXbz there is no forceget argument in the spec!  See bug 1037721.
  [Throws, NeedsSubjectPrincipal]
  undefined reload(optional boolean forceget = false);

  // Bug 1085214 [SameObject] readonly attribute USVString[] ancestorOrigins;
};
