const { foo } = ChromeUtils.importESModule("chrome://mochitests/content/browser/devtools/client/debugger/test/mochitest/examples/worker-esm-dep.mjs",
  { global: "current" }
);
// Using a regular import doesn't reproduce bug 1907977 issue:
// import { foo } from "./worker-esm-dep.mjs";

console.log("Worker ESM main script", foo);
