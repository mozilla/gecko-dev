/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

//
// Array equality
//
function arraysAreEqual(arr1, arr2) {
  if (arr1.length !== arr2.length) {
    return false;
  }
  for (let i = 0; i < arr1.length; i++) {
    if (arr1[i] !== arr2[i]) {
      return false;
    }
  }
  return true;
}

//
// Serialization / Derserialization helpers
//
function stringToByteArray(str) {
  return new TextEncoder().encode(str);
}

function byteArrayToString(byteArray) {
  return new TextDecoder().decode(new Uint8Array(byteArray).buffer);
}

function stringToArrayBuffer(str) {
  return new Uint8Array(new TextEncoder().encode(str)).buffer;
}

function byteArrayToHexString(buffer) {
  const byteArray = new Uint8Array(buffer);
  const hexParts = [];
  for (let i = 0; i < byteArray.length; i++) {
    const hex = byteArray[i].toString(16);
    const paddedHex = ("00" + hex).slice(-2);
    hexParts.push(paddedHex);
  }
  return hexParts.join("");
}
