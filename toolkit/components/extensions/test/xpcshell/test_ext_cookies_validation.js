"use strict";

add_task(
  { pref_set: [["extensions.cookie.rejectWhenInvalid", true]] },
  async function test_no_reject_invalid_cookies() {
    await do_test_invalid_cookies({ failure: true });
  }
);

add_task(
  { pref_set: [["extensions.cookie.rejectWhenInvalid", false]] },
  async function test_warn_on_invalid_cookies() {
    await do_test_invalid_cookies({ failure: false });
  }
);

async function do_test_invalid_cookies(options) {
  async function backgroundScript() {
    browser.test.onMessage.addListener(async message => {
      let failure = true;
      try {
        await browser.cookies.set({
          ...message.cookie,
          url: "https://example.com",
        });
        failure = false;
      } catch (e) {
        browser.test.assertEq(
          e.message,
          message.errorString,
          `${message.title} - correct exception`
        );
      } finally {
        browser.test.assertEq(failure, message.failure, message.title);
        browser.test.sendMessage("completed");
      }
    });

    browser.test.sendMessage("ready");
  }

  const extension = ExtensionTestUtils.loadExtension({
    background: backgroundScript,
    manifest: {
      permissions: ["cookies", "https://example.com/*"],
    },
  });

  let readyPromise = extension.awaitMessage("ready");
  await extension.startup();
  await readyPromise;

  const tests = [
    {
      cookie: { name: "", value: "" },
      title: "Empty name and value",
      errorString:
        "Cookie with an empty name and an empty value has been rejected.",
    },
    {
      cookie: { name: "a".repeat(3000), value: "a".repeat(3000) },
      title: "Name/value oversize",
      errorString: `Cookie “${"a".repeat(3000)}” is invalid because its size is too big. Max size is 4096 B.`,
    },
    {
      cookie: { name: ";", value: "a" },
      title: "Invalid chars in the name",
      errorString:
        "Cookie “;” has been rejected for invalid characters in the name.",
    },
    {
      cookie: { name: " ", value: "a" },
      title: "Invalid chars in the name (2)",
      errorString:
        "Cookie “ ” has been rejected for invalid characters in the name.",
    },
    {
      cookie: { name: "a", value: ";" },
      title: "Invalid chars in the value",
      errorString:
        "Cookie “a” has been rejected for invalid characters in the value.",
    },
    {
      cookie: { name: "a", value: " " },
      title: "Invalid chars in the value (2)",
      errorString:
        "Cookie “a” has been rejected for invalid characters in the value.",
    },
    {
      cookie: { name: "", value: "__Secure-wow" },
      title: "Invalid prefix (__Secure)",
      errorString: "Cookie “” has been rejected for invalid prefix.",
    },
    {
      cookie: { name: "", value: "__Host-wow" },
      title: "Invalid prefix (__Host)",
      errorString: "Cookie “” has been rejected for invalid prefix.",
    },
    {
      cookie: { name: "a", value: "b", sameSite: "no_restriction" },
      title: "None requires secure",
      errorString:
        "Cookie “a” rejected because it has the “SameSite=None” attribute but is missing the “secure” attribute.",
    },
    {
      cookie: { name: "a", value: "b", path: "a".repeat(1025) },
      title: "Path oversize",
      errorString:
        "Cookie “a” has been rejected because its path attribute is too big.",
    },
  ];

  for (const test of tests) {
    extension.sendMessage({ ...test, ...options });
    await extension.awaitMessage("completed");
  }

  await extension.unload();
}
