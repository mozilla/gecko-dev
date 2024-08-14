export let Module2 = {
  othertest(arg) {
    Services.obs.notifyObservers(null, "test-modules-from-catman-other-notification", arg);
  },
};

