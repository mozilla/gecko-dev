/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { NetUtil } from "resource://gre/modules/NetUtil.sys.mjs";
import { Assert } from "resource://testing-common/Assert.sys.mjs";

export function checkInputAndSerializationMatch(input) {
  let uri = NetUtil.newURI(input);
  Assert.equal(
    uri.spec,
    input,
    `serialization should match the input: {input}`
  );
}

export function checkInputAndSerializationMatchChild(input) {
  let uri = Services.io.newURI(input);
  Assert.equal(
    uri.spec,
    input,
    `serialization should match the input: {input}`
  );
}

export function removeSecondColon(str) {
  let colonIndex = str.indexOf(":");
  if (colonIndex !== -1) {
    colonIndex = str.indexOf(":", colonIndex + 1);
    if (colonIndex !== -1) {
      return str.slice(0, colonIndex) + str.slice(colonIndex + 1);
    }
  }
  Assert.ok(false, "we expected at least two colons");
  return str;
}

export function checkSerializationMissingSecondColon(input) {
  let uri = NetUtil.newURI(input);
  Assert.equal(
    uri.spec,
    removeSecondColon(input),
    `serialization should be missing second colon from input: {input}`
  );
}

export function checkSerializationMissingSecondColonChild(input) {
  let uri = Services.io.newURI(input);
  Assert.equal(
    uri.spec,
    removeSecondColon(input),
    `serialization should be missing second colon from input: {input}`
  );
}

export function runParentTestSuite() {
  // sanity check
  checkInputAndSerializationMatch("https://example.com/");

  // special scheme uses nsStanardURL
  checkSerializationMissingSecondColon("https://https://example.com");

  // no-bypass protocol uses defaultURI
  checkSerializationMissingSecondColon(
    "defaulturischeme://https://example.com"
  );

  // an unknown protocol in the bypass list (remote settings) uses simpleURI
  checkInputAndSerializationMatch("testsyncscheme://https://example.com");

  // pref-specified scheme bypass uses simpleURI
  checkInputAndSerializationMatch("simpleprotocol://https://example.com");
}
