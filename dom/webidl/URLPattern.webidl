/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Source: URL Pattern Standard (https://urlpattern.spec.whatwg.org/)
 */

typedef (UTF8String or URLPatternInit) URLPatternInput;

[Exposed=(Window,Worker),
  Pref="dom.urlpattern.enabled"]
interface URLPattern {

  [Throws]
  constructor(URLPatternInput input, UTF8String baseURL, optional URLPatternOptions options = {});
  [Throws]
  constructor(optional URLPatternInput input = {}, optional URLPatternOptions options = {});

  [Throws]
  boolean test(optional URLPatternInput input = {}, optional UTF8String baseURL);

  [Throws]
  URLPatternResult? exec(optional URLPatternInput input = {}, optional UTF8String baseURL);

  readonly attribute UTF8String protocol;
  readonly attribute UTF8String username;
  readonly attribute UTF8String password;
  readonly attribute UTF8String hostname;
  readonly attribute UTF8String port;
  readonly attribute UTF8String pathname;
  readonly attribute UTF8String search;
  readonly attribute UTF8String hash;

  readonly attribute boolean hasRegExpGroups;
};

dictionary URLPatternInit {
  UTF8String protocol;
  UTF8String username;
  UTF8String password;
  UTF8String hostname;
  UTF8String port;
  UTF8String pathname;
  UTF8String search;
  UTF8String hash;
  UTF8String baseURL;
};

dictionary URLPatternOptions {
  boolean ignoreCase = false;
};

dictionary URLPatternResult {
  sequence<URLPatternInput> inputs;

  URLPatternComponentResult protocol;
  URLPatternComponentResult username;
  URLPatternComponentResult password;
  URLPatternComponentResult hostname;
  URLPatternComponentResult port;
  URLPatternComponentResult pathname;
  URLPatternComponentResult search;
  URLPatternComponentResult hash;
};

dictionary URLPatternComponentResult {
  UTF8String input;
  record<UTF8String, (UTF8String or undefined)> groups;
};

typedef (UTF8String or URLPatternInit or URLPattern) URLPatternCompatible;
