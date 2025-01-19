gczeal(21);
for (
  let i = 300n; i--;
  (function () {
    let { ...x } = this;
  })()
) {}
