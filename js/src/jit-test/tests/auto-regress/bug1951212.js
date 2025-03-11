// |jit-test| --ion-edgecase-analysis=off

for (var i = 0 ; i < 99 ; i++) {
  (function() { return Math.fround(Math.sqrt(0)) == false; })();
}
