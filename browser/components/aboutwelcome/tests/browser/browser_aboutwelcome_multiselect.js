"use strict";

const BASE_CONTENT = {
  id: "MULTI_SELECT_TEST",
  targeting: "true",
  content: {
    position: "split",
    progress_bar: true,
    logo: {},
    tiles: {
      type: "multiselect",
      data: [
        {
          id: "checkbox-1",
          label: {
            raw: "Pin to taskbar",
          },
          action: {
            type: "PIN_FIREFOX_TO_TASKBAR",
          },
        },
        {
          id: "checkbox-2",
          label: {
            raw: "Label for second option",
          },
          description: {
            raw: "Description for second option",
          },
          action: {
            type: "SET_PREF",
            data: {
              pref: {
                name: "test-pref",
                value: true,
              },
            },
          },
        },
      ],
    },
  },
};

/**
 * Core multiselect functionality is covered in
 * browser_aboutwelcome_multistage_mr.js
 */

/**
 * Test rendering a screen with the MultiSelect checklist including an item with
 * a description.
 */
add_task(async function test_multiselect_with_item_description() {
  const TEST_JSON = JSON.stringify([BASE_CONTENT]);
  let browser = await openAboutWelcome(TEST_JSON);

  await test_screen_content(
    browser,
    "renders screen with a checklist item with no description and an item with a description",
    // Expected selectors:
    [
      // Both items have labels
      `.multi-select-container .multi-select-item:first-of-type label`,
      `.multi-select-container .multi-select-item:last-of-type label`,
      // Second item has input description linked to input
      `.multi-select-container .multi-select-item:last-of-type input[aria-describedby="checkbox-2-description"]`,
      `.multi-select-container .multi-select-item:last-of-type p#checkbox-2-description`,
    ],
    // Unexpected selectors
    [
      // First item has no description paragraph or aria-describedby attribute
      `.multi-select-container .multi-select-item:first-of-type p`,
      `.multi-select-container .multi-select-item:first-of-type input[aria-describedby*="-description"]`,
    ]
  );
});
