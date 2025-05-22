// |jit-test| --fuzzing-safe; --ion-offthread-compile=off; skip-if: helperThreadCount() === 0;
evalInWorker(`
  a = {
    then() {
      b
    }
  }
  Promise.any([a])
  c
`)

