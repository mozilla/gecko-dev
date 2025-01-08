// |jit-test| --disable-main-thread-denormals; skip-if: !getBuildConfiguration("can-disable-main-thread-denormals");

assertEq(5E-324, 0);
