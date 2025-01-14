// |jit-test| --disable-decommit

// Disable decommit and generate a bunch of garbage buffer allocations.

gczeal(10);
const count = 10000;
const length = 1000;
const a = [];
for (let i = 0; i < count; i++) {
  new Array(length);
  a[i] = new Array(length);
}
