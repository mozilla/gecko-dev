// |jit-test| allow-oom

// Exercise growing a very large array.

const growBy = 100000;
const startSize = 1 * 1024 * 1024;
const endSize = 32 * 1024 * 1024;

const array = new Array();
array.fill(1);
const extra = new Array(growBy);
extra.fill(1);
while (array.length < endSize) {
  array.push(...extra);
}
