gczeal(0);
gcparam("highFrequencyTimeLimit", 1000);

assertEq(gcparam('highFrequencyMode'), 0);

gc();
gc();
assertEq(gcparam('highFrequencyMode'), 1);

sleep(1.1);
assertEq(gcparam('highFrequencyMode'), 1);
gc();
assertEq(gcparam('highFrequencyMode'), 0);
