[//]: # (
  This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
)

# Upgrading dagre-d3.js

Get latest release from https://github.com/dagrejs/dagre-d3.
Copy dagre-d3.js in devtools/client/shared/vendor.

## Patch

This library used to create elements with inline styles, which we forbid
in our CSP. Make sure to remove lines responsible for adding inline styles.

With the current version, the following diff was applied:

```
diff --git a/devtools/client/shared/vendor/dagre-d3.js b/devtools/client/shared/vendor/dagre-d3.js
--- a/devtools/client/shared/vendor/dagre-d3.js
+++ b/devtools/client/shared/vendor/dagre-d3.js
@@ -386,7 +386,6 @@ function defaultPostRender(graph, root)
           .attr('markerWidth', 8)
           .attr('markerHeight', 5)
           .attr('orient', 'auto')
-          .attr('style', 'fill: #333')
           .append('svg:path')
             .attr('d', 'M 0 0 L 10 5 L 0 10 z');
   }
```
