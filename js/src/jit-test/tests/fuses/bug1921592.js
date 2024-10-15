// |jit-test| --fuzzing-safe; --no-threads; --ion-eager;

g = newGlobal();
g.eval(`
 Iterator.prototype.return = () => { return { value: 1, done: true } };
 for (let i = 0; i < 100; i++) {
     let [x] = (function () {
         return [0];
     })();
 }
`)
