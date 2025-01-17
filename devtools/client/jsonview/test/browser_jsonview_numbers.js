/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  await addJsonViewTab(
    `data:application/json,{
      "small": 12,
      "negzero": -0,
      "big": 1516340399466235648,
      "precise": 3.141592653589793238462643383279,
      "exp": 1e2
    }`
  );

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const rows = content.document.querySelectorAll(
      ".jsonPanelBox .treeTable .treeRow"
    );
    is(rows.length, 5, "There is 5 properties");

    rows.forEach((row, i) => {
      ok(
        !row.querySelector(".theme-twisty"),
        `item #${i} doesn't have an expand button`
      );
    });

    info("Checking that regular numbers aren't rendered with JsonNumber");
    is(
      rows[0].querySelector(".treeLabel").textContent,
      "small",
      "Got expected first item"
    );
    const smallValueEl = rows[0].querySelector(".objectBox-number");
    is(smallValueEl.textContent, "12", "First item has expected text");
    ok(
      !smallValueEl.classList.contains("objectBox-json-number") &&
        !smallValueEl.querySelector(".source-value"),
      "Regular number does not get the lossy class"
    );

    info("Checking that negative numbers aren't rendered with JsonNumber");
    is(
      rows[1].querySelector(".treeLabel").textContent,
      "negzero",
      "Got expected second item"
    );
    const negZeroValueEl = rows[1].querySelector(".objectBox-number");
    is(negZeroValueEl.textContent, "-0", "-0 has expected text");
    ok(
      !negZeroValueEl.classList.contains("objectBox-json-number"),
      "-0 does not get the lossy class"
    );

    info("Checkt numbers bigger than Number.MAX_SAFE_INTEGER");
    is(
      rows[2].querySelector(".treeLabel").textContent,
      "big",
      "Got expected third item"
    );
    const bigValueEl = rows[2].querySelector(".objectBox-number");
    ok(
      bigValueEl.classList.contains("objectBox-json-number"),
      "Big number get the lossy class"
    );
    is(
      bigValueEl.querySelector(".source-value").textContent,
      "1516340399466235648",
      "Big number has expected source text"
    );
    is(
      bigValueEl.querySelector(".parsed-value").textContent,
      "JS:1516340399466235600",
      "Big number has expected parsed value text"
    );
    ok(
      bigValueEl.querySelector(".parsed-value").getAttribute("title"),
      "Big number parsed value label has a title attribute"
    );

    info("Check numbers with higher precision than what's possible in JS");
    is(
      rows[3].querySelector(".treeLabel").textContent,
      "precise",
      "Got expected fourth item"
    );
    const preciseValueEl = rows[3].querySelector(".objectBox-number");
    ok(
      preciseValueEl.classList.contains("objectBox-json-number"),
      "High precision number get the lossy class"
    );
    is(
      preciseValueEl.querySelector(".source-value").textContent,
      "3.141592653589793238462643383279",
      "High precision number has expected source text"
    );
    is(
      preciseValueEl.querySelector(".parsed-value").textContent,
      "JS:3.141592653589793",
      "High precision number has expected parsed value text"
    );
    ok(
      preciseValueEl.querySelector(".parsed-value").getAttribute("title"),
      "High precision number parsed value label has a title attribute"
    );

    info("Checking that exponential numbers source is displayed");
    is(
      rows[4].querySelector(".treeLabel").textContent,
      "exp",
      "Got expected fourth item"
    );
    const expValueEl = rows[4].querySelector(".objectBox-number");
    ok(
      expValueEl.classList.contains("objectBox-json-number"),
      "Exponential number get the lossy class"
    );
    is(
      expValueEl.querySelector(".source-value").textContent,
      "1e2",
      "Exponential number has expected source text"
    );
    is(
      expValueEl.querySelector(".parsed-value").textContent,
      "JS:100",
      "Exponential number has expected parsed value text"
    );
    ok(
      expValueEl.querySelector(".parsed-value").getAttribute("title"),
      "Exponential number parsed value label has a title attribute"
    );
  });
});
