// |jit-test| --disable-main-thread-denormals; skip-if: !getBuildConfiguration("x64") && !getBuildConfiguration("arm") && !getBuildConfiguration("arm64");

assertEq(5E-324, 0);
