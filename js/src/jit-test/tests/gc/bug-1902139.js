gc();
gczeal(11);
schedulezone({});
startgc(1000);
while (gcstate() !== "NotActive") {
  gcslice(1000);
}
