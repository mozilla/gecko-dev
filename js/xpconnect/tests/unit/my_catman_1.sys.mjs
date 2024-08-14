export let Module1 = {
  test(arg) {
    Services.obs.notifyObservers(null, "test-modules-from-catman-notification", arg);
  },
};
