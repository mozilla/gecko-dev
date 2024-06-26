/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `border-collapse`, `border-spacing`,
// `table-layout` test cases.
export default [
  ...createTestsForProp("border-collapse", "collapse"),
  ...createTestsForProp("border-spacing", "10px"),
  ...createTestsForProp("table-layout", "fixed"),
];

function createTestsForProp(propertyName, propertyValue) {
  return [
    {
      info: `${propertyName} is inactive on block element`,
      property: propertyName,
      tagName: "div",
      rules: [`div { ${propertyName}: ${propertyValue}; }`],
      isActive: false,
    },
    {
      info: `${propertyName} is active on table element`,
      property: propertyName,
      tagName: "div",
      rules: [`div { display: table; ${propertyName}: ${propertyValue}; }`],
      isActive: true,
    },
    {
      info: `${propertyName} is active on inline table element`,
      property: propertyName,
      tagName: "div",
      rules: [
        `div { display: inline-table; ${propertyName}: ${propertyValue}; }`,
      ],
      isActive: true,
    },
  ];
}
