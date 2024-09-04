var min = gcparam('minEmptyChunkCount');

gcparam('minEmptyChunkCount', 10);
assertEq(gcparam('minEmptyChunkCount'), 10);
gc();

gcparam('minEmptyChunkCount', 5);
assertEq(gcparam('minEmptyChunkCount'), 5);
gc();

gcparam('minEmptyChunkCount', min);
assertEq(gcparam('minEmptyChunkCount'), min);
gc();
