if (!this.enqueueMark) {
  quit();
}

gczeal(0);
setMarkStackLimit(1)
a = {}
enqueueMark(a)
gczeal(9)
gc();
