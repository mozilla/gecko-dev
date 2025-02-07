/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { BrowserTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/BrowserTestUtils.sys.mjs"
);

add_task(async function testMozPageNavResponsive() {
  let { html, render } = await import(
    "chrome://global/content/vendor/lit.all.mjs"
  );
  let template = html`
    <link rel="stylesheet" href="chrome://global/skin/in-content/common.css" />
    <div style="width: 200px; height: 100vh;">
      <moz-page-nav heading="Test">
        <moz-page-nav-button
          view="view-one"
          iconSrc="chrome://mozapps/skin/extensions/category-extensions.svg"
        >
          <span class="view-name">View 1</span>
        </moz-page-nav-button>
        <moz-page-nav-button
          view="view-two"
          iconSrc="chrome://mozapps/skin/extensions/category-extensions.svg"
        >
          <span class="view-name">View 2</span>
        </moz-page-nav-button>
        <moz-page-nav-button
          iconSrc="chrome://mozapps/skin/extensions/category-extensions.svg"
          support-page="test"
          slot="secondary-nav"
        >
          <span class="view-name">Support Link</span>
        </moz-page-nav-button>
      </moz-page-nav>
      <moz-page-nav heading="Test without icons">
        <moz-page-nav-button view="view-one">
          <span class="view-name">View 1 without icon</span>
        </moz-page-nav-button>
        <moz-page-nav-button view="view-two">
          <span class="view-name">View 2 without icon</span>
        </moz-page-nav-button>
        <moz-page-nav-button support-page="test" slot="secondary-nav">
          <span class="view-name">Support Link without icon</span>
        </moz-page-nav-button>
      </moz-page-nav>
    </div>
  `;

  // Render the template and wait for all elements to update once.
  let renderTarget = document.createElement("div");
  document.body.append(renderTarget);
  render(template, renderTarget);
  await Promise.all(
    [...renderTarget.children].flatMap(pageNav =>
      [...pageNav.children].map(item => item.updateComplete)
    )
  );

  let win = BrowserWindowTracker.getTopWindow();
  let [firstNav, secondNav] = document.querySelectorAll("moz-page-nav");
  let getNavButtons = nav => [
    ...nav.pageNavButtons,
    ...nav.secondaryNavButtons,
  ];

  const BASE_FONT_SIZE = 15;
  const ORIGINAL_WINDOW_WIDTH = win.outerWidth;

  // Nav layout changes happen at 52em window width, and we're assuming 15px font size.
  const LARGE_WINDOW_WIDTH = BASE_FONT_SIZE * 100;
  const SMALL_WINDOW_WIDTH = BASE_FONT_SIZE * 25;

  // Ensure the window is wide enough before running the first tests.
  let resizedPromise = BrowserTestUtils.waitForEvent(win, "resize");
  win.resizeTo(LARGE_WINDOW_WIDTH, win.outerHeight);
  await resizedPromise;

  function verifyExpandedDisplay({ nav, hasIcons }) {
    let navButtons = getNavButtons(nav);
    ok(
      BrowserTestUtils.isVisible(nav.headingEl),
      "Heading text is visible in larger windows."
    );
    navButtons.forEach(button => {
      let textEl = button.shadowRoot.querySelector("slot");
      ok(
        BrowserTestUtils.isVisible(textEl),
        "Buttons have visible text in larger windows."
      );
    });
    if (hasIcons) {
      ok(
        navButtons.every(button =>
          button.shadowRoot.querySelector(".page-nav-icon")
        ),
        "Buttons have icons in larger windows when iconSrc is provided."
      );
    }
  }

  verifyExpandedDisplay({ nav: firstNav, hasIcons: true });
  verifyExpandedDisplay({ nav: secondNav, hasIcons: false });

  // Resize to smaller width to test how the nav displays.
  resizedPromise = BrowserTestUtils.waitForEvent(win, "resize");
  win.resizeTo(SMALL_WINDOW_WIDTH, win.outerHeight);
  await resizedPromise;

  function verifySmallerDisplay({ nav, hasIcons }) {
    let navButtons = getNavButtons(nav);
    ok(
      !BrowserTestUtils.isVisible(nav.headingEl),
      "Heading text is not visible in smaller windows."
    );
    if (hasIcons) {
      ok(
        navButtons.every(button =>
          button.shadowRoot.querySelector(".page-nav-icon")
        ),
        "Buttons have icons in smaller windows when iconSrc is provided."
      );
      navButtons.forEach(button => {
        let textEl = button.shadowRoot.querySelector("slot");
        ok(
          !BrowserTestUtils.isVisible(textEl),
          "Buttons do not have visible text in smaller windows when iconSrc is provided."
        );
      });
    } else {
      navButtons.forEach(button => {
        let textEl = button.shadowRoot.querySelector("slot");
        ok(
          BrowserTestUtils.isVisible(textEl),
          "Buttons have visible text in smaller windows when iconSrc is not provided."
        );
      });
    }
  }

  verifySmallerDisplay({ nav: firstNav, hasIcons: true });
  verifySmallerDisplay({ nav: secondNav, hasIcons: false });

  // Reset window dimensions to avoid impacting subsequent tests.
  resizedPromise = BrowserTestUtils.waitForEvent(win, "resize");
  win.resizeTo(ORIGINAL_WINDOW_WIDTH, win.outerHeight);
  await resizedPromise;
});
