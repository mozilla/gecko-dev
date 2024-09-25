/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

"use strict";
add_heuristic_tests([
  {
    description:
      "Ensure fields with 'cc' or 'card' and 'type' are recognized as credit card type fields, and other 'type' fields are not.",
    fixtureData: `
      <html>
        <body>
          <form>
            <input id="cc-number" autocomplete="cc-number">
            <select id="cc-type">
              <option value="0" selected="">Card Type</option>
              <option value="1">Visa</option>
              <option value="2">MasterCard</option>
              <option value="3">Diners Club International</option>
              <option value="4">Discover</option>
            </select>
          </form>
          <form>
            <input id="cc-number" autocomplete="cc-number">
            <select id="card-type">
              <option value="0" selected="">Card Type</option>
              <option value="1">Visa</option>
              <option value="2">MasterCard</option>
              <option value="3">Diners Club International</option>
              <option value="4">Discover</option>
            </select>
          </form>
          <form>
            <input type="text" id="name-type" autocomplete="name">
            <input id="street-address-type" autocomplete="street-address">
            <input type="text" id="country-type" autocomplete="country">
            </form>
        </body>
      </html>
    `,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", reason: "autocomplete" },
          { fieldName: "cc-type", reason: "regex-heuristic" },
        ],
      },
      {
        fields: [
          { fieldName: "cc-number", reason: "autocomplete" },
          { fieldName: "cc-type", reason: "regex-heuristic" },
        ],
      },
      {
        fields: [
          { fieldName: "name", reason: "autocomplete" },
          { fieldName: "street-address", reason: "autocomplete" },
          { fieldName: "country", reason: "autocomplete" },
        ],
      },
    ],
  },
]);
