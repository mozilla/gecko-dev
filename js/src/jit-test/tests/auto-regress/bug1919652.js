// |jit-test| --fast-warmup; --no-threads;

oomTest(function () {
  2n % 2n;
  var x = (function () {
    return 0;
  })();
  for (var i = 0; i < 99; i++) {
    x++;
  }
});
