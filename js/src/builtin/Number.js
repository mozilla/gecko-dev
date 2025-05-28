/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// ES6 draft ES6 20.1.2.4
function Number_isFinite(num) {
  if (typeof num !== "number") {
    return false;
  }
  return num - num === 0;
}

// ES6 draft ES6 20.1.2.2
function Number_isNaN(num) {
  if (typeof num !== "number") {
    return false;
  }
  return num !== num;
}

// ES2021 draft rev 889f2f30cf554b7ed812c0984626db1c8a4997c7
// 20.1.2.3 Number.isInteger ( number )
function Number_isInteger(number) {
  // Step 1. (Inlined call to IsIntegralNumber)

  // 7.2.6 IsIntegralNumber, step 1.
  if (typeof number !== "number") {
    return false;
  }

  var integer = std_Math_trunc(number);

  // 7.2.6 IsIntegralNumber, steps 2-4.
  // |number - integer| ensures Infinity correctly returns false, because
  // |Infinity - Infinity| yields NaN.
  return number - integer === 0;
}

// ES2021 draft rev 889f2f30cf554b7ed812c0984626db1c8a4997c7
// 20.1.2.5 Number.isSafeInteger ( number )
function Number_isSafeInteger(number) {
  // Step 1. (Inlined call to IsIntegralNumber)

  // 7.2.6 IsIntegralNumber, step 1.
  if (typeof number !== "number") {
    return false;
  }

  var integer = std_Math_trunc(number);

  // 7.2.6 IsIntegralNumber, steps 2-4.
  // |number - integer| to handle the Infinity case correctly.
  if (number - integer !== 0) {
    return false;
  }

  // Steps 1.a, 2.
  // prettier-ignore
  return -((2 ** 53) - 1) <= integer && integer <= (2 ** 53) - 1;
}

function Global_isNaN(number) {
  return Number_isNaN(ToNumber(number));
}

function Global_isFinite(number) {
  return Number_isFinite(ToNumber(number));
}
