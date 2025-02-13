/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Check that the font editor has a section for "All fonts" which shows all fonts
// used on the page.

const TEST_URI = URL_ROOT_SSL + "doc_browser_fontinspector.html";

add_task(async function () {
  const { view } = await openFontInspectorForURL(TEST_URI);
  const viewDoc = view.document;

  const allFontsAccordion = getFontsAccordion(viewDoc);
  ok(allFontsAccordion, "There's an accordion in the panel");
  is(
    allFontsAccordion.textContent,
    "All Fonts on Page",
    "It has the right title"
  );

  await expandAccordion(allFontsAccordion);
  const allFontsEls = getAllFontsEls(viewDoc);

  const ostrichMetadata = [
    {
      name: "Version:",
      value: "1.000",
    },
    {
      name: "Designer:",
      value: "Tyler Finck",
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      href: "http://www.sursly.com/",
    },
    {
      name: "Manufacturer:",
      value: "Tyler Finck",
    },
    {
      name: "Vendor:",
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      value: "http://www.sursly.com",
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      href: "http://www.sursly.com/",
    },
    {
      name: "Description:",
      value: "Copyright (c) 2011 by Tyler Finck. All rights reserved.",
    },
    {
      name: "License:",
      value: "Just let me know if and where you use it! Enjoy :)",
    },
    {
      name: "License Info URL:",
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      value: "http://www.sursly.com",
      // eslint-disable-next-line @microsoft/sdl/no-insecure-url
      href: "http://www.sursly.com/",
    },
  ];

  const MPLExcerpt =
    "The licenses granted in this Section 2 are the only rights granted under this License. No additional rights or licenses will be implied from the distribution or licensing of Covered Software under this License. Notwithstanding Section 2.1(b) above, no patent license is granted by a Contributor:      for any code that a Contributor has removed from Covered Software; or      for infringements caused by: (i) Your and any other third party’s modifications of Covered Software, or (ii) the combination of its Contributions with other software (except as part of its Contributor Version); or      under Patent Claims infringed by Covered Software in the absence of its Contributions.  This License does not grant any rights in the trademarks, service marks, or logos of any Contributor (except as may be necessary to comply with the notice requirements in Section 3.4).";

  const EXPECTED_FONTS = [
    {
      familyName: "DevToolsMono",
      name: "DevToolsMono Regular",
      remote: true,
      url: URL_ROOT_SSL + "devtools-mono.otf",
      metadata: [
        {
          name: "Version:",
          // Font version is `Version 0.1 early alpha`, so this checks that we properly
          // strip the "Version" prefix
          value: "0.1 early alpha",
        },
        {
          name: "Designer:",
          value: "Ötto Land",
          // No Designer URL in the font info so we can check that this isn't rendered
          // as a link
          href: null,
        },
        {
          name: "Manufacturer:",
          value: "Mozilla",
        },
        {
          name: "Vendor:",
          value: "https://www.mozilla.org",
          href: "https://www.mozilla.org/",
        },
        {
          name: "Description:",
          value: "This is the font description.".repeat(20),
          truncated: true,
        },
        {
          name: "License:",
          value: MPLExcerpt,
          truncated: true,
        },
        {
          name: "License Info URL:",
          value: "https://www.mozilla.org/en-US/MPL/2.0/",
          href: "https://www.mozilla.org/en-US/MPL/2.0/",
        },
      ],
    },
    {
      familyName: "bar",
      name: "Ostrich Sans Medium",
      remote: true,
      url: URL_ROOT_SSL + "ostrich-regular.ttf",
      metadata: ostrichMetadata,
    },
    {
      familyName: "bar",
      name: "Ostrich Sans Black",
      remote: true,
      url: URL_ROOT_SSL + "ostrich-black.ttf",
      metadata: ostrichMetadata,
    },
    {
      familyName: "bar",
      name: "Ostrich Sans Black",
      remote: true,
      url: URL_ROOT_SSL + "ostrich-black.ttf",
      metadata: ostrichMetadata,
    },
    {
      familyName: "barnormal",
      name: "Ostrich Sans Medium",
      remote: true,
      url: URL_ROOT_SSL + "ostrich-regular.ttf",
      metadata: ostrichMetadata,
    },
    {
      // On Linux, Arial does not exist. Liberation Sans is used instead.
      familyName: ["Arial", "Liberation Sans"],
      name: ["Arial", "Liberation Sans"],
      remote: false,
      url: "system",
    },
    {
      // On Linux, Times New Roman does not exist. Liberation Serif is used instead.
      familyName: ["Times New Roman", "Liberation Serif"],
      name: ["Times New Roman", "Liberation Serif"],
      remote: false,
      url: "system",
    },
  ];

  is(allFontsEls.length, EXPECTED_FONTS.length, "All fonts used are listed");

  for (let i = 0; i < EXPECTED_FONTS.length; i++) {
    const li = allFontsEls[i];
    const expectedFont = EXPECTED_FONTS[i];

    const fontName = getName(li);
    if (Array.isArray(expectedFont.name)) {
      ok(
        expectedFont.name.includes(fontName),
        `The DIV font has the right name - Got "${fontName}", expected one of ${JSON.stringify(expectedFont.name)}`
      );
    } else {
      is(fontName, expectedFont.name, `The DIV font has the right name`);
    }

    info(fontName);
    const fontFamilyName = getFamilyName(li);
    if (Array.isArray(expectedFont.familyName)) {
      ok(
        expectedFont.familyName.includes(fontFamilyName),
        `font has the right family name - Got "${fontFamilyName}", expected one of ${JSON.stringify(expectedFont.familyName)}`
      );
    } else {
      is(
        fontFamilyName,
        expectedFont.familyName,
        `font has the right family name`
      );
    }

    info(fontFamilyName);
    is(isRemote(li), expectedFont.remote, `font remote value correct`);
    is(getURL(li), expectedFont.url, `font url correct`);

    if (expectedFont.metadata) {
      const dts = li.querySelectorAll("dt");
      const dds = li.querySelectorAll("dd");
      is(
        dts.length,
        expectedFont.metadata.length,
        `"${expectedFont.name}" - Got expected number of metadata rows`
      );
      for (let j = 0; j < expectedFont.metadata.length; j++) {
        const expectedData = expectedFont.metadata[j];
        const dtText = dts[j].textContent;
        is(
          dtText,
          expectedData.name,
          `"${expectedFont.name}" - Got expected data for row #${j}`
        );
        const dd = dds[j];
        const linkEl = dd.querySelector("a");
        if (expectedData.href) {
          isnot(
            linkEl,
            null,
            `"${expectedFont.name}" - Value of row ${j} ("${dtText}") is a link`
          );
          is(
            linkEl.textContent,
            expectedData.value,
            `"${expectedFont.name}" - Got expected data value for row #${j} ("${dtText}")`
          );
          const { link } = await simulateLinkClick(linkEl);
          is(
            link,
            expectedData.href,
            `"${expectedFont.name}" - Clicking on link on row #${j} ("${dtText}") navigates to expected link`
          );
        } else {
          if (expectedData.truncated) {
            is(
              dd.textContent,
              expectedData.value.substring(0, 250),
              `"${expectedFont.name}" - Got expected truncated data value for row #${j} ("${dtText}")`
            );
            is(
              getTwistyToggle(dd).getAttribute("aria-expanded"),
              "false",
              "The twisty is collapsed by default"
            );

            info(
              `"${expectedFont.name}" - Click the string expander for row #${j} ("${dtText}")`
            );
            const expanderEl = getTruncatedStringExpander(dd);
            expanderEl.click();
            await waitFor(() => !getTruncatedStringExpander(dd));
            ok(true, "The expander gets hidden once it's clicked");
            is(
              getTwistyToggle(dd).getAttribute("aria-expanded"),
              "true",
              `"${expectedFont.name}" - The twisty is expanded once the expander was clicked`
            );
            is(
              dd.textContent,
              expectedData.value,
              `"${expectedFont.name}" - Got expected full data value for row #${j} ("${dtText}") after expander was clicked`
            );

            info(
              `"${expectedFont.name}" - Click the twisty toggle for row #${j} ("${dtText}")`
            );
            getTwistyToggle(dd).click();
            await waitFor(() => getTruncatedStringExpander(dd));
            ok(
              true,
              `"${expectedFont.name}" - The expander gets displayed again after clicking the twisty`
            );
            is(
              getTwistyToggle(dd).getAttribute("aria-expanded"),
              "false",
              `"${expectedFont.name}" - The twisty is collapsed again`
            );
            is(
              dd.textContent,
              expectedData.value.substring(0, 250),
              `"${expectedFont.name}" - Got expected truncated data value again for row #${j} ("${dtText}")`
            );
          } else {
            is(
              dd.textContent,
              expectedData.value,
              `"${expectedFont.name}" - Got expected data value for row #${j} ("${dtText}")`
            );
          }

          is(
            linkEl,
            null,
            `"${expectedFont.name}" - Value of row ${j} ("${dtText}") isn't a link`
          );
        }
      }
    }
  }
});

function getTruncatedStringExpander(parentEl) {
  return parentEl.querySelector("button.font-truncated-string-expander");
}

function getTwistyToggle(parentEl) {
  return parentEl.querySelector("button.theme-twisty");
}
