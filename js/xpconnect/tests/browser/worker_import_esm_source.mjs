const { testSourceAccess } = ChromeUtils.importESModule(
  "chrome://mochitests/content/browser/js/xpconnect/tests/browser/worker_source.mjs",
  { global: "current" }
);

self.postMessage({
  string: testSourceAccess.toString(),
});
