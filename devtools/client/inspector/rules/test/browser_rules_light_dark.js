/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for light-dark() in rule view.

const TEST_URI = `data:text/html,<meta charset=utf8>
  <style>
    @media not (prefers-color-scheme: dark) {
      html {
        color-scheme: light;
      }
    }

    @media (prefers-color-scheme: dark) {
      html {
        color-scheme: dark;
      }
    }

    h1 {
      background-color: light-dark(gold, tomato);
    }

    .light {
      color-scheme: light;
      color: light-dark(red, blue);
      border-color: light-dark(pink, cyan);
    }

    .dark {
      color-scheme: dark;
      background: linear-gradient(
        light-dark(blue, darkblue),
        light-dark(red, darkred)
      );
    }
  </style>
  <h1>Hello</h1>
  <div class="light">
    <pre>color-scheme: light</pre>
    <div class="dark">
      <pre>color-scheme: dark</pre>
    </div>
  </div>`;

add_task(async function () {
  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();

  info("Select node with color-scheme: light");
  await selectNode(".light", inspector);

  // `color: light-dark(red, blue)`
  assertColorSpans(view, ".light", "color", [
    { color: "red", matched: true },
    { color: "blue", matched: false },
  ]);

  // `border-color: light-dark(pink, cyan)`
  assertColorSpans(view, ".light", "border-color", [
    { color: "pink", matched: true },
    { color: "cyan", matched: false },
  ]);

  info("Select node with color-scheme: dark");
  await selectNode(".dark", inspector);

  // `background: linear-gradient(light-dark(blue, darkblue),light-dark(red, darkred))`
  assertColorSpans(view, ".dark", "background", [
    { color: "blue", matched: false },
    { color: "darkblue", matched: true },
    { color: "red", matched: false },
    { color: "darkred", matched: true },
  ]);

  // We show the inherited rule for color, which is matched by `.light`, so here, even if
  // the node has a dark color scheme, light-dark should mark second parameters as unmatched
  // `color: light-dark(red, blue)`
  assertColorSpans(view, ".light", "color", [
    { color: "red", matched: true },
    { color: "blue", matched: false },
  ]);

  info("Select node without explicit color-scheme property");
  // h1 has no color-scheme so it inherits from the html one, which depends on the actual
  // media query value. light-dark in the associated rule should be updated when toggling
  // simulation.
  await selectNode("h1", inspector);

  // Read the color scheme to know if we should click on the light or dark button
  // to trigger a change.
  info("Enable light mode simulation if needed");
  const isDarkScheme = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    () => {
      return content.matchMedia("(prefers-color-scheme: dark)").matches;
    }
  );
  if (isDarkScheme) {
    const onRuleViewRefreshed = view.once("ruleview-refreshed");
    inspector.panelDoc
      .querySelector("#color-scheme-simulation-light-toggle")
      .click();
    await onRuleViewRefreshed;
  }

  // `background-color: light-dark(gold, tomato)`
  assertColorSpans(view, "h1", "background-color", [
    { color: "gold", matched: true },
    { color: "tomato", matched: false },
  ]);

  info("Trigger dark mode simulation");
  const onRuleViewRefreshed = view.once("ruleview-refreshed");
  inspector.panelDoc
    .querySelector("#color-scheme-simulation-dark-toggle")
    .click();
  await onRuleViewRefreshed;

  // `background-color: light-dark(gold, tomato)`
  assertColorSpans(view, "h1", "background-color", [
    { color: "gold", matched: false },
    { color: "tomato", matched: true },
  ]);
});

function assertColorSpans(view, ruleSelector, propertyName, expectedColors) {
  const colorSpans = getRuleViewProperty(
    view,
    ruleSelector,
    propertyName
  ).valueSpan.querySelectorAll("span:has(> .ruleview-color)");
  is(
    colorSpans.length,
    expectedColors.length,
    "Got expected number of color spans"
  );
  for (let i = 0; i < expectedColors.length; i++) {
    const colorSpan = colorSpans[i];
    const expectedColorData = expectedColors[i];
    is(
      colorSpan.textContent,
      expectedColorData.color,
      `Expected color ${i} for ${ruleSelector} ${propertyName}`
    );
    is(
      !colorSpan.classList.contains("inspector-unmatched"),
      expectedColorData.matched,
      `Color ${i} for ${ruleSelector} ${propertyName} is ${
        expectedColorData.matched ? "matched" : "unmatched"
      }`
    );
  }
}
