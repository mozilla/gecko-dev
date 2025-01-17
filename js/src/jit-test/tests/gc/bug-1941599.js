gczeal(4);
function c() {
  let {
    object: d,
    transplant
  } = transplantableObject();
  for (let i = 0; i < 3000; i++) {
    d[i] = transplant(this);
  }
}
c();
