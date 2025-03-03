// Ensure the species fuse triggers invalidation.

function useSpecies(arr, func) {
  return arr.map(func);
}
function f() {
  with (this) {} // Don't Ion-compile.

  var arr = [1, 2, 3];
  var func = x => x;

  var counter = 0;
  for (var i = 0; i < 5000; i++) {
    useSpecies(arr, func);
    if (i === 4800) {
      Object.defineProperty(Array.prototype, "constructor", {get: function() {
        counter++;
        return Array;
      }});
    }
  }
  assertEq(counter, 199);
}
f();
