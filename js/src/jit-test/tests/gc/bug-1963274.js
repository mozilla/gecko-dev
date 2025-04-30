try {
  startgc(0, "shrinking");
  function f70(bailout) {
    for (var i = 10000; i != -100000; i--)
      arguments[i] = 0;
  }
  f70();
} catch (exc) {}
