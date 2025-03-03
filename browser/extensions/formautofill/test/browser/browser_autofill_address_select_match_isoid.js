/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test ensures that an inexact substring match on an option label is performed
// when autofilling dropdown values.

const TEST_PROFILE = {
  "given-name": "Joe",
  "family-name": "Smith",
  "street-address": "7 First St",
  "address-level1": "BR",
  country: "IN",
};

const MARKUP_SELECT_STATE = `
  <html><body>
    <form>
     <input id="given-name" autocomplete="given-name">
     <input id="family-name" autocomplete="family-name">
     <input id="street" autocomplete="street-address">
     <input id="address-level2" autocomplete="address-level2">
     <select id="address-level1" autocomplete="address-level1">
       <option value=""> --- Please Select --- </option>
       <option value="1475">Andaman and Nicobar Islands</option>
       <option value="1476">Andhra Pradesh</option>
       <option value="1477">Arunachal Pradesh</option>
       <option value="1478">Assam</option>
       <option value="1479">Bihar</option>
       <option value="1480">Chandigarh</option>
       <option value="1481">Dadra and Nagar Haveli</option>
     </select>
    </form>
  </body></html>
`;

add_autofill_heuristic_tests([
  {
    fixtureData: MARKUP_SELECT_STATE,
    profile: TEST_PROFILE,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "given-name", autofill: TEST_PROFILE["given-name"] },
          { fieldName: "family-name", autofill: TEST_PROFILE["family-name"] },
          {
            fieldName: "street-address",
            autofill: TEST_PROFILE["street-address"],
          },
          {
            fieldName: "address-level2",
            autofill: TEST_PROFILE["address-level2"],
          },
          { fieldName: "address-level1", autofill: 1479 },
        ],
      },
    ],
  },
]);
