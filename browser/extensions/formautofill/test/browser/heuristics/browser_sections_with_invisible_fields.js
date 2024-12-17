/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

"use strict";

add_heuristic_tests([
  {
    description: `Create a new section when the section already has a field with the same field name`,
    fixtureData: `
        <html><body>
            <input type="text" autocomplete="cc-number"/>
            <input type="text" autocomplete="cc-name"/>
            <input type="text" autocomplete="cc-exp"/>
            <input type="text" autocomplete="cc-exp"/>
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
  },
  {
    description: `Do not create a new section for an invisible field`,
    fixtureData: `
        <html><body>
            <input type="text" autocomplete="cc-number"/>
            <input type="text" autocomplete="cc-name"/>
            <input type="text" autocomplete="cc-exp"/>
            <select autocomplete="cc-exp" style="display:none"></select>
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
  },
  {
    description: `Do not create a new section when the field with the same field name is an invisible field`,
    fixtureData: `
        <html><body>
            <input type="text" autocomplete="cc-number""/>
            <input type="text" autocomplete="cc-name"/>
            <select autocomplete="cc-exp" style="display:none"></select>
            <select autocomplete="cc-exp"></select>
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
  },
  {
    description: `Do not create a new section for an invisible field (match field is not adjacent)`,
    fixtureData: `
        <html><body>
            <select autocomplete="cc-exp"></select>
            <input type="text" autocomplete="cc-name"/>
            <input type="text" autocomplete="cc-number"/>
            <select autocomplete="cc-exp" style="display:none"></select>
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "cc-exp" },
          { fieldName: "cc-name" },
          { fieldName: "cc-number" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
  },
  {
    description: `Do not create a new section when the field with the same field name is an invisible field (match field is not adjacent)`,
    fixtureData: `
        <html><body>
            <select autocomplete="cc-exp" style="display:none"></select>
            <input type="text" autocomplete="cc-name"/>
            <input type="text" autocomplete="cc-number"/>
            <select autocomplete="cc-exp"></select>
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "cc-exp" },
          { fieldName: "cc-name" },
          { fieldName: "cc-number" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
  },
]);
