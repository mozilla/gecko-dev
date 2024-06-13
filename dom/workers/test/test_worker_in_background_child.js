onmessage = async e => {
  testStage = e.data;
  switch (e.data) {
    case "CheckIsBackground":
      if (WorkerTestUtils.IsRunningInBackground()) {
        postMessage({ stage: testStage, status: "PASS", msg: "" });
      } else {
        postMessage({
          stage: testStage,
          status: "FAIL",
          msg: `wrong flag`,
        });
      }
      break;

    case "CheckIsForeground":
    case "CheckIsForegroundAgain":
      if (!WorkerTestUtils.IsRunningInBackground()) {
        postMessage({ stage: testStage, status: "PASS", msg: "" });
      } else {
        postMessage({
          stage: testStage,
          status: "FAIL",
          msg: `wrong flag`,
        });
      }
      break;
  }
};
