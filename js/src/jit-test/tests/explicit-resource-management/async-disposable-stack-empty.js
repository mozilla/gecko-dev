// |jit-test| skip-if: !getBuildConfiguration("explicit-resource-management")


async function testAsyncDisposableStackEmpty() {
  {
    await using stack = new AsyncDisposableStack();
  }
}

testAsyncDisposableStackEmpty();
drainJobQueue();
