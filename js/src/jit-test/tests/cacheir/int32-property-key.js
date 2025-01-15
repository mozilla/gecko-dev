function test(n) {
  // Test keys from -n to n.
  // We will attach n GetDynamicSlot stubs for negative values,
  // and one GetDenseElementResult stub for non-negative values.
  let arr = [];
  for (let i = -n; i <= n; i++) {
    arr[i] = i;
  }
  let range = n * 2 + 1;

  for (let i = 0; i < 10 * range; i++) {
    let index = i % range - n;
    arr[index] = arr[index] + 1;
  }

  for (let i = -n * 2; i <= n * 2; i++) {
    let shouldContain = i >= -n && i <= n;
    assertEq(i in arr, shouldContain);
    if (shouldContain) {
      assertEq(arr[i], i + 10);
    }
  }

}

// Specialized
test(2);

// Megomorphic
test(10);
