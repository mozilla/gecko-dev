export let Module1 = {
  test(arg) {
    Services.obs.notifyObservers(
      null,
      "test-modules-from-catman-notification",
      arg
    );
  },
  throwingFunction() {
    throw new Error("Uh oh. I have a bad feeling about this.");
  },
};
