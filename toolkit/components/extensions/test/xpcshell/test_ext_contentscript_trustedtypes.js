"use strict";

/* globals exportFunction, browserTestSendMessage, browserTestAssertEq */

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

add_setup(() => {
  Services.prefs.setBoolPref("dom.security.trusted_types.enabled", true);
});

async function test_contentscript_trusted_types(manifest) {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://example.com/*/file_trusted_types.html"],
          run_at: "document_end",
          js: ["content_script_isolated.js"],
          world: "ISOLATED",
        },
        {
          matches: ["http://example.com/*/file_trusted_types.html"],
          run_at: "document_idle",
          js: ["content_script_main.js"],
          world: "MAIN",
        },
      ],
      ...manifest,
    },
    files: {
      async "content_script_isolated.js"() {
        // Make these functions usable in content_scripts_main.js
        exportFunction(browser.test.sendMessage, window, {
          defineAs: "browserTestSendMessage",
        });
        exportFunction(browser.test.assertEq, window, {
          defineAs: "browserTestAssertEq",
        });

        function onPostMessage(message) {
          return new Promise(resolve => {
            function listener(event) {
              if (event.data === message) {
                resolve(event.data);
                window.removeEventListener("message", event);
              }
            }
            window.addEventListener("message", listener);
          });
        }

        let SINKS = {
          "script-text": {
            element: "script",
            property: "text",
            value: `window.postMessage("script-text", "*");`,
          },
          "script-innerText": {
            element: "script",
            property: "innerText",
            value: `window.postMessage("script-innerText", "*");`,
          },
          "script-src": {
            element: "script",
            property: "src",
            attribute: "src",
            value: browser.runtime.getURL("/test_script_src.js"),
          },
          "iframe-srcdoc": {
            element: "iframe",
            property: "srcdoc",
            attribute: "srcdoc",
            value: `<script>window.parent.postMessage("iframe-srcdoc", "*");</script>`,
          },
        };

        for (const [
          message,
          { element, property, attribute, value },
        ] of Object.entries(SINKS)) {
          let runTest = async fun => {
            let elem = document.createElement(element);
            document.body.append(elem);
            let messagePromise = onPostMessage(message);
            fun(elem);
            browser.test.assertEq(await messagePromise, message);
            elem.remove();
          };

          if (property) {
            await runTest(elem => (elem[property] = value));
          }

          if (attribute) {
            await runTest(elem => elem.setAttribute(attribute, value));

            await runTest(elem => {
              let attr = document.createAttribute(attribute);
              attr.value = value;
              elem.setAttributeNode(attr);
            });

            await runTest(elem => {
              let attr = document.createAttribute(attribute);
              attr.value = value;
              elem.attributes.setNamedItem(attr);
            });
          }
        }

        let div = document.createElement("div");
        document.body.append(div);

        // innerHTML sink
        div.innerHTML = "<b>Hello</b>";
        browser.test.assertEq(div.outerHTML, "<div><b>Hello</b></div>");

        // outerHTML sink
        div.firstChild.outerHTML = "<i>World</i>";
        browser.test.assertEq(div.innerHTML, "<i>World</i>");

        // setHTMLUnsafe sink
        div.setHTMLUnsafe("<em>Foo</em>");
        browser.test.assertEq(div.innerHTML, "<em>Foo</em>");

        /* TODO(Bug 1963277)
        // Worker sink
        let worker = new Worker("data:text/javascript,self.postMessage('worker-script');");
        let workerMessage = new Promise(resolve => worker.addEventListener("message", evt => resolve(evt.data), {once: true}));
        browser.test.assertEq(await workerMessage, "worker-script");
        */

        browser.test.sendMessage("content_script_isolated.js-finished");
      },
      "content_script_main.js"() {
        function assertThrows(fun) {
          let error = undefined;
          try {
            fun();
          } catch (e) {
            error = e;
          }

          browserTestAssertEq(error?.name, "TypeError");
        }

        let script = document.createElement("script");
        assertThrows(() => {
          script.text = "foo";
        });
        assertThrows(() => {
          script.innerText = "foo";
        });
        assertThrows(() => {
          script.src = "http://example.com/script.js";
        });
        assertThrows(() => {
          script.setAttribute("src", "http://example.org/script.js");
        });

        let iframe = document.createElement("iframe");
        assertThrows(() => {
          iframe.srcdoc = "foo";
        });

        let div = document.createElement("div");
        assertThrows(() => {
          div.innerHTML = "foo";
        });
        assertThrows(() => {
          div.outerHTML = "foo";
        });
        assertThrows(() => div.setHTMLUnsafe("foo"));

        assertThrows(() => new Worker("data:text/javascript,void 0"));

        browserTestSendMessage("content_script_main.js-finished");
      },
      "test_script_src.js"() {
        window.postMessage("script-src", "*");
      },
    },
  });

  await extension.startup();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    `http://example.com/data/file_trusted_types.html`
  );

  await Promise.all([
    extension.awaitMessage("content_script_isolated.js-finished"),
    extension.awaitMessage("content_script_main.js-finished"),
  ]);

  await extension.unload();
  await contentPage.close();
}

add_task(async function test_contentscript_trusted_types_v2() {
  await test_contentscript_trusted_types({
    manifest_version: 2,
    web_accessible_resources: ["test_script_src.js"],
  });
});

add_task(async function test_contentscript_trusted_types_v3() {
  await test_contentscript_trusted_types({
    manifest_version: 3,
    web_accessible_resources: [
      {
        resources: ["test_script_src.js"],
        matches: ["http://example.com/*"],
      },
    ],
  });
});
