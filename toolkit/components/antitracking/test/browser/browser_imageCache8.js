ChromeUtils.import("resource://gre/modules/Services.jsm");

let cookieBehavior = BEHAVIOR_REJECT_TRACKER;
let blockingByContentBlockingRTUI = true;
let blockingByAllowList = true;
let expectedBlockingNotifications = Ci.nsIWebProgressListener.STATE_COOKIES_BLOCKED_TRACKER;

let rootDir = getRootDirectory(gTestPath);
let jar = getJar(rootDir);
if (jar) {
  let tmpdir = extractJarToTmp(jar);
  rootDir = "file://" + tmpdir.path + "/";
}
/* import-globals-from imageCacheWorker.js */
Services.scriptloader.loadSubScript(rootDir + "imageCacheWorker.js", this);

