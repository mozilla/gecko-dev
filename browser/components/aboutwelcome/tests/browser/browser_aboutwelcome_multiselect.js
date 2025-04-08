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

const PICKER_CONTENT = {
  id: "MULTI_SELECT_TEST",
  targeting: "true",
  content: {
    fullscreen: true,
    position: "split",
    progress_bar: true,
    logo: {},
    tiles: [
      {
        type: "multiselect",
        multiSelectItemDesign: "picker",
        subtitle: { raw: "What are you using Firefox for?" },
        data: [
          {
            id: "checkbox-school",
            defaultValue: false,
            pickerEmoji: "🎓",
            pickerEmojiBackgroundColor: "#c3e0ff",
            label: {
              raw: "School",
            },
            checkedAction: {
              type: "SET_PREF",
              data: {
                pref: {
                  name: "onboarding-personalization.school",
                  value: true,
                },
              },
            },
            uncheckedAction: {
              type: "SET_PREF",
              data: {
                pref: {
                  name: "onboarding-personalization.school",
                  value: false,
                },
              },
            },
          },
        ],
      },
    ],
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

/**
 * Test multiselect styles with picker configuration
 */
add_task(async function test_picker_multiselect_styles() {
  const TEST_JSON = JSON.stringify([PICKER_CONTENT]);
  let browser = await openAboutWelcome(TEST_JSON);

  await test_screen_content(
    browser,
    "renders screen with a picker checklist item",
    // Expected selectors:
    [
      // multiselect container has picker class
      `.multi-select-container.picker`,
      // Checkbox container should have role, tabindex, aria-checked properties
      `.checkbox-container[role="checkbox"]`,
      `.checkbox-container[tabIndex="0"]`,
      `.checkbox-container[aria-checked="false"]`,
    ],
    // Unexpected selectors
    [
      // Hidden input should be unchecked
      `input[type="checkbox"]:checked`,
    ]
  );

  // Hidden input should indeed be hidden
  await test_element_styles(browser, ".checkbox-container input", {
    width: "0px",
    height: "0px",
    opacity: "0",
  });

  // Picker icon background color should match passed value
  await test_element_styles(browser, ".picker-icon", {
    backgroundColor: "rgb(195, 224, 255)",
  });
});
