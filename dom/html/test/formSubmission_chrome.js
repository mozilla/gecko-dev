const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.importGlobalProperties(["File"]);

addMessageListener("files.open", function (message) {
  sendAsyncMessage("files.opened", message.map(path => new File(path)));
});
