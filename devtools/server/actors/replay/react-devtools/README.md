# React Devtools Integration

The files in this directory are from the React Devtools (https://github.com/facebook/react/tree/master/packages/react-devtools), and are loaded into recording processes so that the devtools hooks will be detected by any React application on the page and allow events to be sent to the driver and from there on to any clients viewing the recording.

From the base React revision 0203b6567c6fd6274866c853ef938241d24551ec, the other files are as follows:

### contentScript.js

Modified from `react/packages/react-devtools-extensions/src/contentScript.js`

### hook.js

Modified from `packages/react-devtools-shared/src/hook.js`

### react_devtools_backend.js

After building React, this is modified from the generated file `packages/react-devtools-extensions/firefox/build/unpacked/build/react_devtools_backend.js` with the patch below.

```
@@ -1,3 +1,5 @@
+function reactDevtoolsBackend(window) {
+
 /******/ (function(modules) { // webpackBootstrap
 /******/ 	// The module cache
 /******/ 	var installedModules = {};
@@ -9738,6 +9740,18 @@
     bridge.send('isSynchronousXHRSupported', Object(utils["g" /* isSynchronousXHRSupported */])());
     setupHighlighter(bridge, this);
     TraceUpdates_initialize(this);
+
+    // Hook for sending messages via record/replay evaluations.
+    window.__RECORD_REPLAY_REACT_DEVTOOLS_SEND_MESSAGE__ = (inEvent, inData) => {
+      let rv;
+      this._bridge = {
+        send(event, data) {
+          rv = { event, data };
+        }
+      };
+      this[inEvent](inData);
+      return rv;
+    };
   }
 
   get rendererInterfaces() {
@@ -10469,7 +10483,7 @@
 // This is to avoid issues like: https://github.com/facebook/react-devtools/issues/1039
 
 function welcome(event) {
-  if (event.source !== window || event.data.source !== 'react-devtools-content-script') {
+  if (event.data.source !== 'react-devtools-content-script') {
     return;
   }
 
@@ -10511,13 +10525,8 @@
     },
 
     send(event, payload, transferable) {
-      window.postMessage({
-        source: 'react-devtools-bridge',
-        payload: {
-          event,
-          payload
-        }
-      }, '*', transferable);
+      // Synchronously notify the record/replay driver.
+      window.__RECORD_REPLAY_REACT_DEVTOOLS_SEND_BRIDGE__(event, payload);
     }
 
   });
@@ -14460,3 +14469,7 @@
 
 /***/ })
 /******/ ]);
+
+}
+
+exports.reactDevtoolsBackend = reactDevtoolsBackend;
```