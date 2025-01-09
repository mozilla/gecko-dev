function testMaxLength() {
  let arr = [];
  const MAX_ARRAY_INDEX = 2 ** 32 - 2;
  arr[MAX_ARRAY_INDEX] = 1;
  arr[MAX_ARRAY_INDEX + 1] = 1;
  assertEq(arr.length, MAX_ARRAY_INDEX + 1);
  arr.length = 0;
  assertEq(arr[MAX_ARRAY_INDEX], undefined);
  assertEq(arr[MAX_ARRAY_INDEX + 1], 1);
  assertEq(Object.keys(arr).length, 1);
}
testMaxLength();
