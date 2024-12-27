/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Browser test for `L10nCache` in `UrlbarUtils.sys.mjs`.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  L10nCache: "resource:///modules/UrlbarUtils.sys.mjs",
});

let gL10n;

add_setup(async function () {
  // Set up a mock `Localization`.
  let { l10n, cleanup } = initL10n({
    args0: "Zero args value",
    args0Markup: "Another <strong>zero args</strong> value",
    args1: "One arg value is { $arg1 }",
    args1Markup: "Another one arg value <strong>is { $arg1 }</strong>",
    attrs0: [".label = attrs0 label has zero args"],
    attrs0Markup: [
      ".label = Markup <strong>should not be parsed</strong> in attributes",
    ],
    attrs1: [
      ".label = attrs1 label has zero args",
      ".tooltiptext = attrs1 tooltiptext arg value is { $arg1 }",
    ],
  });
  gL10n = l10n;
  registerCleanupFunction(cleanup);
});

add_task(async function comprehensive() {
  // This task runs a bunch of subtests that call `setElementL10n()` on a span a
  // few times. Each subtest object has the following properties:
  //
  // {object} l10n
  //   An l10n object to pass to `L10nCache.setElementL10n()` and other
  //   `L10nCache` methods. It should never contain `cacheable`.
  // {Function} assert
  //   This function is called as `await assert(span)` at the end of each
  //   subtest and should check final state of the span after the cached string
  //   specified by `l10n` has been set on it. It should check the span's child
  //   nodes or attributes as appropriate for the subtest.
  let tests = [
    {
      l10n: {
        id: "args0",
      },
      assert: span => {
        checkChildren(span, [
          { name: "#text", textContent: "Zero args value" },
        ]);
      },
    },

    {
      l10n: {
        id: "args0Markup",
        // no `parseMarkup: true`
      },
      assert: span => {
        checkChildren(span, [
          {
            name: "#text",
            textContent: "Another <strong>zero args</strong> value",
          },
        ]);
      },
    },

    {
      l10n: {
        id: "args0Markup",
        parseMarkup: true,
      },
      assert: span => {
        checkChildren(span, [
          { name: "#text", textContent: "Another " },
          { name: "strong", textContent: "zero args" },
          { name: "#text", textContent: " value" },
        ]);
      },
    },

    {
      l10n: {
        id: "args1",
        args: { arg1: "foo" },
      },
      assert: span => {
        checkChildren(span, [
          { name: "#text", textContent: "One arg value is foo" },
        ]);
      },
    },

    {
      l10n: {
        id: "args1Markup",
        args: { arg1: "bar" },
        // no `parseMarkup: true`
      },
      assert: span => {
        checkChildren(span, [
          {
            name: "#text",
            textContent: "Another one arg value <strong>is bar</strong>",
          },
        ]);
      },
    },

    {
      l10n: {
        id: "args1Markup",
        args: { arg1: "bar" },
        parseMarkup: true,
      },
      assert: span => {
        checkChildren(span, [
          { name: "#text", textContent: "Another one arg value " },
          { name: "strong", textContent: "is bar" },
        ]);
      },
    },

    {
      l10n: {
        id: "attrs0",
        attribute: "label",
      },
      assert: span => {
        checkAttributes(span, {
          label: "attrs0 label has zero args",
        });
      },
    },

    {
      l10n: {
        id: "attrs0Markup",
        attribute: "label",
        // no `parseMarkup: true`
      },
      assert: span => {
        checkAttributes(span, {
          label: "Markup <strong>should not be parsed</strong> in attributes",
        });
      },
    },

    {
      l10n: {
        id: "attrs0Markup",
        attribute: "label",
        // `parseMarkup` should be ignored since `attribute` is defined
        parseMarkup: true,
      },
      assert: span => {
        checkAttributes(span, {
          label: "Markup <strong>should not be parsed</strong> in attributes",
        });
      },
    },

    {
      l10n: {
        id: "attrs1",
        attribute: "label",
        args: { arg1: "foo" },
      },
      assert: span => {
        checkAttributes(span, {
          label: "attrs1 label has zero args",
        });
      },
    },

    {
      l10n: {
        id: "attrs1",
        attribute: "tooltiptext",
        args: { arg1: "foo" },
      },
      assert: span => {
        checkAttributes(span, {
          tooltiptext: "attrs1 tooltiptext arg value is foo",
        });
      },
    },
  ];

  let cache = new L10nCache(gL10n);

  for (let { l10n, assert } of tests) {
    info("Doing subtest: " + JSON.stringify(l10n));

    let span = document.createElement("span");

    // Set the string without caching it. (We assume none of the `l10n` objects
    // in `tests` have `cacheable` set, which they shouldn't.)
    await cache.setElementL10n(span, l10n);
    Assert.ok(!cache.get(l10n), "String should not be cached");
    Assert.equal(
      span.dataset.l10nId,
      l10n.id,
      "span.dataset.l10nId should be set"
    );
    if (l10n.attribute) {
      Assert.equal(
        span.dataset.l10nAttrs,
        l10n.attribute,
        "span.dataset.l10nAttrs should be set"
      );
    }

    // Set the string again but cache it this time. `setElementL10n()` will
    // cache the string but not wait for it to finish being cached and then
    // immediately look up the string in the cache, so it may or may not use the
    // cached value at this point.
    await cache.setElementL10n(span, {
      ...l10n,
      cacheable: true,
    });
    Assert.ok(cache.get(l10n), "String should be cached");

    // Set the string again. It's definitely cached now, so `setElementL10n()`
    // should use the cached value.
    let cachePromise = cache.setElementL10n(span, {
      ...l10n,
      cacheable: true,
    });
    Assert.ok(cache.get(l10n), "String should still be cached");
    for (let a of ["data-l10n-id", "data-l10n-attrs", "data-l10n-args"]) {
      Assert.ok(!span.hasAttribute(a), "Attribute should be unset: " + a);
    }
    await assert(span);
    await cachePromise;

    cache.clear();
  }
});

add_task(async function removeElementL10n() {
  let cache = new L10nCache(gL10n);
  let span = document.createElement("span");

  // Call `setElementL10n()`. It should set l10n attributes on the span since
  // the string isn't cached.
  let l10n = {
    id: "attrs1",
    attribute: "tooltiptext",
    args: { arg1: "foo" },
  };
  await cache.setElementL10n(span, l10n);

  Assert.equal(
    span.dataset.l10nId,
    l10n.id,
    "span.dataset.l10nId should be set"
  );
  Assert.equal(
    span.dataset.l10nAttrs,
    l10n.attribute,
    "span.dataset.l10nAttrs should be set"
  );
  Assert.equal(
    span.dataset.l10nArgs,
    JSON.stringify(l10n.args),
    "span.dataset.l10nArgs should be set"
  );

  // Call `removeElementL10n()`. It should remove the l10n attributes.
  cache.removeElementL10n(span, l10n);
  Assert.equal(span.textContent, "", "textContent should be empty");
  for (let a of ["data-l10n-id", "data-l10n-attrs", "data-l10n-args"]) {
    Assert.ok(!span.hasAttribute(a), "Attribute should be unset: " + a);
  }
});

// Tests arg updates w/r/t `excludeArgsFromCacheKey`.
add_task(async function excludeArgsFromCacheKey() {
  // This task is more of a real-world test than the others. It relies on the
  // test element being present in a document so that the automatic translation
  // behavior of `DOMLocalization` is triggered. We'll use the current browser
  // document and insert a span in the document element.
  let cache = new L10nCache(document.l10n);
  let span = document.createElement("span");
  document.documentElement.append(span);
  registerCleanupFunction(() => span.remove());

  // We'll also use a real string since that's easier than trying to inject mock
  // strings. We need a string with an argument since we're specifically testing
  // arg updates and `excludeArgsFromCacheKey`.
  let id = "urlbar-result-action-search-w-engine";
  let arg = "engine";
  let value = a => `Search with ${a}`;

  // Call `setElementL10n()` with an initial arg value for the string.
  // `setElementL10n()` should set l10n attributes on the span since the string
  // isn't cached.
  await cache.setElementL10n(span, {
    id,
    args: { [arg]: "aaa" },
    cacheable: true,
    excludeArgsFromCacheKey: true,
  });

  Assert.equal(span.dataset.l10nId, id, "span.dataset.l10nId should be set");
  Assert.equal(
    span.dataset.l10nArgs,
    JSON.stringify({ [arg]: "aaa" }),
    "span.dataset.l10nArgs should be set"
  );
  Assert.deepEqual(
    cache.get({ id }),
    {
      attributes: null,
      value: value("aaa"),
    },
    "String should be cached with 'aaa' arg"
  );

  // Call `setElementL10n()` again but with a different arg value. It should do
  // three things: (1) Cache the new value of the string but not wait for that
  // to finish, (2) immediately set the span's `textContent` to the currently
  // cached string, which remains the old value, and (3) set the span's l10n
  // attributes so that `DOMLocalization` will generate the string's new value
  // and assign it to the span's `textContent` when translation is done.
  let cachePromise = cache.setElementL10n(span, {
    id,
    args: { [arg]: "bbb" },
    cacheable: true,
    excludeArgsFromCacheKey: true,
  });

  Assert.equal(
    span.textContent,
    value("aaa"),
    "span.textContent should be the old cached value with 'aaa'"
  );
  Assert.equal(span.dataset.l10nId, id, "span.dataset.l10nId should be set");
  Assert.equal(
    span.dataset.l10nArgs,
    JSON.stringify({ [arg]: "bbb" }),
    "span.dataset.l10nArgs should be set with the new 'bbb' arg value"
  );

  // The new string value should be cached.
  await cachePromise;
  Assert.deepEqual(
    cache.get({ id }),
    {
      attributes: null,
      value: value("bbb"),
    },
    "String should be cached with 'bbb' arg"
  );

  // `DOMLocalization` should update the span's `textContent` with a new string
  // value containing the new 'bbb' arg value.
  await TestUtils.waitForCondition(() => {
    info("Waiting for new textContent, current is: " + span.textContent);
    return span.textContent == value("bbb");
  }, "Waiting for new textContent with 'bbb' arg value");
  Assert.equal(
    span.textContent,
    value("bbb"),
    "span.textContent should have the new 'bbb' arg value"
  );

  span.remove();
});

/**
 * Sets up a mock localization.
 *
 * @param {object} pairs
 *   Fluent strings as key-value pairs.
 * @returns {object}
 *   An object `{ l10n, cleanup }`
 *
 *   {Localization} l10n
 *     The mock `Localization` object.
 *   {Function} cleanup
 *     This should be called when you're done with the l10n object.
 */
function initL10n(pairs) {
  let source = Object.entries(pairs)
    .map(([key, value]) => {
      if (Array.isArray(value)) {
        value = value.map(s => "  \n" + s).join("");
      }
      return `${key} = ${value}`;
    })
    .join("\n");

  let registry = new L10nRegistry();
  registry.registerSources([
    L10nFileSource.createMock(
      "test",
      "app",
      ["en-US"],
      "/localization/{locale}",
      [{ source, path: "/localization/en-US/test.ftl" }]
    ),
  ]);

  return {
    l10n: new Localization(["/test.ftl"], true, registry, ["en-US"]),
    cleanup: () => {
      registry.removeSources(["test"]);
    },
  };
}

function checkChildren(element, expected) {
  let children = [...element.childNodes].map(n => ({
    name: n.nodeName,
    textContent: n.textContent,
  }));
  Assert.deepEqual(children, expected, "Children should be correct");
}

function checkAttributes(element, expected) {
  let attrs = {};
  for (let i = 0; i < element.attributes.length; i++) {
    let a = element.attributes.item(i);
    attrs[a.name] = a.value;
  }
  Assert.deepEqual(attrs, expected, "Attributes should be correct");
}
