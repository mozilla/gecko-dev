/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that unsupported CSS properties are correctly reported as issues.

const {
  COMPATIBILITY_ISSUE_TYPE,
} = require("resource://devtools/shared/constants.js");

const TEST_URI = `
  <style>
  body {
    color: blue;
    scrollbar-width: thin;
    user-modify: read-only;
    object-view-box: inset(0% 19% -33% 57%);
    overflow-clip-box: padding-box;
  }
  div {
    ruby-align: center;
  }
  </style>
  <body>
    <div>test</div>
  </body>
`;

const TEST_DATA_SELECTED = [
  {
    type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
    property: "scrollbar-width",
    url: "https://developer.mozilla.org/docs/Web/CSS/scrollbar-width",
    deprecated: false,
    experimental: false,
  },
  {
    type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY_ALIASES,
    property: "user-modify",
    url: "https://developer.mozilla.org/docs/Web/CSS/user-modify",
    aliases: ["user-modify"],
    deprecated: true,
    experimental: false,
  },
  {
    type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
    property: "object-view-box",
    // No MDN url but a spec one
    specUrl: "https://drafts.csswg.org/css-images-5/#propdef-object-view-box",
    deprecated: false,
    experimental: true,
  },
  // TODO: Re-enable it when we have another property with no MDN url nor spec url Bug 1840910
  /*{
    // No MDN url nor spec url
    type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
    property: "overflow-clip-box",
    deprecated: false,
    experimental: false,
  },*/
];

const TEST_DATA_ALL = [
  ...TEST_DATA_SELECTED,
  {
    type: COMPATIBILITY_ISSUE_TYPE.CSS_PROPERTY,
    property: "ruby-align",
    url: "https://developer.mozilla.org/docs/Web/CSS/ruby-align",
    deprecated: false,
    experimental: false,
  },
];

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const { allElementsPane, selectedElementPane } =
    await openCompatibilityView();

  // If the test fail because the properties used are no longer in the dataset, or they
  // now have mdn/spec url although we expected them not to, uncomment the next line
  // to get all the properties in the dataset that don't have a MDN url.
  // logCssCompatDataPropertiesWithoutMDNUrl()

  info("Check the content of the issue list on the selected element");
  await assertIssueList(selectedElementPane, TEST_DATA_SELECTED);

  info("Check the content of the issue list on all elements");
  await assertIssueList(allElementsPane, TEST_DATA_ALL);
});
