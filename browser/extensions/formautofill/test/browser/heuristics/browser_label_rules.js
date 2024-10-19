/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

"use strict";

add_heuristic_tests([
  {
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="name" autocomplete="name"/>
            <input type="text" id="country" autocomplete="country"/>
            <label for="test1">sender-address</label>
            <input type="text" id="test1"/>
            <input type="text" id="test2" name="sender-address"/>
          </form>
        </body>
        </html>`,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        description: `Only "sender-address" keywords in labels"`,
        fields: [
          { fieldName: "name", reason: "autocomplete" },
          { fieldName: "country", reason: "autocomplete" },
          { fieldName: "address-line1" },
        ],
      },
    ],
  },
  {
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="name" autocomplete="name"/>
            <input type="text" id="country" autocomplete="country"/>
            <input type="text" id="test" aria-label="street-address"/>
          </form>
        </body>
        </html>`,
    expectedResult: [
      {
        description: `keywords are in aria-label`,
        fields: [
          { fieldName: "name", reason: "autocomplete" },
          { fieldName: "country", reason: "autocomplete" },
          { fieldName: "street-address", reason: "regex-heuristic" },
        ],
      },
    ],
  },
  {
    fixtureData: `
        <html>
        <body>
          <form>
            <div><input id="fname"><label>First name</label></div>
            <div><input id="thel"><label>Last name</label></div>
            <div><input id="line1"><label>Street</label></div>
            <div><label>Postal Code</label><input id="pc"></div>
          </form>
        </body>
        </html>`,
    expectedResult: [
      {
        description: `label does not have a directly specified control`,
        fields: [
          { fieldName: "given-name", reason: "regex-heuristic" },
          { fieldName: "family-name", reason: "regex-heuristic" },
          { fieldName: "street-address", reason: "regex-heuristic" },
          { fieldName: "postal-code", reason: "regex-heuristic" },
        ],
      },
    ],
  },
]);
