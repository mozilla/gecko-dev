function waitForBackground(isBackground, testStage) {
  if (WorkerTestUtils.IsRunningInBackground() != isBackground) {
    setTimeout(waitForBackground, 100, isBackground);
  } else {
    postMessage({
      stage: testStage,
      status: "PASS",
      msg: "",
    });
  }
}

onmessage = async e => {
  switch (e.data) {
    case "CheckIsBackground":
      waitForBackground(true, e.data);
      break;

    case "CheckIsForeground":
    case "CheckIsForegroundAgain":
      waitForBackground(false, e.data);
      break;
  }
};
