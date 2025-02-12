// META: resource=support/test-partitioned-access-in-different-origin-iframes.https.sub.html
// META: resource=support/test-partitioned-access-in-same-origin-iframes.https.sub.html
// META: resource=support/test-partitioned-iframes-in-different-origin-windows.https.sub.html
// META: resource=support/test-partitioned-iframes-in-same-origin-windows.https.sub.html
// META: resource=support/test-partitioned-parent-reader-and-different-origin-window-writer-iframe.https.sub.html
// META: resource=support/test-partitioned-parent-writer-and-different-origin-window-reader-iframe.https.sub.html
// META: resource=support/test-read-and-notify-worker.https.html
// META: resource=support/test-read-and-notify-wrapper.https.sub.html
// META: resource=support/test-read-and-notify.https.html
// META: resource=support/test-read-and-notify.js
// META: resource=support/test-write-and-notify-worker.https.html
// META: resource=support/test-write-and-notify-wrapper.https.sub.html
// META: resource=support/test-write-and-notify.https.html
// META: resource=support/test-write-and-notify.js
// META: script=/resources/testharness.js
// META: script=/resources/testharnessreport.js
// META: script=support/testHelpers.js

/**
 * BucketFS
 */

// [ write: A, read: A ]
promise_test(
  expectNamesForTestWindow(
    ["SameOriginIFramesWithFS", "WorkerSameOriginIFramesWithFS"],
    "support/test-partitioned-access-in-same-origin-iframes.https.sub.html?api=FS"
  ),
  "iframes of origin A in a window of origin B can access the same data with BucketFS"
);

// [ write: A, read: B ]
promise_test(
  expectNamesForTestWindow(
    ["DifferentOriginIFramesWithFS", "WorkerDifferentOriginIFramesWithFS"],
    "support/test-partitioned-access-in-different-origin-iframes.https.sub.html?api=FS"
  ),
  "iframes of origin A and B are isolated with BucketFS"
);

// [ write: B[A], read: B[A] ]
promise_test(
  expectNamesForTestWindow(
    ["SameOriginWindowsWithFS", "WorkerSameOriginWindowsWithFS"],
    "support/test-partitioned-iframes-in-same-origin-windows.https.sub.html?api=FS"
  ),
  "iframes of origin A in two child windows of origin B can access the same data with BucketFS"
);

// [ write: A[A], read: B[A] ]
promise_test(
  expectNamesForTestWindow(
    ["DifferentOriginWindowsWithFS", "WorkerDifferentOriginWindowsWithFS"],
    "support/test-partitioned-iframes-in-different-origin-windows.https.sub.html?api=FS"
  ),
  "iframe of origin A in a child window of origin B cannot read data written by iframe of origin A in a child window of origin A with BucketFS"
);

// [ write: A, read: B[A] ]
promise_test(
  expectNamesForTestWindow(
    [
      "ReadIFrameWriteDifferentOriginWindowWithFS",
      "WorkerReadIFrameWriteDifferentOriginWindowWithFS",
    ],
    "support/test-partitioned-parent-reader-and-different-origin-window-writer-iframe.https.sub.html?api=FS"
  ),
  "iframe of origin A in a child window of origin B cannot cannot read data by written by iframe of origin A in the parent window with BucketFS"
);

// [ write: B[A], read: A ]
promise_test(
  expectNamesForTestWindow(
    [
      "WriteIFrameReadDifferentOriginWindowWithFS",
      "WorkerWriteIFrameReadDifferentOriginWindowWithFS",
    ],
    "support/test-partitioned-parent-writer-and-different-origin-window-reader-iframe.https.sub.html?api=FS"
  ),
  "iframe of origin A in the parent window cannot read data written by iframe of origin A in child a window of origin B with BucketFS"
);
