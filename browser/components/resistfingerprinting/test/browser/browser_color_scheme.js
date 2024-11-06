/**
 * Bug 1912129 - A test case for verifying color scheme
 * is spoofed.
 */

function getBgColor(tab) {
  const extractBgColor = function () {
    const css = `
      @media (prefers-color-scheme: light) {
        body {
          background: rgb(3, 2, 1);
        }
      }

      @media (prefers-color-scheme: dark) {
        body {
          background: rgb(1, 2, 3);
        }
      }
    `;

    const styleSheet = document.createElement("style");
    styleSheet.textContent = css;
    document.head.appendChild(styleSheet);

    return getComputedStyle(document.body).backgroundColor;
  };

  const extractBgColorExpr = `(${extractBgColor.toString()})();`;

  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [extractBgColorExpr],
    async funccode => content.eval(funccode)
  );
}

function getMatchMedia(tab) {
  const extractMatchMedia = function () {
    return matchMedia("(prefers-color-scheme: light)").matches;
  };

  const extractMatchMediaExpr = `(${extractMatchMedia.toString()})();`;

  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [extractMatchMediaExpr],
    async funccode => content.eval(funccode)
  );
}

function getBgColorInOpenWindow(tab) {
  const extractBgColor = async function () {
    const popup = open("about:blank", "popup", "width=100,height=100");
    const func = function () {
      const css = `
        @media (prefers-color-scheme: light) {
          body {
            background: rgb(3, 2, 1);
          }
        }
  
        @media (prefers-color-scheme: dark) {
          body {
            background: rgb(1, 2, 3);
          }
        }
      `;

      const styleSheet = document.createElement("style");
      styleSheet.textContent = css;
      document.head.appendChild(styleSheet);

      return getComputedStyle(document.body).backgroundColor;
    };

    const script = document.createElement("script");
    script.innerHTML = `window.func = ${func.toString()}`;
    popup.document.head.appendChild(script);

    const bgColor = await popup.func();
    popup.close();

    return bgColor;
  };

  const extractBgColorExpr = `(${extractBgColor.toString()})();`;

  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [extractBgColorExpr],
    async funccode => content.eval(funccode)
  );
}

add_task(async function test_color_scheme() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.fingerprintingProtection", true],
      ["privacy.fingerprintingProtection.overrides", "+CSSPrefersColorScheme"],
    ],
  });

  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: TEST_PATH + "file_dummy.html",
  });

  const bgColor = await getBgColor(tab);
  is(bgColor, "rgb(3, 2, 1)", "The background color is light");

  const matches = await getMatchMedia(tab);
  is(matches, true, "The media query matches light");

  const bgColorInOpenWindow = await getBgColorInOpenWindow(tab);
  is(
    bgColorInOpenWindow,
    "rgb(3, 2, 1)",
    "The background color is light in open window"
  );

  BrowserTestUtils.removeTab(tab);
  await SpecialPowers.popPrefEnv();
});
