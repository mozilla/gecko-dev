"use strict";

const BASE_CONTENT = {
  id: "EMBEDDED_BROWSER",
  content: {
    tiles: {
      type: "embedded_browser",
      data: {
        style: {
          width: "100%",
          height: "200px",
        },
        url: "https://example.com",
      },
    },
  },
};

/**
 * Confirm that the embedded browser tile does not render
 * when XUL elements are not available
 */
add_task(
  async function test_aboutwelcome_does_not_render_embedded_browser_in_content() {
    const TEST_JSON = JSON.stringify([BASE_CONTENT]);
    let browser = await openAboutWelcome(TEST_JSON);

    await test_screen_content(
      browser,
      "does not render embedded browser",
      // Expected selectors:
      [".main-content"],
      // Unexpected selectors:
      [`div.embedded-browser-container`]
    );
  }
);
