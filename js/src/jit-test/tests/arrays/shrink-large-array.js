// Shrink a large dense elements allocation of ~2MB to half its size.
const sizeMB = 2;
const elementCount = (sizeMB * 1024 * 1024) / 8;
const array = new Array(elementCount);
for (let i = 0; i < elementCount; i++) {
  array[i] = i;
}
array.length = elementCount / 2;
