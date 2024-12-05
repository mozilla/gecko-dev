/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Check if errors are logged as expected

"use strict";

const TEST_URI = `data:text/html;charset=utf8,<!DOCTYPE html>Errors`;

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  const TEST_DATA = [
    {
      desc: "ReferenceError: asdf is not defined",
      expression: `
        function bar() {
          asdf()
        }
        function foo() {
          bar()
        }

        foo()`,
      expected: `Uncaught ReferenceError: asdf is not defined`,
    },

    {
      desc: "SyntaxError: redeclaration of let a",
      expression: `let a, a;`,
      expected: `SyntaxError: redeclaration of let a`,
      assert: messageEl => {
        info("Check that error notes are displayed");
        const notes = messageEl.querySelectorAll(".error-note .message-body");
        is(notes.length, 1, "There's only one note");
        is(notes[0].innerText, "note: Previously declared at line 1, column 5");
      },
    },

    {
      desc: "TypeError longString message",
      expression: `throw new Error("Long error ".repeat(10000))`,
      expected: `Uncaught Error: Long error Long error`,
    },

    {
      desc: `throw string with URL`,
      expression: `throw "“https://evil.com/?${"a".repeat(
        200
      )}“ is evil and “https://not-so-evil.com/?${"b".repeat(
        200
      )}“ is not good either"`,
      expected: `Uncaught “https://evil.com/?${"a".repeat(
        200
      )}“ is evil and “https://not-so-evil.com/?${"b".repeat(
        200
      )}“ is not good either`,
      assert: messageEl => {
        const links = messageEl.querySelectorAll(".message-body a.cropped-url");
        is(links.length, 2, "2 links are displayed");

        const evilURL = `https://evil.com/?${"a".repeat(200)}`;
        const badURL = `https://not-so-evil.com/?${"b".repeat(200)}`;

        is(
          links[0].getAttribute("href"),
          evilURL,
          "first link has expected href"
        );
        is(
          links[0].getAttribute("title"),
          evilURL,
          "first link has expected title"
        );

        is(
          links[1].getAttribute("href"),
          badURL,
          "second link has expected href"
        );
        is(
          links[1].getAttribute("title"),
          badURL,
          "second link has expected title"
        );
      },
    },

    {
      desc: `throw ""`,
      expression: `throw ""`,
      expected: `Uncaught <empty string>`,
    },
    {
      desc: `throw "tomato"`,
      expression: `throw "tomato"`,
      expected: `Uncaught tomato`,
    },
    {
      desc: `throw false`,
      expression: `throw false`,
      expected: `Uncaught false`,
    },
    { desc: `throw 0`, expression: `throw 0`, expected: `Uncaught 0` },
    { desc: `throw null`, expression: `throw null`, expected: `Uncaught null` },
    {
      desc: `throw undefined`,
      expression: `throw undefined`,
      expected: `Uncaught undefined`,
    },
    {
      desc: `throw Symbol`,
      expression: `throw Symbol("potato")`,
      expected: `Uncaught Symbol("potato")`,
    },
    {
      desc: `throw Object`,
      expression: `throw {vegetable: "cucumber"}`,
      expected: `Uncaught Object { vegetable: "cucumber" }`,
    },
    {
      desc: `throw Error Object`,
      expression: `throw new Error("pumpkin")`,
      expected: `Uncaught Error: pumpkin`,
    },
    {
      desc: `throw Error Object with custom name`,
      expression: `
        var err = new Error("pineapple");
        err.name = "JuicyError";
        err.flavor = "delicious";
        throw err;`,
      expected: `Uncaught JuicyError: pineapple`,
    },
    {
      desc: `throw Error Object with error cause`,
      expression: `
        var originalError = new SyntaxError("original error")
        var err = new Error("something went wrong", {
          cause: originalError
        });
        throw err;`,
      expected: `Uncaught Error: something went wrong`,
      assert: messageEl => {
        const causeEl = messageEl.querySelector(".error-rep-cause");
        is(
          causeEl.innerText,
          `Caused by: SyntaxError: original error`,
          "Caused by show expected"
        );
      },
    },
    {
      desc: `throw Error Object with cause chain`,
      expression: `
        var a = new Error("err-a")
        var b = new Error("err-b", { cause: a })
        var c = new Error("err-c", { cause: b })
        var d = new Error("err-d", { cause: c })
        throw d;`,
      expected: `Uncaught Error: err-d`,
      assert: messageEl => {
        const causeEl = messageEl.querySelector(".error-rep-cause");
        is(
          causeEl.innerText,
          [
            "Caused by: Error: err-c",
            "Caused by: Error: err-b",
            "Caused by: Error: err-a",
          ].join("\n"),
          "The cause chain is properly displayed"
        );
      },
    },
    {
      desc: `throw Error Object with cyclical cause chain`,
      expression: `
        var a = new Error("err-a", { cause: b})
        var b = new Error("err-b", { cause: a })
        throw b;`,
      expected: `Uncaught Error: err-b`,
      assert: messageEl => {
        const causeEl = messageEl.querySelector(".error-rep-cause");
        is(
          causeEl.innerText,
          // TODO: This is not how we should display cyclical cause chain, but we have it here
          // to ensure it's displaying something that makes _some_ sense.
          // This should be properly handled in Bug 1719605.
          [
            "Caused by: Error: err-a",
            "Caused by: Error: err-b",
            "Caused by: Error: err-a",
          ].join("\n"),
          "The cyclical cause chain is properly displayed"
        );
      },
    },
    {
      desc: `throw Error Object with falsy cause`,
      expression: `throw new Error("null cause", { cause: null });`,
      expected: `Uncaught Error: null cause`,
      assert: messageEl => {
        const causeEl = messageEl.querySelector(".error-rep-cause");
        is(
          causeEl.innerText,
          "Caused by: null",
          "The null cause is properly displayed"
        );
      },
    },
    {
      desc: `throw Error Object with number cause`,
      expression: `throw new Error("number cause", { cause: 0 });`,
      expected: `Uncaught Error: number cause`,
      assert: messageEl => {
        const causeEl = messageEl.querySelector(".error-rep-cause");
        is(
          causeEl.innerText,
          "Caused by: 0",
          "The 0 cause is properly displayed"
        );
      },
    },
    {
      desc: `throw Error Object with string cause`,
      expression: `throw new Error("string cause", { cause: "cause message" });`,
      expected: `Uncaught Error: string cause`,
      assert: messageEl => {
        const causeEl = messageEl.querySelector(".error-rep-cause");
        is(
          causeEl.innerText,
          `Caused by: "cause message"`,
          "The string cause is properly displayed"
        );
      },
    },
    {
      desc: `throw Error Object with object cause`,
      expression: `throw new Error("object cause", { cause: { code: 234, message: "ERR_234"} });`,
      expected: `Uncaught Error: object cause`,
      assert: messageEl => {
        const causeEl = messageEl.querySelector(".error-rep-cause");
        is(
          causeEl.innerText,
          `Caused by: Object { … }`,
          "The object cause is properly displayed"
        );
      },
    },
    {
      desc: `Promise reject ""`,
      expression: `Promise.reject("")`,
      expected: `Uncaught (in promise) <empty string>`,
    },
    {
      desc: `Promise reject "tomato"`,
      expression: `Promise.reject("tomato")`,
      expected: `Uncaught (in promise) tomato`,
    },
    {
      desc: `Promise reject false`,
      expression: `Promise.reject(false)`,
      expected: `Uncaught (in promise) false`,
    },
    {
      desc: `Promise reject 0`,
      expression: `Promise.reject(0)`,
      expected: `Uncaught (in promise) 0`,
    },
    {
      desc: `Promise reject null`,
      expression: `Promise.reject(null)`,
      expected: `Uncaught (in promise) null`,
    },
    {
      desc: `Promise reject undefined`,
      expression: `Promise.reject(undefined)`,
      expected: `Uncaught (in promise) undefined`,
    },
    {
      desc: `Promise reject Symbol`,
      expression: `Promise.reject(Symbol("potato"))`,
      expected: `Uncaught (in promise) Symbol("potato")`,
    },
    {
      desc: `Promise reject Object`,
      expression: `Promise.reject({vegetable: "cucumber"})`,
      expected: `Uncaught (in promise) Object { vegetable: "cucumber" }`,
    },
    {
      desc: `Promise reject Error Object`,
      expression: `Promise.reject(new Error("pumpkin"))`,
      expected: `Uncaught (in promise) Error: pumpkin`,
    },
    {
      desc: `Promise reject Error Object with custom name`,
      expression: `
        var err = new Error("pineapple");
        err.name = "JuicyError";
        err.flavor = "delicious";
        Promise.reject(err);`,
      expected: `Uncaught (in promise) JuicyError: pineapple`,
    },
    {
      desc: `Promise reject Error Object with error cause`,
      expression: `Promise.resolve().then(() => {
        try {
          unknownFunc();
        } catch(e) {
          throw new Error("something went wrong", { cause: e })
        }
      })`,
      expected: `Uncaught (in promise) Error: something went wrong`,
      assert: messageEl => {
        const causeEl = messageEl.querySelector(".error-rep-cause");
        is(
          causeEl.innerText,
          `Caused by: ReferenceError: unknownFunc is not defined`,
          "The cause is properly displayed"
        );
      },
    },
  ];

  // javascript.options.experimental.explicit_resource_management is set to true, but it's
  // only supported on Nightly at the moment
  if (AppConstants.ENABLE_EXPLICIT_RESOURCE_MANAGEMENT) {
    TEST_DATA.push({
      desc: `SuppressedError`,
      expression: `throw new SuppressedError(
          new Error("foo"),
          new Error("bar"),
          "the suppressed error message"
        )`,
      expected: `Uncaught SuppressedError: the suppressed error message`,
    });
  }

  for (const { desc, expression, expected, assert } of TEST_DATA) {
    info(`Check error: ${desc}`);

    const onErrorLogged = waitForMessageByType(hud, expected, ".error");

    await SpecialPowers.spawn(
      gBrowser.selectedBrowser,
      [expression],
      function (expr) {
        const script = content.document.createElement("script");
        script.append(content.document.createTextNode(expr));
        content.document.body.append(script);
        script.remove();
      }
    );

    const message = await onErrorLogged;
    if (assert) {
      assert(message.node);
    }
  }
});
